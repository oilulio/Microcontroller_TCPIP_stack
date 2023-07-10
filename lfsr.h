#ifndef LFSR_H
#define LFSR_H

void initLFSR(void);
void shuffleLFSR(uint16_t shuffle);
void shuffleTimeLFSR(void);
uint8_t nextLFSR(void);
uint8_t Rnd12(void);
uint32_t Rnd32bit(void);
uint16_t Rnd16bit(void);
uint8_t  Rnd8bit(void);

#endif
