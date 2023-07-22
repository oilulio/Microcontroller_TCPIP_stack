/*
Copyright (C) 2018-20  S Combes

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

#include "STK500.h"
#include "isp.h"
#include "network.h" // for delay_ms()
#include "application.h" // for genericUDP

extern char buffer[MSG_LENGTH]; // For debug

ISP_chipData chipData;
/*chipData.vendor=ISP_UNKNOWN;
chipData.partFamily=ISP_UNKNOWN;
chipData.partCode=ISP_UNKNOWN;  // Signature bytes*/

uint8_t ISP_state;
uint8_t fuseL,fuseH,fuseE;

// ISP programmer, uses data lines of ours to control ANOTHER SPI interface acting as ISP

// Information from Application Note AVR910
// http://ww1.microchip.com/downloads/en/appnotes/atmel-0943-in-system-programming_applicationnote_avr910.pdf

// Specifics from ATMega328p datasheet - may need some adaptations for other AVR devices ***** 

// Key points :
// SCK pattern is defined in chip-by-chip datasheet; example is:
// SCK must be low for > 1 system clock
//   ditto    high for > 4 system clocks

// Hardwired throughout as: (Can change settings in config.h)
// PD6 controls SCK   ->
// PD7 controls MISO  <-
// PD3 controls MOSI  ->
// PD5 controls RESET ->

void pulseSCK();
uint8_t sendByte(uint8_t data);

// -----------------------------------------------------------------------------------
uint8_t sendByte(uint8_t data) { // MSb first (3.1 of AVR910)
  
uint8_t bit=(1<<7);
uint8_t rcvd=0;

while (bit!=0) {
  if ((bit&data)!=0) ISP_CONTROL_PORT|= (1<<ISP_CONTROL_MOSI); 
  else               ISP_CONTROL_PORT&=~(1<<ISP_CONTROL_MOSI);

  rcvd<<=1;
  if ((ISP_CONTROL_PORT_IN&(1<<ISP_CONTROL_MISO))!=0) rcvd|=1;  
  pulseSCK();
  bit>>=1;  
}
return rcvd;  
}
// -----------------------------------------------------------------------------------
uint8_t sendCommandData(uint8_t cmd,uint8_t adrHigh,uint8_t adrLow,uint8_t data) { 
// All commands are 4 byte cycles
sendByte(cmd);
sendByte(adrHigh);
sendByte(adrLow);
sendByte(data);
return (0);
}
// -----------------------------------------------------------------------------------
uint8_t sendCommand(uint8_t cmd,uint8_t adrHigh,uint8_t adrLow) { // All commands are 4 byte cycles
                                return sendCommandData(cmd,adrHigh,adrLow,ISP_DUMMY);}
// -----------------------------------------------------------------------------------
void ISPchipErase() { sendCommand(0xAC,0x80,0x00); delay_ms(12); } 
//ISP_state=ISP_ERASING; } 
// 
// -----------------------------------------------------------------------------------
uint8_t ISPgetLockBits()  { return sendCommand(0x58,0x00,0x00); } 
// -----------------------------------------------------------------------------------
uint8_t ISPgetFuseBits()  { return sendCommand(0x50,0x00,0x00); } 
// -----------------------------------------------------------------------------------
uint8_t ISPgetHFuseBits() { return sendCommand(0x58,0x08,0x00); } 
// -----------------------------------------------------------------------------------
uint8_t ISPgetEFuseBits() { return sendCommand(0x50,0x08,0x00); } 
// -----------------------------------------------------------------------------------
uint8_t ISPgetCalibration() { return sendCommand(0x38,0x00,0x00); } 
// -----------------------------------------------------------------------------------
uint8_t ISPgetSignature(uint8_t sigid) { return sendCommand(ISP_READ_SIG,0x00,sigid); }
// -----------------------------------------------------------------------------------
uint8_t isReady() { return (sendCommand(0xF0,0x00,0x00)&0x01); } // LSb. Busy active LOW
// -----------------------------------------------------------------------------------
uint8_t ISP_Ready() { // External API
  if (ISP_state==ISP_READY)   return TRUE;
  if (ISP_state==ISP_ERASING) {
    if (isReady()) {
      ISP_state=ISP_READY;
      return TRUE;
    } else return FALSE;
  }
  return FALSE;
}
// -----------------------------------------------------------------------------------
void pulseSCK() {
  
ISP_CONTROL_PORT&=~(1<<ISP_CONTROL_SCK); // Low to start
asm("nop"); // Seems OK with no delay, but waste 2 cycles in case
asm("nop"); // Will depend on relative speeds of clocks

ISP_CONTROL_PORT|=(1<<ISP_CONTROL_SCK); // Pulse
for (int i=0;i<20;i++) asm("nop");  // 6 too few, 10 OK, but will depend on relative clocks
ISP_CONTROL_PORT&=~(1<<ISP_CONTROL_SCK); // Low to end
}
// -----------------------------------------------------------------------------------
uint16_t ISPreadFlashHighByte(uint16_t wordAddress) { 
                                return sendCommand(0x28,(wordAddress>>8),wordAddress&0xFF);  }
// -----------------------------------------------------------------------------------
uint16_t ISPreadFlashLowByte(uint16_t wordAddress) { 
                                return sendCommand(0x20,(wordAddress>>8),wordAddress&0xFF);  }
// -----------------------------------------------------------------------------------
void ISPloadFlashPageH(uint16_t wordAddress,uint8_t data) { // Page is 128 bytes in Mega328P TODO
//Loads into preparatory buffer (cement with ISPwriteFlashPage)
//while (!isReady())  { /* WAIT */ }
sendCommandData(0x48,0x00,wordAddress&0x3F,data);  
}
// -----------------------------------------------------------------------------------
void ISPloadFlashPageL(uint16_t wordAddress,uint8_t data) { // Page is 128 bytes in Mega328P TODO
//Loads into preparatory buffer (cement with ISPwriteFlashPage)
//while (!isReady())  { /* WAIT */ }
sendCommandData(0x40,0x00,wordAddress&0x3F,data);  
}
// -----------------------------------------------------------------------------------
void ISPloadFlashPageLH(uint16_t wordAddress,uint8_t dataL,uint8_t dataH) 
{ // Preferred as enforces L,H order
//Loads into preparatory buffer (cement with ISPwriteFlashPage)
//while (!isReady())  { /* WAIT */ }
sendCommandData(0x40,0x00,wordAddress&0x3F,dataL);  
sendCommandData(0x48,0x00,wordAddress&0x3F,dataH);  
}
// -----------------------------------------------------------------------------------
void ISPwriteFlashPage(uint16_t address) { // address is byte address
// To get nth page, >>7.  But chip routine uses word address

//while (!isReady())  { /* WAIT */ }
sendCommandData(0x4C,address>>9,(address>>1)&0xC0,ISP_DUMMY); // Convert to word address and mask
delay_ms(8.0);
}
// -----------------------------------------------------------------------------------
uint8_t ISPreadEEPROMbyte(uint16_t address) { 
                                return sendCommand(0xA0,(address>>8),address&0xFF);  }
// -----------------------------------------------------------------------------------
void ISPwriteEEPROMbyte(uint16_t address,uint8_t data) {

//while (!isReady())  { /* WAIT */ }
sendCommandData(0xC0,address>>8,address&0xFF,data);  
delay_ms(4.0);
}
// -----------------------------------------------------------------------------------
void ISPloadEEPROMpage(uint16_t address,uint8_t data) { // Page is 4 bytes in Mega328P TODO

//while (!isReady())  { /* WAIT */ }
sendCommandData(0xC1,0x00,address&0x03,data);  
}
// -----------------------------------------------------------------------------------
void ISPwriteEEPROMpage(uint16_t address) { 

//while (!isReady())  { /* WAIT */ }
sendCommandData(0xC2,address>>8,address&0xFC,ISP_DUMMY);
delay_ms(8.0);
}
// -----------------------------------------------------------------------------------
void ISPwriteLockBits(uint8_t data)
{
while (!isReady())  { /* WAIT */ }
sendCommandData(0xAC,0xE0,0x00,data);
}
// -----------------------------------------------------------------------------------
void ISPwriteFuseBits(uint8_t data)
{
while (!isReady())  { /* WAIT */ }
sendCommandData(0xAC,0xA0,0x00,data);
} 
// -----------------------------------------------------------------------------------
void ISPwriteHFuseBits(uint8_t data)
{
while (!isReady())  { /* WAIT */ }
sendCommandData(0xAC,0xA8,0x00,data);
} // -----------------------------------------------------------------------------------
void ISPwriteEFuseBits(uint8_t data)
{
while (!isReady())  { /* WAIT */ }
sendCommandData(0xAC,0xA4,0x00,data);
} 
// -----------------------------------------------------------------------------------
uint8_t ISPactivate() {
  
// Move into ISP mode.  
//   Set up the pins.  
//   Pull target into reset.  
//   Issue 'programming enable' command - and ensure success
  

ISP_SEL_DDR|=(1<<ISP_SPI_SEL_CS);    // RESET/SS as output
ISP_SEL_PORT&=~(1<<ISP_SPI_SEL_CS);  // and LOW

ISP_CONTROL_DDR|=(1<<ISP_CONTROL_SCK);   // SCK as output [AVR910 2.3 : SCK low IMMEDIATELY]
ISP_CONTROL_PORT&=~(1<<ISP_CONTROL_SCK); // Low to start

ISP_CONTROL_DDR&=~(1<<ISP_CONTROL_MISO);   // MISO as input
ISP_CONTROL_PORT&=~(1<<ISP_CONTROL_MISO);  // No pullup (?)

ISP_CONTROL_DDR|=(1<<ISP_CONTROL_MOSI);   // MOSI as output 
ISP_CONTROL_PORT&=~(1<<ISP_CONTROL_MOSI); // Low to start

uint8_t tries=0;
while (TRUE) {
  delay_ms(50);
  ISP_CONTROL_PORT|=(1<<ISP_SPI_SEL_CS);  // Positive pulse on RESET now that SCK is clean (ATMega328p datasheet 25.8.2)
  delay_ms(2);      // At least 2 clock cycles (AtMega8 datasheet)
  ISP_CONTROL_PORT&=~(1<<ISP_SPI_SEL_CS);  
  delay_ms(40);     // >20 ms required (AtMega8 datasheet)
  
  sendByte(0xAC);  
  sendByte(0x53);  
  uint8_t rcvd=sendByte(ISP_DUMMY);  
  if (rcvd!=0x53) {
    if (tries++>30) {  // Fail
      ISPquiescent();  // Make safe
      return TRUE;
    }
  }
  sendByte(ISP_DUMMY);

  ISP_state=ISP_READY;
  break;           // Success
}  
return FALSE;  // SUCCESS
}
// -----------------------------------------------------------------------------------
void ISPquiescent() {
  
// Tristate everything - inputs without pullups
// Target device will only reset if it has its own pull up circuit on reset.
// But it probably wouldn't work anyway if it didn't

ISP_CONTROL_DDR&=~(1<<ISP_CONTROL_SCK);   
ISP_CONTROL_DDR&=~(1<<ISP_CONTROL_MISO);   
ISP_CONTROL_DDR&=~(1<<ISP_CONTROL_MOSI);   
ISP_SEL_DDR&=~(1<<ISP_SPI_SEL_CS);   

ISP_CONTROL_PORT&=~(1<<ISP_CONTROL_SCK);
ISP_CONTROL_PORT&=~(1<<ISP_CONTROL_MISO);
ISP_CONTROL_PORT&=~(1<<ISP_CONTROL_MOSI);
ISP_SEL_PORT&=~(1<<ISP_SPI_SEL_CS);

ISP_state=ISP_QUIESCENT;
}
#endif