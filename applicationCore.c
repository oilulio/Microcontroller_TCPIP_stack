/*********************************************
Copyright (C) 2009-23  S Combes

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

 Code for Core (usually unchanging) Application layer protocols :
 
 1. DHCP
 2. DNS
 3. NTP
 4. LLMNR (technically non-compliant - doesn't check for conflicts)
 5. mDNS (technically non-compliant - doesn't check for conflicts)

 *********************************************/

#include "config.h"

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>  // for Malloc/free
#include <string.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

#include "link.h"
#include "network.h"
#include "transport.h"
#include "application.h"
#include "lfsr.h"

extern IP4_address NullIP,myIP;
extern IP4_address BroadcastIP;
extern IP4_address GWIP;
extern IP4_address DNSIP;
extern IP4_address subnetMask;
extern IP4_address subnetBroadcastIP;
extern IP4_address NTPIP; 
extern char buffer[MSG_LENGTH];
extern uint8_t ISP_state;
char hex[]={0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x41,0x42,0x43,0x44,0x45,0x46};  // Saves byte without trailing /0

uint16_t lfsrsave;
extern uint16_t lfsr;
char tmp[85];

extern uint32_t time_now;
extern uint32_t protect_time;
extern uint32_t time_set;
extern uint8_t deferral;
extern volatile uint8_t timecount;

extern Status MyState;
extern IP4_address IsolatedIP;
extern IP4_address FTP_IP;

extern MAC_address myMAC;
extern uint8_t DHCP_lease[4];
extern TCP_TCB TCB[MAX_TCP_ROLES]; 
extern uint16_t UDP_Port[MAX_UDP_PORTS];
extern MergedPacket MashE;

#ifdef AUTH7616
uint8_t nonce[]={0,0,0,0};
uint8_t opaque[]={0,0,0,0};
#endif

static const char myhost[]=HOSTNAME; 

uint8_t dummy=0;
uint16_t expectLen;  // Special use - passing param to callback
uint8_t progress;    // Special use - passing param to callback

#ifdef USE_DHCP
IP4_address myPreferredIP;
uint8_t dhcp_option_overload;  
#endif

#ifdef USE_POP3
uint8_t iPOP3_Connect; 
#endif

#ifdef USE_mDNS 
extern IP4_address mDNS_IP4; 
#endif

// ----------------------------------------------------------------------------------
#ifdef USE_HTTP
uint16_t HTTP_404(uint16_t start,uint16_t length,uint8_t * result) {

static const char text[] PROGMEM={"HTTP/1.1 404 Not Found\r\nConnection: keep-alive\r\n\r\n"\
                             "<html><head><title>404 Not Found</title></head>"\
                             "<body><h1>Resource not found</h1></body></html>"};
// 'keep-alive' works better than 'close'!  Perhaps because we are reusing ...
uint16_t i=0;
  while ((length--) && start<sizeof(text)) result[i++]=pgm_read_byte(&text[start++]);
  return sizeof(text);  // sizeof(text)-1 should be right ... but doesn't work? TODO
}
#ifdef AUTH7616
// ----------------------------------------------------------------------------------
uint16_t UnauthorisedData(uint16_t start,uint16_t length,uint8_t * result) {
  
  // TODO - while testing, nonce,opaque fixed
  
   //   "realm=\"prog@iot-isp\",qop=\"auth\",nonce=\"NoNcE\",opaque=\"12345\"\r\n\r\n"
   // DROPPING opaque now

   static const char text[] PROGMEM={"HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Digest "\
      "realm=\"prog@iot-isp\",qop=\"auth\",algorithm=SHA-256,nonce=\"NoNcE\"\r\n\r\n"\
      "<html><head><title>Error</title></head>"\
                             "<body><h1>401 Unauthorized</h1></body></html>"};
  uint16_t i=0;
  while ((length--) && start<sizeof(text)) {
    result[i++]=pgm_read_byte(&text[start++]); 
  }
  return sizeof(text);
}
// ----------------------------------------------------------------------------------
void send401Unauthorised(void) {
  if (!(nonce[0]|nonce[1]|nonce[2]|nonce[3])) {
    nonce[0]=nextLFSR();
    nonce[1]=nextLFSR();
    nonce[2]=nextLFSR();
    nonce[3]=nextLFSR();
  }
  if (!(opaque[0]|opaque[1]|opaque[2]|opaque[3])) {
    opaque[0]=nextLFSR();
    opaque[1]=nextLFSR(); 
    opaque[2]=nextLFSR();
    shuffleTimeLFSR();  // Ensure the LFSR internal state changes now it's been shown
    opaque[3]=nextLFSR();
  }
  TCP_ComplexDataOut(&MashE,TCP_SERVER,UnauthorisedData(0,0,&dummy),&UnauthorisedData,0,TRUE);
}
#endif
#endif
// ----------------------------------------------------------------------------------
uint8_t hexDigit(char c) { // No checks - input =0..9A..F or a..f
if (toupper(c)>='A' && toupper(c)<='F') return (10-'A'+toupper(c));
return (toupper(c)-'0');
}
// ---------------------------------------------------------------------------------------
static inline uint8_t leapYear(uint16_t * year)
{ 
if (!((*year)%400)) return (TRUE);
if (!((*year)%100)) return (FALSE);
if (!((*year)%4))   return (TRUE);
return (FALSE);
}
// ---------------------------------------------------------------------------------------
uint8_t summerTimeCorrection(uint8_t * hour, uint8_t * day, uint8_t * month, uint16_t * year)
{ // European rules as at 2009.  From Wikipedia.
// Returns hours correction +1=Summer, 0=Winter
// Hence can also be viewed as BOOL for BST
#define ONE_HOUR (1)
#define NIL      (0)

uint16_t sunday; // The change day (will be a Sunday)

if (*month < 2  || *month > 10) return NIL;
if (*month > 3  && *month < 10) return ONE_HOUR; // Easy ones

if (*month == 3)
{
  sunday=(31-((5*(*year))/4 +4)%7);
  if (*day < sunday) return NIL;
  if (*day > sunday) return ONE_HOUR;
  if (*hour < 1) return NIL;
  return 1;
}
// Must be October
sunday=(31-((5*(*year))/4 +1)%7);
if (*day < sunday) return ONE_HOUR;
if (*day > sunday) return NIL;
if (*hour < 1) return ONE_HOUR;
return NIL;
}
// ---------------------------------------------------------------------------------------
uint32_t protectedGetTime(uint8_t day, uint8_t month, uint16_t year,
                  uint8_t hour, uint8_t minute, uint8_t second)
{
// Returns time measured in seconds from 1 Jan 2000 0000 UT 
// capping out of bounds values
// COULD later get more sophisticated - e.g correlated DOW with date
uint8_t mon[12]={31,28,31,30,31,30,31,31,30,31,30,31};

if (second > 59) second=59;
if (minute>59)   minute=59;
if (hour > 23)   hour=23;
if (month>12)    month=12;
if (day>mon[month]) day=mon[month]; // TODO check 0-12 or 1-12?

return getTime(day, month, year, hour, minute, second);
}
// ---------------------------------------------------------------------------------------
uint32_t getTime(uint8_t day, uint8_t month, uint16_t year,
                 uint8_t hour, uint8_t minute, uint8_t second)
{
// Returns time measured in seconds from 1 Jan 2000 0000 UT 
uint16_t i;
uint32_t time=0;
uint8_t mon[12]={31,28,31,30,31,30,31,31,30,31,30,31};
uint8_t BST;

for (i=2000;i<year;i++) 
  time+=((uint32_t)YEAR_IN_SECONDS + (uint32_t)leapYear(&i)*DAY_IN_SECONDS);
 
mon[1]+=leapYear(&year);

for (i=0;i<(month-1);i++)
  time+=((uint32_t)mon[i]*DAY_IN_SECONDS);

time+=((uint32_t)(day-1)*DAY_IN_SECONDS);  // On 1st of month, 0 days have elapsed
time+=((uint32_t)hour*HOUR_IN_SECONDS);
time+=((uint32_t)minute*MINUTE_IN_SECONDS);
time+=((uint32_t)second);

BST=summerTimeCorrection(&hour,&day,&month,&year); // Uses GMT values to decide

if (BST)
  time+=((uint32_t)HOUR_IN_SECONDS); 

return time;  
}
// ---------------------------------------------------------------------------------------
void displayTime(uint32_t time)
{ // Displays time based on uint32 secs since 1 Jan 2000

  TimeAndDate timeanddate;

  timeanddate=parseTime(time);
#ifdef USE_LCD
  lcd_gotoxy(0,0);
#endif
  lineOne(&timeanddate); 
  buffer[0]='\t';
  //genericUDP((uint16_t *)buffer,10);   
#ifdef USE_LCD
  lcd_puts(buffer);
  lcd_gotoxy(0,1);
#endif
  lineTwo(&timeanddate);    
#ifdef USE_LCD
  lcd_puts(buffer);
#endif

  buffer[0]='\t';
  //genericUDP((uint16_t *)buffer,10);

//#endif
}

void lineOne(TimeAndDate * tad)
{
static const char cday[7][4] PROGMEM ={"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
static const char cmon[12][4] PROGMEM ={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

sprintf(buffer,"%3s %2d %3s %4d       ",cday[tad->wk_day],tad->day,cmon[tad->month-1],tad->year);  

return;
}
// ---------------------------------------------------------------------------------------
void lineTwo(TimeAndDate * tad)
{  
  sprintf(buffer,"   %2d:%02d:%02d %s   ",tad->hour,tad->minute,tad->secs,(tad->BST)?"BST":"GMT");
}
/*uint16_t year=2000;
uint8_t day=1;
uint8_t month=1;
uint8_t hour=0;
uint8_t minute=0;
uint8_t BST;
uint8_t wk_day;

uint8_t mon[12]={31,28,31,30,31,30,31,31,30,31,30,31}; // TODO - use Hex to avoid /n 4th char?
static const char str_pstr[] PROGMEM cday[7][4]={"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
ststic const char str_pstr[] PROGMEM cmon[12][4]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

wk_day=(time/DAY_IN_SECONDS+5)%7;

while ((time > (YEAR_IN_SECONDS + (uint32_t)Leap_year(&year)*DAY_IN_SECONDS)))
{
  time-=(YEAR_IN_SECONDS + (uint32_t)Leap_year(&year)*DAY_IN_SECONDS);
  year++;
}

mon[1]+=Leap_year(&year);  // Set Feb as appropriate

while (time > ((uint32_t)mon[month-1]*DAY_IN_SECONDS))
{
 time-= ((uint32_t)mon[month-1]*DAY_IN_SECONDS);
 month++; 
}

day=time/DAY_IN_SECONDS;
time-=((uint32_t)day*DAY_IN_SECONDS);
day++;

hour=time/HOUR_IN_SECONDS;
time-=((uint32_t)hour*HOUR_IN_SECONDS);

minute=time/MINUTE_IN_SECONDS;
time-=((uint32_t)minute*MINUTE_IN_SECONDS);

BST=summerTimeCorrection(&hour,&day,&month,&year); // Uses GMT values to decide

if (BST)
{
  hour++;
  if (hour==24) // this is where it gets complicated
  {
    hour=0;
    wk_day=(wk_day+1)%7;
    day++;
    if (day > mon[month-1])
    {
      day=0;
      month++;  // Don't have to worry about year because summer time can't happen in December
    }
  }
}
MyState.TIME=TIME_SET;

// Don't clearscreen or it flickers
#ifdef USE_LCD
  lcd_gotoxy(0,0);
  sprintf(buffer,"%3s %2d %3s %4d       ",cday[wk_day],day,cmon[month-1],year);  
      
  lcd_puts(buffer);
  lcd_gotoxy(0,1);
  sprintf(buffer,"   %2d:%02d:%02d %s   ",hour,minute,(int)time,(BST)?"BST":"GMT");
  lcd_puts(buffer);
#endif
}
*/
// ---------------------------------------------------------------------------------------
TimeAndDate parseTime(uint32_t time)
{ // Displays time based on uint32 secs since 1 Jan 2000

TimeAndDate result;

result.year=2000;
result.day=1;
result.month=1;
result.hour=0;
result.minute=0;

uint8_t mon[12]={31,28,31,30,31,30,31,31,30,31,30,31}; // TODO - use Hex to avoid /n 4th char?

result.wk_day=(time/DAY_IN_SECONDS+5)%7;

while ((time > (YEAR_IN_SECONDS + (uint32_t)leapYear(&result.year)*DAY_IN_SECONDS)))
{
  time-=(YEAR_IN_SECONDS + (uint32_t)leapYear(&result.year)*DAY_IN_SECONDS);
  result.year++;
}

mon[1]+=leapYear(&result.year);  // Set Feb as appropriate

while (time > ((uint32_t)mon[result.month-1]*DAY_IN_SECONDS))
{
 time-= ((uint32_t)mon[result.month-1]*DAY_IN_SECONDS);
 result.month++; 
}

result.day=time/DAY_IN_SECONDS;
time-=((uint32_t)result.day*DAY_IN_SECONDS);
result.day++;

result.hour=time/HOUR_IN_SECONDS;
time-=((uint32_t)result.hour*HOUR_IN_SECONDS);

result.minute=time/MINUTE_IN_SECONDS;
time-=((uint32_t)result.minute*MINUTE_IN_SECONDS);

result.BST=summerTimeCorrection(&result.hour,&result.day,&result.month,&result.year); // Uses GMT values to decide

if (result.BST)
{
  result.hour++;
  if (result.hour==24) // this is where it gets complicated
  {
    result.hour=0;
    result.wk_day=(result.wk_day+1)%7;
    result.day++;
    if (result.day > mon[result.month-1])
    {
      result.day=0;
      result.month++;  // Don't have to worry about year because summer time can't happen in December
    }
  }
}
MyState.TIME=TIME_SET;
result.secs=(int)time;

return result;
}

#ifdef USE_NTP
// ---------------------------------------------------------------------------------------
void handleNTP(NTP_message * NTP)
{ // Handle a returned NTP message i.e. from server

uint32_t isecs;

time_set=time_now;  // Temporarily store the current estimate
time_now=BYTESWAP32(NTP->tx_stamp_int)-FIRSTJAN2000_0000_UT;
isecs=BYTESWAP32(NTP->tx_stamp_fract);

if (((isecs>>24)&0xFF)>=128) time_now++;  // Just use the 1st byte - represents 256ths of a second
// so round up if reaches 128.  Quite accurate enough

protect_time=time_now - 12;  // allow time to be shown

#ifdef WHEREABOUTS
// If we are < 2 mins fast, just allow hands to catch up 
// Note this calculation is unsigned.  Hence (correctly) only trips when actualy fast.  
if ((time_set-time_now)<120) deferral=10+(uint8_t)((time_set-time_now)); // Play safe with 10s buffer
else deferral=0;
#endif

time_set=time_now;  // Now record when I was actually set

displayTime(time_now);
TimeAndDate tad;
tad=parseTime(time_now);
#ifdef WHEREABOUTS
hour=tad.hour;
minute=tad.minute;
#endif
MyState.TIME=TIME_SET;
}
// ---------------------------------------------------------------------------------------
void queryNTP(void)
{ // EITHER launches a NTP packet asking the time; OR, if the NTP IP address is not set,
  // launches a DNS packet to set the NTP IP address. 

//MergedPacket Mash;  // The main data structure

if (MyState.TIME==TIME_SET) return;       // Its done

if (!NTPIP && (MyState.TIME==TIME_UNSET)) 
{ // IP address is null and we aren't doing anything.  Need to use DNS
  // Any of these should do.

  uint8_t rnd=Rnd12();
  if (rnd<4)      queryDomainName(&MashE,"ntp2b.mcc.ac.uk",NTP_ID);
  else if (rnd<8) queryDomainName(&MashE,"ntp.cis.strath.ac.uk",NTP_ID);
  else            queryDomainName(&MashE,"ntp-3.vt.edu",NTP_ID);

  MyState.TIME=TIME_WAIT_DNS;

  return;
} 

if (!NTPIP) return;  // Not ready to go
if (MyState.TIME==TIME_REQUESTED) return;  // In flight already

MashE.NTP.Leap=0b11;
MashE.NTP.Status=0x23;
MashE.NTP.theType=0;
MashE.NTP.precision=BYTESWAP16(0x04FA);
MashE.NTP.estimated_error=0x01000100;  // Note endian change
MashE.NTP.estimated_drift_rate=0;
MashE.NTP.reference_id=0;
MashE.NTP.reference_time_int=0;
MashE.NTP.reference_time_fract=0;
MashE.NTP.origin_time_int=0;
MashE.NTP.origin_time_fract=0;
MashE.NTP.rx_stamp_int=0;
MashE.NTP.rx_stamp_fract=0;
MashE.NTP.tx_stamp_int=0;
MashE.NTP.tx_stamp_fract=0;

UDP_Port[NTP_CLIENT_PORT_REF]=newPort(UDP_PORT);

launchUDP(&MashE,&NTPIP,UDP_Port[NTP_CLIENT_PORT_REF],
                 NTP_SERVER_PORT,(sizeof(MashE.NTP)),NULL,0);

MyState.TIME=TIME_REQUESTED;

return;
}
#endif
// ---------------------------------------------------------------------------------------
uint8_t genericUDPTo(uint16_t words[],uint16_t length,IP4_address * sendIP)
{ // Launches a generic UDP packet - mainly for testing

if (!myIP) return FALSE;   // Wait until we have an address

for (uint16_t i=0;i<length;i++)
{
//  words[i]=i;
  MashE.UDP_payload.words[i]=/*BYTESWAP16*/(words[i]);
}

launchUDP(&MashE, sendIP,38858,51000,length*2,NULL,0);

return (TRUE);
}
// ---------------------------------------------------------------------------------------
uint8_t genericUDP(uint16_t words[],uint16_t length)
{ // Launches a generic UDP packet - mainly for testing/debugging

IP4_address sendIP=MAKEIP4(192,168,0,210); // debugging address - needs to exist so ARP works
return genericUDPTo(words,length,&sendIP);
}
// ---------------------------------------------------------------------------------------
uint8_t genericUDPBcast(uint16_t words[],uint16_t length)
{ // Launches a generic UDP broadcast packet - mainly for testing/debugging

return genericUDPTo(words,length,&BroadcastIP);
}
#ifdef USE_DNS
// ---------------------------------------------------------------------------------------
uint8_t skipName(uint8_t * Message)
{ // Moves past a DNS name record, either formatted e.g. 3far7reacher3net0 (i.e. nxxxnxxxnxxxx0)
// or formatted as an indirect reference (indicated by C0 bits set)
uint8_t i;
i=0;

while (TRUE)
{
  if ((Message[i]&0xC0) == 0xC0) return (i+2);
  
  if (Message[i]==0) return (i+1);

  i+=(Message[i]+1);  // Move along by the index (plus the index byte itself)
}

}
// ---------------------------------------------------------------------------------------
void handleDNS(DNS_message * DNS)
{ // Handle a received DNS message

uint16_t i;
uint8_t j;

// Longer term should match answers to replies (i.e use QueryID) etc, for now 
// assume one at a time.
// Not well tested given number of possible answers

if (DNS->RCODE) return; // Some invalid message

// Endianism
DNS->QDCOUNT=BYTESWAP16(DNS->QDCOUNT);
DNS->ANCOUNT=BYTESWAP16(DNS->ANCOUNT);
DNS->NSCOUNT=BYTESWAP16(DNS->NSCOUNT);
DNS->ARCOUNT=BYTESWAP16(DNS->ARCOUNT);

i=0;  

for (j=0;j<(DNS->QDCOUNT);j++)
{
  i+=skipName(&DNS->message[i]);  // Skip the question
  i+=4;  // Move to next question
}

for (j=0;j<(DNS->ANCOUNT);j++)
{
  i+=skipName(&DNS->message[i]);  // Skip the URL

  if (DNS->message[i]==0 && DNS->message[i+1]==1)
  { // This is the answer
    if (DNS->message[i+9]!=4) // screwup, because will have size of IP address if we are in right place.
    {
      sprintf(buffer,"DNS Error %d", i);  
      Error();
    }
//TODO - at Some point need to be cleverer - not just cope with NTP, eg. use QueryID
    
    if (DNS->id==NTP_ID) {
      NTPIP=MAKEIP4(DNS->message[i+10],DNS->message[i+11],
                    DNS->message[i+12],DNS->message[i+13]); 
    }

#ifdef USE_LCD
  lcd_clrscr();
  lcd_puts_P(PSTR("DNS response"));  

  lcd_gotoxy(0,1);
  IP4toBuffer(MAKEIP4(DNS->message[i+10],DNS->message[i+11],
                      DNS->message[i+12],DNS->message[i+13])); 
  lcd_puts(buffer);
#endif

    return;
  }
  else if (DNS->message[i]==0 && DNS->message[i+1]==5)
  {
    i+=DNS->message[i+8]*256+DNS->message[i+9]+10;  // Move past this answer type
  }
  else // we're stuck - a message type we can't handle
    return;  // TODO
}

return;
}
// ----------------------------------------------------------------------------
uint8_t parseDomainName(char domain[MAX_DNS],char parse[MAX_DNS])
{ // Parses a domain name such as aaa.bbbb.ccc into the DNS format 3aaa4bbbb3ccc\0

int8_t len,i,j;

len=strlen(domain);

i=len-1;
j=0;

while (i>-1)
{
  while (domain[i]!='.' && (i+1)) 
  {
    parse[i+1]=domain[i];
    i--;
    j++;
  }
  parse[i+1]=j;
  j=0;
  i--;
}
parse[len+1]=0; // terminating zero 
return (len);
}
// ----------------------------------------------------------------------------
void queryDomainName(MergedPacket * Mash,char domain[MAX_DNS],uint16_t id)
{
uint8_t len;

Mash->DNS.id=id;
Mash->DNS.QR=Mash->DNS.OPCODE=Mash->DNS.AA=Mash->DNS.TC=0;
Mash->DNS.RD=1;
Mash->DNS.RA=Mash->DNS.Z=Mash->DNS.RCODE=0;

Mash->DNS.QDCOUNT=BYTESWAP16(1);
Mash->DNS.ANCOUNT=0;  // No endianism on zero
Mash->DNS.NSCOUNT=0;
Mash->DNS.ARCOUNT=0;

len=parseDomainName(domain,(char *)&Mash->DNS.message[0]);
  
Mash->DNS.message[2+len]=0;  // IPv4
Mash->DNS.message[3+len]=1;
Mash->DNS.message[4+len]=0;  // Internet
Mash->DNS.message[5+len]=1;

UDP_Port[DNS_CLIENT_PORT_REF]=newPort(UDP_PORT);

launchUDP(Mash,&DNSIP,UDP_Port[DNS_CLIENT_PORT_REF],DNS_SERVER_PORT,(18+len),NULL,0);
}
#endif
#ifdef USE_LLMNR
// ---------------------------------------------------------------------------------------
void handleLLMNR(MergedPacket * Mash)
{ // Handle a received LLMNR query

uint8_t i,j;

DNS_message * DNS=&(Mash->DNS);
IP4_address fromIP;

// Endianism
DNS->QDCOUNT=BYTESWAP16(DNS->QDCOUNT);
//DNS->ANCOUNT=BYTESWAP16(DNS->ANCOUNT); // Don't bother with endianism on those that
//DNS->NSCOUNT=BYTESWAP16(DNS->NSCOUNT); // should be zero
//DNS->ARCOUNT=BYTESWAP16(DNS->ARCOUNT);

if (DNS->QDCOUNT!=1) return;
if (DNS->ANCOUNT)    return;
if (DNS->NSCOUNT)    return;

if (DNS->QR)    return; // Must be query (=0)
if (DNS->RCODE) return; // Must be zero

i=hostMatch(DNS,0);  
if (!i) return; // doesn't match

if (DNS->message[i+0]!=0 || (DNS->message[i+1]!=1 && DNS->message[i+1]!=0xFF)) return;
// Qtype  != "A", coded as 0x0001. i.e. IP4, or any class (0xFF)
if (DNS->message[i+2]!=0 || DNS->message[i+3]!=1) return; 
// Qclass != 0x0001 i.e. Internet

// Prepare reply
DNS->QR=1;  // A reply
DNS->QDCOUNT=BYTESWAP16(1);  // Swap back.  1 question
DNS->ANCOUNT=BYTESWAP16(1);  // 1 answer
DNS->ARCOUNT=0;

i=i+4;  // Move after end of question
for (j=0;j<i;j++)
  DNS->message[i+j]=DNS->message[j]; // Copy question to answer

DNS->message[i+j+0]=0x00; // TTL is 32 bit
DNS->message[i+j+1]=0x00;
DNS->message[i+j+2]=0x00;
DNS->message[i+j+3]=0x1E; // 30s, default recommended

DNS->message[i+j+4]=0x00;
DNS->message[i+j+5]=0x04; // IPv4 4 Bytes

DNS->message[i+j+6]=OCTET1(myIP);
DNS->message[i+j+7]=OCTET2(myIP);
DNS->message[i+j+8]=OCTET3(myIP);
DNS->message[i+j+9]=OCTET4(myIP);

fromIP=Mash->IP4.source; // Create copy to avoid overwriting

launchUDP(Mash,&fromIP,LLMNR_PORT,Mash->UDP.sourcePort,(i+j+8+14),NULL,0); // Send back whence it came, 14 is header  

return;
}
#endif
// ---------------------------------------------------------------------------------------
#ifdef USE_mDNS
void handleMDNS(MergedPacket * Mash)
{ // Handle a received mDNS message

uint8_t i,j,k;

DNS_message * DNS=&(Mash->DNS);

// Endianism
DNS->QDCOUNT=BYTESWAP16(DNS->QDCOUNT);
//DNS->ANCOUNT=BYTESWAP16(DNS->ANCOUNT); // Don't bother with endianism on those that
//DNS->NSCOUNT=BYTESWAP16(DNS->NSCOUNT); // should be zero
//DNS->ARCOUNT=BYTESWAP16(DNS->ARCOUNT);

if (DNS->ANCOUNT)    return;
if (DNS->NSCOUNT)    return;

if (DNS->QR)    return; // Must be query (=0)
if (DNS->RCODE) return; // Must be zero

i=0;
k=0;
for (j=0;j<DNS->QDCOUNT;j++) { // Automatically ensures at least one question

  i=hostMatch(DNS,i);  
  if (!i) return; // doesn't match
  if (j==0) k=i;  // Stored in case we're not the first question

  uint8_t fail=FALSE;
  if (DNS->message[i+0]!=0 || DNS->message[i+1]!=1) fail=TRUE;
  // Qtype  != "A", coded as 0x0001. i.e. IP4
  if ((DNS->message[i+2]&0x7f)!=0 || DNS->message[i+3]!=1) fail=TRUE;
  // Qclass != 0x01 i.e. Internet
  // N.B. Apple asks '8001' where the '8' is a question bit, hence &0x7F.

  if (!fail) {

    // Prepare reply
    DNS->QDCOUNT=0;              // 0 questions
    DNS->ANCOUNT=BYTESWAP16(1);  // 1 answer (we'll only answer IP4 (A) even if we also heard IP6 (AAAA))
    DNS->ARCOUNT=0;
    DNS->AA=1; 
    DNS->QR=1; 

    DNS->message[k+0]=0x00;   
    DNS->message[k+1]=0x01;   
    DNS->message[k+2]=0x80;
    DNS->message[k+3]=0x01;  

    DNS->message[k+4]=0x00; // TTL is 32 bit
    DNS->message[k+5]=0x00;
    DNS->message[k+6]=0x00; // example had 78 here (8hrs) not in LSB
    DNS->message[k+7]=0x78; // 120s

    DNS->message[k+8]=0x00;
    DNS->message[k+9]=0x04; // IPv4 4 Bytes

    DNS->message[k+10]=OCTET1(myIP);
    DNS->message[k+11]=OCTET2(myIP);
    DNS->message[k+12]=OCTET3(myIP);
    DNS->message[k+13]=OCTET4(myIP);

    launchUDP(Mash,&mDNS_IP4,mDNS_PORT,mDNS_PORT,(12+k+14),NULL,0); // Multicast it back 

    return;  // Only answer once (at most)
  }
  else {
    i+=4;  // Ready to try again
  }
}
return;
}
#endif
// ----------------------------------------------------------------------------
uint8_t hostMatch(DNS_message * DNS,uint8_t i) 
{
uint8_t j;
// looks for matching hostname, e.g. "host" will match "host" or "host.local"
// Returns 0 (FALSE) for no match and index of next byte in message
// to read (i.e. 2+length of FQ hostname including trailing 0) otherwise, 
// e.g. a match to "host" will either return 6 (4host0) or 12 (4host5local0)
// Also return OK if repeated hostname indicated by C0

// Note case insensitivity

if ((DNS->message[i]&0xC0)==0xC0) { // Name was a cross reference
  return (i+2);
}

if (strlen(myhost)!=DNS->message[0]) return FALSE; // can't match

i=0;
while (i<strlen(myhost)) {
  if (toupper((uint8_t)myhost[i])!=toupper((uint8_t)DNS->message[i+1]))
    return FALSE;  // Silently fail
  i++;
}

i++; // Move to next length marker
if (!(DNS->message[i])) return (i+1);  // .local not added.

if (DNS->message[i]!=5) return FALSE; // local has length 5

j=0;
while (j<5) {
  if ("LOCAL"[j]!=toupper((int)DNS->message[i+j+1]))
    return FALSE;  // Silently fail
  j++;
}
return (i+j+2); 
}
#ifdef USE_DHCP
// ----------------------------------------------------------------------------
uint16_t prepDHCP(DHCP_message * DHCP, uint8_t type)
{
uint16_t i;
uint8_t j;
// Only supports DHCP_DISCOVER and DHCP_REQ (we aren't a server)
// Need to add DHCP_RELEASE and perhaps DHCP_DECLINE

DHCP->op = 1; // Always seems to be 1 for client : BOOTP legacy?
DHCP->hardware_type = 1;   // 10Mb Ethernet
DHCP->hardware_length = 6; // Ethernet
DHCP->hops = 0;
DHCP->XID=MYID;  
DHCP->secs=0;
DHCP->flags=0;  
DHCP->CIAddr=DHCP->YIAddr=DHCP->SIAddr=DHCP->GIAddr=NullIP;
DHCP->MAC=myMAC;

//OLD method DHCP->Hware.MAC=myMAC;
//for (i=0;i<10;i++) DHCP->Hware.dummy[i]=0x00;  // Pad with zero

//for (i=0;i<64;i++)  DHCP->Sname[i]=0x00;     As these are always zero, new method doesn't store them
//for (i=0;i<128;i++) DHCP->Bootfile[i]=0x00;

//for (i=0;i<4;i++)   DHCP->magic_cookie[i]=magic_cookie[i]; // This will be added by physical layer

i=0;
DHCP->options[i++]=0x35;  // Message type
DHCP->options[i++]=0x1;   // Length
DHCP->options[i++]=(type);

DHCP->options[i++]=0x3D;  // Client ID
DHCP->options[i++]=7;  
DHCP->options[i++]=1;  // type = hardware addr
for (j=0;j<6;j++) DHCP->options[i++]=myMAC.MAC[j];

DHCP->options[i++]=0x32;  // Requested address
DHCP->options[i++]=4;  
for (j=0;j<4;j++) DHCP->options[i++]=(int)(0xFF&(myPreferredIP>>(8*j)));

if ((type) == DHCP_REQ)
{
  DHCP->options[i++]=0x36;  // Specify DHCP server who replied to us
  DHCP->options[i++]=4;  
  for (j=0;j<4;j++) DHCP->options[i++]=(int)(0xFF&(GWIP>>(8*j)));
}

DHCP->options[i++]=0x0C;  // Hostname
DHCP->options[i++]=strlen(myhost);
for (j=0;j<strlen(myhost);j++) DHCP->options[i++]=myhost[j];
 
DHCP->options[i++]=0x37;  // Parameter list
DHCP->options[i++]=4;     // # Parameters
DHCP->options[i++]=0x01;  // Subnet Mask
DHCP->options[i++]=0x0F;  // Domain Name
DHCP->options[i++]=0x03;  // Router
DHCP->options[i++]=0x06;  // DNS

DHCP->options[i++]=0xFF;  // End

return (i);
}
// ----------------------------------------------------------------------------
void initiateDHCP(void)
{ // Start the ball rolling with a DHCP DISCOVER message

uint16_t i;

myPreferredIP=myIP;  // Store it here and make null so that IP routines use null
myIP=NullIP;

i=prepDHCP(&MashE.DHCP,DHCP_DISCOVER);

launchUDP(&MashE,&BroadcastIP,DHCP_CLIENT_PORT,DHCP_SERVER_PORT,(i+240),NULL,0); 
// 240 is length of DHCP header pre options

MyState.IP=DHCP_WAIT_OFFER;  // Move to next state

return;
}
// ----------------------------------------------------------------------------
void requestDHCP(void)
{ // Reply to DHCP OFFER with a DHCP REQUEST message

uint16_t i;

myIP=NullIP;

i=prepDHCP(&MashE.DHCP,DHCP_REQ);

launchUDP(&MashE,&BroadcastIP,DHCP_CLIENT_PORT,DHCP_SERVER_PORT,(i+240),NULL,0); 

MyState.IP=DHCP_WAIT_ACK;  // Move to next state

return;
}
// ----------------------------------------------------------------------------
static uint8_t processDHCPoption(uint8_t * DHCP_type)
{ // Handle a single DHCP option
// Note that options cannot extend past their own file section (sname, fname, options)

union {
  uint32_t IP4;
  uint8_t data[4];
} join;

uint8_t option_type=linkNextByte();
uint8_t length=linkNextByte();
uint8_t i;

switch (option_type) {

case (DHCP_OPT_END):
  if (!dhcp_option_overload) return (0);  // All done  
  if ((dhcp_option_overload>>4) == 0x02) return (0); // All done

  if (dhcp_option_overload & DHCP_FILE_OVERLOAD && (!(dhcp_option_overload&0x03))) {
    // At stage 0, but boot file data is overloaded
    linkReadRandomAccess(DHCP_BOOTFILE_OFFSET);
    dhcp_option_overload|=(DHCP_FILE_OVERLOAD<<4);
    return (1);
  }

  if (dhcp_option_overload & DHCP_SNAME_OVERLOAD) {
    // At stage 0 or 1 (because we returned for stage 2), but sname file data is overloaded
    linkReadRandomAccess(DHCP_SNAME_OFFSET);
    dhcp_option_overload|=(DHCP_SNAME_OVERLOAD<<4);
    return (1);
  }
  return (0);  // All done

case (DHCP_OPT_SUBNET_MASK): 
  linkReadBufferMemoryArray(4,&join.data[0]);
  subnetMask=join.IP4;
  return (1);

case (DHCP_OPT_ROUTER_IP): // Variable length option
  linkReadBufferMemoryArray(4,&join.data[0]);
  GWIP=join.IP4;
  for (i=4;i<length;i++) linkNextByte(); // Use first, discard rest
  return (1);

case (DHCP_OPT_DNS): // Variable length option
  linkReadBufferMemoryArray(4,&join.data[0]);
  for (i=4;i<length;i++) linkNextByte(); // Use first, discard rest

#ifdef USE_DNS
  DNSIP=join.IP4;
#endif  
  return (1);

case (DHCP_OPT_LEASE_TIME): 
  linkReadBufferMemoryArray(4,&DHCP_lease[0]);
  return (1);

case (DHCP_OPT_MSG_TYPE):
  *DHCP_type=linkNextByte();
  return (1);

case (DHCP_OPT_OVERLOAD):
  dhcp_option_overload=linkNextByte();
  return (1);

default:  // Something we don't handle
  for (i=0;i<length;i++) linkNextByte(); // Shuffle past
  return (2);
}
}
// ----------------------------------------------------------------------------
void handleDHCP(DHCP_message * DHCP)
{ // Handle a received DHCP message
uint8_t DHCP_type;
uint8_t i;

if (DHCP->XID!=MYID) return;  // Not for us

if ((MyState.IP != DHCP_WAIT_OFFER) && (MyState.IP != DHCP_WAIT_ACK)) return;  
// We must have asked

if (DHCP->op != 2) return;  // We expect a reply

for (i=0;i<6;i++) if (DHCP->MAC.MAC[i]!=myMAC.MAC[i]) return;

// Now done in driver : for (i=0;i<4;i++) if (DHCP->magic_cookie[i]!=magic_cookie[i]) return;

//i=0;
DHCP_type=0;

while (processDHCPoption(&DHCP_type)) { } // Move to next option done in loop

if ((DHCP_type == DHCP_OFFER) && (MyState.IP == DHCP_WAIT_OFFER))  { // Let's take up that offer

  myPreferredIP=DHCP->YIAddr;  // Listen to the offer
  MyState.IP=DHCP_SEND_REQ;  // Schedule a request

  return; 
}

if ((DHCP_type == DHCP_ACK) && (MyState.IP == DHCP_WAIT_ACK)) {

  MyState.IP = IP_SET;  // All done
  myIP=DHCP->YIAddr;

#ifdef USE_EEPROM
  //eeprom_write_block((uint8_t *)&myIP,(void *)EEPROM_MY_IP,sizeof(myIP));     
#endif

subnetBroadcastIP=((~subnetMask)|myIP);  
}
}
#endif // End of DHCP
// ----------------------------------------------------------------------------
uint16_t PreambleData(uint16_t start,uint16_t length,uint8_t * result) {
// TCP preamble.  Important bit is auto-insertion of content length
const static char head[] PROGMEM={"HTTP/1.1 200 OK\r\nConnection: close\r\nCache-control: no-cache,no-store\r\nContent-Length: "};
const static char tail[] PROGMEM={"\r\n\r\n"};

uint16_t i=0;

while (length--) {
  if (start<(sizeof(head)-1))  // -1 because sizeof includes trailing \0
    result[i++]=pgm_read_byte(&head[start]);
  else if (start>=(sizeof(head)-1+HTTP_NO_LEN))
    result[i++]=pgm_read_byte(&tail[start-(sizeof(head)-1+HTTP_NO_LEN)]);
  else if (start==(sizeof(head)-1))
    result[i++]=(expectLen/10000)%10+'0';
  else if (start==(sizeof(head)-0))
    result[i++]=(expectLen/1000)%10+'0';  
  else if (start==(sizeof(head)+1))
    result[i++]=(expectLen/100)%10+'0';
  else if (start==(sizeof(head)+2))
    result[i++]=(expectLen/10)%10+'0';
  else if (start==(sizeof(head)+3))
    result[i++]=(expectLen)%10+'0';  
  
  start++;
}
return (sizeof(head)-1+HTTP_NO_LEN+sizeof(tail)-1);
}
// ----------------------------------------------------------------------------
uint16_t PreambleDataCache(uint16_t start,uint16_t length,uint8_t * result) {

const static char head[] PROGMEM={"HTTP/1.1 200 OK\r\nConnection: close\r\nCache-control: max-age=2628000,public\r\nContent-Length: "};
const static char tail[] PROGMEM={"\r\n\r\n"};

uint16_t i=0;

while (length--) {
  if (start<(sizeof(head)-1))  // -1 because sizeof includes trailing \0
    result[i++]=pgm_read_byte(&head[start]);
  else if (start>=(sizeof(head)-1+HTTP_NO_LEN))
    result[i++]=pgm_read_byte(&tail[start-(sizeof(head)-1+HTTP_NO_LEN)]);
  else if (start==(sizeof(head)-1))
    result[i++]=(expectLen/10000)%10+'0';
  else if (start==(sizeof(head)-0))
    result[i++]=(expectLen/1000)%10+'0';
  else if (start==(sizeof(head)+1))
    result[i++]=(expectLen/100)%10+'0';
  else if (start==(sizeof(head)+2))
    result[i++]=(expectLen/10)%10+'0';
  else if (start==(sizeof(head)+3))
    result[i++]=(expectLen)%10+'0';  
  
  start++;
}
return (sizeof(head)-1+HTTP_NO_LEN+sizeof(tail)-1);
}
// ----------------------------------------------------------------------------
