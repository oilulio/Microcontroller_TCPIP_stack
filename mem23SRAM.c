/********************************************
 Driver for Microchip 23 series SRAM (specifically tested for
 23LCV1024 - 1MBit SPI Serial SRAM)

 So far - only for simplest (slowest) mode

 S Combes, December 2016
 
*********************************************/

#include "config.h"

#ifdef NET_PROG

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>

#include "mem23SRAM.h"

// MACROS
#define SPIN_SPI(X)    SPDR=(X);WAIT_SPI()   // One spin cycle, sending X
#define WAIT_SPI()     while(!(SPSR&(1<<SPIF)))
#define RAM_SPI_ACTIVATE   (RAM_SPI_SEL_PORT&=(~(1<<RAM_SPI_SEL_CS)))
#define RAM_SPI_DEACTIVATE (RAM_SPI_SEL_PORT|=  (1<<RAM_SPI_SEL_CS))

// ---------------------------------------------------------------------------
uint8_t memReadModeRegister() 
{ 
RAM_SPI_ACTIVATE;
SPIN_SPI(SRAM_RDMR);
SPIN_SPI(0);  
RAM_SPI_DEACTIVATE;
return (SPDR&0xC0);   // restrict to allowed bits
}
// ---------------------------------------------------------------------------
void memWriteModeRegister(uint8_t mode) 
{ 
RAM_SPI_ACTIVATE;
SPIN_SPI(SRAM_WRMR);
SPIN_SPI(mode&0xC0);  // restrict to allowed bits
RAM_SPI_DEACTIVATE;
return;
}
// ---------------------------------------------------------------------------
uint8_t memReadByte(uint32_t address) 
{ 
RAM_SPI_ACTIVATE;
SPIN_SPI(SRAM_READ);
SPIN_SPI((address>>16)&0xFF);
SPIN_SPI((address>>8)&0xFF);
SPIN_SPI((address)&0xFF);
SPIN_SPI(0);
RAM_SPI_DEACTIVATE;
return (SPDR);
}
// ---------------------------------------------------------------------------
void memWriteByte(uint32_t address,uint8_t data) 
{ 
RAM_SPI_ACTIVATE;
SPIN_SPI(SRAM_WRITE);
SPIN_SPI((address>>16)&0xFF);
SPIN_SPI((address>>8)&0xFF);
SPIN_SPI((address)&0xFF);
SPIN_SPI(data);
RAM_SPI_DEACTIVATE;
return;
} 
// ---------------------------------------------------------------------------
void memReadBufferMemoryArray(uint32_t address,uint16_t len,uint8_t * dataBuffer) 
{ 
setMemSequentialMode();
RAM_SPI_ACTIVATE;
SPIN_SPI(SRAM_READ);
SPIN_SPI((address>>16)&0xFF);
SPIN_SPI((address>>8)&0xFF);
SPIN_SPI((address)&0xFF);
while (len--) { 
  SPIN_SPI(0);
  *(dataBuffer++)=SPDR;
}
RAM_SPI_DEACTIVATE;
}
// ---------------------------------------------------------------------------
void memWriteBufferMemoryArray(uint32_t address,uint16_t len,uint8_t * dataBuffer) 
{ 
setMemSequentialMode();
RAM_SPI_ACTIVATE;
SPIN_SPI(SRAM_WRITE);
SPIN_SPI((address>>16)&0xFF);
SPIN_SPI((address>>8)&0xFF);
SPIN_SPI((address)&0xFF);
while (len--) {  SPIN_SPI(*(dataBuffer++)); }
RAM_SPI_DEACTIVATE;
}
// ---------------------------------------------------------------------------
void setMemPageMode(){ memWriteModeRegister(SRAM_PAGE_MODE); } 

void setMemSequentialMode() { memWriteModeRegister(SRAM_SEQUENTIAL_MODE); } 

void setMemByteMode() { memWriteModeRegister(SRAM_BYTE_MODE); } 
// ---------------------------------------------------------------------------
void memInitialise()
{
RAM_SPI_SEL_DDR |= (1<<RAM_SPI_SEL_CS);
RAM_SPI_DEACTIVATE;
SPI_CONTROL_DDR |= ((1<<SPI_CONTROL_MOSI) | (1<<SPI_CONTROL_SCK));
SPI_CONTROL_DDR &= (~(1<<SPI_CONTROL_MISO));

SPI_CONTROL_PORT&=(~((1<<SPI_CONTROL_MOSI)|(1<<SPI_CONTROL_SCK)));

SPSR |= (1 << SPI2X); //  Fast as we can 
SPCR &=  ~((1 << SPR1) | (1 << SPR0));

SPCR |= (1<<SPE)|(1<<MSTR);

/*
setMemByteMode(); 
uint32_t address=0;
uint8_t data=0;
for (uint16_t i=0;i<1000;i++) { memWriteByte(address++,data) ; data+=17; }
*/

return;
}
#endif
// ---------------------------------------------------------------------------
