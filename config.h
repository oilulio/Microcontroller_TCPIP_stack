// This file configures the build for a TCP/IP-based application

// It SHOULD BE INCLUDED in all .c files


#define str(s) #s    // Stringification of tokens, see C preprocessor Wikipedia
#define xstr(s) str(s)

#define LITTLEENDIAN      // For AVR devices

// ----------- User settable ------ ONE only

#define HELLO_HTTP_WORLD    // Minimal configuration for webserver
//#define POWER_METER       // Set if hardware is the Clamp Meter
//#define NETWORK_CONSOLE   // Hardware is console box
//#define MSF_CLOCK         // When wired as a MSF clock on a Tuxgraphics board
//#define NET_PROG          // When a network programmer
//#define WHEREABOUTS       // Autoclock
//#define HOUSE 

//#define USE_EEPROM  

//#define VERBOSE       // More LCD comments
//#define DEBUGGER      // Mainly removes LCD commands
//#define REGRESS       // Perform regression tests
//#define STATS          // Record statistics


#define MSG_LENGTH  (68)  // Save space by using same char everywhere
// N.B.  MD5 requires MSG_LENGTH >=68 **********
// Optimise by making device specific

// **************** NOT user settable below ********

#define LOCAL_ADMIN   (0x02) 
// Unset LSb of 1st byte : Unicast; 2nd LS bit of 1st byte : set = Locally adminstered
// Hence 0x02 is Local and 0x01 is Multicast 


#ifdef POWER_METER  //****************************************************************
// An ATMega328-based device with an ENC28J60 chip 

#define HOSTNAME ("powermeter")  // Length automatic

  #define MYID              (21640)
  #define POWER_MY_PORT     (15123)
  #define ATMEGA328     
  #define USE_ENC28J60     // Set this if using the ENC28J60 chip (converse is ISA)
  #define CLK_DIVISOR (2)  // Usually 2 (12.5MHz) or 3 (8MHz); see also ONE_CYCLE, DELAY_CALIBRATE
  #define F_CPU  ((25000000UL)/CLK_DIVISOR)  // 25 MHz divided by ...
  // This defines the ratio of the ENC28J60 clock (25Mhz) to the ATMega-328 (e.g. 12.5MHz)

  #define DELAY_CALIBRATE (1.0) // Depends on clock. 1.0 correct for power meter at 12.5MHz

  #define LEDOFF PORTB|=(1<<PORTB1)  // Which port is our LED on?
  #define LEDON  PORTB&=~(1<<PORTB1)

  #define USE_APIPA   // We are useful even without a DHCP server

  #define USE_DHCP          
//#define USE_SMTP         // TCP 
//#define USE_POP3         // TCP
//#define USE_DNS          
//#define USE_NTP          
  #define IMPLEMENT_PING      // Useful unless space critical

  #define MAC_0  (LOCAL_ADMIN | 0)
  #define MAC_1  (0x01)
  #define MAC_2  (0x03)
  #define MAC_3  (0x05)
  #define MAC_4  (0x07)
  #define MAC_5  (0x09)

  #define MAX_TIME_SAMPLES (188) // Number of points to transmit (NB = 16 bits each)
  #define CYCLE   (188)  // integer number of cycles of 50Hz - N.B. f'n of clock etc

#endif

#ifdef MSF_CLOCK  //****************************************************************

#define HOSTNAME "msfclock"  // Length automatic
  #define MYID  (21641)

  #define ATMEGA328     
  #define CLK_DIVISOR (3)  // Usually 2 (12.5MHz) or 3 (8MHz): 3 best to keep within spec
  #define TWI_SCALE   (32) // 100kHz TWI from CPU/(16+2*n).  Hence n=32 for 8 MHz and 55 (inexact, not tested) for 12.5 MHz
  #define USE_ENC28J60      // Set this if using the ENC28J60 chip (converse is ISA)
  #define F_CPU  ((25000000UL)/CLK_DIVISOR)  // 25 MHz divided by ...
  #define RTC           // Real Time Clock Present
//  #define USE_FTP     
  #define PROTECT_TCP      // Safest, uses extra ?? bytes

  #define PINFORICR   (0) // The ICR pin B0

#ifndef DEBUGGER
//  #define USE_LCD    
#endif

// TODO will need to adjust for clock speed.  132 is correct for 8MHz
  #define TIME_START   (132) // Time fine tuning.  Overflows at 256, so this can control x->256
// Increasing number leads to faster clock (overflows sooner)

  #define DELAY_CALIBRATE (1.0) 

  #define LEDOFF PORTB|=(1<<PORTB1)
  #define LEDON PORTB&=~(1<<PORTB1)

//  #define USE_APIPA   // We are useful even without a DHCP server

  #define USE_DHCP          
//#define USE_SMTP         // TCP 
//#define USE_POP3         // TCP
  #define USE_DNS          // Needed for NTP
  #define USE_NTP          
  #define IMPLEMENT_PING      // Usefull unless space critical

  #define MAC_0  (LOCAL_ADMIN | 0)
  #define MAC_1  (0x02)  // Changes these numbers for each instance
  #define MAC_2  (0x04)
  #define MAC_3  (0x06)
  #define MAC_4  (0x08)
  #define MAC_5  (0x0A)

#endif

#ifdef WHEREABOUTS  

#define HOSTNAME "whereabouts"  // Length automatic
  #define MYID  (21643)
  #define WHEREABOUTS_SERVER_PORT    (12345+17)

  #define ATMEGA328     
  #define F_CPU 12500000UL  // 12.5 MHz fed from ENC28J60 crystal
//  #define TWI_SCALE   (32) // 100kHz TWI from CPU/(16+2*n).  Hence n=32 for 8 MHz and 55 (inexact, not tested) for 12.5 MHz
  #define USE_ENC28J60      // Set this if using the ENC28J60 chip (converse is ISA)
  #define CLK_DIVISOR (2)  

//  #define RTC           // Real Time Clock Present
//  #define USE_FTP     
  #define PROTECT_TCP      // Safest, uses extra ?? bytes

//  #define PINFORICR   (0) // The ICR pin B0

#ifndef DEBUGGER
//  #define USE_LCD    
#endif

  //#define LEDON  {}   // No LED
  //#define LEDOFF {}  // No LED

  #define LEDOFF PORTB|=(1<<PORTB0)  // Which port is our LED on?
  #define LEDON  PORTB&=~(1<<PORTB0) // DEBUG model only
  
// TODO will need to adjust for clock speed.  0 is correct for 8MHz, when paired wth 61
  #define TIME_START   (180) // Time fine tuning.  Overflows at 256, so this can control x->256
// Increasing number leads to faster clock (overflows sooner)

  #define DELAY_CALIBRATE (1.0) 

//  #define USE_APIPA   

  #define USE_DHCP          
//#define USE_SMTP         // TCP 
//#define USE_POP3         // TCP
  #define USE_DNS          // Needed for NTP
  #define USE_NTP          
  #define IS_HTTP_SERVER     // TCP
  #define USE_mDNS        
  #define USE_LLMNR       
  #define IMPLEMENT_PING      // Useful unless space critical

  #define MAC_0  (LOCAL_ADMIN | 0x50)   // R=0x52
  #define MAC_1  (0x47)  // G=0x47
  #define MAC_2  (0x46)  // F=0x46
  #define MAC_3  (0x52)
  #define MAC_4  (0x47)
  #define MAC_5  (0x46)

  #define MINUTE_TDC  (1<<0)
  #define HOUR_TDC    (1<<1)

  #define MODE_WEB   (1<<5)  // Select switch on Port D
  #define MODE_LAN   (1<<6)  // Note they are active low
  #define MODE_TIME  (1<<7)
  #define MODE_DEMO  (1<<1)  // PB1
  #define MODE_ANY   (MODE_WEB|MODE_LAN|MODE_TIME|MODE_DEMO)
  #define MODE_OFF   (0)
  #define MODE_WEB_UPDATE  (1<<6)

  #define WEB_REFRESH_SECS (300) // 5 mins

//  #define SWITCHED_OFF ((PINC & MODE_ANY)==MODE_ANY)
  #define SWITCHED_OFF (FALSE) 
//((~PINC & MODE_WEB))


#define HEAD_LEN  (115)  // NB escaped chars count as one
#define ITEM_LEN   (64)
#define ROWEND_LEN (12)
#define TAIL_LEN   (66)  

// These are the web page data lengths in the EEPROM - must match!
 //  #define START_LEN (116)
//  #define FRAME_LEN (44)
//  #define NAME_INDEX (26) // byte to insert "G" or "R"
//  #define PTR_INDEX (36)  // place to insert 00, 01, 02 ... 11 (hand position)
//  #define END_LEN   (68)
//  #define PAYLOAD (1412) // Total created packet (+Ethernet, IP and TCP headers <1500 for ethernet, i.e. <1460)
//  #define CSUM_PAY (0x661E) // Its checksum
  #define ICON_LEN (1152)

// Interface with daughter board
#define GET_TIME    (0x00)  // 1 byte command, reply with 2 bytes
#define SET_TIME    (0x10)  // 3 bytes : command then two byte time data
#define USE_MOTOR   (0x20)  // 1 byte command - low nibble specifies motor

// Three status cases (all selectable per-motor)
#define FREE_RUN    (0x30)  // Tick onwards
#define POSITION    (0x40)  // No clock behaviour, but still move to pre-defined place
#define DEMO        (0x50)  // Random moves to hour points.  3 byte commend - next two are random seed.
#define STOP        (0x60)  // No movement at all 

#define DIVISIONS  (60000.0)  // Caller needs to know.  This is divisions per-rotation
  
#endif

#ifdef HOUSE  

#define HOSTNAME "house"  // Length automatic
  #define MYID  (22573)

  #define ATMEGA328     
  #define F_CPU 16000000UL  // 16 MHz 
//  #define TWI_SCALE   (32) // 100kHz TWI from CPU/(16+2*n).  Hence n=32 for 8 MHz and 55 (inexact, not tested) for 12.5 MHz
  #define USE_ENC28J60      // Set this if using the ENC28J60 chip (converse is ISA)
  #define CLK_DIVISOR (2)  

//  #define RTC           // Real Time Clock Present
//  #define USE_FTP     
  #define PROTECT_TCP      // Safest, uses extra ?? bytes

  #define LEDON  {}   // No LED
  #define LEDOFF {}  // No LED

  //#define LEDOFF PORTB|=(1<<PORTB0)  // Which port is our LED on?
  //#define LEDON  PORTB&=~(1<<PORTB0) // DEBUG model only
  
// TODO will need to adjust for clock speed.  0 is correct for 8MHz, when paired wth 61
  #define TIME_START   (180) // Time fine tuning.  Overflows at 256, so this can control x->256
// Increasing number leads to faster clock (overflows sooner)

  #define DELAY_CALIBRATE (1.0) 

  #define USE_APIPA   

  #define USE_DHCP          
//#define USE_SMTP         // TCP 
//#define USE_POP3         // TCP
  #define USE_DNS          // Needed for NTP
  //#define USE_NTP          
  #define IS_HTTP_SERVER        // TCP
  #define USE_mDNS        
  #define USE_LLMNR       
  #define IMPLEMENT_PING      // Useful unless space critical

  #define MAC_0  (LOCAL_ADMIN | 0x50)   
  #define MAC_1  (0x40)  
  #define MAC_2  (0x52)  
  #define MAC_3  (0x60)
  #define MAC_4  (0x70)
  #define MAC_5  (0x80)

  #define ICON_LEN (1152)
  
#endif

#ifdef NETWORK_CONSOLE  //****************************************************************

#define HOSTNAME "console"  // Length automatic
  #define MYID  (21644)

  #define ATMEGA32      
  #define F_CPU 16000000UL  // 16 MHz with crystal

  #define ISA_SPI     

  #define DELAY_CALIBRATE (3.0) // check

#ifndef DEBUGGER
  #define USE_LCD    
#endif

  #define LEDON   (1==1)  // Hasn't got a led, needs dummy function
  #define LEDOFF  (1==1)

  #define TIME_START   (140) // Time fine tuning.  Overflows at 256, so this can control x->256
// Increasing number leads to faster clock (overflows sooner)

  #define USE_DHCP          
  #define USE_SMTP         // TCP 
  #define USE_POP3         // TCP
  #define USE_DNS          
  #define USE_NTP          
  #define IS_HTTP_SERVER      // TCP
  #define IMPLEMENT_PING     // Usefull unless space critical

  #define PROTECT_TCP      // Safest, uses extra ?? bytes

// ISA card has own MAC 

#endif


// Next few used to be in NET_PROG ...
#define SPI_CONTROL_PORT  (PORTB)
#define SPI_CONTROL_DDR   (DDRB)
#define SPI_CONTROL_MISO  (PORTB4)
#define SPI_CONTROL_MOSI  (PORTB3)
#define SPI_CONTROL_SCK   (PORTB5)

// ... whereas CHIP SELECT can be a different port, and there may be more than one CSUM_PAY
// if we use SPI for multiple things.

#define ETH_SPI_SEL_PORT       (PORTB)
#define ETH_SPI_SEL_DDR        (DDRB)
#define ETH_SPI_SEL_CS         (2)   


#ifdef NET_PROG  //****************************************************************
// An ATMega328-based device with a local ENC28J60 interface and SPI RAM memory or
// [W25Qxx flash memory]

  #define HOSTNAME "iot-isp"  // Length automatic (valid chars are letters, digits and '-')
  
  #define KNOWN_PROGS (5)     // The microcontrollers we know how to program
  #define LONGEST_MICRO (13)  // Name of the longest
// 1.  Hardware defintion.  Assumes SPI (CONTROL) is on common port ...

 
  
#define W25_SPI_SEL_PORT       (PORTB)
#define W25_SPI_SEL_DDR        (DDRB)
#define W25_SPI_SEL_CS         (0)      
  
#define RAM_SPI_SEL_PORT       (PORTD)
#define RAM_SPI_SEL_DDR        (DDRD)
#define RAM_SPI_SEL_CS         (4)
  
#define ISP_SEL_PORT       (PORTD)
#define ISP_SEL_DDR        (DDRD)
#define ISP_SPI_SEL_CS     (5)      
 
#define ISP_CONTROL_PORT     (PORTD)
#define ISP_CONTROL_PORT_IN  (PIND)  // For MISO
#define ISP_CONTROL_DDR   (DDRD)
#define ISP_CONTROL_MISO  (7)
#define ISP_CONTROL_MOSI  (3)
#define ISP_CONTROL_SCK   (6)
    
  #define SOURCE_ISP (0)  // Do we read direct from ISP ...
  //#define SOURCE_RAM (1)  // ... or buffer via SPI RAM // TO DO DOESNT WORK YET
  
  #define MYID  (21645) // Useful if unique on LAN
  
  #define ATMEGA328     

  #define F_CPU 16000000UL  // 16 MHz. Allow to clash if F_CPU set in compiler so we get a warning

  #define USE_ENC28J60      // Set this if using the ENC28J60 chip (converse is ISA)
  //#define CLK_DIVISOR (2)

  #define TIME_START   (180) // Time fine tuning.  Overflows at 256, so this can control x->256
  #define DELAY_CALIBRATE (1.0) // Depends on clock. 1.0 correct for power meter at 12.5MHz

  #define LEDON  {}   // No LED
  #define LEDOFF {}   // No LED
  #define PROTECT_TCP      // Safest, uses extra ?? bytes
  
  #define USE_APIPA        // We are useful even without a DHCP server
  #define USE_DHCP          
  //#define STATIC_IP       
  
    #define OCT0 (192)  // required with STATIC_IP
    #define OCT1 (168)  // We ASSUME submask=255.255.255.0
    #define OCT2 (123)  // and GW=same as myIp with last octet=0
    #define OCT3 (15)
  
//#define USE_MD5   
  
//#define USE_SMTP         // TCP 
//#define USE_POP3         // TCP
  #define USE_DNS          
//#define USE_NTP          // Usually off when debugging to avoid flooding
  #define IS_HTTP_SERVER         // TCP
  #define USE_mDNS        
  #define USE_LLMNR         
  #define IMPLEMENT_PING     // Useful unless space critical

  //#define AUTH7616   // RFC 7616 authorisation

  #define MAC_0  (LOCAL_ADMIN | 0x30)   // Make unique if more than one
  #define MAC_1  (0x40)  
  #define MAC_2  (0x50)
  #define MAC_3  (0x60)
  #define MAC_4  (0x70)
  #define MAC_5  (0x80)
  
  #define ICON_LEN (2453)
  
#endif

#ifdef HELLO_HTTP_WORLD  //****************************************************************
// A minimal ATMega328-based device with a local ENC28J60 interface

  #define HOSTNAME "hello-world"  // Length automatic (valid chars are letters, digits and '-')
    
  #define MYID  (6167) // Useful if unique on LAN
  
  #define ATMEGA328     

  #define F_CPU 16000000UL  // 16 MHz. Allow to clash if F_CPU set in compiler so we get a warning

  #define USE_ENC28J60      // Set this if using the ENC28J60 chip (converse is ISA)
  //#define CLK_DIVISOR (2)

  #define TIME_START   (180) // Time fine tuning.  Overflows at 256, so this can control x->256
  #define DELAY_CALIBRATE (1.0) // Depends on clock. 1.0 correct for power meter at 12.5MHz

  #define LEDON  {}   // No LED
  #define LEDOFF {}   // No LED
  #define PROTECT_TCP      // Safest, uses extra ?? bytes
  
  #define USE_APIPA        // We are useful even without a DHCP server
  #define USE_DHCP          
  //#define STATIC_IP       
  
    #define OCT0 (192)  // required with STATIC_IP
    #define OCT1 (168)  // We ASSUME submask=255.255.255.0
    #define OCT2 (123)  // and GW=same as myIp with last octet=0
    #define OCT3 (15)
  
//#define USE_MD5   
  
//#define USE_SMTP         // TCP 
//#define USE_POP3         // TCP
  #define USE_DNS          
//#define USE_NTP          // Usually off when debugging to avoid flooding
  #define IS_HTTP_SERVER         // TCP
  #define USE_mDNS        
  #define USE_LLMNR         
  #define IMPLEMENT_PING     // Useful unless space critical

  //#define AUTH7616   // RFC 7616 authorisation

  #define MAC_0  (LOCAL_ADMIN | 0x32)   // Make unique if more than one
  #define MAC_1  (0x42)  
  #define MAC_2  (0x52)
  #define MAC_3  (0x62)
  #define MAC_4  (0x72)
  #define MAC_5  (0x82)
  
#endif


// Automatic

#ifdef IS_HTTP_SERVER
#define USE_HTTP
#endif

#ifdef IS_HTTP_CLIENT
#define USE_HTTP
#endif


// Invoke TCP automatically if required by Application level protocols
#ifdef USE_SMTP  
  #define USE_TCP    
#endif

#ifdef USE_POP3  
  #define USE_TCP    
#endif

#ifdef USE_HTTP  
  #define USE_TCP    
#endif

#ifdef USE_FTP
  #define USE_TCP    
#endif

#ifdef ATMEGA32
#ifdef ATMEGA328
Error cant both be defined
#endif
#endif

#ifndef USE_DHCP
#ifndef USE_APIPA
#ifndef STATIC_IP
Error cant all be undefined 
#endif
#endif
#endif

#ifdef USE_DHCP
#ifdef STATIC_IP
Error cant both be defined 
#endif
#endif

#ifdef USE_APIPA
#ifdef STATIC_IP
Error cant both be defined 
#endif
#endif

#ifdef SOURCE_ISP   
#ifdef SOURCE_RAM  
Error cant both be defined 
#endif
#endif

