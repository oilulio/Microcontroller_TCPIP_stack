# Microcontroller-TCPIP-stack
A size-optimised stack for (primarily Atmel/Arduino) microcontrollers with UDP and TCP, and zero configuration networking.

Uses DHCP to obtain an IP address, or APIPA or a static IP address.

Is discoverable via zero-configuration protocols (LLMNR, mDNS).  

Supports ARP, limited ICMP (PING) and IP, UDP and TCP.  At the application layer, is a NTP client, a DNS client, and a HTTP client or server. 

Is capable of sending TCP packets larger than the 2k microcontroller memory (e.g. by constructing both the boilerplate and possibly the payload on the fly)
Is capable of receiving TCP packets also larger than the microcontroller memory by incrementally downloading them from the network card (which has typically 8k)

Functionality can be included/excluded at compile time to trade code size for capability

To compile select the required solution in config.h.  There are 6 (mutually exclusive) examples at present.  Suitable code is selected if ATMEGA328 or ATMEGA32 macros are selected.
Other microcontrollers will need to be set up as required.

Macros must suit your hardware.  e.g. LEDON/LEDOFF may be:

  #define LEDOFF PORTB|=(1<<PORTB1)  // LED on PORT B1

  #define LEDON  PORTB&=~(1<<PORTB1)

or may be ignored : #define LEDON ()

Generally MAC addesses should be specified and be unique.  Using 'LOCAL_ADMIN' means it need not be globally registered

  #define MAC_0  (LOCAL_ADMIN | 0x50)   
  
  #define MAC_1  (0x40)  
  
  #define MAC_2  (0x52)  
  
  #define MAC_3  (0x60)
  
  #define MAC_4  (0x70)
  
  #define MAC_5  (0x80)

Specific hardware needs to be intialised.  Conditionally called from main.c, e.g.

#if defined MSF_CLOCK

  InitMSFclock();

#endif

A hostname should be set in config.c

And a suitable Init routine should be included in init.c (or a further file liked).   Again example initialisation routines for different scenarios exist in init.c

You will either need the hash function files (sha1.h, sha1.c etc) from https://github.com/oilulio/Microcontroller-hashes in the same directory, connected by symbolic links, or to comment out the requirement for them.  Having them included does not increase code size if they are not called.

Then run 'make' from the directory.  Files Net.elf, Net.hex will be created in the directory and object files in subdirectory obj.

Dowload Net.hex to the suitably configured microcontroller board with a suitable programmer and suitable fuse settings and the device should run.  If it is on a network it can usually be pinged : ping hostname
