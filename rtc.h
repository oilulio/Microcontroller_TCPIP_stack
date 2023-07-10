#ifndef RTC_H
#define RTC_H

#define   TO_RTC_BCD(X)    ((((X)/10) << 4) | ((X)%10))
#define FROM_RTC_BCD(X)    ((((X) & 0xF0) >> 4)*10 + ((X) & 0x0F))


#define WRITE_RTC  (0xD0)
#define READ_RTC   (0xD1)


void TWIInit(void);
void TWIStart(void);
void TWIStop(void);
void TWIWrite(uint8_t u8data);
uint8_t TWIReadACK(void);
uint8_t TWIReadNACK(void);
uint8_t TWIGetStatus(void);
void StartRTC(void);
void StopRTC(void);
void SetRTC(uint8_t day, uint8_t dofmon,  uint8_t month, uint16_t year, 
            uint8_t hour, uint8_t minutes, uint8_t seconds, uint8_t twentyfourhr);
uint32_t ReadRTC(void);


#endif
