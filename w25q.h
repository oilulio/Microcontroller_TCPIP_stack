#ifndef W25Q_H
#define W25Q_H

#define WINBOND_SERIAL_FLASH (0xEF)
#define WB_80  (0x13)
#define WB_16  (0x14)
#define WB_32  (0x15)
#define WB_64  (0x16)
#define WB_128 (0x17)  // Devices use 24 bit addressing, so 128Mb=16MB=largest

// Errors
#define W25Q_IS_BUSY (1)
#define W25Q_WRAP    (2)  // Invalid Write - would wrap around

#define W25_DUMMY (0) // Just to make clear 'dont case' values

void w25init(void);
uint8_t w25ReadSizeMB(void);  // 0 = failed to find

uint8_t w25ChipErase(void);  
uint8_t w25SectorErase(uint8_t adrH,uint8_t adrL);

uint8_t w25PageProgram(uint8_t adrH,uint8_t adrL,uint8_t offset,uint8_t length, // UINT8 LENGTH
                    uint8_t * data);                    
uint8_t w25ReadData(uint8_t adrH,uint8_t adrL,uint8_t offset,uint16_t length, // UINT16 LENGTH
                    uint8_t * data);
                    
#endif