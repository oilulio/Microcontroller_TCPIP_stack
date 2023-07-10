/********************************************
 Header code for Driver for 23 series SRAM
 S Combes, December 2016

*********************************************/

#ifndef _MEM23SRAM_
#define _MEM23SRAM_


// User specific adaptations

#define SRAM_READ    (0x03)
#define SRAM_WRITE   (0x02)
#define SRAM_EDIO    (0x3B)
#define SRAM_EQIO    (0x38)
#define SRAM_RSTIO   (0xFF)
#define SRAM_RDMR    (0x05)
#define SRAM_WRMR    (0x01)

#define SRAM_BYTE_MODE        (0x00)
#define SRAM_PAGE_MODE        (0x80)
#define SRAM_SEQUENTIAL_MODE  (0x40)
#define SRAM_RESERVED_MODE    (0xC0)

void memInitialise(void);
void setMemPageMode(void);
void setMemSequentialMode(void);
void setMemByteMode(void);
uint8_t memReadByte(uint32_t address);
void memWriteByte(uint32_t address,uint8_t data);
void memReadBufferMemoryArray(uint32_t address,uint16_t len,uint8_t * buffer);
void memWriteBufferMemoryArray(uint32_t address,uint16_t len,uint8_t * buffer);

#endif







