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

 Code for Application layer HTTP-GET demonstration/reference design
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

extern MergedPacket MashE;

extern uint8_t dummy;
uint16_t expectLen;  // Special use - passing param to callback
uint8_t progress;    // Special use - passing param to callback

#ifdef HELLO_HTTP_WORLD

// ----------------------------------------------------------------------------------
#ifdef USE_HTTP
// ----------------------------------------------------------------------------------
void resetHTTPServer(void) { // On SYN on server, don any cleanup
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
      TCP_SimpleDataOutProgmem(PSTR("HTTP/1.1 200 OK\r\nConnection: keep-alive\r\n\r\n<html><head><title>Hello world</title></head><body><h1>Hello World</h1></body></html>\n"),TCP_SERVER,FALSE);
      TCP_FIN(Mash,TCP_SERVER);
    } 
    else { SEND_404; }
  }
  else { SEND_404; }
} 
}
#endif
#endif
