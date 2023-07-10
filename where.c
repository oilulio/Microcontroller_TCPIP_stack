/*********************************************
 Code for Stepper Motor interface over SPI

*********************************************/

#include "config.h"
 
#ifdef WHEREABOUTS

#include <avr/io.h>
#include <stdlib.h>
#include "network.h"

#include <stdio.h>
#include <string.h>
#include <avr/interrupt.h>
#include <util/delay.h> // after defintion of F_CPU
#include "link.h"
#include "where.h"
#include "lfsr.h"

extern char buffer[MSG_LENGTH];
extern MAC_address myMAC;
extern /*union TODO*/ MergedPacket MashE;
extern IP4_address myIP;
extern volatile uint8_t timecount;

extern uint8_t rpos;
extern uint8_t lpos;
volatile extern uint8_t timecount;

#define PATTERN(x) ((1<<(x>>1))|(1<<(((x+1)%8)>>1)))
// Creates a 4 bit pattern based on x, which should lie in range 0 to 7
// This is the pattern for half-step on a 5 wire motor
// 0001 ... 0011 ... 0010 ... 0110 .. etc

// MACROS
#define SPIN_SPI(X)    SPDR=(X);WAIT_SPI()   // One spin cycle, sending X
#define WAIT_SPI()     while(!(SPSR&(1<<SPIF)))

#define HM_ACTIVATE   (PORTD&=(~(1<<1))) // Hour or Minute
#define HM_DEACTIVATE (PORTD|=  (1<<1))
#define S_ACTIVATE    (PORTD&=(~(1<<0))) // Seconds
#define S_DEACTIVATE  (PORTD|=  (1<<0))

// -----------------------------------------------------------------------------
void InitWhereabouts() {

DDRD=0x07;   // SS Outputs are PB0,1,2; rest inputs/unused
PORTD=0xE7;  // 5,6,7 I/P w/pullup; 3,4 unused; 0,1,2 active lo - so init high

// Port C unused : inputs, without pullups.  only PC0-5 exist
PORTC=0; // No Pullups
DDRC=0;  // Inputs

// Pin B2 : *** SS must be high or output to make master work !!!
DDRB|=(1<<2);     // Output (unused)
PORTB|=(1<<2);    

// Pin B1
DDRB&=~(1<<1);     // Input (unused)
PORTB&=~(1<<1);    // No pullup

// Pin B0
//DDRB&=~(1<<0);     // Input
//PORTB|=(1<<0);    // High : Pullup

// Indicator LED for now (uses switch pin)
DDRB|=(1<<0);     // Output
LEDOFF;

//DisableMotors(); // Start off (redundant, done above for full port)

myIP=MAKEIP4(192,168,0,142);  // Subject to DHCP  

// Timer 0 - our internal clock

// Timer prescaler = FCPU/1024
TCCR0B|=(1<<CS02)|(1<<CS00);  

TIMSK0|=(1<<TOIE0);  //Enable Overflow Interrupt Enable

TCNT0 = (TIME_START);  // Initialize Counter - fine tuning

sei();   
timecount=0;

#ifndef DEBUGGER
delay_ms(1000);
linkInitialise(myMAC);
delay_ms(50);
setClockout12pt5MHz();
delay_ms(10);
//enc28j60powerUp();
#endif

if (!SWITCHED_OFF) {

//FindDatum(0);  // Both hands to 1200
//FindDatum(1);
}
initLFSR();    // Hands should have taken random time, so seeds accordingly
delay_ms(1000);
}
// ----------------------------------------------------------------------------
void FreeRun(uint8_t motor) {

(motor<3)?HM_ACTIVATE:S_ACTIVATE;

//SPI()

(motor<3)?HM_DEACTIVATE:S_DEACTIVATE;
  
}
#endif


