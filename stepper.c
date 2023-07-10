/*********************************************
 Code for Stepper Motor driver

Currently written specific for Clock
although hardly used in the end - because 
it ended up interrut driven whereas 
these routines hog cpu

*********************************************/

/* Currently not used

#include "config.h"

#include <avr/io.h>
#include <stdlib.h>
#include "network.h"

#include <stdio.h>
#include <string.h>
#include <avr/interrupt.h>
#include <util/delay.h> // after defintion of F_CPU
#include "link.h"
#include "stepper.h"
#include "lfsr.h"

extern char buffer[MSG_LENGTH];
extern MAC_address myMAC;
//extern union MergedPacket MashE;
extern MergedPacket MashE;
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
//#define DTR_ACTIVATE   (PORTB&=(~(1<<1)))
//#define DTR_DEACTIVATE (PORTB|=  (1<<1))

// -----------------------------------------------------------------------------
void InitStepper(void)
{
// Motor controls.  All pins on Port D
//PORTD=0;   // Outputs are low.  
//DDRD=0xFF; // Outputs

DDRD=0x07; // SS Outputs are PB0,1,2; rest inputs/unused
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

// TODO Indicator LED for now (uses switch pin)
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
//timecount=0;

//FindDatum(0);  // Both hands to 1200 TODO *************************

#ifndef DEBUGGER
delay_ms(1000);
linkInitialise(myMAC);
delay_ms(50);
setClockout12pt5MHz();
delay_ms(10);
//enc28j60powerUp();
#endif

//FindDatum(1);

if (!SWITCHED_OFF) {

//FindDatum(0);  // Both hands to 1200
//FindDatum(1);
}
initLFSR();    // Hands should have taken random time, so seeds accordingly
delay_ms(1000);
//while (enc28j60PacketReceive(20,&MashE)) {} // Ignore so far
//while (PacketReceive((uint16_t)MAX_PACKET_SIZE+2, &(MashE.bytes[0]))) ;
}

// -----------------------------------------------------------------------------
void EnableMotors() 
{  
//PORTD=(PATTERN(lpos)<<4)|PATTERN(rpos);
}
// -----------------------------------------------------------------------------
void DisableMotors() 
{  
//PORTD=(0x00);
}
// -----------------------------------------------------------------------------
void EnableMotor(uint8_t motor) 
{  
delay_ms(MSDELAY); 

//if (motor!=0) PORTD=(PORTD&0xF0)|PATTERN(rpos);
//else          PORTD=(PORTD&0x0F)|(PATTERN(lpos)<<4);

delay_ms(MSDELAY); 
}
// -----------------------------------------------------------------------------
void DisableMotor(uint8_t motor) 
{  
delay_ms(MSDELAY); 

//if (motor!=0) PORTD=(PORTD&0xF0);
//else          PORTD=(PORTD&0x0F);

delay_ms(MSDELAY); 
}
// -----------------------------------------------------------------------------
void StepR(uint8_t motor) // Single step.  Needs delay. 10ms OK, 1ms too low.
{  
// 5ms OK - leads to 360 in ~8s.
// 3ms OK - leads to 360 in ~6s.
// 2ms OK - leads to 360 in ~4s.

// For stepping, note motor 0 and motor 1 turn hands in opposite directions
// (because 1 has an intervening gear).  Hence +1 to advance for one is
// equivalent to +7 (=(-1)MOD 8) for the other and vice-versa.

if (motor!=0) {
  rpos=(rpos+7)%8;
  //PORTD=(PORTD&0xF0)|PATTERN(rpos);
}
else {
  lpos=(lpos+1)%8;
  //PORTD=(PORTD&0x0F)|(PATTERN(lpos)<<4);
}
}
// -----------------------------------------------------------------------------
void StepL(uint8_t motor)
{
if (motor!=0) { 
  rpos=(rpos+1)%8;
  //PORTD=(PORTD&0xF0)|PATTERN(rpos);
}
else {
  lpos=(lpos+7)%8;
  //PORTD=(PORTD&0x0F)|(PATTERN(lpos)<<4);
}
}
// -----------------------------------------------------------------------------
void FindDatum(uint8_t motor)
{ // Sensor pulled low on approach to datum, then goes high
uint8_t sensor;

EnableMotor(motor);
if (motor==0) sensor=(1<<0);  // Bit 0
if (motor==1) sensor=(1<<1);  // Bit 1

while (PINC&sensor) { // Move to start of low position
  StepR(motor);
  delay_ms(MSDELAY); 
}
delay_ms(70); // Debounce
while (PINC&sensor) { // Confirm
  StepR(motor);
  delay_ms(MSDELAY); 
}
while (!(PINC&sensor)) { // Move to end of low position
  StepR(motor);
  delay_ms(MSDELAY); 
}
delay_ms(70); // Debounce
while (!(PINC&sensor)) { // Confirm
  StepR(motor);
  delay_ms(MSDELAY); 
}
DisableMotor(motor);
}
// -----------------------------------------------------------------------------
void goToTime(uint8_t hr,uint8_t min)
{
// NB  Applies to both motors because hour hand steps each minute
// This does initial positioning; stepToTime() increments by 1 min

uint8_t motor;
uint32_t advance;

FindDatum(0);
FindDatum(1);

EnableMotor(0);  // Enable both at once, so do not drag other
EnableMotor(1);

for (motor=0;motor<2;motor++) {

  if (motor!=0) advance=(hr+min/60.0)*((uint32_t)A360)/12.0;
  else          advance=min*((uint32_t)B360)/60.0;

  while (advance--) {
    if (!(MODE_TIME & (~PINC))) break; // Must be in time mode
    StepR(motor);
    delay_ms(MSDELAY);
  }
}
DisableMotor(0);
DisableMotor(1);
}
// -----------------------------------------------------------------------------
void stepToTime(uint8_t hr,uint8_t min)
{
// NB  Applies to both motors because hour hand steps each minute
// This does increments by 1 min, initial positioning is done by goToTime() 

uint8_t motor;
int32_t advance;
int32_t previous;

EnableMotor(0);  // Enable both at once, so do not drag other
EnableMotor(1);

for (motor=0;motor<2;motor++) {

  if (motor!=0) {
    advance=(hr+min/60.0)*((int32_t)A360)/12.0;
    previous=(hr+(min-1)/60.0)*((int32_t)A360)/12.0;
    advance-=previous;
    // In theory advance is always same no, but this method copes with rounding errors
    // by always comparing with last conclusion
  }
  else {
    advance=min*((int32_t)B360)/60.0;
    previous=(min-1)*((int32_t)B360)/60.0;
    advance-=previous;  
    // In theory advance is always same no, but this method copes with rounding errors
  }
  while (advance--) {
    if (!(MODE_TIME & (~PINC))) break; // Must be in time mode
    StepR(motor);
    delay_ms(MSDELAY);
  }
}
DisableMotor(0);
DisableMotor(1);
}
// -----------------------------------------------------------------------------
void goToHour(uint8_t hr,uint8_t motor)
{
FindDatum(motor);
uint32_t advance;

if (motor!=0) advance=hr*((uint32_t)A360)/12;
else          advance=hr*((uint32_t)B360)/12;

EnableMotor(motor);
while (advance--) {
  StepR(motor);
  delay_ms(MSDELAY);
}
DisableMotor(motor);
}
// -----------------------------------------------------------------------------
void goToRandomHour(uint8_t motor)
{
goToHour(Rnd12(),motor);
}
// -----------------------------------------------------------------------------
*/

// Daughter Board Stuff
/*
DaughterFreeRun(uint8_t hand) {
  DTR_ACTIVATE;
  SPI()
  
} 
*/


