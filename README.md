# Microcontroller-TCPIP-stack
A size-optimised stack for (primarily Atmel/Arduino) microcontrollers with UDP and TCP, and zero configuration networking.

Uses DHCP to obtain an IP address, or APIPA, or a static IP address.

Is discoverable via zero-configuration protocols (LLMNR, mDNS).  

Supports ARP, limited ICMP (PING) and IP, UDP and TCP.  At the application layer, is a NTP client, a DNS client, and a HTTP client or server. 

Is capable of sending TCP packets larger than the 2k microcontroller memory (e.g. by constructing both the boilerplate and possibly the payload on the fly)
Is capable of receiving TCP packets also larger than the microcontroller memory by incrementally downloading them from the network card (which has typically 8k)

Functionality can be included/excluded at compile time to trade code size for capability

Explained in more detail at : https://wordpress.com/post/oilulio.wordpress.com/413

The core routines are the layers in the stack:

**LinkENC28J60.c** : Link layer specific to an ENC28J60 device.  Should not need to be altered for a new application.

**network.c**      : Network layer, also should not need to be altered.

**transport.c**    : Transport layer, just needs suitable routing to the application layer if a new protocol is used.  For instance the Power meter uses standard UDP packets to a defined port, and which begin "POWE" and then contain the requested ADC channel.  This needs to be passed to a bespoke routine 'handlePower()'

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
Select the required solution in **config.h**.  There are 6 (mutually exclusive) examples at present.  Suitable code is selected if ATMEGA328 or ATMEGA32 macros are selected.
Other microcontrollers will need to be set up as required.

Macros must suit your hardware.  e.g. LEDON/LEDOFF may be:

```c
#define LEDOFF PORTB|=(1<<PORTB1)  // LED on PORT B1
#define LEDON  PORTB&=~(1<<PORTB1)
```
or may be ignored : `#define LEDON {}`.  Having a working LED is really useful for debugging a microcontroller though!

Generally MAC addesses should be specified and be unique.  Setting the LOCAL_ADMIN bit means it need not be globally registered

```c
#define MAC_0  (LOCAL_ADMIN | 0x50)
#define MAC_1  (0x40)
#define MAC_2  (0x50)
#define MAC_3  (0x60)
#define MAC_4  (0x70)
#define MAC_5  (0x80)
```
Specific protocols are enabled by uncommenting macros.  If a protocol needs TCP, the macro code will automatically enable TCP or vice-versa.
```c
//#define USE_SMTP         // TCP 
//#define USE_POP3         // TCP
  #define USE_DNS          
//#define USE_NTP          // Usually off when debugging to avoid flooding
  #define USE_HTTP         // TCP
  #define USE_mDNS        
  #define USE_LLMNR         
  #define IMPLEMENT_PING     // Useful unless space critical
```
Specific hardware needs to be intialised.  Conditionally called from **main.c**, e.g.

```c
#if defined MSF_CLOCK
  InitMSFclock();
#endif
```

The macro `#define LITTLEENDIAN` is normal for Atmel microcontrollers.  Unsetting it should work for other big-endian CPUs but is **untested**.

The macro `#define PROTECT_TCP` enables rejection of packets with malformed flags.  e.g. Can't have FIN set and ACK not.  Adds very little code, but is optional.

A hostname should be set in **config.h** e.g. `#define HOSTNAME "iot-isp"` for the IoT In-System Programmer.

And a suitable Init routine should be included in **init.c** (or a further file linked).   Again example initialisation routines for different scenarios exist in init.c.  They for instance set up the clock parameters for a given microcontroller; the SPI interface clock speed; or the IO status of pins as required for a given hardware configuration.

You will either need the hash function files (**sha1.h, sha1.c** etc) from https://github.com/oilulio/Microcontroller-hashes in the same directory, connected by symbolic links, or to comment out the requirement for them.  Having them included does not increase code size if they are not called.  They are ready for future enhancements for RFC 7616 authorisation.

Then run `make` from the directory.  Files Net.elf, Net.hex will be created in the directory and object files in subdirectory obj.  `make install` will go further and download the code to the microcontroller (see Makefile - this assumes you have AVRDude installed in certain directories and working on a specified COM port)

Dowload Net.hex to the suitably configured microcontroller board with a suitable programmer and suitable fuse settings and the device should run.  If it is on a LAN it can usually be pinged : `ping hostname` from Windows, `ping hostname.local` from linux.

## Hints and Tips

It is difficult to debug a distributed device which depends on message exchange with servers (e.g. DHCP servers) and which has virtually no human-readable output beyond perhaps a LED.

To facilitate some debugging, the user of a Network sniffer such as Wireshark is valuable.  That will help confirm normal operation and highlight errors.

Then it is possible to check that a particular location in the code is reached after a given condition (e.g. HTTP request) and to extract some data by using a generic UDP broadcast e.g.

```c
buffer[0]='A'; // Any data to be extracted, bytewise input
buffer[1]='B';
buffer[2]='C';
buffer[3]='D';
buffer[4]='E';
buffer[5]='F';

genericUDPBcast((uint16_t *)buffer,6); // 6 is the payload length
``` 

buffer is a permanent general purpose scratch pad, accessed as required by `extern char buffer[MSG_LENGTH];`  MSG_LENGTH defined in config.h

Wireshark (or similar) should then see a UDP packet from the device's IP address, sent to the LAN broadcast 255.255.255.255, and containing the data.

There is a similar genericUDP() function that will send to a specific IP address.  However that address must exist on the LAN or the automatic ARP preceding the send will fail.  With a suitable listener on that machine, significant data can be extracted.  This was partly how the hash routines were tested.

The need to interleave #ifdefs through the code to select specific protocols or designs is cumbersome and switching protocols in and out of use makes testing difficult and there is a risk that creating a new device breaks an old one.

## Reference Designs

An issue with Microcontroller software is that it can run on multiple devices (e.g. different versons of the MCU, such as Atmega8 and Atmega328p) with different fuse settings and hardware configurations.  This means the complete specification of a working design involves the software(firmware), the fuse settings and the hardware design (The same MCU may also appear in different physical packages (e.g. PDIP, TQFP)) These designs are references, however other designs may also work.

The following are reference working designs to match the Microcontroller TCPIP stack software.

These are provided in good faith based on my notes, but cannot be guaranteed not to contain errors. 

### Power meter design

This is exceptionally simple.  The reference design is for an Atmega 328p in a PDIP package, although it is likely a simpler Atmega 8 will work.

The design is nothing more than a Atmega328p on a board with necessary power smoothing and reset circuitry.  The only other connections are:

```
SPI SCK on MCU <-> SCK on ENC28J60 board

SPI MOSI on MCU <-> MOSI on ENC28J60 board

SPI MISO on MCU <-> MISO on ENC28J60 board

SPI CS on MCU <-> CS on ENC28J60 board
```

And clearly the MCU and the ENC28J60 need power and earth.

In addition, the clock input (PB6) on the MCU is fed from the ENC28J60, so no crystal is needed.  This means the fuses must reflect an external crystal source, and F_CPU is set as a divisor of that feed.  The file config.h reflects this.

This is not necessary if the MCU board has its own clock crystal, but then fuses and config.h need to change.

Then a (suitable bounded (e.g. 0-VCC)) input should feed the ADC0 (PC0) on the MCU.  This is pin 23 on the PDIP package.

ADC1 and ADC2 can also be used.  The software assumes that the feed repeats over a 50Hz cycle, but that is easily changed.

Fuses used : L 0xFF, H 0xD9, E 0xFF.

### Net programmer

This is a more complex design, but still relatively simple.  An Arduino Pro Mini 328P, with integrated 16MHz crystal, is used.

As before:

```
SPI SCK on MCU <-> SCK on ENC28J60 board

SPI MOSI on MCU <-> MOSI on ENC28J60 board

SPI MISO on MCU <-> MISO on ENC28J60 board

SPI CS on MCU <-> CS on ENC28J60 board
```

And clearly the MCU and the ENC28J60 need power and earth.

In addition, the 'to be programmed' MCU needs to be connected.  This is via the pins defined in config.h.  Currently:

All are on Port D, as defined by ISP_SEL_PORT

```
MOSI = PD3 (ISP_CONTROL_MOSI)

MISO = PD7 (ISP_CONTROL_MISO)

SCK  = PD6 (ISP_CONTROL_SCK)

And CS is PD5 (ISP_SEL_PORT/ISP_SPI_SEL_CS)
```

Fuses used : L 0xDE, H 0xD6, E 0xFD.

The EEPROM must be programmed with the database of target MCUs.  The above fuses will ensure it is not erased on flashing.

While a work in progress, it is expected the programmer will also have SPI connections to either its own flash memory (W25Q) or RAM (Microchip 23 series SRAM)


