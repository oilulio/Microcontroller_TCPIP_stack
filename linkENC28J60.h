/********************************************
 Header code for Driver for ENC28J60 Ethernet module
 S Combes, July 2016

*********************************************/

#ifndef LINKENC28J60_H
#define LINKENC28J60_H

#define ENC28J60_PREAMBLE (6)  // 6 bytes

// Optimisations

#define MAX_TX_PACKET  (1518) // Ethernet maximum.  Keep even - or see below.
#define MAX_RX_PACKET  (1518) // Ethernet maximum

// No routine need to alter below.  Also alter only with care.
// RX start (ERXST) is ideally zero.
// ERXND should be odd - only for convenience to ensure Errata 14 is sustained.
// ETXST= (0x1FFF - (even+7)) and ERXND = ETXST-1 ensures this.

#define ERXST (0x00)   // RX Start is always zero (see errata)
#define ETXST (0x1FFF - (MAX_TX_PACKET + 7))    
// TX Start.  7 bytes spare for status (p33 datasheet)
#define ERXND (ETXST - 1)  // RX End (inclusive in FIFO buffer, datasheet 3.2.1)

#define RX_OK  (1<<7)

#define ETH_EIE   (0x1B)
#define ETH_EIR   (0x1C)
#define ETH_ESTAT (0x1D)
#define ETH_ECON2 (0x1E)
#define ETH_ECON1 (0x1F)

#define EIR_TXERIF   (1<<1)
#define ECON1_RXEN   (1<<2)
#define ECON1_TXRTS  (1<<3)
#define ECON1_TXRST  (1<<7)

#define ECON2_PKTDEC   (1<<6)
#define ECON2_AUTOINC  (1<<7)

#define ERXFCON_BCEN  (1<<0) // Broadcast enable
#define ERXFCON_MCEN  (1<<1) // Multicast enable
#define ERXFCON_HTEN  (1<<2) // Hash table filter enable
#define ERXFCON_MPEN  (1<<3) // Magic packet enable
#define ERXFCON_PMEN  (1<<4) // Pattern Match enable
#define ERXFCON_CRCEN (1<<5) // CRC enable
#define ERXFCON_ANDOR (1<<6) // AND/OR filter
#define ERXFCON_UCEN  (1<<7) // Unicast enable

// ---------------------------------------------------------------------------

// Generic function interfaces are in link.h - i.e. common whatever hardware we use.
// ENC28J60 has some clock output public interfaces as well.

void setClockoutScale(uint8_t data) ;
void setClockout25MHz(void);
void setClockout12pt5MHz(void);
void setClockout8pt33MHz(void);
void setClockout6pt25MHz(void);
void setClockout3pt125MHz(void);

#endif







