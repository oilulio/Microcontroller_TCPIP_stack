/*********************************************
 
Header for Application layer protocols DHCP, DNS, NTP ....

Code portability note : need to set LITTLEENDIAN correctly
For ATMEL use LITTLEENDIAN

 *********************************************/

#ifndef APPLICATION_H
#define APPLICATION_H

#include "network.h"  // Define IP Address

#define MAX_DNS (64) // Max length of FQDND (from Windows Server 2008 TCP/IP protocols & services)

// NB.  The numerical order matters as ">" comparisons are made.
#define FTP_CLOSED        (0) // No FTP ongoing
#define FTP_STARTED       (1) // TCP channel initialised (awaiting '220')
#define FTP_USER_SENT     (2) // Replied 'USER name' awaiting 331 or ???
#define FTP_PASS_SENT     (3) // Send 'PASS pwd' awaiting 230
#define FTP_BIN_SENT      (4) // Binary mode request sent (await 200) 
#define FTP_PASV_SENT     (5) // Passive mode (await 227 ... IP & port)
#define FTP_READY         (6) // Can now open a channel on known port
#define FTP_WAIT_PASSIVE  (7) // Waiting ACK for Passive channel
#define FTP_WAIT_SEND     (8) // Have set up channel and requested STOR, STOU or APPE (await '150 OK to send')
#define FTP_OK_SEND       (9) // Have clearance to send.  Await '226 transfer complete'
#define FTP_QUITTING      (10)// Awaiting '221 Goodbye'

 // FTP commands
#define NONE   (10)   // Nothing to do - empty list
// N.B.  Array indices - do not change.
#define APPE   (0)   // Append file
#define STOR   (1)   // Store file (overwrite if suplicate)
#define STOU   (2)   // Store unique file (rename if duplicate)

typedef struct  {  //  FTP queue item
  uint8_t command;
  char * comd_filename;
  char * payload;
  struct FTP_job * next;
} FTP_job;

typedef struct { // Time and date 
  uint16_t year;
  uint8_t day;
  uint8_t month;
  uint8_t hour;
  uint8_t minute;
  uint8_t secs;
  unsigned BST    : 4;
  unsigned wk_day : 4;
} TimeAndDate;

typedef struct { // DNS message : 12 bytes before options. 
  uint16_t   id;
  unsigned   RD         : 1;  
  unsigned   TC         : 1;  
  unsigned   AA         : 1;  
  unsigned   OPCODE     : 4;
  unsigned   QR         : 1;
  unsigned   RCODE      : 4;  
  unsigned   Z          : 3;  
  unsigned   RA         : 1;  
  uint16_t   QDCOUNT;
  uint16_t   ANCOUNT;
  uint16_t   NSCOUNT;
  uint16_t   ARCOUNT;
  uint8_t    message[1]; // Length padded out later by union
} DNS_message;

#define DHCP_DISCOVER  (1)
#define DHCP_OFFER     (2)
#define DHCP_REQ       (3) 
#define DHCP_DECLINE   (4)
#define DHCP_ACK       (5)
#define DHCP_NACK      (6)
#define DHCP_RELEASE   (7)

#define DHCP_OPT_SUBNET_MASK  (0x01)
#define DHCP_OPT_ROUTER_IP    (0x03)
#define DHCP_OPT_DNS          (0x06)
#define DHCP_OPT_LEASE_TIME   (0x33)
#define DHCP_OPT_MSG_TYPE     (0x35)
#define DHCP_OPT_OVERLOAD     (0x34)
#define DHCP_OPT_END          (0xFF)

#define DHCP_MAGIC_COOKIE_OFFSET (0x116)  // Bytes into packet
#define DHCP_SNAME_OFFSET        (0x56)
#define DHCP_BOOTFILE_OFFSET     (0x96)

#define DHCP_NO_OVERLOAD              (0)
#define DHCP_FILE_OVERLOAD            (1<<0)  // From DHCP std, Option 52 (0x34)
#define DHCP_SNAME_OVERLOAD           (1<<1)  // From DHCP std, Option 52

#define MAX_RING_BUFFER   (82)     // For POST.  "boundary="+"--"+70
#define POST_NONE            (0)   // Not dealing with a POST submisson
#define POST_READING      (1<<1)   //     Dealing with a POST submisson
#define POST_INTO_CONTENT (1<<2)   // Beyond headers, into content
#define POST_FOUND_BDRY   (1<<3)   // Know the multipart boundary
#define POST_FILE_NEXT    (1<<4)   // Into the file's multipart
#define POST_INTO_FILE    (1<<5)   // Into the file itself

#define HTTP_NO_LEN (5)  // Length of Content-length field in HTTP (will have leading zeros)

typedef struct { // Sub-DHCP - supports MACs of up to 16 bytes
 MAC_address MAC;
 uint8_t dummy[10];  // Padding out to 16 bytes
} Hardware;

typedef struct { // DHCP message - note divergence from direct match to packet
// Would normally be 234 bytes before options.  Stored portion reduced to 34 bytes plus options.
 uint8_t  op;
 uint8_t  hardware_type;
 uint8_t  hardware_length;
 uint8_t  hops;
 uint32_t        XID;   // Unique 32 bit number
 uint16_t        secs;  // Seconds since client started asking
 uint16_t        flags;
 IP4_address     CIAddr;
 IP4_address     YIAddr;
 IP4_address     SIAddr;
 IP4_address     GIAddr;

// Hardware        Hware; // Just use MAC_address - it's the meaningful bit
   MAC_address     MAC;   // Either/or this and Hardware above

// unsigned char   Sname[64];      // For our purposes these are always
// unsigned char   Bootfile[128];  // zero, and also quite large, so skip.
// Requires the physical layer driver to do this to save space.
// Also addresses option overloading at the same time, see :
// http://www.tcpipguide.com/free/t_DHCPOptionsOptionFormatandOptionOverloading-4.htm
//  uint8_t         magic_cookie[4]; // And we'll check this at the same time, so
// no need to store either.
   uint8_t         options[1];    // Only really used outbound
} DHCP_message;

typedef struct { // NTP message : 48 bytes
    unsigned    Status           : 6;
    unsigned    Leap             : 2;
    uint8_t     theType;
    uint16_t    precision;
    uint32_t    estimated_error;
    uint32_t    estimated_drift_rate;
    uint32_t    reference_id;
    uint32_t    reference_time_int;   // Ref times are 64-bit fixed point.
    uint32_t    reference_time_fract; // Seconds relative to 0000 UT 1 Jan 1900
    uint32_t    origin_time_int;      // (From RFC 958).  Implicit that they are
    uint32_t    origin_time_fract;    // calendar seconds (not elapsed) because
    uint32_t    rx_stamp_int;         // leap seconds generate 59 or 61 second minutes
    uint32_t    rx_stamp_fract;      
    uint32_t    tx_stamp_int;
    uint32_t    tx_stamp_fract;
} NTP_message;

typedef struct { // Power
  uint16_t  current;
  uint16_t  waveform[1]; // MAX_TIME_SAMPLES];
} Power_message;

typedef struct { // General UDP
  union {
    uint8_t     bytes[MAX_STORED_SIZE-ETH_HEADER_SIZE-IP_HEADER_SIZE-UDP_HEADER_SIZE];
    uint16_t    words[(MAX_STORED_SIZE-ETH_HEADER_SIZE-IP_HEADER_SIZE-UDP_HEADER_SIZE)/2];
  };
} UDP_message;

typedef struct { // General TCP
  union {
    uint8_t  bytes[MAX_STORED_SIZE-ETH_HEADER_SIZE-IP_HEADER_SIZE-TCP_HEADER_SIZE];
    uint16_t words[(MAX_STORED_SIZE-ETH_HEADER_SIZE-IP_HEADER_SIZE-TCP_HEADER_SIZE)/2];
    char chars[(MAX_STORED_SIZE-ETH_HEADER_SIZE-IP_HEADER_SIZE-TCP_HEADER_SIZE)];
  };
} TCP_message;


#define FIRSTJAN2000_0000_UT     (0xBC17C200)
#define DAY_IN_SECONDS           (86400)
#define YEAR_IN_SECONDS          (31536000UL)
#define HOUR_IN_SECONDS          (3600)
#define MINUTE_IN_SECONDS        (60)

typedef struct {      // DHCP_option
  uint8_t       type;
  uint8_t       length;
  union {
    uint8_t     data[10];
    uint32_t    IP4;
  }; 
} DHCP_option;
 
#define MAX_UDP_PORTS  (3)  // Using 3 so far

#define DHCP_CLIENT_PORT   (0x44)   // 68 Dec
#define DHCP_SERVER_PORT   (0x43)   // 67 Dec

#define DNS_CLIENT_PORT_REF   (0)   // _REF are array references  
#define DNS_SERVER_PORT    (0x35)

#define NTP_CLIENT_PORT_REF   (1)  
#define NTP_SERVER_PORT    (0x7B)
#define NTP_ID  (7729) // For DNS identification


#define FTP_SERVER_PORT    (0x15)   // 21 Dec



#define mDNS_PORT         (5353)
#define LLMNR_PORT        (5355)

// TCP assigns its own client ports

#define POP3_SERVER_PORT  (0x6E)   // 110 Dec

#define HTTP_SERVER_PORT  (0x50) // 80 Dec

typedef struct {  // Merges all structures.
  union {
    struct {
      Ethernet_header Ethernet;
      IP4_header IP4; 
      UDP_header UDP;
      union {
        DNS_message   DNS;   // UDP messages
        DHCP_message  DHCP;
        NTP_message   NTP;
        Power_message Power; // My bespoke message
        UDP_message   UDP_payload; // Bespoke
      };
    };

  struct {
    uint8_t dummy2[ETH_HEADER_SIZE+IP_HEADER_SIZE];
    ICMP_message ICMP;
  };
  struct {
    uint8_t dummy3[ETH_HEADER_SIZE];
    ARP_message ARP;
  };
  struct {
    uint8_t dummy1[ETH_HEADER_SIZE+IP_HEADER_SIZE];
    TCP_header TCP;
      
    union {
      uint8_t TCP_options[10];
      char * POP3;   // TCP messages
      char * SMTP;
      char * HTTP;
      TCP_message TCP_payload;
    };
  };
  uint8_t bytes[MAX_STORED_SIZE]; 
  uint16_t words[(MAX_STORED_SIZE)/2];
 };
}  MergedPacket;

#define HTTP_RAW(X,FN) (TCP_ComplexDataOut(&MashE,(X),(FN(0,0,&dummy)),&FN,0,TRUE))

#define HTTP_WITH_PREAMBLE(X,FN) ({expectLen=(FN(0,0,&dummy));\
                                TCP_ComplexDataOut(&MashE,(X),PreambleData(0,0,&dummy),&PreambleData,0,TRUE);\
                                TCP_ComplexDataOut(&MashE,(X),expectLen,&FN,0,TRUE);})

#define HTTP_WITH_PREAMBLE_CACHE(X,FN) ({expectLen=(FN(0,0,&dummy));\
                                TCP_ComplexDataOut(&MashE,(X),PreambleDataCache(0,0,&dummy),&PreambleDataCache,0,TRUE);\
                                TCP_ComplexDataOut(&MashE,(X),expectLen,&FN,0,TRUE);})


#define SEND_404 ({HTTP_RAW(TCP_SERVER,HTTP_404);TCP_FIN(Mash,TCP_SERVER);})


// Function prototypes
uint8_t genericUDPTo(uint16_t words[],uint16_t length,IP4_address * sendIP);
uint8_t genericUDP(uint16_t words[],uint16_t length);
uint8_t genericUDPBcast(uint16_t words[],uint16_t length);

void resetHTTPServer();
void sendHTML(MergedPacket * Mash, uint16_t length);
void parseHTML(MergedPacket * Mash, uint16_t length);
void GET_HTTP(void);
void initiate_POP3(void);
uint8_t hexDigit(char c);
uint8_t summerTimeCorrection(uint8_t * hour, uint8_t * day, uint8_t * month, uint16_t * year);
uint32_t getTime(uint8_t day, uint8_t month, uint16_t year,
                  uint8_t hour, uint8_t minute, uint8_t second);
uint32_t protectedGetTime(uint8_t day, uint8_t month, uint16_t year,
                  uint8_t hour, uint8_t minute, uint8_t second);
void displayTime(uint32_t time);
void handleNTP(NTP_message * NTP);
void queryNTP(void);
uint8_t sendPower(uint32_t time,uint8_t current);//,uint8_t spectrum[]);
uint8_t skipName(uint8_t * Message);
void handleDNS(DNS_message * DNS);
void handleMDNS(MergedPacket * Mash);
void handleLLMNR(MergedPacket * Mash);
uint8_t hostMatch(DNS_message * DNS,uint8_t i);
uint8_t parseDomainName(char domain[MAX_DNS],char parse[MAX_DNS]);
void queryDomainName(MergedPacket * Mash, char domain[MAX_DNS],uint16_t id);
uint16_t prepDHCP(DHCP_message *, uint8_t type);
void initiateDHCP(void);
void requestDHCP(void);
void handleDHCP(DHCP_message * DHCP);
void handleFTP(MergedPacket * Mash, const uint16_t length);
void FTPUpdate(void);
uint8_t queueForFTP(uint8_t command,char * filename, char * data);
void launchWhereabouts(void);

void lineOne(TimeAndDate * tad);
void lineTwo(TimeAndDate * tad);
void displayTime(uint32_t time);
TimeAndDate parseTime(uint32_t time);

// These are designed as callback functions offering random access to
// data within an internal length.  Returns the requested number (length) of
// bytes from start in the result pointer.  Returns argument of the
// internal (available) length.  When called with (0,0,&dummy) will
// return available length only.

uint16_t HTTP_404(uint16_t start,uint16_t length,uint8_t * result);

uint16_t PreambleData(uint16_t start,uint16_t length,uint8_t * result);
uint16_t PreambleDataCache(uint16_t start,uint16_t length,uint8_t * result);
uint16_t IconData(uint16_t start,uint16_t length,uint8_t * result);
//uint16_t ImageData(uint16_t start,uint16_t length,uint8_t * result);
uint16_t ISPbitmap(uint16_t start,uint16_t length,uint8_t * result);
uint16_t Image0(uint16_t start,uint16_t length,uint8_t * result);
uint16_t Image1(uint16_t start,uint16_t length,uint8_t * result);
uint16_t Image2(uint16_t start,uint16_t length,uint8_t * result);
uint16_t Image3(uint16_t start,uint16_t length,uint8_t * result);
uint16_t Image4(uint16_t start,uint16_t length,uint8_t * result);
uint16_t WhereaboutsData(uint16_t start,uint16_t length,uint8_t * result);
uint16_t HouseData(uint16_t start,uint16_t length,uint8_t * result);
uint16_t ProgData(uint16_t start,uint16_t length,uint8_t * result);
uint16_t EEPROMData(uint16_t start,uint16_t length,uint8_t * result);
uint16_t EEPROMDataP(uint16_t start,uint16_t length,uint8_t * result);
uint16_t FlashData(uint16_t start,uint16_t length,uint8_t * result);
uint16_t EraseData(uint16_t start,uint16_t length,uint8_t * result);
uint16_t EraseCfm(uint16_t start,uint16_t length,uint8_t * result);
uint16_t HTTP_404(uint16_t start,uint16_t length,uint8_t * result);
uint16_t UploadSuccess(uint16_t start,uint16_t length,uint8_t * result);
uint16_t UploadFailure(uint16_t start,uint16_t length,uint8_t * result);

uint8_t caseFreeCompare(const char * s1,const char * s2, uint8_t len);
void MDTestSuite(void);
uint8_t AuthorisedSHA256(char * toHash2,char * cnonce,char * target);
      
#endif