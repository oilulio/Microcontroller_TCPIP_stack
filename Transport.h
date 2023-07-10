/*********************************************

Header for Transport layer protocols TCP, UDP

 *********************************************/

#ifndef TRANSPORT_H
#define TRANSPORT_H

#define UDP_PORT  (1)
#define TCP_PORT  (2)

//#define START_DYNAMIC_PORTS   (0xC000) 
#define START_DYNAMIC_PORTS   (0xC080) 
// See http://www.iana.org/assignments/port-numbers

#define TCP_CLOSED      (1)
#define TCP_LISTEN      (2)
#define TCP_SYN_RCVD    (3)
#define TCP_SYN_SENT    (4)
#define TCP_ESTABLISHED (5)
#define TCP_CLOSE_WAIT  (6)
#define TCP_FIN_WAIT1   (7)
#define TCP_FIN_WAIT2   (8)
#define TCP_CLOSING     (9)
#define TCP_TIME_WAIT   (10)
#define TCP_LAST_ACK    (11)

#define MAX_TCP_ROLES   (2) // Can combine the below to sum to less than total.
#define TCP_CLIENT      (0) // Used as array coordinates, hence 0,1
#define TCP_SERVER      (1)
#define TCP_FTP_CLIENT  (0) // TODO : currently duplicates above - so use one at time
#define TCP_FTP_PASSIVE (1)

#define TCP_REJECT      (MAX_TCP_ROLES+1)  // Outside normal range
// Used to reject a wrong connection - so deliberately not one we recognise

#define ACK_BIT     (1)
#define DATA_BIT    (2)
#define CONSEC_BIT  (4)

typedef struct { // UDP header : 8 bytes
  uint16_t sourcePort;
  uint16_t destinationPort;
  uint16_t messageLength;
  uint16_t UDP_checksum;
} UDP_header;

#define UDP_HEADER_SIZE (8)

#define FL_FIN (0x01) 
#define FL_SYN (0x02)
#define FL_RST (0x04)
#define FL_PSH (0x08)
#define FL_ACK (0x10)
#define FL_URG (0x20)

typedef struct { // TCP header : 20 bytes
  uint16_t      sourcePort;
  uint16_t      destinationPort;
  int32_t       sequence;  // Making these signed allows easy sequence comparisons
  int32_t       ack;       // See Comer, Internetworking with TCP/IP Vol 2 p173
  unsigned      unused2        : 4;
  unsigned      headerLength   : 4;  // In 32 bit words
  uint8_t       flags;
  uint16_t      windowSize;
  uint16_t      TCP_checksum;
  uint16_t      urgent;
} TCP_header;

#define TCP_HEADER_SIZE (20)

#define MAX_RETX    (10)  // TCP Packets to keep ready to re transmit

typedef struct { // Details of TCP packet needed for possible Re-transmission
  uint8_t     role;
  unsigned    active      :2;
  unsigned    retries     :6;
  uint8_t     timeout;
  uint16_t    payloadLength;  
  uint16_t    headerLength;
  int32_t     lastByte;
  uint8_t     * data;   // Either the packet data ... or a callback to make it.
  uint16_t    (* callback)(uint16_t start,uint16_t length,uint8_t * result);
  int32_t     sequence;  // Only needed for callback : the sequence no of start of TCP stream
  uint16_t    start;     // Only needed for callback : the offset for the callback
} Retransmit;

typedef struct { // TCP Transmission Control Block (TCB)
  unsigned     status          :4;  // State machine
  unsigned     age             :4;  // countdown
  uint16_t     localPort;
  uint16_t     remotePort;
  IP4_address  remoteIP;
  int32_t      lastByteSent;
// int32_t     lastAckSent;
  int32_t      lastByteReceived;
  int32_t      lastAckReceived;
  uint16_t     windowSize;
} TCP_TCB;
 
#define TCP_MAX_AGE   (5)  // Unused connection will timeout after this many s.
#define TCP_TIMEOUT   (1)  // seconds before retransmit, then BEB
#define TCP_RETRIES   (3)  // Timeout * 2^Retries should be < 64.

typedef struct  {  // Headers only.  Enough to ACK a TCP
  Ethernet_header Ethernet;
  IP4_header IP4; 
  union {
    TCP_header TCP;      
    struct {
      UDP_header UDP;
      uint8_t dummy[12];
    };
  };
} MergedACK;

#include "application.h"

#define MAX_PACKET_PAYLOAD (MAX_PACKET_SIZE-IP_HEADER_SIZE-TCP_HEADER_SIZE-ETH_HEADER_SIZE)

// Global functions
void initialiseTCP(void);
void countdownTCP(void);
void retxTCP(void);
void cleanupOldTCP();
void initiateTCPConnection(MergedPacket * Mash, uint16_t source_port,
                                uint16_t destination_port, IP4_address ToIP, uint8_t role);
void TCP_SimpleDataOut(const char * send,const uint8_t role,uint8_t reTx);
void TCP_SimpleDataOutProgmem(const char * send,const uint8_t role,uint8_t reTx);
void TCP_DataIn(MergedPacket * Mash, const uint16_t length, const uint8_t role);
void TCP_ComplexDataOut(MergedPacket * Mash, const uint8_t role, const uint16_t payloadLength,
      uint16_t (* callback)(uint16_t start,uint16_t length,uint8_t * result),uint16_t offset,
	    uint8_t reTx);

void handleTCP(MergedPacket * Mash);
void handleUDP(MergedPacket * Mash, uint8_t flags);
void launchUDP(MergedPacket * Mash, IP4_address * ToIP,uint16_t sourcePort, uint16_t destinationPort, 
   uint16_t data_length,void (* callback)(uint16_t start,uint16_t length,uint8_t * result),uint16_t offset);
uint16_t newPort(uint8_t protocol);
void MemOverflow(void);


#endif