/*********************************************
 Code for Real Time Clock DS1307

*********************************************/

#include <stdio.h>
#include <string.h>
#include <avr/interrupt.h>
#include "net_config.h"
//#include <util/delay.h> // after defintion of F_CPU
#include "rtc.h"
#include "lcd.h"
#include "application.h"

extern char buffer[MSG_LENGTH];

#ifdef RTC

// Ideas from http://www.embedds.com/programming-avr-i2c-interface/
//        and http://dangerousprototypes.com/docs/DS1307_real_time_clock

// -----------------------------------------------------------------------------
void TWIInit(void)
{
TWSR=0x00;
TWBR=TWI_SCALE;
TWCR=(1<<TWEN);  // Enable
}
// -----------------------------------------------------------------------------
void TWIStart(void)
{
TWCR=(1<<TWINT)|(1<<TWSTA)|(1<<TWEN);  
while ((TWCR & (1<<TWINT))==0);
}
// -----------------------------------------------------------------------------
void TWIStop(void)
{
TWCR=(1<<TWINT)|(1<<TWSTO)|(1<<TWEN); 
}
// -----------------------------------------------------------------------------
void TWIWrite(uint8_t u8data)
{
TWDR = u8data;
TWCR = (1<<TWINT)|(1<<TWEN);
while ((TWCR & (1<<TWINT)) == 0);
}
// -----------------------------------------------------------------------------
uint8_t TWIReadACK(void)
{
TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
while ((TWCR & (1<<TWINT)) == 0);
return TWDR;
}
// -----------------------------------------------------------------------------
//read byte with NACK
uint8_t TWIReadNACK(void)
{
TWCR = (1<<TWINT)|(1<<TWEN);
while ((TWCR & (1<<TWINT)) == 0);
return TWDR;
}
// -----------------------------------------------------------------------------
uint8_t TWIGetStatus(void)
{
uint8_t status;
//mask status
status = TWSR & 0xF8;
return status;
}
// -----------------------------------------------------------------------------
void StartRTC(void)
{
  uint8_t secs;
  TWIStart();
  TWIWrite(WRITE_RTC);
  TWIWrite(0x00);  // Address is 0x00

  TWIStart();
  TWIWrite(READ_RTC);  
  secs=TWIReadNACK();

  TWIStart();
  TWIWrite(WRITE_RTC);
  TWIWrite(0x00);  // Address is 0x00
  TWIWrite(0x3F & secs);  // Start clock, preserving seconds
  TWIStop();
}
// -----------------------------------------------------------------------------
void StopRTC(void)
{
  uint8_t secs;
  TWIStart();
  TWIWrite(WRITE_RTC);
  TWIWrite(0x00);  // Address is 0x00

  TWIStart();
  TWIWrite(READ_RTC);  
  secs=TWIReadNACK();

  TWIStart();
  TWIWrite(WRITE_RTC);
  TWIWrite(0x00);  // Address is 0x00
  TWIWrite(0x80 | secs);  // Stop clock, preserving seconds
  TWIStop();
}

// -----------------------------------------------------------------------------
void SetRTC(uint8_t day, uint8_t dofmon,  uint8_t month, uint16_t year, 
            uint8_t hour, uint8_t minutes, uint8_t seconds, uint8_t twentyfourhr)
{  // Data always 24h, but entered into clock based on 24h flag
// Sets clock to given time/date - but does not change running state, so may
// need to follow call with StartRTC()

// See http://dangerousprototypes.com/docs/DS1307_real_time_clock

  uint8_t secs;
  TWIStart();
  TWIWrite(WRITE_RTC);
  TWIWrite(0x00);  // Address is 0x00

  TWIStart();
  TWIWrite(READ_RTC);  
  secs=(TWIReadNACK() & 0x80) | TO_RTC_BCD(seconds);

  TWIStart();
  TWIWrite(WRITE_RTC);
  TWIWrite(0x00);  // Address is 0x00
  TWIWrite(secs);  
  TWIWrite(TO_RTC_BCD(minutes));  

  if (twentyfourhr)
    TWIWrite(TO_RTC_BCD(hour));
  else
    TWIWrite(0x40 | ((hour>=12)?0x20:0x00) |  (TO_RTC_BCD(((hour+11)%12)+1))); // Convert 0-23 to 1-12

  TWIWrite(day);  // TODO would ideally calculate  
  TWIWrite(TO_RTC_BCD(dofmon));  

  TWIWrite(TO_RTC_BCD(month));  
  TWIWrite(TO_RTC_BCD(year%100));  

  TWIStop();

}
// -----------------------------------------------------------------------------
uint32_t ReadRTC(void)
{  // Data always 24h, but entered into clock based on 24h flag
// Reads clock - but does not change running state

// See http://dangerousprototypes.com/docs/DS1307_real_time_clock

  uint8_t secs,minutes,hours,month,day,date,year;
//  uint32_t temp;

  TWIStart();
  TWIWrite(WRITE_RTC);
  TWIWrite(0x00);  // Address is 0x00

  TWIStart();
  TWIWrite(READ_RTC);  
  secs=(TWIReadACK() & 0x7F);
  secs=FROM_RTC_BCD(secs);

  minutes=TWIReadACK();
  minutes=FROM_RTC_BCD(minutes);

  hours=TWIReadACK();
   
  if (hours & 0x40)  // 12 hr clock
    hours=FROM_RTC_BCD(hours & 0x1F) + (hours & 0x20)?12:0;
  else
    hours=FROM_RTC_BCD(hours & 0x2F);
   
  day=TWIReadACK();
  date=TWIReadACK();
  date=FROM_RTC_BCD(date);

  month=TWIReadACK();
  month=FROM_RTC_BCD(month);

  year=TWIReadNACK();
  year=FROM_RTC_BCD(year);

  TWIStop();

  /*lcd_clrscr();
  sprintf(buffer,"%d.%d.%d.%d...",secs,minutes,hours,day);
  lcd_puts(buffer);
  lcd_gotoxy(0,1);
  sprintf(buffer,"%d.%d.%d....",date,month,year);
  lcd_puts(buffer);
*/

  return Get_Time(date,month,(uint16_t)2000+year,hours,minutes,secs);
 
}
#endif
