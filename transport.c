
/*********************************************
 Code for Transport layer protocols UDP, TCP
 
Copyright (C) 2009-20  S Combes


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

UDP always present
Define USE_TCP (from config.h) includes TCP code

 *********************************************/

#include "config.h"

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "link.h"
#include "network.h"
#include "transport.h"
#include "application.h"
#include "power.h"
#include "lfsr.h" // available pseudorandomness

extern IP4_address myIP;

#ifdef USE_TCP
TCP_TCB TCB[MAX_TCP_ROLES];
Retransmit ReTx[MAX_RETX];
#endif
extern uint16_t UDP_Port[MAX_UDP_PORTS];
extern uint16_t UDP_low_port;
extern MergedPacket MashE;

extern char buffer[MSG_LENGTH];

#ifdef WHEREABOUTS
extern uint8_t gis,ris,fis;  // Locations of G & R (0-11)
extern uint8_t lastMode;
#endif

void TCP_Endianism(MergedPacket * Mash);
void UDP_Endianism(UDP_header * Header);

// Local functions
static void cancelAckdReTx(const uint8_t * role);
static void cancelAllReTx(const uint8_t * role);
static void scheduleReTx(MergedPacket * Mash, uint16_t payloadLength, uint16_t headerLength, 
   const uint8_t * role,void (* callback)(uint16_t start,uint16_t length,uint8_t * result),uint16_t offset);
static uint16_t handleMetrics(MergedPacket * Mash, const uint8_t * role, uint8_t * ack);
static void defaultHead(MergedPacket * Mash,const uint8_t * role);
void TCP_SYN_ACK(MergedPacket * Mash, uint16_t sourcePort,
                   uint16_t destinationPort, IP4_address ToIP, uint8_t role);
void TCP_FIN(MergedPacket * Mash, uint8_t role);
void launchTCP(MergedPacket * Mash, uint16_t payload_length, IP4_address * ToIP,
               void (* callback)(uint16_t start,uint16_t length,uint8_t * result),uint16_t offset);
// ----------------------------------------------------------------------------
void MemOverflow()  { strcpy(buffer,"Malloc failed");  Error();  }

#ifdef USE_TCP
extern uint16_t FTP_passive_port;
// ----------------------------------------------------------------------------
void initialiseTCP(void)
{
uint8_t i;
for (i=0;i<MAX_TCP_ROLES;i++)  {
  TCB[i].status=TCP_CLOSED;
  TCB[i].lastByteSent=Rnd32bit(); // New random seq	(If LFSR not present, can use fixed no)
}
for (i=0;i<MAX_RETX;i++) {  ReTx[i].retries=ReTx[i].timeout=ReTx[i].active=0; }
}
// ----------------------------------------------------------------------------
void countdownTCP(void) { // Called 1 per sec from interrupt.  Keep short.
  for (uint8_t i=0;i<MAX_RETX;i++) if (ReTx[i].active) ReTx[i].timeout--; 
}
// ----------------------------------------------------------------------------
uint8_t ActiveReTx(uint8_t role) 
{ // Finds the number of unacknowledged TCP packets ready for retransmission
uint8_t i,j=0;

for (i=0;i<MAX_RETX;i++)
  if ((ReTx[i].role == role) && ReTx[i].active) j++;

return (j);
}
// ----------------------------------------------------------------------------
void retxTCP(void) // Called from main loop.
{
uint8_t  i;
uint16_t j;

// TODO cleanupOldTCP();  // Any old server ones, just kill.  Will cancel their retransmissions.

for (i=0;i<MAX_RETX;i++) {
  if ((ReTx[i].active) &&  (!ReTx[i].timeout)) {
// There is something to resend and now is the time
    if (ReTx[i].retries==0) {// That's enough
      ReTx[i].active=0;
      free(ReTx[i].data);
      if (TCB[ReTx[i].role].status==TCP_TIME_WAIT)  {
        TCB[ReTx[i].role].status=TCP_CLOSED; // All done
      }
      else {   // Send RST and time out
        //TODO  TCP_RST(&MashE,TCB[ReTx[i].role].localPort,TCB[ReTx[i].role].remotePort,
        //               TCB[ReTx[i].role].remoteIP,ReTx[i].role); 
        //TCB[ReTx[i].role].status=TCP_TIME_WAIT; // Error (retries still zero)
        //ReTx[i].timeout=TCP_TIMEOUT;
    } }
    else {
      TCB[ReTx[i].role].age=TCP_MAX_AGE; // Keep alive	
      ReTx[i].timeout=(TCP_TIMEOUT);
      ReTx[i].retries--;

      for (j=0;j<(TCP_RETRIES-ReTx[i].retries);j++) ReTx[i].timeout*=2;
      // Binary exponential increase

      if (ReTx[i].data) {
        for (j=0;j<(ReTx[i].payloadLength+ReTx[i].headerLength);j++)
          MashE.bytes[ETH_HEADER_SIZE+IP_HEADER_SIZE+j]=ReTx[i].data[j];
        launchTCP(&MashE,ReTx[i].payloadLength,&(TCB[ReTx[i].role].remoteIP),NULL,0);
	  } else if (ReTx[i].callback) {		  
	    defaultHead(&MashE,&ReTx[i].role);
        MashE.TCP.sequence=ReTx[i].sequence;     // Override with original value  
		MashE.TCP.headerLength=5;
        MashE.TCP.flags       =(FL_ACK);
        MashE.TCP.windowSize  =MAX_PACKET_PAYLOAD;
        //  Set checksum last (i.e. later)
        MashE.TCP.urgent      =0;

        launchTCP(&MashE,ReTx[i].payloadLength,&TCB[ReTx[i].role].remoteIP,(void *)ReTx[i].callback,ReTx[i].start); 
      }
    }
  }	
}
}
// ----------------------------------------------------------------------------
void cleanupOldTCP()
{
if (TCB[TCP_SERVER].status >= TCP_ESTABLISHED) {
  if (TCB[TCP_SERVER].age) TCB[TCP_SERVER].age--;
  else { // Kill old ones off.  Age is renewed every time we use.
    TCP_FIN(&MashE,TCP_SERVER); //  Should really do RST?
	// FIN will cancel relevant retransmissions
  }
}
}
// ----------------------------------------------------------------------------
void TCP_SimpleDataOutProgmem(const char * send,const uint8_t role,uint8_t reTx)
{ 
uint16_t i=0;  // uint16 because could be up to 1460

if (TCB[role].status != TCP_ESTABLISHED) return; // Fail

while (pgm_read_byte(send) != 0x00) MashE.HTTP[i++]=pgm_read_byte(send++);
TCP_ComplexDataOut(&MashE,role,i,NULL,0,reTx);  // NULL=no callback
}
// ----------------------------------------------------------------------------
void TCP_SimpleDataOut(const char * send,const uint8_t role,uint8_t reTx)
{ 
uint16_t i=0;  // uint16 because could be up to 1460

if (TCB[role].status != TCP_ESTABLISHED) return; // Fail

while (*send != 0x00) MashE.HTTP[i++]=*send++;
TCP_ComplexDataOut(&MashE,role,i,NULL,0,reTx);  // NULL=no callback
}
// ----------------------------------------------------------------------------
uint8_t pendingReTx(const uint8_t * role)
{ // TRUE is there is something to retransmit for given role
  uint8_t i;

  for (i=0;i<MAX_RETX;i++)
    if (ReTx[i].active && (ReTx[i].role == (*role))) return (TRUE);

  return (FALSE);
}
// ----------------------------------------------------------------------------
void TCP_DataIn(MergedPacket * Mash,const uint16_t newData,const uint8_t role)
{  // newData is TCP data (not header) that we haven't heard before (on simple ACK=0)

#ifdef USE_HTTP
if (Mash->TCP.sourcePort==HTTP_SERVER_PORT && role==TCP_CLIENT)
{ 
  parseHTML(Mash,newData);
  return;
}

if (Mash->TCP.destinationPort==HTTP_SERVER_PORT && role==TCP_SERVER) 
{
  sendHTML(Mash,newData);
  return;
}
#endif

#ifdef USE_FTP
if (Mash->TCP.sourcePort==FTP_SERVER_PORT && role==TCP_FTP_CLIENT)
{ 
  handleFTP(Mash,newData);
  return;
}
/*if (Mash->TCP.sourcePort==FTP_passive_port && role==TCP_FTP_PASSIVE)
{ 
  TCP_ACK(Mash,TCP_FTP_PASSIVE);
  return;
}*/
#endif

}
// ----------------------------------------------------------------------------
void cancelAckdReTx(const uint8_t * role)
{ // Cancel those retransmissions for which we have had the packet ACK'd
uint8_t i;
int32_t delta;

for (i=0;i<MAX_RETX;i++)
  if (ReTx[i].active && (ReTx[i].role == (*role)))
  {
    delta=TCB[*role].lastAckReceived-ReTx[i].lastByte;
    if (delta > 0) // >0 because last_ack is lastByte + 1
    {    
      ReTx[i].active=FALSE;
      if (ReTx[i].data) free(ReTx[i].data);
      if (ReTx[i].callback) free(ReTx[i].callback);
    }
  }
}
// ----------------------------------------------------------------------------
void cancelAllReTx(const uint8_t * role)
{ // Cancel all pending retransmissions for this role
uint8_t i;

for (i=0;i<MAX_RETX;i++)
  if (ReTx[i].active && (ReTx[i].role == (*role)))
  {
    ReTx[i].active=FALSE;
    if (ReTx[i].data) free(ReTx[i].data);
    if (ReTx[i].callback) free(ReTx[i].callback);
  }
}
// ----------------------------------------------------------------------------
void scheduleReTx(MergedPacket * Mash, uint16_t payloadLength, 
                  uint16_t headerLength,const uint8_t * role,
				  void (* callback)(uint16_t start,uint16_t length,uint8_t * result),
				  uint16_t offset)
{ // For TCP retransmissions.
//   Always call AFTER transmission, to save memory (transmisison might need to
//   do an ARP, costing extra 42 ephemeral bytes).  

uint8_t i;
 
  return; // TODO testing
 
  i=0;
  while (i<MAX_RETX) {
    if (!ReTx[i].active) break;
    i++;
  }
  if (i==MAX_RETX) return;  // No slot, so just try to cope

//#ifdef RETX // TODO BREACH of protocol if defined, but risks memory overflow if it isn't
 uint8_t j;

  ReTx[i].role=(*role);
  ReTx[i].active=TRUE;
  ReTx[i].retries=TCP_RETRIES;
  ReTx[i].timeout=TCP_TIMEOUT;
  ReTx[i].payloadLength=payloadLength;
  ReTx[i].headerLength=headerLength;
  ReTx[i].lastByte=Mash->TCP.sequence+((payloadLength)?(payloadLength-1):0);
  
  if (callback) {
	  ReTx[i].callback=callback;
	  ReTx[i].data=NULL;
	  ReTx[i].start=offset;
    ReTx[i].sequence=Mash->TCP.sequence;
    } else {
      ReTx[i].callback=NULL;
      ReTx[i].data=malloc(payloadLength+headerLength);

      if (ReTx[i].data == NULL) MemOverflow();  // Maloc failed
      else
      {
        for (j=0;j<(payloadLength+headerLength);j++)
          ReTx[i].data[j]=Mash->bytes[j+ETH_HEADER_SIZE+IP_HEADER_SIZE];
      }
    }
//#endif
}
// ----------------------------------------------------------------------------
/*static uint16_t TCP_Checksum(MergedPacket * Mash,uint16_t TCP_length,
                             IP4_address * FromIP,IP4_address * ToIP)
{  // See http://www.tcpipguide.com/free/t_TCPChecksumCalculationandtheTCPPseudoHeader.htm,
   // especially for curious use of pseudo header

// Unlike UDP, have to pass length in.  UDP has length field itself, TCP doesn't
// From received packet could calculate length from IP4 length - IP4 header length
// but on transmitted packet, IP4 has not yet been constructed.

uint32_t sum, carry;

if (TCP_length % 2) Mash->bytes[ETH_HEADER_SIZE+IP_HEADER_SIZE+TCP_length++]=0; 
// evens/odds : padding byte if required  

sum=((uint32_t)((*FromIP)>>16)+(uint32_t)((*FromIP)&0xFFFF)+(uint32_t)((*ToIP)>>16)+
     (uint32_t)((*ToIP)&0xFFFF)+BYTESWAP16(TCPinIP4)+(uint32_t)BYTESWAP16(TCP_length)); 
// Effectively pseudo header

sum+= (checksumSupport((uint16_t * )&Mash->bytes[ETH_HEADER_SIZE+IP_HEADER_SIZE],
           TCP_length/2)); // /2 for words 

carry   = (sum & 0xFFFF0000)>>16;
sum     = (sum & 0x0000FFFF)+carry;
sum    += (sum>>16); // that last add could have overflowed

return ((0xFFFF ^ (uint16_t)sum));
}*/
// ----------------------------------------------------------------------------
void defaultHead(MergedPacket * Mash,const uint8_t * role)
{
  Mash->TCP.sourcePort     =TCB[*role].localPort;
  Mash->TCP.destinationPort=TCB[*role].remotePort;
  Mash->TCP.sequence       =TCB[*role].lastByteSent;
  Mash->TCP.ack            =TCB[*role].lastByteReceived+1;  // Comer V1 p208
  // You provide the ack for the byte you next expect.
  Mash->TCP.unused2        =0;
}
// ----------------------------------------------------------------------------
void initiate_TCP_connection(MergedPacket * Mash,uint16_t sourcePort,
                   uint16_t destinationPort,IP4_address ToIP,uint8_t role)
{ // i.e. Send a SYN packet
uint8_t payloadLength;

  Mash->TCP.sourcePort     =sourcePort;
  Mash->TCP.destinationPort=destinationPort;
  Mash->TCP.sequence       =++TCB[role].lastByteSent;
  Mash->TCP.ack            =0x0;
  Mash->TCP.unused2        =0;
  Mash->TCP.headerLength   =6;
  Mash->TCP.flags          =FL_SYN;

  Mash->TCP.windowSize     =MAX_PACKET_PAYLOAD;
//  Set checksum last (i.e. later)
  Mash->TCP.urgent         =0;

// Options
  Mash->TCP_options[0]=02;  // MSS
  Mash->TCP_options[1]=04;  // Length
  Mash->TCP_options[2]=((MAX_PACKET_SIZE-ETH_HEADER_SIZE-IP_HEADER_SIZE) >> 8); // MSS=0x240 (576)
  Mash->TCP_options[3]=((MAX_PACKET_SIZE-ETH_HEADER_SIZE-IP_HEADER_SIZE) & 0xFF);
//  Mash->TCP_options[4]=01;  // NOP
//  Mash->TCP_options[5]=01;  // NOP
//  Mash->TCP_options[6]=04;  // SACK permitted
//  Mash->TCP_options[7]=02;  // Length

  TCB[role].status         =TCP_SYN_SENT;
  TCB[role].age            =TCP_MAX_AGE; 
  TCB[role].localPort      =sourcePort;
  TCB[role].remotePort     =destinationPort;
  copyIP4(&TCB[role].remoteIP,&ToIP);
  TCB[role].lastAckReceived=TCB[role].lastByteSent-1; // initial condition
  TCB[role].windowSize     =MAX_PACKET_PAYLOAD;
  payloadLength=0;

  launchTCP(Mash,0,&ToIP,NULL,0); 
  scheduleReTx(Mash,payloadLength,Mash->TCP.headerLength*4,&role,NULL,0);

  TCB[role].lastByteSent++; // SYN counts as a byte in the stream
} 
// ----------------------------------------------------------------------------
void TCP_SYN_ACK(MergedPacket * Mash, uint16_t sourcePort,
                   uint16_t destinationPort, IP4_address ToIP,uint8_t role)
{ // Acknowledge a proffered connection with a SYN-ACK packet
// Don't pass source/dest as refs as we overwrite in Mash
  uint8_t payloadLength;

  Mash->TCP.sourcePort     =TCB[role].localPort  = sourcePort;
  Mash->TCP.destinationPort=TCB[role].remotePort = destinationPort;
  TCB[role].lastByteReceived=Mash->TCP.sequence;
  shuffleLFSR(Mash->TCP.sequence); // Seed some randomness into our LFSR
  Mash->TCP.ack            =TCB[role].lastByteReceived+1;
  Mash->TCP.sequence       =TCB[role].lastByteSent+=(1500+(uint32_t)Rnd16bit()); // New random seq
  Mash->TCP.unused2        =0;
  Mash->TCP.headerLength   =6;
  Mash->TCP.flags=(FL_SYN | FL_ACK);

  Mash->TCP.windowSize     =TCB[role].windowSize = MAX_PACKET_PAYLOAD;
//  Set checksum last (i.e. later)
  Mash->TCP.urgent         =0;

// Options
  Mash->TCP_options[0]=02;  // MSS
  Mash->TCP_options[1]=04;  // Length
  Mash->TCP_options[2]=((MAX_PACKET_SIZE-ETH_HEADER_SIZE-IP_HEADER_SIZE)>>8); // MSS=0x240 (576)
  Mash->TCP_options[3]=((MAX_PACKET_SIZE-ETH_HEADER_SIZE-IP_HEADER_SIZE)&0xFF);

  TCB[role].status         =TCP_SYN_RCVD;
  TCB[role].age            =TCP_MAX_AGE; 
  payloadLength=0;

  copyIP4(&TCB[role].remoteIP,&ToIP);
  TCB[role].lastAckReceived=TCB[role].lastByteSent-1; // initial condition

  launchTCP(Mash,0,&TCB[role].remoteIP,NULL,0); 
  scheduleReTx(Mash,payloadLength,Mash->TCP.headerLength*4,&role,NULL,0);

  TCB[role].lastByteSent++; // SYN-ACK counts as a byte in the stream 
}
// ----------------------------------------------------------------------------
void TCP_ACK(MergedPacket * Mash,uint8_t role)
{ // Acknowledge a TCP packet

  defaultHead(Mash,&role);
  Mash->TCP.headerLength=5;
  Mash->TCP.flags     =(FL_ACK);

  Mash->TCP.windowSize=MAX_PACKET_PAYLOAD;
//  Set checksum last (i.e. later)
  Mash->TCP.urgent    =0;

  launchTCP(Mash,0,&TCB[role].remoteIP,NULL,0); 
  TCB[role].age       =TCP_MAX_AGE;   // Keep alive
}
// ----------------------------------------------------------------------------
void TCP_PrivateDataOut(MergedPacket * Mash,const uint8_t role, 
              const uint16_t payloadLength,
              uint16_t (* callback)(uint16_t start,uint16_t length,uint8_t * result),
              uint16_t offset,uint8_t reTx)
{ // Pass some data - the internal workhorse, not part of API

  defaultHead(Mash,&role);
  Mash->TCP.headerLength=5;
  Mash->TCP.flags       =(/*FL_PSH |*/ FL_ACK); // PSH should be unnecessary
  // Setting ACK may lead to repeated ACKs if we pass data in response to a packet
  // we acked on receipt.  This should be harmless (Wireshark points it out) but helpful
  // in case function is slow - so host gets a quick ACK, at the price of a later duplicate.

  Mash->TCP.windowSize  =MAX_PACKET_PAYLOAD;
//  Set checksum last (i.e. later)
  Mash->TCP.urgent      =0;

  TCB[role].lastByteSent+=(payloadLength);

  launchTCP(Mash,payloadLength,&TCB[role].remoteIP,(void *)callback,offset); 
  if (reTx) scheduleReTx(Mash,payloadLength,Mash->TCP.headerLength*4,&role,(void *)callback,offset);

  TCB[role].age         =TCP_MAX_AGE; // Keep alive
}
// ----------------------------------------------------------------------------
void TCP_ComplexDataOut(MergedPacket * Mash, const uint8_t role, 
              const uint16_t payloadLength,
              uint16_t (* callback)(uint16_t start,uint16_t length,uint8_t * result),
              uint16_t offset,uint8_t reTx)
{ // Pass some data.  Splits into separate packets if required.  Relies on callback
//   functions to allow data larger than our own free RAM.

#define MAX_PAYLOAD (1460)  // <=1460 for Ethernet for TCP

uint16_t totalData=payloadLength;
while (totalData>MAX_PAYLOAD) {
  TCP_PrivateDataOut(&MashE,TCP_SERVER,MAX_PAYLOAD,callback,offset,reTx); 
  totalData-=MAX_PAYLOAD;
  offset+=MAX_PAYLOAD;
}
if (totalData) TCP_PrivateDataOut(&MashE,TCP_SERVER,totalData,callback,offset,reTx);  // Leftovers
}
// ----------------------------------------------------------------------------
void TCP_FIN(MergedPacket * Mash,uint8_t role)
{ // Signal we wish to close a connection (ESTABLISHED->FIN_WAIT1)
uint8_t payloadLength;

  defaultHead(Mash,&role);
  Mash->TCP.headerLength=5;
  Mash->TCP.flags       =(FL_FIN | FL_ACK);

  Mash->TCP.windowSize  =(MAX_PACKET_PAYLOAD);
//  Set checksum last (i.e. later)
  Mash->TCP.urgent      =0;
  payloadLength=0;

  launchTCP(Mash,payloadLength,&TCB[role].remoteIP,NULL,0); 
// TODO - re tx for FIN? Probably not.
//  scheduleReTx(Mash,payloadLength,Mash->TCP.headerLength*4,&role,NULL,0);
  cancelAllReTx(&role); 

  if (TCB[role].status==TCP_ESTABLISHED) {  // Closing was our idea 
    TCB[role].status=TCP_FIN_WAIT1; // If their idea, go to CLOSE_WAIT (done in caller)
  }
}
// ----------------------------------------------------------------------------
void TCP_RST(MergedPacket * Mash, uint16_t sourcePort,
                uint16_t destinationPort,IP4_address ToIP,uint8_t role)
{ // Reset a TCP connection we are unhappy with
  Mash->TCP.sourcePort     =sourcePort;
  Mash->TCP.destinationPort=destinationPort;
  Mash->TCP.sequence       =0;  
//  Mash->TCP.ack  : set outside            
  Mash->TCP.unused2        =0;
  Mash->TCP.headerLength   =5;
  Mash->TCP.flags          =FL_RST;

  Mash->TCP.windowSize     =MAX_PACKET_PAYLOAD;
//  Set checksum last (i.e. later)
  Mash->TCP.urgent         =0;
  
  if (role != TCP_REJECT) cancelAllReTx(&role); 
  // TCP_REJECT role specific to reject, where it was never our connection

  launchTCP(Mash,0,&ToIP,NULL,0); // 0 is payload length; no payload at RST
}
// ----------------------------------------------------------------------------
void launchTCP(MergedPacket * Mash,uint16_t payloadLength,IP4_address * ToIP,
              void (* callback)(uint16_t start,uint16_t length,uint8_t * result),
              uint16_t offset)
{ // Setup the TCP checksum and then send it on
// payloadLength is the TCP length less the TCP header

TCP_Endianism(Mash);

//Mash->TCP.TCP_checksum=0x0000;  // 0 does not suffer from endianism
//Mash->TCP.TCP_checksum=TCP_Checksum(Mash,4*(Mash->TCP.headerLength)+payloadLength,&myIP,ToIP);
// Checksum now offloaded to link layer

// Insert IP4 header into mash
prepareIP4(Mash, 4*(Mash->TCP.headerLength)+(payloadLength),ToIP,TCPinIP4);  

launchIP4(Mash,CS_TCP,callback,offset);
TCP_Endianism(Mash); // Returns Mash in same state as started

return;
}
// ----------------------------------------------------------------------------
void handleTCP(MergedPacket * Mash)
{ // Handle a received TCP packet.  Generally treat LISTEN and CLOSED as same thing :
  // We know if we are meant to respond on this port, irrespective of CLOSED/LISTEN
uint16_t TCP_length,newData;
uint8_t role,ack,i;
int32_t delta;

MergedACK * Mack;

TCP_length = Mash->IP4.totalLength - 4*Mash->IP4.headerLength; 

// Check TCP checksum, silently reject if necessary
//if (TCP_Checksum(Mash,TCP_length,&Mash->IP4.source,&Mash->IP4.destination)) return;
// New method has checked in network layer

TCP_Endianism(Mash);

#ifdef PROTECT_TCP
// Reject invalid TCP flags (may largely be swept up in later logic anyway)
// Extracted from http://pikt.org/pikt/samples/iptables_tcp_flags_programs.cfg.html
// 1.  Can't have FIN set and ACK not
if ((Mash->TCP.flags & (FL_FIN | FL_ACK))==FL_FIN) return;
// 2.  Can't have PSH set and ACK not
if ((Mash->TCP.flags & (FL_PSH | FL_ACK))==FL_PSH) return;
// 3.  Can't have URG set and ACK not
if ((Mash->TCP.flags & (FL_URG | FL_ACK))==FL_URG) return;
// 4.  Can't have FIN and RST both set
if ((Mash->TCP.flags & (FL_FIN | FL_RST))==(FL_FIN | FL_RST)) return;
// 5.  Can't have SYN and FIN both set
if ((Mash->TCP.flags & (FL_FIN | FL_SYN))==(FL_FIN | FL_SYN)) return;
// 6.  Can't have SYN and RST both set
if ((Mash->TCP.flags & (FL_RST | FL_SYN))==(FL_RST | FL_SYN)) return;
// 7.  Must have something set ...
if (!Mash->TCP.flags) return;
// 8. ... but not everything
if ((Mash->TCP.flags & (FL_FIN | FL_SYN | FL_ACK | FL_URG | FL_RST | FL_PSH))== 
                       (FL_FIN | FL_SYN | FL_ACK | FL_URG | FL_RST | FL_PSH)) return;
// 9.  Can't just have FIN, PSH, URG whatever state SYN is
if ((Mash->TCP.flags & (FL_FIN | FL_ACK | FL_URG | FL_RST | FL_PSH))==
                       (FL_FIN | FL_URG | FL_PSH)) return;
// 10.  Can't have just SYN, RST, ACK, FIN, URG
if ((Mash->TCP.flags & (FL_FIN | FL_SYN | FL_ACK | FL_URG | FL_RST | FL_PSH))== 
                       (FL_FIN | FL_SYN | FL_ACK | FL_URG | FL_RST)) return;
#endif
// ---------------------------------------------------------------------------
//  First see if this is a new connection to our server

if (Mash->TCP.destinationPort == HTTP_SERVER_PORT)
{
  if (TCB[TCP_SERVER].status==TCP_CLOSED  || TCB[TCP_SERVER].status==TCP_LISTEN) 
  {   // A new connection and we're ready
    /* Option : reject all.  Which is what we should do if closed.  Skip this test behaviour for LISTEN
    Mash->TCP.ack=(Mash->TCP.sequence+1);
    TCP_RST(Mash,Mash->TCP.destinationPort,Mash->TCP.sourcePort,
            Mash->IP4.source,TCP_REJECT);
    return; */
    if (Mash->TCP.flags & FL_SYN) // Connection initiation
    {
	  resetHTTPServer(); // Clean the paramaters
      TCP_SYN_ACK(Mash, Mash->TCP.destinationPort,Mash->TCP.sourcePort,
            Mash->IP4.source, TCP_SERVER);
    }     
    return; 
  }
// ---------------------------------------------------------------------------
  else if (TCB[TCP_SERVER].localPort !=Mash->TCP.destinationPort ||
           TCB[TCP_SERVER].remotePort!=Mash->TCP.sourcePort)
  { // A new connection and we're already busy on another
    Mash->TCP.ack=(Mash->TCP.sequence+1);
    TCP_RST(Mash,Mash->TCP.destinationPort,Mash->TCP.sourcePort,
            Mash->IP4.source,TCP_REJECT);
    // No need to keep mash - we're not expecting a re-Tx
    // N.B. Source/destination reversed in response
    return;
  }
  // Fall through to normal procedure
}
// ---------------------------------------------------------------------------
// If it's not new, it should match one of our TCB : ignore those that don't

role=MAX_TCP_ROLES+1;
for (i=0;i<MAX_TCP_ROLES;i++) {

  if ((TCB[i].status!=TCP_CLOSED) && 
       TCB[i].localPort ==Mash->TCP.destinationPort &&
       TCB[i].remotePort==Mash->TCP.sourcePort && // deliberate reversal
       IP4_match(&TCB[i].remoteIP,&Mash->IP4.source))
    role=i;
}

if (role == (MAX_TCP_ROLES+1)) return; // failure (But this test is the same as one above for server?)

// Only get to here if it actually matches the extant connection

// ---------------------------------------------------------------------------
// Above all, silently accept in all cases if they want to reset.
if (Mash->TCP.flags & FL_RST)
{
  TCB[role].status=TCP_CLOSED;  
  cancelAllReTx(&role);
  return;
}
// ---------------------------------------------------------------------------
if (Mash->TCP.flags & FL_ACK) // In all cases, if ACK field is significant : cancel relevant ReTx
{ 
  delta=Mash->TCP.ack-TCB[role].lastAckReceived;
  if (delta > 0)  TCB[role].lastAckReceived=Mash->TCP.ack; // update 
  cancelAckdReTx(&role); 
}
// ---------------------------------------------------------------------------
// Now the main control sequence

switch (TCB[role].status)
{
  case (TCP_CLOSED): return;  // No connection active or pending
  case (TCP_LISTEN): return;  // Server waiting for incoming call
  // We looked at LISTEN earlier because the port & destination won't match,
  // so we wouldn't get to here.
// ---------------------------------------------------------------------------
  case (TCP_SYN_RCVD):  // Request has arrived; we've SYN-ACK'd; waiting for ACK ..
    if (Mash->TCP.flags & FL_ACK) 
    {
      if (Mash->TCP.ack==TCB[role].lastByteSent) // ... got it.
      {
        TCB[role].lastAckReceived=Mash->TCP.ack; 
        TCB[role].lastByteReceived=Mash->TCP.sequence;
      }
      TCB[role].status=TCP_ESTABLISHED;  // No need to ACK their ACK
    }
    return;
// ---------------------------------------------------------------------------
  case (TCP_SYN_SENT):  // We have started to open a connection, so we want a SYN-ACK

    if ((Mash->TCP.flags & FL_ACK) && (Mash->TCP.flags & FL_SYN))
    { // A valid SYN-ACK, so reply with ACK

      if (Mash->TCP.ack==TCB[role].lastByteSent)
      {
        TCB[role].lastAckReceived=Mash->TCP.ack; // initialise
        TCB[role].lastByteReceived=Mash->TCP.sequence;
      }

      TCP_ACK(Mash, role);
      TCB[role].status=TCP_ESTABLISHED;
      cancelAllReTx(&role);  // Don't resend SYN etc now we've ACKd (it doesn't get ACKd)
      // TODO - necessary?
    }
    return;
// ---------------------------------------------------------------------------
  case (TCP_ESTABLISHED):  // The normal data transfer state
    
    newData=handleMetrics(Mash,&role,&ack);      // Sets last byte received
    uint8_t theirFin=(Mash->TCP.flags & FL_FIN); // Record, 'cos gets overwritten

    if (ack) { // An ACK is required.  Create temp structure 'cos Mash has data > 0
	  // Note if ACK not required, this isn't new data, so we ignore
      //Mack=malloc(sizeof(MergedACK));
      Mack=(MergedACK *)buffer; // Use buffer instead
      //if (Mack) {
        TCP_ACK((MergedPacket *)Mack,role);
        //free (Mack);
      //}
      TCP_DataIn(Mash,newData,role); // Process the data in the packet

      if (theirFin) {
      // Notionally skip through TCP_CLOSE_WAIT;  
        TCP_FIN(Mash,role);
        TCB[role].status=TCP_LAST_ACK;
      }
    }

    return;
// ---------------------------------------------------------------------------
  case (TCP_CLOSE_WAIT): return; // The other side has initiated a release
// ---------------------------------------------------------------------------
  case (TCP_FIN_WAIT1):  // We have said we are finished

    newData=handleMetrics(Mash,&role,&ack);  // Sets last byte received

    if (Mash->TCP.flags & FL_FIN) { // An ACK is required.  Create temp structure 'cos Mash has data > 0
      //Mack=malloc(sizeof(MergedACK));
      Mack=(MergedACK *)buffer;  // Use buffer instead
      //if (Mack) {
        TCP_ACK((MergedPacket *)Mack, role);
        //free (Mack);
      //} 
      TCB[role].status=TCP_CLOSED; // Should really be TIME_WAIT;
      return;
    }
    if (Mash->TCP.flags & FL_ACK) 
      TCB[role].status=TCP_FIN_WAIT2;

    return;
// ---------------------------------------------------------------------------
  case (TCP_FIN_WAIT2):  // The other side has agreed to release
    if (Mash->TCP.flags & FL_FIN)   {
      TCP_ACK(Mash, role);
      TCB[role].status=TCP_CLOSED;  
    }
    return;
// ---------------------------------------------------------------------------
  case (TCP_CLOSING):  // Both sides have tried to close simultaneously
    // todo TBC   if (Valid_ACK(Mash,role)) TCB[role].status=TCP_TIME_WAIT;
    return;
// ---------------------------------------------------------------------------
  case (TCP_TIME_WAIT): return;  // Wait for packets to die off
// ---------------------------------------------------------------------------
  case (TCP_LAST_ACK):   // Wait for packets to die off
    if (Mash->TCP.flags & FL_ACK) TCB[role].status=TCP_CLOSED; // Seq no checked above?
    return;
}
return;
}
// -------------------------------------------------------------------------------
uint16_t handleMetrics(MergedPacket * Mash, const uint8_t * role, uint8_t * ack)
{ // Returns byte count of NEW data received, i.e. zero if seen already
  // Only call for ESTABLISHED connections, so we have a TCB
  // Accept only if we have in order data receipt, we have insufficient memory otherwise

  // Updates the last ack received & lastByteReceived counter

int32_t lastByte,payloadLength,delta;

delta=Mash->TCP.sequence-TCB[*role].lastByteReceived;
if (delta>1) { // Enforce in order delivery
  *ack=FALSE;
  return 0;
}
payloadLength=Mash->IP4.totalLength-(Mash->IP4.headerLength+Mash->TCP.headerLength)*4;

lastByte=Mash->TCP.sequence+payloadLength-1+((Mash->TCP.flags & FL_FIN)?1:0);  // FIN takes a byte

// Don't ack packets that are zero length, but ACK FINs (N.B. SYNs handled elsewhere)
*ack=(payloadLength!=0 || (Mash->TCP.flags & FL_FIN));

delta=lastByte-TCB[*role].lastByteReceived;

if (delta<=0) return (0); // Already have it

TCB[*role].lastByteReceived=lastByte;

return ((uint16_t)(delta));
}
// ----------------------------------------------------------------------------
void TCP_Endianism(MergedPacket * Mash)
{ // For just after arrival/just before send : reverse endianism of TCP fields
   Mash->TCP.sourcePort=BYTESWAP16(Mash->TCP.sourcePort);
   Mash->TCP.destinationPort=BYTESWAP16(Mash->TCP.destinationPort);
   Mash->TCP.sequence=BYTESWAP32(Mash->TCP.sequence);
   Mash->TCP.ack=BYTESWAP32(Mash->TCP.ack);
   Mash->TCP.windowSize=BYTESWAP16(Mash->TCP.windowSize);
   Mash->TCP.urgent=BYTESWAP16(Mash->TCP.urgent);
}
#endif  // End of TCP specific
// ----------------------------------------------------------------------------
/*static uint16_t UDP_Checksum(MergedPacket * Mash,IP4_address * FromIP, IP4_address * ToIP) 
{  // See http://www.tcpipguide.com/free/t_UDPMessageFormat-2.htm, especially
   // for curious use of pseudo header

// UDP Checksum calculated over 1) Pseudo header; 2) Header; 3) Data & 4) Padding byte if required 

uint16_t length;
uint32_t sum, carry;

length=BYTESWAP16(Mash->UDP.messageLength);
if (length % 2)   Mash->bytes[ETH_HEADER_SIZE+IP_HEADER_SIZE+length++]=0; 
// evens/odds : padding byte

sum=((uint32_t)((*FromIP)>>16)+(uint32_t)((*FromIP)&0xFFFF)+(uint32_t)((*ToIP)>>16)+
     (uint32_t)((*ToIP)&0xFFFF)+BYTESWAP16(UDPinIP4)+(uint32_t)Mash->UDP.messageLength); 
// Effectively pseudo header

sum+= (checksumSupport((uint16_t * )&Mash->bytes[ETH_HEADER_SIZE+IP_HEADER_SIZE],(length/2)));  // /2 for words

carry   = (sum & 0xFFFF0000) >> 16;
sum     = (sum & 0x0000FFFF) + carry;
sum    += (sum>>16); // that last add could have overflowed

return ((0xFFFF ^ (uint16_t)sum));
}*/
// ----------------------------------------------------------------------------
void inline UDP_Endianism(UDP_header * Header)
{  // Swap the UDP byte order immediately after receipt/before transmission
Header->sourcePort      =BYTESWAP16(Header->sourcePort);
Header->destinationPort =BYTESWAP16(Header->destinationPort);
Header->messageLength   =BYTESWAP16(Header->messageLength);  
}
// ----------------------------------------------------------------------------
void launchUDP(MergedPacket * Mash, IP4_address * ToIP,uint16_t sourcePort, 
                 uint16_t destinationPort, uint16_t data_length,
          void (* callback)(uint16_t start,uint16_t length,uint8_t * result),
          uint16_t offset)
{ // Setup the UDP header, checksum, IP4 header and then send the packet 

Mash->UDP.sourcePort      =sourcePort;
Mash->UDP.destinationPort =destinationPort;
Mash->UDP.messageLength   =data_length+UDP_HEADER_SIZE;  

UDP_Endianism(&Mash->UDP);

//Mash->UDP.UDP_checksum=0x0000;  // 0 does not suffer from endianism
//Mash->UDP.UDP_checksum=UDP_Checksum(Mash,&myIP,ToIP);  // NOW DONE BY LINK LAYER

// Insert IP4 header into Mash
prepareIP4(Mash, BYTESWAP16(Mash->UDP.messageLength),ToIP,UDPinIP4);  

launchIP4(Mash,CS_UDP,callback,offset);

UDP_Endianism(&Mash->UDP);  // Return Mash in usable state

return;
}
// ----------------------------------------------------------------------------
void handleUDP(MergedPacket * Mash,uint8_t flags)
{
// Check UDP checksum, silently reject if necessary
/* Now done outside
if (!Mash->UDP.UDP_checksum) // Special case for UDP : csum = 0 => not set => OK
  if  (UDP_Checksum(Mash,&Mash->IP4.source,&Mash->IP4.destination)) return;
*/
UDP_Endianism(&Mash->UDP);

// Send to correct handling routine (on basis of ports)
// Time first, it is time critical, after all!
#ifdef USE_NTP
if  (Mash->UDP.destinationPort ==UDP_Port[NTP_CLIENT_PORT_REF] && 
     Mash->UDP.sourcePort      ==NTP_SERVER_PORT)   handleNTP(&Mash->NTP);  
else 
#endif
#ifdef USE_DHCP
// Test now baked into CS_DHCP
//      if  (Mash->UDP.destinationPort ==DHCP_CLIENT_PORT &&  // DHCP client is a constant 
//           Mash->UDP.sourcePort      ==DHCP_SERVER_PORT)
  if (flags & CS_DHCP)           handleDHCP(&Mash->DHCP);  
	else
#endif
#ifdef USE_DNS
  if  (Mash->UDP.destinationPort ==UDP_Port[DNS_CLIENT_PORT_REF] &&
       Mash->UDP.sourcePort      ==DNS_SERVER_PORT)   handleDNS(&Mash->DNS);  
#endif

#ifdef USE_mDNS 
// Check PORT, MAC and IP match ??? Or allow unicast 
  if  (Mash->UDP.destinationPort ==mDNS_PORT && 
       Mash->UDP.sourcePort      ==mDNS_PORT && 
       MACForUs(&MashE.Ethernet.destinationMAC) == mDNS_MULTICAST &&
       mDNS(&Mash->IP4.destination))
    handleMDNS(Mash);  
#endif

#ifdef USE_LLMNR
// Check PORT, MAC and IP match
  if  (Mash->UDP.destinationPort ==LLMNR_PORT && 
       MACForUs(&MashE.Ethernet.destinationMAC) == LLMNR_MULTICAST &&
       LLMNR(&Mash->IP4.destination))
    handleLLMNR(Mash);  
#endif
#ifdef POWER_METER // Message format is "POWER n" here n is an ASCII digit 0,1,2 etc
  if (Mash->UDP.destinationPort ==POWER_MY_PORT && 
	    Mash->UDP_payload.words[0] ==BYTESWAP16(0x504F) && // "PO"
	    Mash->UDP_payload.words[1] ==BYTESWAP16(0x5745))   // "WE" 
      {
        handlePower((Mash->UDP_payload.bytes[6]-0x30));  
// Test correct port, and msg begins "POWE" then convert n to 0-2 by subtracting ASCII "0"

/*
for (uint8_t i=0;i<4;i++) { // TODO TEST
  LEDON;
  delay_ms(200);
  LEDOFF;
  delay_ms(200);
}*/

      }


#endif

#ifdef WHEREABOUTS // Message format is "G=nnR=nnF=nn" here n is an ASCII digit 0,1,2 etc
// Not overly fussy - don't bother matching the port we sent from (Firewall will enforce for external)
  if  (Mash->UDP.sourcePort == WHEREABOUTS_SERVER_PORT && 
	  Mash->UDP_payload.bytes[0] =='G' &&
	  Mash->UDP_payload.bytes[4] =='R' &&
	  Mash->UDP_payload.bytes[8] =='F' ) {
	    gis=(10*(Mash->UDP_payload.bytes[2] -'0')+(Mash->UDP_payload.bytes[3] -'0'))%12;  // %12 for safety  
	    ris=(10*(Mash->UDP_payload.bytes[6] -'0')+(Mash->UDP_payload.bytes[7] -'0'))%12;  // %12 for safety  
	    fis=(10*(Mash->UDP_payload.bytes[10]-'0')+(Mash->UDP_payload.bytes[11]-'0'))%12;  // %12 for safety  
        if (lastMode==MODE_WEB) lastMode=MODE_WEB_UPDATE;  // Force a refresh
  }
#endif

// Should reply with ICMP for unknown ports, choose instead to be stealthy.
return;
}
// ----------------------------------------------------------------------------
uint16_t newPort( uint8_t protocol)
{ // Finds a port for TCP or UDP See http://www.iana.org/assignments/port-numbers
// Avoid using recent port by incrementing the port and avoiding existing.

uint8_t pass,i;
uint16_t port;

switch (protocol)
{
  case (UDP_PORT):
    do
    {
      if (++UDP_low_port < START_DYNAMIC_PORTS) UDP_low_port = START_DYNAMIC_PORTS;

      pass=TRUE;
      for (i=0;i<MAX_UDP_PORTS;i++)
        if (UDP_low_port == UDP_Port[i]) pass=FALSE;
        // Does look at uninitialised bit of array, don't care if avoids a random port

    } while (!pass);  // This is a port we are using already

#ifdef USE_EEPROM
    //TODO eeprom_write_word((uint16_t *)EEPROM_UDP_PORT,UDP_low_port);
    // By saving in EEPROM we can do quick reboots without worrying about clashes
#endif

    return (UDP_low_port);

#ifdef USE_TCP  
  case (TCP_PORT):
    port=TCB[TCP_CLIENT].localPort;  // One of the two - doesn't matter [TODO now more than two]
    do
    {
      if (++port < (uint16_t)START_DYNAMIC_PORTS) port = START_DYNAMIC_PORTS;
    } while (port == TCB[TCP_CLIENT].localPort || port == TCB[TCP_SERVER].localPort);

#ifdef USE_EEPROM
    // TODO  eeprom_write_word((uint16_t *)EEPROM_TCP_PORT,port); 
    // By saving in EEPROM we can do quick reboots without worrying about clashes
#endif
    
    return (port);
#endif
}
return (0); // Failure to get here
}
