/*********************************************
 Code for LFSR for pseudo random numbers 

 Copyright (C) 2016-19  S Combes

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
 
*********************************************/
#include "config.h"
#include <avr/interrupt.h>
#include "lfsr.h"

extern uint16_t lfsr;
volatile extern uint8_t timecount;

// -----------------------------------------------------------------------------
void initLFSR(void) {

// Randomly initialises the LFSR

lfsr=(((uint16_t) TCNT0)<<8)|timecount|0x08; // TCNT0 is only 8 bit, timecount perhaps 4
// 0x08 ensures not initialised as 0.
}
// -----------------------------------------------------------------------------
void shuffleLFSR(uint16_t shuffle) {

// Deterministically shuffles the LFSR
if (shuffle!=lfsr) lfsr^=shuffle; // avoid if XOR would create 0.

return;
}
// -----------------------------------------------------------------------------
void shuffleTimeLFSR(void) {

// Stochastically shuffles the LFSR
uint16_t shuffle=(((uint16_t) timecount)<<8)|TCNT0;
shuffleLFSR(shuffle); 

return;
}
// -----------------------------------------------------------------------------
uint8_t Rnd12(void) {

// Advances the LFSR and returns a uniformly distributed number in range 0 to 11,
// Galois LFSR, maximum period, example from Wikipedia

uint8_t output;
uint8_t i;

do {  // Extract a 4-bit sequence from the LFSR, and keep if it is <12

  output=0;
  for (i=0;i<4;i++) {
    output<<=1;
    output|=nextLFSR();
  }
} while (output>=12);  // reject if not in range 0 to 11

return (output);
}
// -----------------------------------------------------------------------------
uint32_t Rnd32bit(void) {

// Advances the LFSR and returns a uniformly distributed 32 bit number

uint32_t output;
uint8_t i;

output=0;
for (i=0;i<32;i++) {
  output<<=1;
  output|=nextLFSR();
}

return (output);
}
// -----------------------------------------------------------------------------
uint16_t Rnd16bit(void) {

// Advances the LFSR and returns a uniformly distributed 16 bit number

uint16_t output;
uint8_t i;

// Extract a 16-bit sequence from the LFSR

output=0;
for (i=0;i<16;i++) {
  output<<=1;
  output|=nextLFSR();
}

return (output);
}
// -----------------------------------------------------------------------------
uint8_t Rnd8bit(void) {

// Advances the LFSR and returns a uniformly distributed 8 bit number

uint8_t output;
uint8_t i;

// Extract a 8-bit sequence from the LFSR

output=0;
for (i=0;i<8;i++) {
  output<<=1;
  output|=nextLFSR();
}

return (output);
}
// -----------------------------------------------------------------------------
uint8_t nextLFSR(void) {

// Advances the LFSR and returns one bit (i.e. 0 or 1 as uint8),

// Galois LFSR, maximum period, example from Wikipedia

uint8_t output;

#define TOGGLE_MASK (0xB400u)

output=lfsr&1;
lfsr>>=1;
if (output&1)
  lfsr^=TOGGLE_MASK;

return (output);
}
// -----------------------------------------------------------------------------
