/********************************************
 Driver for ENC28J60 Ethernet module
 S Combes, July 2016

Copyright (C) 2016-19  S Combes

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


 N.B. ENC28J60 has errata sheet, some significant.

 Used with Microcontroller clock below 16MHz may be unreliable, because 
 errata (on versions before B5) says SPI unreliable on setting MAC registers
 below 8MHz and fastest SPI is clk/2.  

 Designed for use by space constrained microcontrollers, (e.g. where a 2kB RAM 
 system will struggle with a 1,500 byte ethernet packet) so allows partial read of
 packets and stream access.  Quid pro quo is that we must handle higher level
 protocol checksums at this level so we do not start acting on packets whose
 checksum subsequently fails.  Annoyingly all hardware checksum capabilities 
 seem to be useless (see errata), since they stop receipt by silently 
 discarding incoming packets received while active.

 Only attempts to address half-duplex (could be adapted) - however it appears that
 for all general purpose use, only half-duplex makes any sense.  Any auto-sensing
 switch will see the unit as half duplex (datasheet p53) and see also :
 https://www.ccsinfo.com/forum/viewtopic.php?p=63555.  Full duplex only works if
 the other end is hardwired to expect 10Mbps full duplex.

 *** For space reasons, breaches (where necessary) the protocol layering
 approach.  Instances are :

 1.  Calculates IP and TCP/UDP checksums at link layer : latter necessary because 
 larger packets are not passed complete to the network layer for space reason - 
 instead read/written as streams or callbacks.  Effectively checksum offloading.
 2.  Drops the DHPC protocol's Sname and Bootfile fields - 196 bytes that are always
 zero for our application; the 10 zero bytes in its Hardware spec (we use the ethernet
 6) and the magic cookie (known).
 3.  Drops the IP options field from the linear address space reply (but allows 
 access to it via a function call).

 ***** CODE DOES NOT PROTECT THE PROGRAMMER - it is possible to pass a MAC register address
 to an Eth setting routine.  It does not check the bank, either.

// Public interfaces (hardware invariant - see link.h) 

 uint8_t linkPacketsAvailable(void);  // Count of available packets, also T/F for available

 linkInitialise(MAC_address myMAC);   // Initialisation with defined MAC address

 linkSoftReset();

 linkDoneWithPacket(void);  // Call when packet can be discarded from ENC28J60 memory

 uint16_t linkPacketHeader(uint16_t maxSize,uint8_t * dataBuffer,uint8_t * flags) // Flags : IP OK, UDP OK, TCP OK
 // TCP csum only good if IP header is present, IP csum good, TCP header present, TCP CSUM good

 linkNextByte();
 void linkReadRandomAccess(uint16_t offset);  
 // for Instance, copes with DHCP's non-linear 'overloaded', data structure

// Typical use (pseudocode)

 Initialise;
 while (TRUE)
   if (PacketsAvailable) 
     get PacketHeader(), which might be all packet
     check CSUMs
     act on header
     while (more data)
       call for more bytes if required
     DoneWithPacket
   if (I have something to send)
     packetSend()
       construct Checksums if asked
       send

// Ethernet packets have 8 byte preamble/start : hidden from us by module.
// Then 14 byte header and payload from 46-1500 bytes (Fig 5-1 of datasheet)
// Ends 4 byte CRC : module can generate and check automatically (intend this
// always to be so)

*********************************************/

#include "config.h"
#ifdef USE_ENC28J60

#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <string.h>

#include "network.h"
#include "transport.h"
#include "link.h"

#ifdef USE_DHCP
uint8_t magic_cookie[4]={99,130,83,99};
#define MAGIC_CSUM (BYTESWAP16(0xB6E5))
uint8_t dhcp_option_overload;  
// Bits 0,1 indicate whether file and sname fields are overloaded
// Bits 4,5 indicate where we are in the read process (options->file->sname)
// 4,5 = 0 is reading options; =1 is reading file field, =2 is reading sname field
#endif

// MACROS
#define SPIN_SPI(X)    SPDR=(X);WAIT_SPI()   // One spin cycle, sending X (No closing ;)
#define WAIT_SPI()     while(!(SPSR&(1<<SPIF)))
#define ETH_ACTIVATE    (ETH_SPI_SEL_PORT&=(~(1<<ETH_SPI_SEL_CS)))
#define ETH_DEACTIVATE  (ETH_SPI_SEL_PORT|=  (1<<ETH_SPI_SEL_CS))

extern IP4_address myIP;
extern MAC_address myMAC;
 
static uint16_t ptrNextPacket;  // ptr variables are pointers into ENC28J60 memory
static uint16_t ptrThisPacket;
static uint8_t inProgress=FALSE;  // Am I processing a packet?
static uint8_t currentBank=99;    // Force an initial setting
static uint16_t IPoptlen;         // IPv4 option length

union {  // Machine endianism solution
  uint16_t word;
  struct { 
    uint8_t byte_1;   // 1st byte, irresepective of endianism
    uint8_t byte_2;   // 2nd ditto
  };
} join;

uint8_t  linkPacketsAvailable(void);

// ---------------------------------------------------------------------------
static uint8_t readEthRegister(uint8_t reg) 
{ // Need to be in the right bank before we start
ETH_ACTIVATE;
SPIN_SPI(reg);
SPIN_SPI(0);
ETH_DEACTIVATE ;
return (SPDR);
}
// ---------------------------------------------------------------------------
static void writeEthRegister(uint8_t reg,uint8_t data) 
{ // Need to be in the right bank before we start
ETH_ACTIVATE ;
SPIN_SPI(0x40 | reg);
SPIN_SPI(data);
ETH_DEACTIVATE ;
return;
}
// ---------------------------------------------------------------------------
static uint8_t readMACRegister(uint8_t reg) 
{
uint8_t sure;  //  Copes with potential unreliable reads - errata (1)
do {
  ETH_ACTIVATE ;
  SPIN_SPI(reg);
  SPIN_SPI(0);  // Extra dummy read
  SPIN_SPI(0); 
  ETH_DEACTIVATE ;
  ETH_ACTIVATE ;
  SPIN_SPI(reg);
  SPIN_SPI(0);  // Extra dummy read
  SPIN_SPI(0); 
  sure=SPDR;
  ETH_DEACTIVATE ;
  ETH_ACTIVATE ;
  SPIN_SPI(reg);
  SPIN_SPI(0);  // Extra dummy read
  SPIN_SPI(0); 
  ETH_DEACTIVATE ;
} while (sure!=SPDR);

return (sure);
}
// ---------------------------------------------------------------------------
// Reads are same for MAC and MII
#define readMIIRegister(X) (readMACRegister((X))) // Macro for zero call overhead
// ---------------------------------------------------------------------------
// Writes are all the same, but note (errata 1) that SPI writes could be unrealiable,
// so keep going until it works.
static void writeMACRegister(uint8_t reg,uint8_t data) 
{
do    { writeEthRegister(reg,data); } 
while (readMACRegister(reg)!=data); 
}
// ---------------------------------------------------------------------------
#define writeMIIRegister(X,Y) (writeEthRegister((X),(Y))) // Macro for zero call overhead
// ---------------------------------------------------------------------------
static void setBank(uint8_t bank) 
{
if (currentBank!=bank) {
  uint8_t dummy=readEthRegister(ETH_ECON1) & 0xFC;   
  currentBank=(bank & 0x03);
  writeEthRegister(ETH_ECON1,(dummy | currentBank));
}
}
// ---------------------------------------------------------------------------
void linkReadBufferMemoryArray(uint16_t len,uint8_t * dataBuffer) 
{ 
// Needs AUTOINC set.  If so, will just keep reading consecutive bytes,
// without resending 0x3A, as long as CS held low (i.e. no ETH_DEACTIVATE ).

ETH_ACTIVATE ;
SPIN_SPI(0x3A);
while (len--) { 
  SPIN_SPI(0);
  *(dataBuffer++)=SPDR;
}
ETH_DEACTIVATE ;
}
// ---------------------------------------------------------------------------
static void readBufferByte(uint8_t * data) { linkReadBufferMemoryArray(1,data); }
// ---------------------------------------------------------------------------
uint8_t linkNextByte(void) 
          { uint8_t data; linkReadBufferMemoryArray(1,&data); return data; }
// ---------------------------------------------------------------------------
static void writeBufferMemoryArray(uint16_t len,uint8_t * dataBuffer) 
{ 
// Needs AUTOINC set.  If so, will just keep writing consecutive bytes,
// without resending 0x&A, as long as CS held low (i.e. no ETH_DEACTIVATE ).

ETH_ACTIVATE ;
SPIN_SPI(0x7A);
while (len--) {  SPIN_SPI(*(dataBuffer++)); }
ETH_DEACTIVATE ;
}
// ---------------------------------------------------------------------------
static void writeBufferMemoryZeros(uint16_t len) 
{ 
// Needs AUTOINC set. 

ETH_ACTIVATE ;
SPIN_SPI(0x7A);
while (len--) {  SPIN_SPI(0); }
ETH_DEACTIVATE ;
}
// ---------------------------------------------------------------------------
static void writeBufferByte(uint8_t data) {writeBufferMemoryArray(1,&data); }
// ---------------------------------------------------------------------------
static void ethBitFieldSet(uint8_t reg,uint8_t mask)
{ // Eth registers only.  No equivalent for MAC,MII.  Sets bits in mask (i.e. bitwise OR with mask)
ETH_ACTIVATE ;
SPIN_SPI(0x80 | reg);
SPIN_SPI(mask);
ETH_DEACTIVATE ;
}
// ---------------------------------------------------------------------------
static void ethBitFieldClr(uint8_t reg,uint8_t mask)
{ // Eth registers only.  No equivalent for MAC,MII.  Clears bits in mask (i.e. bitwise AND with ~mask)
ETH_ACTIVATE ;
SPIN_SPI(0xA0 | reg);
SPIN_SPI(mask);
ETH_DEACTIVATE ;
}
// ---------------------------------------------------------------------------
static uint16_t readPhyRegister(uint8_t reg)
{  // Note changes bank.  Datasheet p19.

setBank(2);
writeEthRegister(0x14,reg);  // MIREGADR needs to hold target reg
ethBitFieldSet(0x12,1<<0);   // MIIRD in MICMD
delay_ms(1.0);  // spec says 10.24us
setBank(3);
uint8_t state=TRUE;
while (state) state=readEthRegister(0x0A)&0x01;  // MISTAT BUSY?
setBank(2);
ethBitFieldClr(0x12,1<<0);   // MIIRD in MICMD
uint16_t result=readEthRegister(0x18);         // MIRDL
result|=((uint16_t)readEthRegister(0x19))<<8;  // MIRDH

return result;
}
// ---------------------------------------------------------------------------
static void writePhyRegister(uint8_t reg,uint16_t data)
{  // Note changes bank.  Datasheet p19.

setBank(2);
writeEthRegister(0x14,reg);        // MIREGADR needs to hold target reg
writeEthRegister(0x16,data&0xFF);  // MIWRL
writeEthRegister(0x17,data>>8);    // MIWRH.  Starts transfer.
delay_ms(1.0);  // spec says 10.24us
setBank(3);
uint8_t state=TRUE;
while (state) state=readEthRegister(0x0A)&0x01;  // MISTAT BUSY?

return;
}
// ---------------------------------------------------------------------------
static void linkSoftReset(void)
{
ETH_ACTIVATE ;
SPIN_SPI(0xFF);
ETH_DEACTIVATE ;
delay_ms(100);
currentBank=99;  // Force setting
}
// ---------------------------------------------------------------------------
void setClockoutScale(uint8_t data) 
{ 
setBank(3);
writeEthRegister(0x15,data&0x07);  // ECOCON, Mask to relevant bits
return;
}
// ---------------------------------------------------------------------------
void setClockout25MHz(void)     { setClockoutScale(1); }
// ---------------------------------------------------------------------------
void setClockout12pt5MHz(void)  { setClockoutScale(2); }
// ---------------------------------------------------------------------------
void setClockout8pt33MHz(void)  { setClockoutScale(3); }
// ---------------------------------------------------------------------------
void setClockout6pt25MHz(void)  { setClockoutScale(4); }
// ---------------------------------------------------------------------------
void setClockout3pt125MHz(void) { setClockoutScale(5); }
// ---------------------------------------------------------------------------
static inline void wrapReadIndex(uint16_t * index) 
{
if (*index>ERXND) *index-=(ERXND+1);
return;
}
// ---------------------------------------------------------------------------
static inline void setReadPointer(uint16_t ptr,uint8_t isReadBuffer) 
{ // This is where the SPI reads from the ENC28J60, not the RX read pointer which
  // tells the ENC28J60 the end of its buffer that has been finally read (and so
  // up to which can be overwritten).

if (isReadBuffer) wrapReadIndex(&ptr);
writeEthRegister(0x00,ptr&0xFF);           // L,H Read pointer
writeEthRegister(0x01,ptr>>8);  
}
// ---------------------------------------------------------------------------
static uint16_t resolveCsum(uint32_t csum)
{
// I've added it all together in a uint32_t, now boil it down to the checksum

uint32_t carry=(csum&0xFFFF0000)>>16;
csum =(csum&0x0000FFFF)+carry; 
csum+=(csum>>16);   // Could have regenerated a carry by the last add
// (see Internetworking with TCP/IP Comer & Stevens)

return (((uint16_t)csum)^0xFFFF);  
}
// ---------------------------------------------------------------------------
static uint32_t pktCsumRaw(uint16_t ptrStart,uint16_t bytes,uint8_t isReadBuffer) 
{
// Gets a checksum from a (portion of) a packet in the ENC28J60 memory 
// (i.e. access via SPI)
// 'Raw' because adds to uint32 - result needs to pass to resolveCsum()

// 'ptrStart' : index in ENC28J60 memory (Note may need to wrap - handled 
// automatically in RX data, never happens in TX)
// 'bytes' : number over which checksum is calculated (Copes with odd/even length)
// 'isReadBuffer' : T/F whether we are in RX data (which may wrap around)

setBank(0);
setReadPointer(ptrStart,isReadBuffer); // Handles wrap, if required
uint32_t result=0;

uint16_t i;
for (i=0;i<(bytes/2);i++) {
  linkReadBufferMemoryArray(2,&join.byte_1);
  result+=join.word;
}

if (bytes%2) { // Odd, so add a trailing zero after byte read
  readBufferByte(&join.byte_1);
  join.byte_2=0;
  result+=join.word;
}
return (result);
}
// ---------------------------------------------------------------------------
static uint16_t pktCsum(uint16_t ptrStart,uint16_t bytes,uint8_t isReadBuffer) 
{ // Gets a checksum from a packet in the ENC28J60 memory
// N.B. Ideally would do in hardware, but current errata suggests this is dangerous
// Therefore now not used.
return (resolveCsum(pktCsumRaw(ptrStart,bytes,isReadBuffer)));
}
// ----------------------------------------------------------------------------
static uint16_t ICMPchecksum(MergedPacket * Mash) { 
// Returns checksum of ICMP packet found in Mash
// Details see http://www.netfor2.com/checksum.html
// Note that this is endian independent
// Note BYTESWAP16 - don't call from outside
uint16_t length;

length=(BYTESWAP16(Mash->IP4.totalLength)) - Mash->IP4.headerLength*4;  // *4 for words to bytes

if (length % 2) // Pad to even byte with zero, and increment length to hit next word
  if (length<(MAX_STORED_SIZE-ETH_HEADER_SIZE+Mash->IP4.headerLength*4)) // Overflow guard (but CSUM will fail)
    Mash->bytes[ETH_HEADER_SIZE+Mash->IP4.headerLength*4+length++]=0;

return (checksum(&(Mash->words[(ETH_HEADER_SIZE+Mash->IP4.headerLength*4)/2]),(length/2))); 
}                                                        // /2 for bytes->words

// ---------------------------------------------------------------------------
static uint16_t TransportCsum(uint16_t ptrStart,uint8_t * dataBuffer,
                uint16_t inBuffer,uint16_t stopAt,uint32_t csum,
                uint16_t protocol,uint8_t isReadBuffer) 
{ // Gets a TCP or UDP checksum from a packet.  Header and some data will
  // be in microcontroller RAM, rest in the ENC28J60 memory (slow, uses SPI)
  // Hence try to work with as much as possible from RAM.
  // Also - routines that construct the packet can help by pre-computing
  // checksum elements while in RAM.
  
// N.B. Ideally would do in ENC28J60 hardware, but current errata suggests 
// this is dangerous.

// 'ptrStart' : (measured by index into ENC28J60 memory) should be the start
// of the ethernet packet, because of the need to include pseudo header.
// 'inBuffer' : count of bytes in own memory
// 'stopAt' : only calculate CSUM over first n bytes of packet (if zero, use full size)
// 'csum' : either 0 or incoming precomputed partial checkcum
// 'protocol' : UDPinIP4 or TCPinIP4
// 'isReadBuffer' : whether we are looking at a received packet (T/F)

// Assumes that we have read header into dataBuffer.

// *** Ignores extant checksum field, so can't use process over that, expecting a zero

MergedPacket * Mash=(MergedPacket *)dataBuffer;

// Note that after end of smallest IP header (20 bytes) the tcp packet is
// offset in buffer memory compared to the original packet (_link) by the
// size of any IP options (we never use IP options outbound)

uint16_t optionOffset=(Mash->IP4.headerLength-5)*4;  // Bytes of IP headers

uint16_t size=ETH_HEADER_SIZE+BYTESWAP16(Mash->IP4.totalLength)-optionOffset;
if (stopAt==0) stopAt=size;
// This is a faked size, knowing that any IP options have been removed
// However it is also more realistic than the received packet size, because that
// includes any padding of short packets.

if (protocol==TCPinIP4) {
  csum+=BYTESWAP16(TCPinIP4); 
  checksumBare(&csum,(uint16_t *)&Mash->TCP,8); 
  // Count of words before csum (implicity sets csum to zero as we ignore it)

  uint16_t payloadInBuffer=inBuffer-(ETH_HEADER_SIZE+Mash->IP4.headerLength*4+TCP_HEADER_SIZE);
  uint16_t payloadToConsider=stopAt-(ETH_HEADER_SIZE+Mash->IP4.headerLength*4+TCP_HEADER_SIZE);
  if (payloadInBuffer>payloadToConsider) payloadInBuffer=payloadToConsider;
  checksumBare(&csum,&Mash->TCP.urgent,1+payloadInBuffer/2); 
  // Start at 1 word before data (the Urgent word) and add 1 word to length

  if (stopAt>inBuffer) {
    uint16_t ptrToLink=ptrStart+inBuffer+optionOffset;  // Wrapped in call if required
    csum+=pktCsumRaw(ptrToLink,(stopAt-inBuffer),isReadBuffer);
  } else { // All in dataBuffer but if count was odd ...
    if (payloadInBuffer%2) { 
      join.word=Mash->TCP_payload.words[payloadInBuffer/2];
      join.byte_2=0;  // Zeroise beyond our packet
      csum+=join.word;
    }
  }
  csum+=BYTESWAP16((size-(ETH_HEADER_SIZE+IP_HEADER_SIZE))); // no-options IP header
} else if (protocol==UDPinIP4) {
  csum+=BYTESWAP16(UDPinIP4); 
  checksumBare(&csum,(uint16_t *)&Mash->UDP,3); 
  // Count of words before csum (implicity sets csum to zero as we ignore it)
 
  uint16_t payloadInBuffer=inBuffer-(ETH_HEADER_SIZE+Mash->IP4.headerLength*4+UDP_HEADER_SIZE);
  uint16_t payloadToConsider=stopAt-(ETH_HEADER_SIZE+Mash->IP4.headerLength*4+UDP_HEADER_SIZE);
  if (payloadInBuffer>payloadToConsider) payloadInBuffer=payloadToConsider;
  checksumBare(&csum,Mash->UDP_payload.words,payloadInBuffer/2); // Start at data

  if (stopAt>inBuffer) {
    uint16_t ptrToLink=ptrStart+inBuffer+optionOffset;  // Wrapped in call if required
    csum+=pktCsumRaw(ptrToLink,(stopAt-inBuffer),isReadBuffer);
  } else { // All in dataBuffer but if count was odd ...
    if (payloadInBuffer%2) { 
      join.word=Mash->UDP_payload.words[payloadInBuffer/2];
      join.byte_2=0;  // Zeroise beyond our packet
      csum+=join.word;
    }
  }
  csum+=Mash->UDP.messageLength; 
}

// Pseudo header - know to be in dataBuffer RAM
checksumBare(&csum,(uint16_t *)&dataBuffer[ETH_HEADER_SIZE+12],4);

return (resolveCsum(csum));
}
// ---------------------------------------------------------------------------
void linkInitialise(MAC_address myMAC)
{ // Active - probably needs to happen after any other possible users of SPI have 
  // set their CS high to avoid conflicts.
  
ETH_SPI_SEL_DDR |= (1<<ETH_SPI_SEL_CS);
ETH_DEACTIVATE ;
SPI_CONTROL_DDR |= ((1<<SPI_CONTROL_MOSI) | (1<<SPI_CONTROL_SCK));
SPI_CONTROL_DDR &= (~(1<<SPI_CONTROL_MISO));

SPI_CONTROL_PORT&=(~((1<<SPI_CONTROL_MOSI)|(1<<SPI_CONTROL_SCK)));

SPSR |= (1 << SPI2X); //  Fast as we can - see errata Pt1.
SPCR &=  ~((1 << SPR1) | (1 << SPR0));

SPCR |= (1<<SPE)|(1<<MSTR);

linkSoftReset();

setBank(0);    // Should reset here anyway
ptrNextPacket=0;  // Initialisation from Errata

// Datasheet 6.1 : Receive buffer

writeEthRegister(0x08,ERXST&0xFF);  // L,H RX buffer start
writeEthRegister(0x09,ERXST>>8);    // Automatically sets ERXWRPT (datasheet p33)

writeEthRegister(0x0A,ERXND&0xFF);  // L,H RX buffer end (inclusive)
writeEthRegister(0x0B,ERXND>>8);

writeEthRegister(0x0C,ERXST&0xFF);  // L,H RX read pointer (start of buffer)
writeEthRegister(0x0D,ERXST>>8);

// Datasheet 6.2 : Transmit Buffer
// TX buffer needs no initialisation.  7 spare bytes included in .h file in ETXST

// Datasheet 6.3 : Receive Filters
setBank(1); 

// Define the packets we accept.  Have found a firewall that Broadcasts the DHCP offer
// so need broadcast to enable DHCP reliably.

#if defined USE_mDNS | defined USE_LLMNR

// Use pattern match to accept mDNS and LLMNR packets (which are very similar) with
// (most of) our hostname as remaining key.

// CSUM based on 1 : 1st 5 digits of destination MAC = (01 00) (5E 00) (00 ...
//               2 : 1st 3 digits of destination IP  = ... E0) (00 00)
//               3 : Hostname length (1 byte) & 1st character of hostname
//               4 : Up to next 8 hostname characters in pairs
  
// *** TODO - this DESTROYS case insensitivity of hostname.  Can't really use HOSTNAME in patternmatch.
  
writeEthRegister(0x14, 0x00);
writeEthRegister(0x15, 0x00); // Zero offset in EPMO

writeEthRegister(0x08, 0x1f);  // Destination MAC (1st 5)
writeEthRegister(0x09, 0x00);
writeEthRegister(0x0A, 0x00);
writeEthRegister(0x0B, 0xC0);  // Destination IP 1st 3
writeEthRegister(0x0C, 0x01);  // ditto
writeEthRegister(0x0D, 0x00);
//writeEthRegister(0x0E, 0xC0);  // Length of HOSTNAME and 1st char (always assume >=1 char)
writeEthRegister(0x0E, 0x00);  // Length of HOSTNAME and 1st char (always assume >=1 char)

uint8_t pat=0;
//uint32_t csum=0x0100 + 0x5E00 + 0x00E0 + (((uint16_t)strlen(HOSTNAME))<<8) + (uint8_t)HOSTNAME[0];
uint32_t csum=0x0100 + 0x5E00 + 0x00E0;// + (((uint16_t)strlen(HOSTNAME))<<8) + (uint8_t)HOSTNAME[0];

// Do in pairs as not convinced 00 padding works in Pmatch
/*
if (strlen(HOSTNAME)>2) {
  pat|=(0x03);
  csum+=(((uint16_t)HOSTNAME[1])<<8) + (uint8_t)HOSTNAME[2];
}
if (strlen(HOSTNAME)>4) {
  pat|=(0x0C);
  csum+=(((uint16_t)HOSTNAME[3])<<8) + (uint8_t)HOSTNAME[4];
}
if (strlen(HOSTNAME)>6) {
  pat|=(0x30);
  csum+=(((uint16_t)HOSTNAME[5])<<8) + (uint8_t)HOSTNAME[6];
}
if (strlen(HOSTNAME)>8) {
  pat|=(0xC0);
  csum+=(((uint16_t)HOSTNAME[7])<<8) + (uint8_t)HOSTNAME[8];
}
*/
writeEthRegister(0x0F,pat);

uint32_t carry=(csum&0xFFFF0000)>>16;
csum =(csum&0x0000FFFF)+carry; 
csum+=(csum>>16);   // Could have regenerated a carry by the last add
// (see Internetworking with TCP/IP Comer & Stevens)

uint16_t csum16=((uint16_t)csum^0xFFFF);  // 16 bit 1s complement (i.e. XOR 1111...1111);

writeEthRegister(0x10,(uint8_t)(csum16&0xFF)); 
writeEthRegister(0x11,(uint8_t)(csum16>>8));

writeEthRegister(0x18, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_BCEN|ERXFCON_PMEN);

// *** Pattern match does not seem to work - allows other hosts through *** 

// Instead of pattern match above, Could allow multicast (mDNS and LLMNR) and 
// broadcast (some DHCP), not pattern match.  Using instead  :  
//   writeEthRegister(0x18, ERXFCON_UCEN|ERXFCON_MCEN|ERXFCON_BCEN|ERXFCON_CRCEN);

// On the assumption that most packets not for us are filtered by the switch
// these days, suppressing multicast to a narrow window helps reduce remaining 
// processing load on the microcontroller.
// However experiments suggest load is not that bad anyway.

#else

// Always need Unicast and Broadcast and CRC checking is wise
writeEthRegister(0x18, ERXFCON_UCEN|ERXFCON_BCEN|ERXFCON_CRCEN);

#endif

// Datasheet 6.4 
delay_ms(5.0);      // Errata : just wait.

setBank(2); 
// Datasheet 6.5
uint8_t tmp=readMACRegister(0x00);  // MACON1
writeMACRegister(0x00,tmp | 0x01);  // MACON1 : Set MARXEN - enable packets to be received by MAC

// Next line generates FCS errors in Wireshark (but packets get through OK).  
// Replace with 60 byte version.  Wiresharks documentation asserts FCS part of min 64 bytes.
//writeMACRegister(0x02,0xF2); // MACON3 : Pad to 64 bytes with CRC, no jumbo frames, half-duplex
writeMACRegister(0x02,0x32); // MACON3 : Pad to 60 bytes with CRC, no jumbo frames, half-duplex

writeMACRegister(0x03,0x40); // MACON4 : Normal, compliant
writeMACRegister(0x0A,MAX_RX_PACKET & 0xFF); // MAMXFLL
writeMACRegister(0x0B,MAX_RX_PACKET>>8);     // MAMXFLH
writeMACRegister(0x04,0x12);                 // MABBIPG half-duplex
writeMACRegister(0x06,0x12);                 // MAIPGL standard
writeMACRegister(0x07,0x0C);                 // MAIPGH standard for half-duplex

// Leave MACLCON1,2 at defaults

setBank(3); 
for (uint8_t i=0;i<6;i++) {
  writeMACRegister(((i%2)?(i-1):(i+1)),myMAC.MAC[5-i]);     
  // MAC is words individually in little-endian order
}

// Datasheet 6.6
// Ignore PHCON1 - assume external circuitry correct.  Also, how would we act if differ?

setBank(2);
uint16_t data=readPhyRegister(0x10);    // PHCON2
writePhyRegister(0x10,data | (1<<8));   // Set HDLDIS bit : no automatic loopback

ethBitFieldSet(ETH_ECON2,ECON2_AUTOINC); // Reset should have set anyway 

delay_ms(5);  // Just in case

writeEthRegister(ETH_ECON1,ECON1_RXEN);  // Start receiving
}
// ---------------------------------------------------------------------------
uint16_t linkPacketHeader(uint16_t maxSize,uint8_t * dataBuffer,uint8_t * flags) 
{ // Gets the next packet, or at least size header bytes.  
  // Returns true packet size, which could be > or < maxSize.  In former case
  // the rest of the packet is unread.
  // See also linkMorePacket();
  // Should call packetDone() before next one, but automates this on next call.
  // Consequence is that if called twice on 'same' packet, actually moves to next.
 
(*flags)=0;  // Clear on entry
IPoptlen=0;  // ditto

uint16_t size; // Size of received packet.  For small (padded) packets may not represent 
// true size - for which refer to in-packet data (e.g. IP length field)

if (inProgress) linkDoneWithPacket();   // Cancel last one before we do this one.
if (!linkPacketsAvailable()) return 0;  // Might have just changed

MergedPacket * mp=(MergedPacket *)dataBuffer;

ptrThisPacket=ptrNextPacket;

setBank(0);
setReadPointer(ptrThisPacket,TRUE);

linkReadBufferMemoryArray(2,&join.byte_1);   // Note Next Packet pointer is little endian
ptrNextPacket=join.byte_1|((uint16_t)(join.byte_2)<<8);  // Our endianism is known by our compiler

linkReadBufferMemoryArray(2,(uint8_t *)&join.byte_1); // Also Littleendian
size=(join.byte_1|((uint16_t)(join.byte_2)<<8))-4;    // -4 to drop CRC bytes

uint8_t status[2];
linkReadBufferMemoryArray(2,status);
if (!(status[0]&RX_OK)) {  // Dud packet
  linkDoneWithPacket();
  return (0);
}
inProgress=TRUE;

uint16_t toRead=(size<MAX_HEADER_SIZE)?size:MAX_HEADER_SIZE;

linkReadBufferMemoryArray(toRead,dataBuffer);
// MAX_HEADER_SIZE(=42) reads Ethernet and IP headers and, as long as there
// are no IP options, will have read an ICMP or UDP header as well.

if (size<14) return (size);

uint16_t protocol=BYTESWAP16(mp->Ethernet.type);

// if (protocol<=1500) return (protocol+14); // Non standard protocol, treated as length
if (protocol<=1500) { linkDoneWithPacket(); return (0);} // Non standard protocol, to ignore

switch (protocol) {

  case ARPinETHERNET: // Fully handle ARP replies at the link level -------------------
    
    if (MACForUs(&mp->Ethernet.destinationMAC) &&  // Only reply to ours
        IP4ForUs(&mp->ARP.destinationIP) &&
        mp->ARP.type==BYTESWAP16(ARP_REQUEST)) {

      mp->ARP.type=BYTESWAP16(ARP_REPLY);

      copyMAC(&mp->Ethernet.destinationMAC,&mp->Ethernet.sourceMAC);
      copyMAC(&mp->Ethernet.sourceMAC,&myMAC);

      copyIP4(&mp->ARP.destinationIP,&mp->ARP.sourceIP);
      copyIP4(&mp->ARP.sourceIP,&myIP);

      copyMAC(&mp->ARP.destinationMAC,&mp->ARP.sourceMAC);
      copyMAC(&mp->ARP.sourceMAC,&myMAC);

      mp->ARP.hardware=BYTESWAP16(DLLisETHERNET); 
      mp->ARP.protocol=BYTESWAP16(ARPforIP);  	
      mp->ARP.hardware_size=6;  // Length of MAC
      mp->ARP.protocol_size=4;	// Length of IP V4

      linkPacketSend(dataBuffer,42,NO_CSUM,NULL,0);
    }
    linkDoneWithPacket(); // Done with any ARP we heard, whether we replied or not.
    return (0);

  case IP4inETHERNET: // ---------------------------------------------------------------

    // So, are there IP options?
    IPoptlen=4*mp->IP4.headerLength-20;

    // We probably have the IP header in memory, so we can do a quick checksum.
    // However if it had a lot of opions, it might not all be in memory.

    int16_t topUp=(IPoptlen+ETH_HEADER_SIZE+IP_HEADER_SIZE)-MAX_HEADER_SIZE;
    if ((topUp+toRead)>maxSize) topUp=maxSize-toRead;  // Stops buffer overflow
    // if header read is larger than allocated buffer, but will then fail checksum.
    // Unlikely to arise (IP options are rare) and impossible if buffer is 
    // > (128+14+20)bytes (max IP4 headers + ethernet + IP4 header).
    // Hence 162 is safe buffer length.  TODO - make more robust.
  
    if (topUp>0) linkReadBufferMemoryArray(topUp,&dataBuffer[toRead]);

    if (!IP4checksum(mp)) { 
      *flags|=(CS_IP4); // Valid IP4
      // Now does it contain something else?

      if (size>=(ETH_HEADER_SIZE+IP_HEADER_SIZE+IPoptlen)) {
        uint8_t protocol2=mp->IP4.protocol;

        // Now read rest of requested bytes of packet
        uint16_t remainingRead=((size<maxSize)?size:maxSize)-toRead;
        linkReadBufferMemoryArray(remainingRead,&dataBuffer[toRead]); // TODO is this right if options existed? ****
        // N.B. This read now overwrites previous IP4 Options (if any existed)

        if (protocol2==UDPinIP4) { 

          uint16_t ibegin=ptrThisPacket+ENC28J60_PREAMBLE; 
          wrapReadIndex(&ibegin);
          uint16_t csum=TransportCsum(ibegin,dataBuffer,toRead+remainingRead,0,0,UDPinIP4,TRUE);
          if (csum==mp->UDP.UDP_checksum) { 
            *flags|=(CS_UDP);

#ifdef USE_DHCP 
            if ((mp->UDP.destinationPort==BYTESWAP16(DHCP_CLIENT_PORT)) &&
                (mp->UDP.sourcePort     ==BYTESWAP16(DHCP_SERVER_PORT))) {

              setBank(0);
              setReadPointer(ibegin+IPoptlen+DHCP_MAGIC_COOKIE_OFFSET,TRUE);  
              uint8_t tmp;
              readBufferByte(&tmp);
              if (tmp!=magic_cookie[0]) break;
              readBufferByte(&tmp);
              if (tmp!=magic_cookie[1]) break;
              readBufferByte(&tmp);
              if (tmp!=magic_cookie[2]) break;
              readBufferByte(&tmp);
              if (tmp!=magic_cookie[3]) break;
 
              *flags|=(CS_DHCP);
              // NB. Read pointer now in right place
              dhcp_option_overload=DHCP_NO_OVERLOAD; // Default.  Overwritten when options read.
            } 
#endif          
          } else {
            LEDON; // Debug - should not reach
            eeprom_write_word((uint16_t *)0,mp->UDP.UDP_checksum);
            eeprom_write_word((uint16_t *)2,csum);
            eeprom_write_word((uint16_t *)4,ibegin);
            eeprom_write_word((uint16_t *)6,size);
            eeprom_write_word((uint16_t *)8,toRead);
            eeprom_write_word((uint16_t *)10,remainingRead);
            eeprom_write_word((uint16_t *)12,0);
            
            eeprom_write_block((uint8_t *)dataBuffer,(void *)32,size);        
            
            while (1); 
          }
        }
        else if (protocol2==TCPinIP4) { 
          uint16_t ibegin=ptrThisPacket+ENC28J60_PREAMBLE;  
          wrapReadIndex(&ibegin);
          uint16_t csum=TransportCsum(ibegin,dataBuffer,toRead+remainingRead,0,0,TCPinIP4,TRUE);
          if (csum==mp->TCP.TCP_checksum) {
            *flags|=(CS_TCP);
          } else {
            LEDON; // Debug - should not reach
            eeprom_write_word((uint16_t *)0,mp->TCP.TCP_checksum);
            eeprom_write_word((uint16_t *)2,csum);
            eeprom_write_word((uint16_t *)4,ibegin);
            eeprom_write_word((uint16_t *)6,size);
            eeprom_write_word((uint16_t *)8,toRead);
            eeprom_write_word((uint16_t *)10,remainingRead);
            eeprom_write_word((uint16_t *)12,0);
            
            eeprom_write_block((uint8_t *)dataBuffer,(void *)32,size);        
            
            while (1); 
          }
        }

        else if (protocol2==ICMPinIP4) {
          if (!ICMPchecksum(mp)) {
            *flags|=(CS_ICMP);

            if (IP4ForUs(&mp->IP4.destination)==OUR_IP_UNICAST) { 
  
              // Needs all of packet in memory, hence size limited.  Could be designed 
              // otherwise, but doesn't really need to be for routine use.
              // Note that in theory ICMP packets can be very large. ~64kB  For now, 
              // just do the small (but usual) ones.  Corner case not really 
              // relevant to PING (what OS uses huge ping packet?)              

#ifdef IMPLEMENT_PING
              if (mp->ICMP.messagetype == PING) { 

                mp->ICMP.messagetype = PONG;  // Set packet as reply
                copyIP4(&mp->IP4.destination,&mp->IP4.source);  
                copyIP4(&mp->IP4.source,&myIP);
                copyMAC(&mp->Ethernet.destinationMAC,&mp->Ethernet.sourceMAC);
                copyMAC(&mp->Ethernet.sourceMAC,&myMAC);
                mp->IP4.headerLength=5; // No IP options in reply
                mp->IP4.TTL=0x80;        

                linkPacketSend(dataBuffer,size<maxSize?size:maxSize,(CS_IP4 | CS_ICMP),NULL,0);
                // Avoid heartbleed-like behaviour, potentially at the risk of an invalid response
                // Although checksum test probably stops large incoming packets.

                linkDoneWithPacket(); 
                return (0);
              }
#endif  // Doing PING
            }
          }
        }
      }
    }

    break;

  default: //-------------------------------------------------------------------------
    // What are you sending me that's not IP4 or ICMP?
    linkDoneWithPacket(); 
    return (0);
    break;
}
return size;  // Flags return the checksum state
}
// ---------------------------------------------------------------------------
void linkReadRandomAccess(uint16_t offset) 
  { setBank(0); setReadPointer(ptrThisPacket+offset+ENC28J60_PREAMBLE,TRUE); }
// ---------------------------------------------------------------------------
void linkDoneWithPacket(void) 
{
uint16_t oddERXRDPT;
if (ptrNextPacket==ERXST) oddERXRDPT=ERXND;        // Compensate for errata 14 - must be odd.
else                      oddERXRDPT=ptrNextPacket-1; // Method needs ERXND to be odd

writeEthRegister(0x0C,oddERXRDPT&0xFF);  // L,H RX read pointer (must write low first)
writeEthRegister(0x0D,oddERXRDPT>>8);    // Frees the space

ethBitFieldSet(ETH_ECON2,ECON2_PKTDEC);  // Decrement packet counter   

inProgress=FALSE;
}
// ---------------------------------------------------------------------------
uint8_t linkPacketsAvailable(void) // Acts as boolean and count
{
setBank(1);
return (readEthRegister(0x19)); 
}
// ---------------------------------------------------------------------------
void linkPacketSend(uint8_t * dataBuffer,uint16_t length,uint8_t checksums,
            void (* callback)(uint16_t start,uint16_t length,uint8_t * result),
            uint16_t offset)
{
// N.B. Cannot assume whole packet is in dataBuffer because of 'oversize' technique.
// Note that packet is preceded by single byte instruction (allows override of
// default tx settings).  Hence dataBuffer[0] aligns with ETXST+1.  Often obscured
// by un-indexed 'stream' access.

#define CTRL_HEADER_SIZE (1)  // 1 byte header
#define IP_CHECKSUM_AT   (CTRL_HEADER_SIZE+ETH_HEADER_SIZE+10)
#define ICMP_CHECKSUM_AT (CTRL_HEADER_SIZE+ETH_HEADER_SIZE+IP_HEADER_SIZE+2)
#define UDP_CHECKSUM_AT  (CTRL_HEADER_SIZE+ETH_HEADER_SIZE+IP_HEADER_SIZE+6)
#define TCP_CHECKSUM_AT  (CTRL_HEADER_SIZE+ETH_HEADER_SIZE+IP_HEADER_SIZE+16)

MergedPacket * mp=(MergedPacket *)dataBuffer;

if (length==0) return;
if (length>MAX_TX_PACKET) length=MAX_TX_PACKET;  // Truncate better than drop?

uint16_t forCsum=0;    // Transport csums : Default zero=full packet
uint32_t precompute=0; // Transport csums : precomputed portion

// Wait for last one
while (readEthRegister(ETH_ECON1) & ECON1_TXRTS) { // Probably something being sent
// Errata point 12.
  if ((readEthRegister(ETH_EIR) & EIR_TXERIF) ) {
    ethBitFieldSet(ETH_ECON1,ECON1_TXRST);
    ethBitFieldClr(ETH_ECON1,ECON1_TXRST);
  }
}

setBank(0);
writeEthRegister(0x04,ETXST&0xFF);           // L,H TX buffer start
writeEthRegister(0x05,ETXST>>8);    

writeEthRegister(0x06,(length+ETXST)&0xFF);  // L,H TX buffer end
writeEthRegister(0x07,(length+ETXST)>>8);    

writeEthRegister(0x02,ETXST&0xFF);           // L,H write pointer - put packet here
writeEthRegister(0x03,ETXST>>8);    

writeBufferByte(0x00);  // 1st byte is Control; zero is to follow MACON3

if ((mp->UDP.destinationPort==BYTESWAP16(DHCP_SERVER_PORT)) && // Order for speed
    (mp->UDP.sourcePort     ==BYTESWAP16(DHCP_CLIENT_PORT)) &&
    (mp->Ethernet.type==BYTESWAP16(IP4inETHERNET)) &&
    (mp->IP4.protocol==UDPinIP4)) {
#ifdef USE_DHCP 

  // Always do checksum on DHCP : bespoke, because buffer is not same as packet, but
  // we can predict, so can avoid slow method used in real checksum routine.
  uint32_t csum=BYTESWAP16(UDPinIP4)+MAGIC_CSUM;  // Precomputed 

  checksums&=(~CS_UDP);                    // Suppress recalculation later

  checksumBare(&csum,(uint16_t *)&dataBuffer[ETH_HEADER_SIZE+12],7);    // Pseudo IP addresses from 
                                             // Eth header and ports and length from UDP header.
  csum+=(mp->UDP.messageLength);  

  uint16_t csumOver=(length-(ETH_HEADER_SIZE+IP_HEADER_SIZE+UDP_HEADER_SIZE+10+64+128+4));
  checksumBare(&csum,mp->UDP_payload.words,csumOver/2); 

  if (csumOver%2) { 
    join.word=mp->UDP_payload.words[csumOver/2];
    join.byte_2=0;  // Zeroise beyond our packet
    csum+=join.word;
  }
            
  // Everything else is zero, so does not affect checksum

  mp->UDP.UDP_checksum=resolveCsum(csum);

  // Now copy into card memory

  writeBufferMemoryArray(ETH_HEADER_SIZE+IP_HEADER_SIZE+UDP_HEADER_SIZE+34,dataBuffer);
  // 1st 34 bytes of data are in matching places in dataBuffer

  writeBufferMemoryZeros(10+64+128);  // Rest of Hardware, Sname, Bootfile.  Known zero.
  writeBufferMemoryArray(4,magic_cookie);
  // Continue with data in dataBuffer (is DHCP options).
  writeBufferMemoryArray(length-(ETH_HEADER_SIZE+IP_HEADER_SIZE+UDP_HEADER_SIZE+34+10+64+128),
                     &dataBuffer[ETH_HEADER_SIZE+IP_HEADER_SIZE+UDP_HEADER_SIZE+34]);

#endif
} else if (callback) { // Complex packet with callback function
  if (mp->Ethernet.type==BYTESWAP16(IP4inETHERNET)) {

    uint16_t dataAt;
    if      (mp->IP4.protocol==UDPinIP4) dataAt=ETH_HEADER_SIZE+IP_HEADER_SIZE+UDP_HEADER_SIZE;
    else if (mp->IP4.protocol==TCPinIP4) dataAt=ETH_HEADER_SIZE+IP_HEADER_SIZE+TCP_HEADER_SIZE;
    else return; // No other protocols handled (ICMP elsewhere).
    
    // Now move in intervals of size BLOCK_SIZE, retrieving data from callback and passing
    // it to the ENC28J60 memory.  Use top half of dataBuffer as temp storage (repeated reuse).
    // Do payload csum at this point (faster, SRAM) 

    writeBufferMemoryArray(dataAt,dataBuffer);  // All headers, no data
    #define BLOCK_SIZE (MAX_STORED_SIZE-(ETH_HEADER_SIZE+IP_HEADER_SIZE+TCP_HEADER_SIZE)) 
    // Quickest if even no., achieved by MAX STORED even
    uint16_t dataLen=length-dataAt;
    for (uint16_t i=0;i<dataLen;i+=BLOCK_SIZE) {
      uint16_t blen=dataLen-i;
      if (blen>BLOCK_SIZE) blen=BLOCK_SIZE;
      callback(offset+i,blen,&dataBuffer[dataAt]);
      checksumBare(&precompute,(uint16_t *)&dataBuffer[dataAt],blen/2);  // Precompute its checksum while in RAM
      if (blen%2) {
        join.word=mp->words[(dataAt+blen)/2]; 
        join.byte_2=0;
        precompute+=join.word;
      }
      writeBufferMemoryArray(blen,&dataBuffer[dataAt]);
    }
    forCsum=dataAt;    
  } else return; // Ditto
    
} else { // Short packet within RAM array
  writeBufferMemoryArray(length,dataBuffer);  
}
// Now set checksums if required

if (checksums & CS_ICMP) {  

  mp->ICMP.checksum=0;  // 0 does not suffer from endianism
  join.word=ICMPchecksum(mp);  

  writeEthRegister(0x02,(ETXST+ICMP_CHECKSUM_AT)&0xFF);  // Put in the packet
  writeEthRegister(0x03,(ETXST+ICMP_CHECKSUM_AT)>>8);    

  writeBufferMemoryArray(2,&join.byte_1);  // Same (unknown) endianism as the calculator
}

if (checksums & CS_IP4) {  // IPv4 checksum is wanted.  Starts at byte 14 and is of length 20 bytes

// Option A - slow - read back from ENC28J60. Tested OK.

/*
  writeEthRegister(0x02,(ETXST+IP_CHECKSUM_AT)&0xFF);  // L,H write pointer - checksum goes here.  Start as zero.
  writeEthRegister(0x03,(ETXST+IP_CHECKSUM_AT)>>8);  
  writeBufferMemoryZeros(2);

  join.csum=pktCsum((CTRL_HDR_SIZE+ETXST+ETH_HEADER_SIZE),IP_HEADER_SIZE,FALSE);
*/

// Option B - we know we have it in RAM, so do quick read.  Tested OK.

  mp->IP4.checksum=0;
  join.word=IP4checksum(mp);

//Back to common code.

  writeEthRegister(0x02,(ETXST+IP_CHECKSUM_AT)&0xFF);  // Put in the packet
  writeEthRegister(0x03,(ETXST+IP_CHECKSUM_AT)>>8);    

  writeBufferMemoryArray(2,&join.byte_1);  // Same (unknown) endianism as the calculator
}

if (checksums & CS_UDP) {

  join.word=TransportCsum(CTRL_HEADER_SIZE+ETXST,dataBuffer,
     (length<MAX_STORED_SIZE)?length:MAX_STORED_SIZE,forCsum,precompute,UDPinIP4,FALSE);  
  writeEthRegister(0x02,(ETXST+UDP_CHECKSUM_AT)&0xFF);  // Put into packet
  writeEthRegister(0x03,(ETXST+UDP_CHECKSUM_AT)>>8);    
  //join.word=0; // Testing override - works 'cos UDP CSUM is allowed to be zero
  writeBufferMemoryArray(2,&join.byte_1);  // Same (unknown) endianism as the calculator
}
if (checksums & CS_TCP) {

  join.word=TransportCsum(CTRL_HEADER_SIZE+ETXST,dataBuffer,
             (length<MAX_STORED_SIZE)?length:MAX_STORED_SIZE,forCsum,precompute,TCPinIP4,FALSE);  
  writeEthRegister(0x02,(ETXST+TCP_CHECKSUM_AT)&0xFF);  // Put back where it came from
  writeEthRegister(0x03,(ETXST+TCP_CHECKSUM_AT)>>8);    
  writeBufferMemoryArray(2,&join.byte_1);  // Same (unknown) endianism as the calculator  
}

ethBitFieldSet(ETH_ECON1,ECON1_TXRTS); // launch the packet

return;
}
#endif
// ---------------------------------------------------------------------------



