// *********************************************************************************
// *********** Main routine for Microcontroller network devices ********************
// *********************************************************************************
/*
Copyright (C) 2009-20  S Combes

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
    
// ----------- Current capabilities -------------------------------------------
// 
// Data Link Layer   : Ethernet only.  Either ENC28J60 or an ISA card.  ARP.
// Network Layer     : ICMP and IP4 only (some IP6 structures ready, but IP6
// likely to be a challenge with need for large packets and encryption)
// Transport Layer   : UDP and TCP
// Application Layer : NTP (client), DHCP (client), HTTP (client and server),
// DNS (client), mDNS (responder), LLMNR (responder)
//
// -------------- Description --------------------------------------------------
//
// On boot, will use DHCP to obtain IP address.  If DHCP does not respond in ~10s,
// will select IP address from 169.254.x.y private subnet.  In this case will use 
// ARP to ensure address is not already in use
//
// ICMP : Will respond to PINGs if IMPLEMENT_PING is set on build
//
// ARP : Will respond to requests for own MAC.  Will listen to other requests
// to populate IP<->MAC cache.  Before send, if address not in cache, will
// use ARP to obtain address (send and listen).  Will HANG if no response.
//
// UDP : full implementation, including checksums
//
// TCP.
//
// NTP.  If USE_NTP is set on build, will use NTP to obtain time
//
// DNS.  Obtains IP address from test.  Rudimentary implementation
//
// HTTP. Rudimentary stream.  GET and POST.
//
// ------- Specific physical implementations ------------------
//
// NETWORK_CONSOLE.  A device that has a LCD display and can use e,g. HTTP to get
// useful information debug code. This is a homemade board interfacing with a computer ISA 
// network card.  Uses Atmega32
//
// POWER_METER.  A UDP server which responds with the current and waveform sampled 
// from an AC clamp meter.  This is a homemade board using an ENC28J60 module from 
// and an ATMega-328.  See https://oilulio.wordpress.com/2016/01/01/iot-clamp-meter/
// 
// MSF_CLOCK. A UDP/NTP server that responds with time from a Rugby MSF clock 
// receiver.  This is built on a tuxgraphics ethernet board with an ENC28J60 
// and an ATMega-328 
//
// NETWORK PROGRAMMER.  A Arduino Pro Mini and an ENC28J60 backed by SPI RAM
// operating as a webserver and Atmel microcontroller programmer
//
// config.h configures the build for the specific hardware for each of these
//
// ********************************************************************************

#include <avr/io.h>
#include "config.h"

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <avr/eeprom.h>
// TODO #include <avr/signature.h>   // Puts signature in ELF file, but avrdude error
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/wdt.h>
 
#include "network.h"
#include "application.h"
#include "where.h"
#include "lfsr.h"
#include "link.h"
#include "isp.h"
#include "init.h"
#include "w25q.h"
#include "mem23SRAM.h"

// For combined hex file see
// https://www.kanda.com/blog/microcontrollers/avr-microcontrollers/atmel-studio-elf-production-files-avr/

#ifdef NET_PROG
/*FUSES = // For Atmega328p
{
	.extended = 0xFD,
	.high     = 0xD6, 
	.low      = 0xDE
};
LOCKBITS =  (LB_MODE_1 & BLB0_MODE_1 & BLB1_MODE_1); // 0xFF = no protection */


typedef struct { // Microcontrollers that can be programmed, stored in EEPROM
  char         name[LONGEST_MICRO];
  ISP_chipData data;
} ucData;


  uint8_t vendor;
  uint8_t partFamily;
  uint8_t partCode;  // Signature bytes
  uint16_t sizeOfFlash;
  uint16_t flashPageSize;
  uint16_t sizeOfEEPROM;

/*
//                                                012345678901234  => LONGEST_MICRO=13 (incl /0)
__attribute__((section(".eeprom"))) char name0[]="ATMega48p   ";  // Not tested, but should work by analogy Mega328p
__attribute__((section(".eeprom"))) uint8_t  dat8_0 [] ={0x1E,0x92,0x0A};
__attribute__((section(".eeprom"))) uint16_t dat16_0[] ={0x800,0x40,0x100};

__attribute__((section(".eeprom"))) char name1[]="ATMega88p   ";  // Not tested, but should work by analogy Mega328p
__attribute__((section(".eeprom"))) uint8_t  dat8_1 [] ={0x1E,0x93,0x0F};
__attribute__((section(".eeprom"))) uint16_t dat16_1[] ={0x1000,0x40,0x200};

__attribute__((section(".eeprom"))) char name2[]="ATMega168p  ";  // Not tested, but should work by analogy Mega328p
__attribute__((section(".eeprom"))) uint8_t  dat8_2 [] ={0x1E,0x94,0x0B};
__attribute__((section(".eeprom"))) uint16_t dat16_2[] ={0x2000,0x80,0x200};

__attribute__((section(".eeprom"))) char name3[]="ATMega328p  ";
__attribute__((section(".eeprom"))) uint8_t  dat8_3 [] ={0x1E,0x95,0x0F};
__attribute__((section(".eeprom"))) uint16_t dat16_3[] ={0x8000,0x80,0x400};

__attribute__((section(".eeprom"))) char name4[]="ATMega8      ";  // Not tested, probably won't work as different signals
__attribute__((section(".eeprom"))) uint8_t  dat8_4 [] ={0x1E,0x93,0x07};
__attribute__((section(".eeprom"))) uint16_t dat16_4[] ={0x1000,0x40,0x200};
*/
// KNOWN_PROGS=5 (in config.h)

// note line below: eeprom_read_block((void*)buffer , (const void*)name0, 10); // Dummy to
// prevent optimiser removing the eeprom data!

#endif

// EEMEM is defined in avr/eeprom.h as "• #define EEMEM __attribute__((section(".eeprom")))"

//__attribute__((section(".eeprom"))) char hello[]="Hello World";
//EEMEM char hello[]="Hello World";  // Populates the .eeprom
//EEMEM int  test = 12345;

const MAC_address BroadcastMAC={.MAC={0xff,0xff,0xff,0xff,0xff,0xff}};
const IP4_address NullIP=0;
const IP4_address BroadcastIP=0xFFFFFFFF; 

#if defined USE_ENC28J60 
  const MAC_address myMAC={.MAC={MAC_0,MAC_1,MAC_2,MAC_3,MAC_4,MAC_5}};
#endif
#ifdef ISA_SPI
  MAC_address myMAC={.MAC={0,0,0,0,0,0}}; // The ISA card has its own MAC
#endif

IP4_address GWIP;    // Gateway default
#ifdef USE_DNS
  IP4_address DNSIP; 
#endif

IP4_address NTPIP=0; // Start null
IP4_address subnetMask; 
IP4_address subnetBroadcastIP;

char buffer[MSG_LENGTH]; // messages
uint8_t DHCP_lease[4];
volatile uint8_t timecount;

#ifdef USE_FTP
IP4_address FTP_IP=MAKEIP4(192,168,0,210);
uint8_t FTP_status=FTP_CLOSED; // Start here ...
uint8_t FTP_target=FTP_CLOSED; // ... and stay there
uint16_t FTP_passive_port;
FTP_job FTP_queue;
#endif

Status MyState;
MergedPacket MashE; // Ephemeral Mash. Used for any input, UDP and initial TCP

uint32_t time_now;                // Secs since FIRSTJAN2000_0000_UT
uint32_t protect_time, time_set;  // Protect time allows clock when idle

IP4_address myIP; 

uint16_t lfsr=0xACE1u;  // Actually want random init, get later from clock
#ifdef WHEREABOUTS
uint8_t minute,hour;
//uint8_t hstate=HAND_STOP,mstate=HAND_STOP;  // State for hour and minute hands
uint8_t started=FALSE;
uint8_t gis=0,ris=0,fis=0;  // Locations of GRF (0-11).  Start at 1200/Mortal Peril
uint8_t lastMode;
uint8_t deferral=0;  // seconds to slip to allow time to catch up
IP4_address WHEREABOUTSIP=MAKEIP4(192,168,0,210);     // Testing
uint32_t gotweb=0;
#endif

// ----------------------------------------------------------------------------
#if defined NETWORK_CONSOLE || defined MSF_CLOCK || defined WHEREABOUTS || defined HOUSE || defined NET_PROG
ISR(TIMER0_OVF_vect)
{  //CPU automatically calls when TIMER0 overflows.

//  char * timereport;
//  TimeAndDate timeanddate;

   TCNT0 = (TIME_START);  // fine tuning

   //Increment our variable
   timecount++;

   if(timecount>=205) {    // Tick over 1s
      timecount=0;
      time_now++;
	  
#ifdef WHEREABOUTS
      if (deferral) deferral--;   
      uint8_t sec=time_now%60;
      if ((!sec)) {
        if ((++minute)>=60) {
          minute=0;
          if (hour==23) hour=0;
          else hour++;

          if (hour==2) { // At 2AM re-parse time - will sweep up any daylight saving change.
            TimeAndDate tad=parseTime(time_now);
            hour=tad.hour;
            minute=tad.minute;
          }
        }
      }
      if ((sec%15)==0 && (((~PIND) & MODE_TIME) && started)) {
        //htarget=((uint16_t)((((hour%12)+minute/60.0+sec/3600.0)*(float)A360)/12.0));
        //mtarget=((uint16_t)((minute+sec/60.0)*((float)B360)/60.0));
/*
        EnableMotors();
        hstate=HAND_RUN;
        mstate=HAND_RUN;
*/
      }
#endif
//      if (MyState.TIME==TIME_SET && ((time_now-protect_time)> 10)) Display_Time(time_now);
      // protect_time allows message to be shown, then drop back to clock.

      refreshMACList();

//      Display_Time(time_now);

/*
if (MyState.IP==IP_SET && ((time_now & 5)==0))
{
timereport=malloc(80); // 
timeanddate=ParseTime(time_now);
LineOne(&timeanddate);
buffer[18]='\0';
strcpy(timereport,buffer);
LineTwo(&timeanddate);
buffer[18]='\0';
strcpy(&timereport[18],buffer);
timereport[36]="\r";
timereport[37]="\n";
timereport[38]="\0";

//Queue_for_FTP(APPE,"time.txt\r\n",timereport);
free(timereport);
}
*/

#ifdef USE_TCP
      countdownTCP();  // Called 1 per sec.  Will determine whether retx ready.
#endif
   }
}
#endif
// ----------------------------------------------------------------------------
#ifdef DEBUGGER // Delay macros - null when debugger running
void delay_ms(uint16_t ms) { return; }
#else 
void delay_ms(uint16_t ms) { while(ms) { _delay_ms(DELAY_CALIBRATE);  ms--; }}
#endif
// ----------------------------------------------------------------------------
int main(void)
{
// Note that the ENC28J60 has 8191 bytes (1FFFh) memory (allows 14 x 576)
// However needs to be shared between Tx and Rx buffers.

uint8_t iPOP,iCount2=0;
//uint8_t retryNTP=7;
uint16_t iCount=0;


while (TRUE) {   // Lets us do resets

// TODO wdt_enable(WDTO_4S);  // Watchdog at 4s

// Do appropriate initialisations
#if defined MSF_CLOCK
  InitMSFclock();
#elif defined POWER_METER
  InitPower();
#elif defined NETWORK_CONSOLE
  InitConsole();
#elif defined HOUSE
  InitHouse();
#elif defined WHEREABOUTS
  lastMode=MODE_OFF; 
  delay_ms(400);
  InitWhereabouts();  // includes linkInitialise(myMAC) 
  
  delay_ms(300);

//  if (SWITCHED_OFF) enc28j60powerDown();
//  while (SWITCHED_OFF) {}  // Wait here

  //enc28j60powerUp(); // won't start with this if net not present, 
  // but needed if was shutdown in power down mode  ...

/*
#define START_HR (13)  // Defaults in absence of LAN.  Mainly for testing.
#define START_MIN (55)
  time_now=START_HR*((uint32_t)3600)+START_MIN*((uint32_t)60.0);
  TimeAndDate tad=ParseTime(time_now);
  hour=tad.hour;
  minute=tad.minute;
*/
  started=TRUE;
#elif defined NET_PROG
  InitProgrammer();
  // TODO eeprom_read_block((void*)buffer , (const void*)name0, 10); // Dummy to
  // prevent optimiser removing the eeprom data!
#endif

#ifdef STATIC_IP
myIP=MAKEIP4(OCT0,OCT1,OCT2,OCT3); 
GWIP=MAKEIP4(OCT0,OCT1,OCT2,1); 
subnetMask=MAKEIP4(0xFF,0xFF,0xFF,0); 
MyState.IP=IP_SET;
#else
MyState.IP=DHCP_IDLE;  // Initialise
#endif

MyState.TIME=TIME_UNSET;
#ifdef USE_FTP
FTP_queue.command=NONE;
#endif

delay_ms(150);
        
#ifdef MSF_CLOCK
TimeAndDate timeanddate;
//  time_now=ReadRTC());
#endif

subnetBroadcastIP=((~subnetMask)|myIP);  

#ifdef USE_EEPROM
InitEEPROM();
#endif

initLFSR(); // Random based on elapsed time
	
#ifdef USE_TCP
initialiseTCP();
#endif

#ifdef USE_DHCP
initiateDHCP();  // Send DHCP DISCOVER
#else
iCount2=6;       // Straight to APIPA, or STATIC
#endif


iPOP=0;

//Queue_for_FTP(APPE,"FTP4test.txt\r\n","The train in Spain\r\n");

uint8_t begun=FALSE;
// ...........................................................................
while (TRUE) {  // Preparation complete - now stay in main loop (can break out to reset)

iCount++;
asm("WDR");  // watchdog

// TRANSMIT : DHCP gets priority --------------------------------

#ifdef USE_DHCP
if (MyState.IP==DHCP_SEND_REQ) requestDHCP();  // Have OFFER, so request 
#endif

#ifdef USE_TCP
retxTCP();
asm("WDR");  // watchdog as retx could take time
#endif

// RECEIVE ------------------------------------------------------

handlePacket();

#ifdef MSF_CLOCK  
    if (PINB & (1<<0)) LEDON;
    else LEDOFF;
#endif

if (!(iCount%2000))  // Only check occasionally
{

#if defined WHEREABOUTS

    if (SWITCHED_OFF) {  // Can only just have happened
      //hstate=HAND_STOP;  // Prevents interrupt changing things
      //mstate=HAND_STOP;
      //DisableMotors();
//      enc28j60powerDown();
      lastMode=MODE_OFF;
      break;  // Out of established loop
    }

    if (((~PINB) & MODE_DEMO)) {
      if (lastMode!=MODE_DEMO) { 
        DemoMode(HOUR_HAND|MIN_HAND|SEC_HAND);  
        lastMode=MODE_DEMO;
      }
    }
    else if (((~PIND) & MODE_TIME)) {
      if (lastMode!=MODE_TIME) {
        TimeMode(HOUR_HAND|MIN_HAND|SEC_HAND);  
        lastMode=MODE_TIME;
      } 
    } 
    else if (((~PIND) & MODE_LAN)) {
      if (lastMode!=MODE_LAN) {
        Position(HOUR_HAND,gis);
        //Position(HOUR_HAND,gis); 
        //Position(HOUR_HAND,gis);
        lastMode=MODE_LAN;
      } // In flight changes done by handler
    }
    else if (((~PIND) & MODE_WEB)) {
      if (lastMode!=MODE_WEB) {

        if (lastMode==MODE_WEB_UPDATE) { // prevents repeated tests 
          //htarget=(uint16_t)(gis*(float)A360/12.0);
          //mtarget=(uint16_t)(ris*(float)B360/12.0);
        } else {
          gotweb=time_now-1000; // Force update
          //htarget=0; // Until we know better
          //mtarget=0;
        }
        lastMode=MODE_WEB;
      } // In flight changes done by HandleUDP
      if ((MyState.IP == IP_SET) && (time_now-gotweb)>WEB_REFRESH_SECS) {  
        gotweb=time_now;
        launchWhereabouts();
      }
    }
#endif

  if (MyState.IP == IP_SET) { // Everything else should happen in this loop

#ifdef USE_FTP
    if (FTP_target < FTP_READY) FTP_target=FTP_READY;  
    // TODO once have IP address, setup FTP, but only if not already past this
    
    FTP_Update(); // Get things moving.
#endif

    if (!begun) {
      //uint8_t mb=w25ReadSizeMB();
      /*buffer[0]='M';
      buffer[1]='B';
      buffer[2]=mb;
      genericUDPBcast((uint16_t *)buffer,5);  // TODO Debug*/
      
      /*buffer[0]='H';
      buffer[1]='S';
      buffer[2]='T';
      buffer[3]='=';
      static const char myhost[20]="iot-isp";// PROGMEM=HOSTNAME;
      
      uint32_t address=0;
      setMemSequentialMode();
      
      memReadBufferMemoryArray(address,50,(uint8_t *)&buffer[3]);
      //for (uint16_t q=0;q<50;q++) { buffer[q+3]=memReadByte(address++); }
      
      for (int q=0;q<strlen(myhost);q++) buffer[4+q]=myhost[q];
      
      genericUDPBcast((uint16_t *)buffer,26);  // TODO */
      
 
#ifdef USE_MD5
      //MDTestSuite();
#endif      
      
      begun=TRUE;
    }
    //GET_HTTP();  // testing

#ifdef USE_NTP
/*
TODO ****    if (MyState.TIME==TIME_WAIT_DNS) {
      if (((time_now&0x000000ff)%retryNTP)==0 && nextLFSR() && nextLFSR()) {  // Every 7 s, 1 in 4 chance of resend (but visits ~4x per sec)
        retryNTP+=3; // Wraps at 256
        NTPIP=0;  // Force new DNS choice - maybe old was dud (small risk we've only just sent)
        MyState.TIME=TIME_UNSET;
        // Make sure we don't stay in 'time requested' indefinitely if no reply.
      } 
    }
*/
    if (MyState.TIME!=TIME_SET && MyState.TIME!=TIME_REQUESTED) { 
      queryNTP();  // Either launches a DNS or, if already know IP, a NTP packet
    } 
    else if (MyState.TIME==TIME_SET && 
#ifdef WHEREABOUTS // Not sure - minute may be used by others?
                 minute==0 &&
#endif
             ((time_now-time_set) > DAY_IN_SECONDS/25)) {

//    else if (MyState.TIME==TIME_SET && (hour%12)==6 && 
//             ((time_now-time_set) > DAY_IN_SECONDS/4)) {
      MyState.TIME=TIME_UNSET;
      NTPIP=0; // Renew the NTP at 0600 and 1800 (enforcing new DNS) if not recently done.
    }
#endif
  } 
#ifdef USE_APIPA
  if ((!iCount) && MyState.IP!=IP_SET) {
  // iCount has done complete cycle without DHCP succeeding ( happens after about 2s), 
  // Allow 5 of these, and then decide to use APIPA (i.e. 169.254.x.y) 
  iCount2++;
  if ((iCount2>=5) || (MyState.IP==APIPA_FAIL)) // Don't wait more if APIPA has failed for current IP
  {
    if (MyState.IP==APIPA_TRY) // We have probed for an APIPA address and heard nothing
	  {
  	  MyState.IP=IP_SET; // Complete circuit, no ARP heard, so we're happy to use	
  	}	else {  // Send ARP to 'self' to ensure we have a vacant IP
  	  if (MyState.IP!=APIPA_FAIL) { // First time 
        uint8_t octet=1+(myMAC.MAC[0]+myMAC.MAC[1]+myMAC.MAC[2]+  // Reduce chance of conflict
                         myMAC.MAC[3]+myMAC.MAC[4]+myMAC.MAC[5])%252;  // 1 to 252 incl

        myIP=MAKEIP4(169,254,0,octet); 
  	    // Update the subnet broadcast address
	      subnetMask=MAKEIP4(0xFF,0xFF,0,0);
        subnetBroadcastIP=((~subnetMask)|myIP);
      } 
      // When IP==APIPA_FAIL, myIP (proposed) will have been updated (in network.c)
      MashE.ARP.type=ARP_REQUEST;  
      copyMAC(&MashE.ARP.destinationMAC,&BroadcastMAC);
  	  copyIP4(&MashE.ARP.destinationIP,&myIP);
      MashE.ARP.hardware=DLLisETHERNET;  
      MashE.ARP.protocol=ARPforIP;  	
      MashE.ARP.hardware_size=6;  	// Length of MAC
      MashE.ARP.protocol_size=sizeof(myIP);	
      copyMAC(&MashE.ARP.sourceMAC,&myMAC);
      copyIP4(&MashE.ARP.sourceIP,&myIP);
 
      launchARP((MergedARP *)&MashE);
      MyState.IP=APIPA_TRY; // Will now trigger every time as iCount2>5
    }
  } 
#ifdef USE_DHCP
  else initiateDHCP();  // Try again
#endif
}
#else
#ifdef USE_DHCP
if ((!iCount) && MyState.IP!=IP_SET) {
  // iCount has done complete cycle without DHCP succeeding ( happens after about 2s), 
  initiateDHCP();   // Try again
}
#endif
#endif
  if ((MyState.IP == IP_SET) )//&& iPOP==0)
  {
#ifdef USE_TCP
// Testing only
    //if (TCB[TCP_CLIENT].status == TCP_CLOSED && (iPOP==0)) {
      // TODO   Initiate_POP3();
      // Query_Domain_Name(&MashOut,"www.yahoo.co.uk ");
      iPOP=1;
    //}
#endif
  }
}
}
}
return 0;
}
// ----------------------------------------------------------------------------
void IP4toBuffer(IP4_address IP) {
  sprintf(buffer,"%d.%d.%d.%d",OCTET1(IP),OCTET2(IP),OCTET3(IP),OCTET4(IP));
// ----------------------------------------------------------------------------
}


