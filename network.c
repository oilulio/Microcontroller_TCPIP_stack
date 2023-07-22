/********************************************
 Code for Network layer protocols IPV4, ARP, ICMP (Ping)

Copyright (C) 2009-19  S Combes

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

 
 Called from Transport layer (UDP, TCP) via "Launch_as_IP4".

 Calls Data Link Layer (Ethernet) specific to either
 
 1. ENC28J60
 2. ISA card interface

 ARP not usually called directly.  On receipt of UDP or TDP
 ARP will be called if required to get MAC.  Maintains 
 table of IP<->MAC with timeouts.  ARP also called (if DHCP fails)
 to establish that IP address not in use

  ********************************************

N.B.   lcd_puts_P("...") uses less 
SRAM than strcpy to buffer                     */

#include "config.h"

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "network.h"
#include "stepper.h" 
#include "link.h"
#include "lfsr.h"

static void learnMAC(IP4_address IP,MAC_address MAC);
static int8_t knownMAC(const IP4_address * target);
static uint8_t MAC_match(const MAC_address * MAC1, const MAC_address * MAC2);

extern MAC_address BroadcastMAC,myMAC;
extern IP4_address NullIP,BroadcastIP,myIP,GWIP,DNSIP,NTPIP,subnetMask,subnetBroadcastIP;
extern char buffer[MSG_LENGTH]; // messages
extern MergedPacket MashE;  // This is the ephemeral Mash.  Used for input, UDP and initial TCP
extern Status MyState;

uint8_t known_IP_addresses=0;
const IP4_address mDNS_IP4 =MAKEIP4(0xE0,0,0,0xFB); 
const IP4_address LLMNR_IP4=MAKEIP4(0xE0,0,0,0xFC); 
uint16_t IP4_ID;     // IP4 Packet ID
uint16_t UDP_Port[MAX_UDP_PORTS];
uint16_t UDP_low_port;
uint8_t DHCP_lease[4];
volatile uint8_t timecount;
TCP_TCB TCB[MAX_TCP_ROLES];  // One for client, one for server

struct ARP_store {
	IP4_address knownIP[MAX_ARP_HELD];
	MAC_address knownMAC[MAX_ARP_HELD];
	int8_t ticks[MAX_ARP_HELD];
} ARP_held;

// ----------------------------------------------------------------------------
void copyIP4(IP4_address * IPTo, const IP4_address * IPFrom)
{
  *IPTo= (*IPFrom);
}
// ----------------------------------------------------------------------------
void copyMAC(MAC_address * MACTo, const MAC_address * MACFrom)
{
  MACTo->MAC[0]=MACFrom->MAC[0];
  MACTo->MAC[1]=MACFrom->MAC[1];
  MACTo->MAC[2]=MACFrom->MAC[2];
  MACTo->MAC[3]=MACFrom->MAC[3];
  MACTo->MAC[4]=MACFrom->MAC[4];
  MACTo->MAC[5]=MACFrom->MAC[5];
}
// ----------------------------------------------------------------------------
uint8_t IP4_match(const IP4_address * IP1, const IP4_address * IP2)
{
  return ((*IP1)==(*IP2)); 
}
// ----------------------------------------------------------------------------
uint8_t MAC_match(const MAC_address * MAC1, const MAC_address * MAC2)
{
  return (MAC1->MAC[0]==MAC2->MAC[0] && MAC1->MAC[1]==MAC2->MAC[1] &&
          MAC1->MAC[2]==MAC2->MAC[2] && MAC1->MAC[3]==MAC2->MAC[3] &&
          MAC1->MAC[4]==MAC2->MAC[4] && MAC1->MAC[5]==MAC2->MAC[5]); 
}
// ----------------------------------------------------------------------------
#ifdef USE_mDNS
uint8_t mDNS_MAC(const MAC_address * MAC)
{
  return (MAC->MAC[0]==0x01 && MAC->MAC[1]==0x00 &&
          MAC->MAC[2]==0x5E && MAC->MAC[3]==0x00 &&
          MAC->MAC[4]==0x00 && MAC->MAC[5]==0xFB); 
}
#endif
// ----------------------------------------------------------------------------
#ifdef USE_LLMNR
uint8_t LLMNR_MAC(const MAC_address * MAC)
{
  return (MAC->MAC[0]==0x01 && MAC->MAC[1]==0x00 &&
          MAC->MAC[2]==0x5E && MAC->MAC[3]==0x00 &&
          MAC->MAC[4]==0x00 && MAC->MAC[5]==0xFC); 
}
#endif
// ----------------------------------------------------------------------------------

/*static*/ void refreshMACList(void)
{ // Called every tick to decrement tics and throw away expired addresses

uint8_t i,j;

for (i=0;i<known_IP_addresses;i++)
{
  if (!(ARP_held.ticks[i]--)) // time expired at 0
  {
    known_IP_addresses--;  // drop it and shuffle list down
    for (j=i;j<known_IP_addresses;j++)
	{
	  ARP_held.knownIP[j]=ARP_held.knownIP[j+1];
	  ARP_held.knownMAC[j]=ARP_held.knownMAC[j+1];
	  ARP_held.ticks[j]=ARP_held.ticks[j+1];
} } }
return;
}
// ----------------------------------------------------------------------------
void IP4_Endianism(MergedPacket * Mash)
{ // Converts an IP4 packet to network byte order or vice-versa
Mash->IP4.totalLength=BYTESWAP16(Mash->IP4.totalLength);
Mash->IP4.id=BYTESWAP16(Mash->IP4.id);
}// ----------------------------------------------------------------------------
void Error(void)  { // Print message and hang
#ifdef USE_LCD
  lcd_clrscr();
  lcd_puts(buffer);
#endif
// TODO - instead copy into EEPROM
  while (1) {};
}
// ----------------------------------------------------------------------------
static void learnMAC(const IP4_address IP, const MAC_address MAC)
{ // Stores MAC-IP pairs and an expiry time for each
int8_t i,j,staleist;

i=knownMAC(&IP);

if (i>MAX_ARP_HELD)
{
  sprintf(buffer,"i out %d",i);
  Error();
}

if (i<(-1)) return; // Don't 'learn' a broadcast

if (i>-1) // We already have it - reset counter as it's fresh
{
  ARP_held.ticks[i]=TICKS_TO_HOLD_MAC; 
  return;
}
// Add new
if (known_IP_addresses >= MAX_ARP_HELD)  // >= just in case
{  // We're maxed out - replace the staleist
  staleist=TICKS_TO_HOLD_MAC+1;
  j=0;
  for (i=0;i<known_IP_addresses;i++)
  {
    if (ARP_held.ticks[i]<staleist) staleist=ARP_held.ticks[j=i];
  }
  ARP_held.ticks[j]=TICKS_TO_HOLD_MAC;
  copyIP4(&ARP_held.knownIP[j],&IP);
  copyMAC(&ARP_held.knownMAC[j],&MAC);
}
else  // Free to add it to end
{
  ARP_held.ticks[known_IP_addresses]=TICKS_TO_HOLD_MAC;
  copyIP4(&ARP_held.knownIP[known_IP_addresses],&IP);
  copyMAC(&ARP_held.knownMAC[known_IP_addresses],&MAC);
  known_IP_addresses++;
}
return;
}
// ----------------------------------------------------------------------------
uint16_t checksum(uint16_t * words, int16_t length)
{ // Standard checksum routine (16 bit 1s complement of ones complement sum
//	 of all 16 bit words in header)  Used by IPv4, ICMP, UDP, TCP 
//   (i.e. used by Network layer AND Transport layer)
//   Details see http://www.netfor2.com/checksum.html
//   Note that this is endian independent.

//   Length is number of 16-bit words and words is a pointer to the words.
return (((uint16_t)checksumSupport(words,length))^0xFFFF);  // 16 bit 1s complement (i.e. XOR 1111...1111);
}
// ----------------------------------------------------------------------------
void checksumBare(uint32_t * scratch,uint16_t * words, int16_t length)
{ // Suport to standard checksum routine : not a checksum itself

//   Length is number of 16-bit words and words is a pointer to the words.
//   Scratch is sum so far

for (;length>0;length--) {
 *scratch+=(*(words++));
}

return;  
}
// ----------------------------------------------------------------------------
uint32_t checksumSupport(uint16_t * words, int16_t length)
{ // Suport to standard checksum routine : not a checksum itself

//   Length is number of 16-bit words and words is a pointer to the words.
uint32_t scratch, carry;

scratch=0x00;

for (;length>0;length--){
 scratch+=(*(words++));
}
carry  =(scratch&0xFFFF0000)>>16;
scratch=(scratch&0x0000FFFF)+carry; 
scratch+=(scratch>>16);   // Could have regenerated a carry by the last add
// (see Internetworking with TCP/IP Comer & Stevens)

return (scratch);  
}
// ----------------------------------------------------------------------------
uint16_t IP4checksum(MergedPacket * Mash) { 
// Calculates checksum of IP4
// NB. No Byteswap on header length because is 8 bit value
// Since header length represented the number of 32bit (4 byte) words, multiplying
// by 2 gives the number of 16 bit words in header

return (checksum((uint16_t *)&Mash->IP4,Mash->IP4.headerLength * 2));
//return (checksum(&Mash->words[ETH_HEADER_SIZE/2],Mash->IP4.header_length * 2));
}
// ----------------------------------------------------------------------------
int8_t MACForUs(MAC_address * MAC)
{  // Determines if this is a unicast for us, a broadcast, or a packet for someone else.
// However defaut is to set DLL to only allow our unicasts and ARP broadcasts
// so should never fail

  if (MAC_match(MAC, &myMAC))        return (OUR_ETHERNET_UNICAST);
  if (MAC_match(MAC, &BroadcastMAC)) return (ETHERNET_BROADCAST);
#ifdef USE_LLMNR
  if (LLMNR_MAC(MAC))                return (LLMNR_MULTICAST);
#endif
#ifdef USE_mDNS
  if (mDNS_MAC(MAC))                 return (mDNS_MULTICAST);
#endif

return (0); // Not for us - i.e. FALSE
}
// ----------------------------------------------------------------------------
int8_t IP4ForUs(IP4_address * IP) {  
// Determines if this is a unicast for us, a broadcast, or a packet for someone else.

  if (IP4_match(IP,&myIP))              return (OUR_IP_UNICAST);
  if (IP4_match(IP,&subnetBroadcastIP)) return (SUBNET_IP_BROADCAST);
  if (IP4_match(IP,&BroadcastIP))       return (LIMITED_IP_BROADCAST);
  if (mDNS(IP))                         return (mDNS_IP_MULTICAST);
  if (LLMNR(IP))                        return (LLMNR_IP_MULTICAST);

return (0); // Not ours
}
// ----------------------------------------------------------------------------
int8_t mDNS(IP4_address * IP)  { return IP4_match(IP,&mDNS_IP4); }
// ----------------------------------------------------------------------------
int8_t LLMNR(IP4_address * IP) { return IP4_match(IP,&LLMNR_IP4); }
// ----------------------------------------------------------------------------
void ARP_Endianism(MergedARP * Mish)
{ // Corrects the ARP header for system endianism or vice-versa
  Mish->ARP.type=BYTESWAP16(Mish->ARP.type);
  Mish->ARP.protocol=BYTESWAP16(Mish->ARP.protocol);
  Mish->ARP.hardware=BYTESWAP16(Mish->ARP.hardware);
}
// ----------------------------------------------------------------------------
void handleARP(MergedARP * Mish)
{ // We have heard an ARP.  From it we learn a MAC & IP pair.  If it was request 
// for us, we reply.  It won't be a reply to a serious request from us (except one we 
// have given up on) as that is handled in ResolveMAC().  It may be a reply to a
// forlorn request from us (i.e. checking that a IP is not used)

uint8_t IPforus;

ARP_Endianism(Mish);  // Get it into host order
 
if (Mish->ARP.type == ARP_REQUEST)  // respond to unicasts and broadcasts (but they are for us in IP terms)
{
  IPforus=IP4ForUs(&(Mish->ARP.destinationIP));

  if (!IPforus)  return; 
  learnMAC(Mish->ARP.sourceIP,Mish->ARP.sourceMAC);

  Mish->ARP.type=ARP_REPLY;

  copyIP4(&Mish->ARP.destinationIP,&Mish->ARP.sourceIP);
  copyIP4(&Mish->ARP.sourceIP,&myIP);

  copyMAC(&Mish->ARP.destinationMAC,&Mish->ARP.sourceMAC);
  copyMAC(&Mish->ARP.sourceMAC,&myMAC);
  Mish->ARP.hardware=DLLisETHERNET; 
  Mish->ARP.protocol=ARPforIP;  	
  Mish->ARP.hardware_size=6;  	// Length of MAC
  Mish->ARP.protocol_size=4;	// Length of IP V4

  launchARP(Mish);
}
else if (Mish->ARP.type==ARP_REPLY) 
{
  learnMAC(Mish->ARP.sourceIP,Mish->ARP.sourceMAC); // Even if it was a response we didn't ask for 
#ifdef USE_APIPA  
  if (MyState.IP==APIPA_TRY) { // Is this a response to our test?
    if (IP4_match(&Mish->ARP.sourceIP,&myIP)) { // Someone already using my proposed IP
      if (OCTET4(myIP)<=253)
        myIP=MAKEIP4(OCTET1(myIP),OCTET2(myIP),OCTET3(myIP),OCTET4(myIP)+1);
      else
        myIP=MAKEIP4(OCTET1(myIP),OCTET2(myIP),OCTET3(myIP)+1,1);
      // Increment last byte, and penultimate if it overflowed
      MyState.IP=APIPA_FAIL;    // To trigger trying the next one
    }
  }
#endif
}
return;
}
// ----------------------------------------------------------------------------
uint8_t handlePacket(void)
{ // See if we have a received ethernet packet and handle appropriately.
  // Mustn't call recursively as have only enough space for one 1,500 byte packet

uint16_t ilength;
uint8_t NeedIP;
uint8_t IPforus,MACforus;

NeedIP=(MyState.IP!=IP_SET);  // IP address as yet unset

uint8_t flags;
ilength=linkPacketHeader(MAX_STORED_SIZE,&MashE.bytes[0],&flags); 

MashE.Ethernet.type=BYTESWAP16(MashE.Ethernet.type);  // Ethernet endianism

if (ilength == 0) return (0);      // Nothing there [TODO - linkDoneWithPacket()??]

MACforus=MACForUs(&MashE.Ethernet.destinationMAC);

if (!MACforus) { linkDoneWithPacket(); return (0); }  // Let's not be promiscuous

if (MashE.Ethernet.type == IP4inETHERNET)   
{
  shuffleLFSR(MashE.IP4.checksum); // Mix some randomness into our LFSR

  if (!(flags&CS_IP4)) { linkDoneWithPacket(); return (0); }  // Checksum tested already
//  if (IP4checksum(&MashE)) { linkDoneWithPacket(); return (0); } // Checksum no good if non-zero

  learnMAC(MashE.IP4.source,MashE.Ethernet.sourceMAC); // Lets remember him

  IP4_Endianism(&MashE);

  IPforus=(IP4ForUs(&MashE.IP4.destination));  // If we're setting, we don't know our IP

  if (!NeedIP && !IPforus) { linkDoneWithPacket(); return (0); }  

  if (MashE.IP4.protocol == UDPinIP4) { // Do this first because a DHCP will be UDP 
    if ((flags&CS_UDP))    handleUDP(&MashE,flags);
    linkDoneWithPacket(); 
    return (0);
  }
  
  if (NeedIP)  { linkDoneWithPacket(); return (0); } // Skip all others if we're looking for a DHCP

  // if  (MashE.IP4.protocol == ICMPinIP4)  { /* Now handled in link layer */ } 

#ifdef USE_TCP
  if (MashE.IP4.protocol == TCPinIP4)   {  
    if ((flags&CS_TCP))   handleTCP(&MashE);
    linkDoneWithPacket(); 
    return (0);
  }
#endif
}

linkDoneWithPacket(); 
return (0);   // Ignore other protocols
}
// ----------------------------------------------------------------------------

static int8_t knownMAC(const IP4_address * target)
{  // We have a target IP.  Do we already know the MAC?
// -1 is a failure; -2 is Null; -3 is broadcast

uint8_t i;

if (IP4_match(target,&NullIP)) return (-2);
if (IP4_match(target,&BroadcastIP)) return (-3);

for (i=0;i < known_IP_addresses;i++) 
  if (IP4_match(target,&ARP_held.knownIP[i])) return (i);

return (-1);
}
// ----------------------------------------------------------------------------

MAC_address resolveMAC(IP4_address * target)
{  // We have a target IP.  Do we already know the MAC or can we get it with ARP?
// We always know our own IP by this point (have run DHCP/APIPA/STATIC)
// Stay blocked in this routine (DISCARDING other packets) until we have an ARP answer
// Discarding isn't polite, but probably necessary in space limited situation

int i,local;
uint16_t ilength;
MergedARP Mish;

local=(((*target) & subnetMask) == (myIP & subnetMask));
 
while (1)
{

#ifdef USE_LLMNR
MAC_address m;

if (*target==LLMNR_IP4) {
  m.MAC[0]=01;
  m.MAC[1]=00;
  m.MAC[2]=0x5E;
  m.MAC[3]=0x00;
  m.MAC[4]=0x00;
  m.MAC[5]=0xFC;
  return (m);  
}
#endif

#ifdef USE_mDNS
if (*target==mDNS_IP4) {
  m.MAC[0]=01;
  m.MAC[1]=00;
  m.MAC[2]=0x5E;
  m.MAC[3]=0x00;
  m.MAC[4]=0x00;
  m.MAC[5]=0xFB;
  return (m);  
}
#endif

  i=knownMAC(target);
  
// Get the broadcasts out of the way
  if (i==(-2)) return (BroadcastMAC);  // -2 codes for broadcast MAC // TODO NULL?

  if (i==(-3)) return (BroadcastMAC);  // -3 codes for broadcast MAC

  // Override with the Gateway IP if it's not a local address
  if (!local) i=knownMAC(&GWIP);  
     
  if (i>=0) return (ARP_held.knownMAC[i]);  // Because 0 is valid array element, fail is -1

// OK.  We don't know it, so we better run an ARP
// Prepare message
    Mish.ARP.type=ARP_REQUEST;
    copyMAC(&Mish.ARP.destinationMAC,&BroadcastMAC);
    copyIP4(&Mish.ARP.destinationIP,((local)?(target):(&GWIP)));
    Mish.ARP.hardware=DLLisETHERNET;  
    Mish.ARP.protocol=ARPforIP;  	
    Mish.ARP.hardware_size=6;  	// Length of MAC
    Mish.ARP.protocol_size=4;	// Length of IP V4
    copyMAC(&Mish.ARP.sourceMAC,&myMAC);
    copyIP4(&Mish.ARP.sourceIP,&myIP);
 
    launchARP(&Mish);

    Mish.Ethernet.type=ARPinETHERNET-1; // Set to anything but ARP, so test below works
    // TODO Make sure DO works, since byteswap has changed this
    uint8_t flags;
    do {
  	  ilength=linkPacketHeader((ETH_HEADER_SIZE+ARP_HEADER_SIZE),&(Mish.bytes[0]),&flags); 
      // Will only return the first part of a packet, enough for ARP.  Rest discarded
	
      Mish.Ethernet.type=BYTESWAP16(Mish.Ethernet.type);  // Ethernet endianism

 	  if (Mish.Ethernet.type==ARPinETHERNET)  handleARP(&Mish);
      
	} while ((Mish.Ethernet.type!=ARPinETHERNET));  // Block until an ARP heard
  }  // and repeat the while (1) until we get a match
}
// ----------------------------------------------------------------------------

void prepareIP4(MergedPacket * Mash, 
                 uint16_t payloadLength, IP4_address * ToIP, uint8_t protocol) 
{ // Takes UDP/TCP message and turns it into IP datagram with prefix, to be sent with Launch_as_IP4
// Only used when we are making a datagram ourselves - copying the incoming is easier
// Payload length in bytes

// Set up the header
  Mash->IP4.version=4; 
  Mash->IP4.headerLength=5; // TODO set length.  Currently always 5 (minimum size - no options)
  Mash->IP4.precedence=Mash->IP4.unused=0;
  Mash->IP4.delay=Mash->IP4.throughput=Mash->IP4.reliability=0;
  Mash->IP4.totalLength=(payloadLength)+Mash->IP4.headerLength*4;     
  Mash->IP4.id=IP4_ID++;              
  Mash->IP4.reserved=Mash->IP4.dontFragment=Mash->IP4.moreFragments=0;
  Mash->IP4.fragmentOffset=0;  // TODO we may need some offset eventually
  Mash->IP4.TTL=DEFAULT_TTL;            
  Mash->IP4.protocol=protocol;
  copyIP4(&Mash->IP4.source,&myIP);
  copyIP4(&Mash->IP4.destination,ToIP);

return;
}
// ----------------------------------------------------------------------------
uint8_t launchIP4(MergedPacket * Mash,uint8_t csums,
           void (* callback)(uint16_t start,uint16_t length,uint8_t * result),
           uint16_t offset)
{ // Takes IP4 datagram, adds checksum and hardware address (MAC) and 
  // sends it to Ethernet.  Resolves unknown IP/MACs (with ARP) 

MAC_address targetMAC;

// Make sure we know target MAC address
targetMAC=resolveMAC(&(Mash->IP4.destination));  // Will look up, or run ARP if unknown

copyIP4(&Mash->IP4.source,&myIP); //  Always force this even if already set 

copyMAC(&Mash->Ethernet.destinationMAC,&targetMAC);
copyMAC(&Mash->Ethernet.sourceMAC,&myMAC);
Mash->Ethernet.type=BYTESWAP16(IP4inETHERNET);

IP4_Endianism(Mash);

Mash->IP4.checksum=0x0000;  // 0 does not suffer from endianism
Mash->IP4.checksum=IP4checksum(Mash);  // Always last job to set checksum

#ifndef DEBUGGER
  linkPacketSend((uint8_t *)Mash,ETH_HEADER_SIZE+BYTESWAP16(Mash->IP4.totalLength),
               csums,callback,offset);   // Put it on the wire 
#endif

IP4_Endianism(Mash);

return (1);
}
// ----------------------------------------------------------------------------
void launchARP(MergedARP * Mish) 
{ // Can send it a Mash if required
// Takes ARP message and turns it into datagram

copyMAC(&Mish->Ethernet.destinationMAC,&Mish->ARP.destinationMAC);
copyMAC(&Mish->Ethernet.sourceMAC,&Mish->ARP.sourceMAC);

Mish->Ethernet.type=BYTESWAP16(ARPinETHERNET);

ARP_Endianism(Mish);
#ifndef DEBUGGER
linkPacketSend(&(Mish->bytes[0]),ETH_HEADER_SIZE+ARP_HEADER_SIZE,0,NULL,0); 
#endif
ARP_Endianism(Mish);

return;
}

