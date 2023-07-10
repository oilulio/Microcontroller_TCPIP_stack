#ifdef NET_PROG    


#ifndef ISP_H
#define ISP_H

#define ISP_READ_SIG    (0x30)
#define ISP_READ_LOCK   (0x58)
//#define ISP_READ_FUSE   (0x50)
//#define ISP_READ_HFUSE  (0x58)

#define ATMEL (0x1E)   // Signature byte 1 for an Atmel chip

#define ISP_UNKNOWN  (0xFF)

#define SIG_BYTE_1    (0x00)
#define SIG_BYTE_2    (0x01)
#define SIG_BYTE_3    (0x02)

// Possible states
#define ISP_QUIESCENT (0)
#define ISP_READY     (1)
#define ISP_ERASING   (2)

#define ISP_DUMMY  (0)

typedef struct  {  // ISP programming data for specific chip
  char name[LONGEST_MICRO];
  uint8_t vendor;
  uint8_t partFamily;
  uint8_t partCode;  // Signature bytes
  uint16_t sizeOfFlash;
  uint16_t flashPageSize;
  uint16_t sizeOfEEPROM;
} ISP_chipData;

uint8_t ISPactivate(void);
void ISPquiescent(void);
void ISPchipErase(void);
uint8_t ISP_Ready(void);
uint8_t ISPgetSignature(uint8_t sigid);
uint8_t ISPgetFuseBits(void);
uint8_t ISPgetHFuseBits(void);
uint8_t ISPgetEFuseBits(void);
uint16_t ISPreadFlashHighByte(uint16_t wordAddress);
uint16_t ISPreadFlashLowByte(uint16_t wordAddress);
void ISPloadFlashPageH(uint16_t wordAddress,uint8_t data);
void ISPloadFlashPageL(uint16_t wordAddress,uint8_t data);
void ISPloadFlashPageLH(uint16_t wordAddress,uint8_t dataL,uint8_t dataH);
void ISPwriteFlashPage(uint16_t address);
uint8_t ISPreadEEPROMbyte(uint16_t address);
void ISPwriteEEPROMbyte(uint16_t address,uint8_t data);
void ISPloadEEPROMpage(uint16_t address,uint8_t data);
void ISPwriteEEPROMpage(uint16_t address);
void ISPwriteLockBits(uint8_t data);
void ISPwriteFuseBits(uint8_t data);
void ISPwriteHFuseBits(uint8_t data);
void ISPwriteEFuseBits(uint8_t data);
#endif
#endif
