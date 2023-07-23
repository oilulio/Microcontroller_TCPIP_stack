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

 Code for Application layer HTTP-GET
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
extern char hex[];

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
extern uint8_t fuseL,fuseH,fuseE;

extern uint8_t nonce;
extern uint8_t opaque;

static const char myhost[]=HOSTNAME; 

extern uint8_t dummy;
uint16_t expectLen;  // Special use - passing param to callback
uint8_t progress;    // Special use - passing param to callback

#ifdef HELLO_HTTP_WORLD

#ifdef USE_HTTP
uint8_t ringBuffer[MAX_RING_BUFFER];  
char bdry[70];  
uint8_t POSTflags=0;  // Note used as bit flags, so more than one may be set
uint8_t bufferPtr=0;
uint16_t bytesInPOST=0;  // Bytes left to read in POST body
uint8_t bdryLength=0;

uint16_t debug=16; // temp

#endif
// ----------------------------------------------------------------------------------
uint8_t caseFreeCompare(const char * s1,const char * s2, uint8_t len)
{
uint8_t i=0;
while (len--) {
  if (toupper(s1[i])!=toupper(s2[i])) return TRUE;
  i++;
} 
return FALSE;
}
// ----------------------------------------------------------------------------------
#ifdef USE_HTTP
uint16_t Progress(uint16_t start,uint16_t length,uint8_t * result) { // *** NOT TESTED/RIGHT WAY? *** TODO
static const char head[] PROGMEM={"<html><head><title>Progress</title></head><body><label for=\"p\""\
">Progress:</label><meter id=\"d\" value=\"0.60\"></meter></body></html>"};

uint16_t i=0;
  while ((length--) && start<sizeof(head)) {
	if (i==103) {
	  result[i++]='0'+(progress/10)%10;
	} else if (i==104) {
	  result[i++]='0'+progress%10;
	} else result[i++]=pgm_read_byte(&head[start]);
	start++;
  }
  return sizeof(head);  // sizeof(text)-1 should be right ... but doesn't work? TODO
}
// ----------------------------------------------------------------------------------
uint8_t ringBufferCompare(uint8_t ptr,char * c1,uint8_t len) {  // Respects ring
  while (len--) {
    if (ringBuffer[ptr]!=*(c1++)) return TRUE;
    ptr=(ptr+1)%MAX_RING_BUFFER;
  }
  return FALSE;
}
// ----------------------------------------------------------------------------------
void resetHTTPServer(void) 
{ // On SYN on server, make sure params are clear
POSTflags=0;
bufferPtr=0;
bytesInPOST=0;
bdryLength=0;
}
// ----------------------------------------------------------------------------------
void sendHTML(/*const*/ MergedPacket * Mash, uint16_t newData)
{ // Routine called sendHTML because we are server.  So if we are asked to do something in HTTP it will
  // be to reply (serve).  But we may not receive enough in
  // a given packet to go ahead and send, especially with POST.  But we will have ACK'd.

uint8_t authorisedPkt=TRUE;  // No auth, implies all packets are OK

if (!newData) return;  // Just an ACK

// We have a TCP packet with a certain payload length
// only the last 'newData' bytes have not been seen before (usually this will be whole payload)
uint16_t length=Mash->IP4.totalLength-(Mash->IP4.headerLength+Mash->TCP.headerLength)*4;

uint16_t offset=length-newData;  // Position of first new data byte
  
if (!strncmp("GET ",Mash->TCP_payload.chars,4)) { // It's a GET 

  if (Mash->TCP_payload.bytes[4]=='/') {  
// Seeking pattern "GET / ","GET \r","GET /index.html"
    if (Mash->TCP_payload.bytes[5]==' ' || Mash->TCP_payload.bytes[5]=='\r' ||
        !caseFreeCompare("index.html",&Mash->TCP_payload.chars[5],10)) {
      // Plain GET or GET/index.html 
      // Assume is it also possible to have "GET /\r\n" if there is no other header data.
        TCP_SimpleDataOut(PSTR("HTTP/1.1 200\r\nConnection: close\r\n\r\n<html><head><title>Hello world</title></head><body><h1>Hello World</h1></body></html>"),TCP_SERVER,TRUE);
      } 
    }
    else { SEND_404; }
} 
}
#endif
#endif
