/********************************************
 Header code for Link layer of TCPIP stack
 S Combes, July 2016

*********************************************/

#ifndef LINK_H
#define LINK_H

#include "network.h"

#define NO_CSUM (0)
#define CS_IP4  (1<<0)  // Checksum indicators
#define CS_ICMP (1<<1)  // need to be unique and <2^8
#define CS_TCP  (1<<2)
#define CS_UDP  (1<<3)
#define CS_DHCP (1<<4)  // For DHCP, not really a checksum - just tests the magic no

void     linkInitialise(MAC_address myMAC);
void     linkPacketSend(uint8_t * buffer, uint16_t length, uint8_t checksums,
            void (* callback)(uint16_t start,uint16_t length,uint8_t * result),
            uint16_t offset);
uint8_t  linkNextByte(void);
void     linkReadBufferMemoryArray(uint16_t len,uint8_t * buffer); 
uint16_t linkPacketHeader(uint16_t maxSize,uint8_t * buffer,uint8_t * flags);
void     linkDoneWithPacket(void);
void     linkReadRandomAccess(uint16_t offset);

#ifdef USE_ENC28J60
#include "linkENC28J60.h"
#endif

#ifdef USE_ISA
#include "linkISA.h"
#endif

#endif







