/*********************************************
 
Header for Network layer protocols IPV4, ARP

Code portability note : need to set LITTLEENDIAN correctly
For AVR use LITTLEENDIAN

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


 *********************************************/

#ifndef NETWORK_H
#define NETWORK_H

#ifndef TRUE
  #define TRUE  (1==1)
  #define FALSE (0==1)
#endif

#ifdef LITTLEENDIAN
  #define BYTESWAP16(A)        (((A) >> 8) | ((A&0xFF) << 8))   // &0xFF removes compiler wng, but unnecessary
  // Inspiration from http://www.codeproject.com/KB/cpp/endianness.aspx
  #define BYTESWAP32(A)      ((((A)>>24)&0xFF) | (((A)>>8)&(0x0000FF00)) | (((A)<<8)&(0x00FF0000))  | (((A)<<24)&(0xFF000000)) )
  // for IP address O1.O2.O3.O4 the octets are:
  #define OCTET1(A)  ((uint8_t)((A)&0xFF)) 
  #define OCTET2(A)  ((uint8_t)(((A)>>8)&0xFF))
  #define OCTET3(A)  ((uint8_t)(((A)>>16)&0xFF))
  #define OCTET4(A)  ((uint8_t)(((A)>>24)&0xFF))  

#else // BIG ENDIAN
  #define BYTESWAP16(A)      (A)   // Do nothing for bigendian
  #define BYTESWAP32(A)      (A)
  // for IP address O1.O2.O3.O4 the octets are:
  #define OCTET1(A)  ((uint8_t)(((A)>>24)&0xFF))  
  #define OCTET2(A)  ((uint8_t)(((A)>>16)&0xFF))
  #define OCTET3(A)  ((uint8_t)(((A)>>8)&0xFF))
  #define OCTET4(A)  ((uint8_t)((A)&0xFF))

#endif

#define ARP_REQUEST     (0x0001)
#define ARP_REPLY       (0x0002)

#define ARPforIP        (0x0800)
#define DLLisETHERNET   (0x0001)

#define ARPinETHERNET   (0x0806)   
#define RARPinETHERNET  (0x8035)
#define IP4inETHERNET   (0x0800)
#define IP6inETHERNET   (0x86DD) 

#define UDPinIP4        (0x11)  
#define TCPinIP4        (0x06)
#define ICMPinIP4       (0x01)  

#define DONT_FRAGMENT   (0x40)   // Bits to set
#define MORE_FRAGMENTS  (0x20)

#define DEFAULT_TTL     (0x40)   // 64 hops

#define OUR_ETHERNET_UNICAST   (1)
#define ETHERNET_BROADCAST     (2)
#define LLMNR_MULTICAST        (3)
#define mDNS_MULTICAST         (4)

#define OUR_IP_UNICAST         (3)
#define LIMITED_IP_BROADCAST   (4)
#define SUBNET_IP_BROADCAST    (5)
#define mDNS_IP_MULTICAST      (6)
#define LLMNR_IP_MULTICAST     (7)
 
#define ICMP_ECHO_REQ          (8)
#define ICMP_ECHO_REPLY        (0)
#define PING (ICMP_ECHO_REQ)    // The common name
#define PONG (ICMP_ECHO_REPLY)

#define MAX_STORED_SIZE    (46+189*2) // 189*2 for power meter *** TODO (140) 
// This is the maximum instantaneous RAM demand for a packet.
// Must cover 8 (UDP header) + 20 (IP header) + 14 (Eth header) = 42 
// plus suitable amount of payload, e.g. +48 for NTP, 12+options for DNS,
// +28 +outgoing options (currently 33 + hostname) for DHCP (note DHCP not stored in full - actually much larger).

// Suggest values : 
//   54 - bare minimum for TCP
//   74 - bare minimum with ICMP (32 bytes data)
//   110 - bare minumum for DHCP with short hostname.
//   90 - bare minimum for NTP.
// Values between 140 and 590 make most sense.  Full functionality shound be available from
// about 120 bytes.  Time/memory trade-off exists above that.

#define MAX_PACKET_SIZE     (590) // What we are prepared to (ask to) have sent to us (we 
// will cope with more, perhaps up to ethernet frame size).
  
//  590 is internet minimum packet, 0x24E - but does not need to be help concurrently
//      made up of 536 (TCP payload) + 20 (TCP header) + 20 (IP header) + 14 (Eth header)
//      This is an ethernet payload of 576. (0x240) c.f Max = 1500; => 1460 TCP payload and 1514 packet

// Note also max frame sizes in link layer (e.g. ethernet 1518)

#define ETH_HEADER_SIZE        (14)
#define ARP_HEADER_SIZE        (28)

#define MAX_HEADER_SIZE        (ETH_HEADER_SIZE+ARP_HEADER_SIZE) // 42 based on Eth (14)+ARP(28); 
#define IP_HEADER_SIZE         (20)  // Without options (handled separately)

//#define MAX_PENDING_STACK      (10)

#define TICKS_TO_HOLD_MAC     (120) // Seconds-ish

#define MAX_ARP_HELD  (3)  // Unlikely to need many (and they replace one another if required)  

typedef uint32_t IP4_address;

typedef struct { uint8_t IP6[16]; } IP6_address;

typedef struct { uint8_t MAC[6];  } MAC_address;

typedef struct { // IPv4 header information : 20 bytes + options
 unsigned headerLength   : 4; // Length normally 5, translates as 20 bytes  
 unsigned version        : 4;    
 unsigned unused         : 2;  
 unsigned reliability    : 1;
 unsigned throughput     : 1;
 unsigned delay          : 1;
 unsigned precedence     : 3;  
 uint16_t totalLength;        
 uint16_t id;  
 unsigned fragmentOffset : 13;
 unsigned moreFragments  : 1;
 unsigned dontFragment   : 1;
 unsigned reserved        : 1;
 uint8_t  TTL;
 uint8_t  protocol;
 uint16_t checksum;
 IP4_address  source;
 IP4_address  destination;  // options follow, but handled separately
} IP4_header;

typedef struct { // IPv6 information
  unsigned version         : 4;  
  unsigned traffic_class   : 8;
  unsigned flowLabelHigh   : 4;  // Don't seem to be able to have unsigned of 20 bits
  unsigned flowLabelLow    : 16;
  unsigned payloadLength   : 16;
  unsigned nextHeader      : 8;
  unsigned hopLimit        : 8;
  IP6_address source;
  IP6_address destination;
} IP6_header;

typedef struct { // ICMP  8 bytes + 32 bytes data
  uint8_t  messagetype;
  uint8_t  code;
  uint16_t checksum;  
  union {
    uint32_t  quench;
    struct {
      uint16_t id;
      uint16_t seq;
    };
  };
uint8_t  data[32];   // Windows XP uses 32, but can be larger [Union pads it out later]
} ICMP_message; 

typedef struct {  // ARP message : 28 byte
 uint16_t    hardware;
 uint16_t    protocol;
 uint8_t     hardware_size;
 uint8_t     protocol_size;
 uint16_t    type;
 MAC_address sourceMAC;
 IP4_address sourceIP;
 MAC_address destinationMAC;
 IP4_address destinationIP;
} ARP_message; 

typedef struct {    // Ethernet header 14 bytes
 MAC_address     destinationMAC;
 MAC_address     sourceMAC;
 uint16_t        type;
} Ethernet_header; 

typedef struct { // Status record (fits in byte)
  unsigned ARP  : 2; // don't think we need
  unsigned IP   : 4;
  unsigned TIME : 2;
} Status;

// Status flags
#define TCP_IDLE      (0)

#define DHCP_IDLE        (0) // We have not yet used DHCP
#define DHCP_WAIT_OFFER  (1) // We have sent DISCOVER and await an offer
#define DHCP_SEND_REQ    (2) // We have got OFFER and should now send REQUEST
#define DHCP_WAIT_ACK    (3) // We have sent REQUEST and await final ACK
#define APIPA_TRY        (4) // DHCP failure, try APIPA  
#define APIPA_FAIL       (5) // APIPA (via ARP) found a duplicate IP address
#define IP_SET           (6) // DHCP or APIPA has succeeded

#define TIME_UNSET     (0)
#define TIME_WAIT_DNS  (1) // DNS request made
#define TIME_REQUESTED (2) // NTP sent
#define TIME_SET       (3)

typedef struct {  // Merges Ethernet and ARP.  Enough for an ARP message (minimum memory)
  union {
    struct {  
      Ethernet_header Ethernet;
      ARP_message     ARP; 
    };
    uint8_t bytes[MAX_HEADER_SIZE];    // Treat whole as bytes
    uint16_t words[MAX_HEADER_SIZE/2]; // treat whole as words
  };
} MergedARP;

#define EEPROM_MAGIC1    (100)
#define EEPROM_MAGIC2    (101)
#define EEPROM_IP_SEQ    (102)
#define EEPROM_TCP_PORT  (104)
//#define EEPROM_TCP_PORT2 (106)
#define EEPROM_UDP_PORT  (108)
#define EEPROM_MY_IP     (110)
#define EEPROM_MY_MAC    (EEPROM_MY_IP+4)

#include "transport.h"

// Prototypes (others internal and static)

// Global
void Error(void);
void copyIP4(IP4_address * IPTo, const IP4_address * IPFrom);
void copyMAC(MAC_address * MACTo, const MAC_address * MACFrom);
uint8_t IP4_match(const IP4_address * IP1, const IP4_address * IP2);
void refreshMACList(void);
void delay_ms(uint16_t ms);
uint16_t checksum(uint16_t * words, int16_t length);
uint32_t checksumSupport(uint16_t * words, int16_t length);
void checksumBare(uint32_t * scratch,uint16_t * words, int16_t length);
uint16_t IP4checksum(MergedPacket * Mash);
int8_t mDNS(IP4_address * IP);
int8_t LLMNR(IP4_address * IP);
int8_t MACForUs(MAC_address * MAC);
int8_t IP4ForUs(IP4_address * IP);
// Main interface (Global)
uint8_t handlePacket(void);
uint8_t launchIP4(MergedPacket *,uint8_t csums,
        void (* callback)(uint16_t start,uint16_t length,uint8_t * result),uint16_t offset);
void launchARP(MergedARP * Mish);
void prepareIP4(MergedPacket * Mash, 
                uint16_t payload_length, IP4_address * ToIP, uint8_t protocol); 

void IP4toBuffer(IP4_address IP);

// Our internal representation of an IP4 address is a 32 bit integer s.t. we
// can directly assign from, e.g., a packet's 4 byte record
// Hence our internal representations DIFFERS in big/little endian circumstances,
// so our IP4 constructor muct change to match
#ifdef LITTLEENDIAN
#define MAKEIP4(MSB,B2,B3,LSB) ((((uint32_t)LSB)<<24)|(((uint32_t)B3)<<16)|(((uint32_t)B2)<<8)|((uint32_t)MSB))
#else // BIG ENDIAN
#define MAKEIP4(MSB,B2,B3,LSB) ((((uint32_t)MSB)<<24)|(((uint32_t)B2)<<16)|(((uint32_t)B3)<<8)|((uint32_t)LSB))
#endif

#endif