#ifndef POWER_H
#define POWER_H

// ADC is on PORTC.  Connected as follows :
#define ADC_I       (1 << 0)  // The current waveform
#define ADC_V       (1 << 1)  // The voltage waveform
#define ADC_IOVER2  (1 << 2)  // Current waveform divided by 2 

#define INTERNAL_REF  ((1 << REFS1) | (1 << REFS2))
#define AVCC_REF      (1 << REFS0)
#define AREF          (0)

void InitialiseADC(void);
uint16_t ReadADC(uint8_t channel);
void InitPower();
void handlePower(uint8_t myADC);

#endif
