# Microcontroller-TCPIP-stack
A size-optimised stack for (primarily Atmel/Arduino) microcontrollers with UDP and TCP, and zero configuration networking.

Uses DHCP to obtain an IP address, or APIPA or a static IP address.

Is discoverable via zero-configuration protocols (LLMNR, mDNS).  

Supports ARP, limited ICMP (PING) and IP, UDP and TCP.  At the application layer, is a NTP client, a DNS client, and a HTTP client or server. 

Is capable of sending TCP packets larger than the 2k microcontroller memory (e.g. by constructing both the boilerplate and possibly the payload on the fly)
Is capable of receiving TCP packets also larger than the microcontroller memory by incrementally downloading them from the network card (which has typically 8k)

Functionality can be included/excluded at compile time to trade code size for capability

Explained in more detail at : https://wordpress.com/post/oilulio.wordpress.com/413

The core routines are the layers in the stack:

**LinkENC28J60.c** : Link layer specific to an ENC28J60 device.  Should not need to be altered for a new application.

**network.c**      : Network layer, also should not need to be altered.

**transport.c**    : Transport layer, just needs suitable routing to the application layer is a new protocol is used.  For instance the Power meter uses standard UDP packets which begin "POWE" and then conmtain the data.  This needs to be passed to a bespoke routine 'handlePower()'

```c
#ifdef POWER_METER // Message format is "POWER n" here n is an ASCII digit 0,1,2 etc
  if (Mash->UDP.destinationPort ==POWER_MY_PORT && 
	    Mash->UDP_payload.words[0] ==BYTESWAP16(0x504F) && // "PO"
	    Mash->UDP_payload.words[1] ==BYTESWAP16(0x5745))   // "WE" 
      {
        handlePower((Mash->UDP_payload.bytes[6]-0x30));  
      }
#endif
```
Whereas the standard protocol bindings should not need to change (e.g)
```c
#ifdef USE_DNS
  if  (Mash->UDP.destinationPort ==UDP_Port[DNS_CLIENT_PORT_REF] &&
       Mash->UDP.sourcePort      ==DNS_SERVER_PORT)   handleDNS(&Mash->DNS);  
#endif
```

**application.c** : This is where the majority of user changes are likely (although not in stock application layer routines like "queryNTP()" or "handleDNS()") whereas "sendHTML()" is the core routine for a server where it responds to an incoming request.

Some routines, e.g. **power.c**, **stepper.c** are bespoke to specific hardware finished products.

Some routines, e.g. **rtc.c**, **w25q.c** are optional for extra hardware modules used on various devices. 

Some routines, e.g. **lfsr.c**, **sha256.c** are optional general purpose software modules.

## To compile
Select the required solution in config.h.  There are 6 (mutually exclusive) examples at present.  Suitable code is selected if ATMEGA328 or ATMEGA32 macros are selected.
Other microcontrollers will need to be set up as required.

Macros must suit your hardware.  e.g. LEDON/LEDOFF may be:

```c
#define LEDOFF PORTB|=(1<<PORTB1)  // LED on PORT B1
#define LEDON  PORTB&=~(1<<PORTB1)
```
or may be ignored : `#define LEDON ()`

Generally MAC addesses should be specified and be unique.  Using 'LOCAL_ADMIN' means it need not be globally registered

```c
#define MAC_0  (LOCAL_ADMIN | 0x50)
#define MAC_1  (0x40)
#define MAC_2  (0x50)
#define MAC_3  (0x60)
#define MAC_4  (0x70)
#define MAC_5  (0x80)
```

Specific hardware needs to be intialised.  Conditionally called from **main.c**, e.g.

```c
#if defined MSF_CLOCK
  InitMSFclock();
#endif
```

A hostname should be set in **config.c**

And a suitable Init routine should be included in **init.c** (or a further file linked).   Again example initialisation routines for different scenarios exist in init.c.  They for instance set up the clock parameters for a given microcontroller; the SPI interface clock speed; or the IO status of pins as required for a given hardware configuration.

You will either need the hash function files (**sha1.h, sha1.c** etc) from https://github.com/oilulio/Microcontroller-hashes in the same directory, connected by symbolic links, or to comment out the requirement for them.  Having them included does not increase code size if they are not called.

Then run 'make' from the directory.  Files Net.elf, Net.hex will be created in the directory and object files in subdirectory obj.

Dowload Net.hex to the suitably configured microcontroller board with a suitable programmer and suitable fuse settings and the device should run.  If it is on a network it can usually be pinged : ping hostname
