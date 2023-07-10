// *********************************************************************************
// *********** Initialisation routine for Microcontroller network devices ********************
// *********************************************************************************
/*
Copyright (C) 2009-22  S Combes

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */
    
// ----------------------------------------------------------------------------
#include "config.h"

#include <avr/io.h>
#include <util/delay.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <stdlib.h> 

#include "network.h"
#include "link.h"
#include "w25q.h"
#include "isp.h"
#include "power.h"
#include "mem23SRAM.h"

extern IP4_address myIP; 
#ifdef USE_TCP
extern TCP_TCB TCB[MAX_TCP_ROLES];  
extern Retransmit ReTx[MAX_RETX];
#endif
extern volatile uint8_t timecount;
extern   MAC_address myMAC;

uint16_t IP4_ID;     // IP4 Packet ID
uint16_t UDP_Port[MAX_UDP_PORTS];
uint16_t UDP_low_port;

// ----------------------------------------------------------------------------
void InitEEPROM()
{
while (!eeprom_is_ready()) {} 

IP4_ID=12345;
#ifdef USE_TCP
  TCB[TCP_CLIENT].localPort=START_DYNAMIC_PORTS+1;
  TCB[TCP_SERVER].localPort=START_DYNAMIC_PORTS+2;
#endif
  UDP_low_port=START_DYNAMIC_PORTS;

eeprom_read_byte((uint8_t *)0); // Dummy

/* TODO NOT USING EEPROM AS ORIGINAL FOR WHEREABOUTS CLOCK ***
if (eeprom_read_byte((uint8_t *)EEPROM_MAGIC1) == 22 &&
    eeprom_read_byte((uint8_t *)EEPROM_MAGIC2) == 23){
// ok magic numbers match 
  IP4_ID=eeprom_read_word((uint16_t *)EEPROM_IP_SEQ);
#ifdef USE_TCP
  TCB[TCP_CLIENT].local_port=eeprom_read_word((uint16_t *)EEPROM_TCP_PORT)+1;
  TCB[TCP_SERVER].local_port=eeprom_read_word((uint16_t *)EEPROM_TCP_PORT)+2;
#endif
  UDP_low_port=eeprom_read_word((uint16_t *)EEPROM_UDP_PORT);
  eeprom_read_block((uint8_t *)&myIP,(void *)EEPROM_MY_IP,sizeof(myIP));
//  eeprom_read_block((uint8_t *)&myMAC.MAC[0],(void *)EEPROM_MY_MAC,sizeof(myMAC));
}
else
{
  eeprom_write_byte((uint8_t *)EEPROM_MAGIC1,22);
  eeprom_write_byte((uint8_t *)EEPROM_MAGIC2,23);

  eeprom_write_word((uint16_t *)EEPROM_IP_SEQ,12345);
  eeprom_write_word((uint16_t *)EEPROM_TCP_PORT,START_DYNAMIC_PORTS);
//  eeprom_write_word((uint16_t *)EEPROM_TCP_PORT2,START_DYNAMIC_PORTS);
  eeprom_write_word((uint16_t *)EEPROM_UDP_PORT,START_DYNAMIC_PORTS);

  eeprom_write_block((uint8_t *)&myIP,(void *)EEPROM_MY_IP,sizeof(myIP));        
//  eeprom_write_block((uint8_t *)&myMAC.MAC[0],(void *)EEPROM_MY_MAC,sizeof(myMAC));        
}
*/
}
// ----------------------------------------------------------------------------
#ifdef NETWORK_CONSOLE
void InitConsole()
{
uint8_t i;
/* enable PD2/INT0, as input */
//DDRD&= ~(1<<DDD2);

//Enable Port A1 as output and set to low : Write to LCD
DDRA|=(1<<DDA1);
PORTA &= ~(1<<PINA1);

myIP=MAKEIP4(192,168,0,137);  // Subject to DHCP

#ifndef DEBUGGER
lcd_init(LCD_DISP_ON);
delay_ms(100);
lcd_clrscr();
lcd_puts_P(PSTR("Initialising"));

NetworkInit(&myMAC.MAC[0]);
delay_ms(150);
#endif

#ifdef ATMEGA32
   // Prescaler = FCPU/1024
   TCCR0|=(1<<CS02)|(1<<CS00);  // was TCCR0|=(1<<CS02)|(1<<CS00);

   //Enable Overflow Interrupt Enable
   TIMSK|=(1<<TOIE0);

   //Initialize Counter
   TCNT0 = (TIME_START);  // fine tuning

   //Initialize our variable
   timecount=0;

   sei();     //Enable Global Interrupts

#endif

#ifndef DEBUGGER  
  lcd_gotoxy(0,1);
  i=DLL_revision();
  sprintf(buffer,"ISA V %2d.%02d",(i>>4),i&0x0F);
  lcd_puts(buffer);
  delay_ms(500);
  lcd_clrscr();
  lcd_puts_P("MAC");
  sprintf(buffer,"  %02X %02X %02X",myMAC.MAC[0],myMAC.MAC[1],myMAC.MAC[2]);
  lcd_puts(buffer);
  lcd_gotoxy(5,1);
  sprintf(buffer,"%02X %02X %02X",myMAC.MAC[3],myMAC.MAC[4],myMAC.MAC[5]);
  lcd_puts(buffer);
  delay_ms(500);
#endif
}
#endif
// ----------------------------------------------------------------------------
#ifdef HOUSE
void InitHouse()
{
myIP=MAKEIP4(192,168,0,123);  // Subject to DHCP  

// Timer 0 - our internal clock

// Timer prescaler = FCPU/1024
TCCR0B|=(1<<CS02)|(1<<CS00);  

TIMSK0|=(1<<TOIE0);  //Enable Overflow Interrupt Enable

TCNT0 = (TIME_START);  // Initialize Counter - fine tuning

linkInitialise(myMAC);
delay_ms(150);

sei();   
timecount=0;

delay_ms(1000);
}
#endif
// ----------------------------------------------------------------------------
#ifdef POWER_METER
void InitPower()
{
uint8_t i;

myIP=MAKEIP4(192,168,0,135);  // Subject to DHCP

// LED  on PB1 as output 
DDRB|= (1<<DDB1);

// Startup signature (Three flashes) - we have no LCD
for (i=0;i<3;i++) {
  LEDON;
  delay_ms(500);
  LEDOFF;
  delay_ms(500);
}
// Timer prescaler = FCPU/1024
TCCR0B|=(1<<CS02)|(1<<CS00);  

//Enable Overflow Interrupt Enable **** SEEMS TO CONFLICT and not required***
//TIMSK0|=(1<<TOIE0);           // was TIMSK0 not functionlly equivalent,
                              // but this bit (TOIRE0) hasn't changed

#ifndef DEBUGGER
linkInitialise(myMAC);
delay_ms(150);
#endif

InitialiseADC();

// Startup complete (Two long flashes)
for (i=0;i<2;i++) {
  LEDON;
  delay_ms(1000);
  LEDOFF;
  delay_ms(500);
}
}
// ----------------------------------------------------------------------------
void InitialiseADC(void)
{
ADMUX  = (AVCC_REF |  0);  // Channel 0.  Right adjusted result 
ADCSRA = ((1 << ADEN) | (1 << ADSC) | 7); // Enable, start convert, div by 128
// 12.5MHz clock.  Max resolution if prescaler is <200kHz.
// 200kHz achieved with = 62.5.  Use max  0b111 = divide by 128
// Hence 97,656kHz
// At 50Hz, equals 1953 samples if nothing else happening

_delay_ms(50.0);  // just in case

while (!(ADCSRA & (1 << ADIF)));

_delay_ms(50.0);  // just in case

ADCSRA |= (1 << ADIF); // Clear interrupt
}
#endif
// ----------------------------------------------------------------------------
#ifdef NET_PROG
void InitProgrammer()
{
ISPquiescent();               // Tristates everything
myIP=MAKEIP4(192,168,0,129);  // Subject to DHCP  

// Timer 0 - our internal clock
// Timer prescaler = FCPU/1024
TCCR0B|=(1<<CS02)|(1<<CS00);  

TIMSK0|=(1<<TOIE0);  //Enable Overflow Interrupt Enable

TCNT0 = (TIME_START);  // Initialize Counter - fine tuning

//w25init();
memInitialise();

linkInitialise(myMAC);
delay_ms(150);

sei();   
timecount=0;

delay_ms(1000);
}
#endif