// Handler for the W25Qxx SPI Flash memories
// Commands seem to differ with variants.  This implements the
// basic command set judged necessary (by me) and hence nothing
// exotic.

/*
Copyright (C) 2019  S Combes

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
*/

#include "config.h"

#ifdef NET_PROG

#include <avr/io.h>
#include <util/delay.h>

#include "w25q.h"
#include "network.h" // for delay_ms()
#include "application.h" // for genericUDP

// MACROS
#define SPIN_SPI(X)    SPDR=(X);WAIT_SPI()   // One spin cycle, sending X
#define WAIT_SPI()     while(!(SPSR&(1<<SPIF)))
#define W25_ACTIVATE    (W25_SPI_SEL_PORT&=(~(1<<W25_SPI_SEL_CS)))
#define W25_DEACTIVATE  (W25_SPI_SEL_PORT|=  (1<<W25_SPI_SEL_CS))

extern char buffer[MSG_LENGTH]; // For debug

// ---------------------------------------------------------------------------
void w25Cmd_1(uint8_t cmd)
{
W25_ACTIVATE ;
SPIN_SPI(cmd);
W25_DEACTIVATE ;
}
// ---------------------------------------------------------------------------
uint8_t w25Read_1(uint8_t cmd)
{
W25_ACTIVATE ;
SPIN_SPI(cmd);
SPIN_SPI(W25_DUMMY);
W25_DEACTIVATE ;
return SPSR;
}
// ---------------------------------------------------------------------------
void w25WriteEnable()  { w25Cmd_1(0x06); }
// ---------------------------------------------------------------------------
void w25WriteDisable() { w25Cmd_1(0x04); }
// ---------------------------------------------------------------------------
uint8_t w25ReadStatus1()  { return w25Read_1(0x05); }
// ---------------------------------------------------------------------------
uint8_t w25ReadStatus2()  { return w25Read_1(0x35); }
// ---------------------------------------------------------------------------
uint8_t busy()  { return (w25Read_1(0x05)&0x01); }
// ---------------------------------------------------------------------------
uint8_t w25PageProgram(uint8_t adrH,uint8_t adrL,uint8_t offset,uint8_t length,
                    uint8_t * data) 
{
if (busy()) return W25Q_IS_BUSY;
w25WriteEnable();

uint16_t tmp=(uint16_t)offset+(uint16_t)length;
if (tmp&0xFF00) return W25Q_WRAP;  // Failure - would wrap around - fault of caller

W25_ACTIVATE ;
SPIN_SPI(0x02);
SPIN_SPI(adrH);
SPIN_SPI(adrL);
SPIN_SPI(offset);
while (length--) { SPIN_SPI(*(data++)); } // Must have {} - see macro
W25_DEACTIVATE ;

return 0;
}
// ---------------------------------------------------------------------------
uint8_t w25ReadData(uint8_t adrH,uint8_t adrL,uint8_t offset,uint16_t length,
                    uint8_t * data) 
{
  
return w25ReadStatus1(); // TODO
  
if (busy()) return W25Q_IS_BUSY;

W25_ACTIVATE ;
SPIN_SPI(0x03);
SPIN_SPI(adrH);
SPIN_SPI(adrL);
SPIN_SPI(offset);
while (length--) {
  SPIN_SPI(W25_DUMMY);
  *(data++)=SPDR;
}
W25_DEACTIVATE ;

return 0;
}
// ---------------------------------------------------------------------------
uint8_t w25SectorErase(uint8_t adrH,uint8_t adrL) 
{ // Since sector is 4k, lower nibble of adrL is 0

if (busy()) return W25Q_IS_BUSY; 

w25WriteEnable();
W25_ACTIVATE ;
SPIN_SPI(0x20);
SPIN_SPI(adrH);
SPIN_SPI(adrL&0xF0);
SPIN_SPI(W25_DUMMY);
W25_DEACTIVATE ;

return 0; 
} // ---------------------------------------------------------------------------
uint8_t w25ChipErase() 
{
if (busy()) return W25Q_IS_BUSY; 

w25WriteEnable();
w25Cmd_1(0xC7); 

return 0; 
} 
// Needs chip in writeable mode ... And will take some time
// ---------------------------------------------------------------------------
void w25init()  // Passive - just sets up line directions/SPI speed
  {
W25_SPI_SEL_DDR |= (1<<W25_SPI_SEL_CS);
W25_DEACTIVATE ;
SPI_CONTROL_DDR |= ((1<<SPI_CONTROL_MOSI) | (1<<SPI_CONTROL_SCK));
SPI_CONTROL_DDR &= (~(1<<SPI_CONTROL_MISO));

SPI_CONTROL_PORT&=(~((1<<SPI_CONTROL_MOSI)|(1<<SPI_CONTROL_SCK)));

SPSR |= (1 << SPI2X); //  Fast as we can 
SPCR &=  ~((1 << SPR1) | (1 << SPR0));

SPCR |= (1<<SPE)|(1<<MSTR);

return;
}
// ---------------------------------------------------------------------------
uint8_t w25ReadSizeMB() // 0 = no W23Qxx signature detected or unknown size
{ 
W25_ACTIVATE ;
SPIN_SPI(0x90);
SPIN_SPI(0);    // 24 bit addr ...
SPIN_SPI(0);    // ...
SPIN_SPI(0);    // ... is zero
SPIN_SPI(W25_DUMMY);
if (SPDR!=WINBOND_SERIAL_FLASH) {W25_DEACTIVATE ; return 0;}
SPIN_SPI(W25_DUMMY);
uint8_t devID=SPDR;
W25_DEACTIVATE ;

if (devID==WB_16)  return 2;   // 2MB device (16Mb)
if (devID==WB_32)  return 4;   // 4MB device (32Mb)
if (devID==WB_64)  return 8;   // 8MB device (64Mb)
if (devID==WB_80)  return 10;  // 10MB device (80Mb)
if (devID==WB_128) return 16;  // 16MB device (128Mb)

return (0);  // Unknown - may need more WB_xx as product matures
}
#endif
