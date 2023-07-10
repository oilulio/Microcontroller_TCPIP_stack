#include "config.h"
#ifndef STEPPER_H
#define STEPPER_H

// B motor is inner (concentric) motor = minute hand
// A motor is outer (geared)     motor = hour hand

#define A360 (5461)  // 360 degrees for 1st motor (HOUR)
#define B360 (4096)  // 360 degrees for 2nd motor (MINUTES)

#define MSDELAY (5)  // Default interval between steps in ms.  Can be as low as 2

#define HAND_STOP    (0) // Make sure this is 0 
#define HAND_RUN     (1)

void InitStepper(void);
void EnableMotors(void);
void DisableMotors(void);
void EnableMotor(uint8_t motor);
void DisableMotor(uint8_t motor);
void StepR(uint8_t motor);
void StepL(uint8_t motor);
void FindDatum(uint8_t motor);
void goToHour(uint8_t hr,uint8_t motor);
void goToRandomHour(uint8_t motor);
void initLFSR();
void shuffleLFSR(uint16_t shuffle);
uint8_t nextLFSR();
uint8_t Rnd12();
uint16_t Rnd16bit(void);
void goToTime(uint8_t hr,uint8_t min);
void stepToTime(uint8_t hr,uint8_t min);

#endif
