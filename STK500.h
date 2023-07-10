/* STK500 constants list, from AVRDUDE, from Optiboot
 *
 * Trivial set of constants derived from Atmel App Note AVR061
 * Not copyrighted.  Released to the public domain.
 
 Also from AVR068
 
 */

#define STK_OK              0x10
#define STK_FAILED          0x11  // Not used
#define STK_UNKNOWN         0x12  // Not used
#define STK_NODEVICE        0x13  // Not used
#define STK_INSYNC          0x14  // ' '
#define STK_NOSYNC          0x15  // Not used
#define ADC_CHANNEL_ERROR   0x16  // Not used
#define ADC_MEASURE_OK      0x17  // Not used
#define PWM_CHANNEL_ERROR   0x18  // Not used
#define PWM_ADJUST_OK       0x19  // Not used
#define CRC_EOP             0x20  // 'SPACE'
#define STK_GET_SYNC        0x30  // '0'
#define STK_GET_SIGN_ON     0x31  // '1'
#define STK_SET_PARAMETER   0x40  // '@'
#define STK_GET_PARAMETER   0x41  // 'A'
#define STK_SET_DEVICE      0x42  // 'B'
#define STK_SET_DEVICE_EXT  0x45  // 'E'
#define STK_ENTER_PROGMODE  0x50  // 'P'
#define STK_LEAVE_PROGMODE  0x51  // 'Q'
#define STK_CHIP_ERASE      0x52  // 'R'
#define STK_CHECK_AUTOINC   0x53  // 'S'
#define STK_LOAD_ADDRESS    0x55  // 'U'
#define STK_UNIVERSAL       0x56  // 'V'
#define STK_PROG_FLASH      0x60  // '`'
#define STK_PROG_DATA       0x61  // 'a'
#define STK_PROG_FUSE       0x62  // 'b'
#define STK_PROG_LOCK       0x63  // 'c'
#define STK_PROG_PAGE       0x64  // 'd'
#define STK_PROG_FUSE_EXT   0x65  // 'e'
#define STK_READ_FLASH      0x70  // 'p'
#define STK_READ_DATA       0x71  // 'q'
#define STK_READ_FUSE       0x72  // 'r'
#define STK_READ_LOCK       0x73  // 's'
#define STK_READ_PAGE       0x74  // 't'
#define STK_READ_SIGN       0x75  // 'u'
#define STK_READ_OSCCAL     0x76  // 'v'
#define STK_READ_FUSE_EXT   0x77  // 'w'
#define STK_READ_OSCCAL_EXT 0x78  // 'x'
#define STK_SW_MAJOR        0x81  // ' '
#define STK_SW_MINOR        0x82  // ' '


enum SpiProgrammingProtocolCommands {
    //**********************************************************
    // Protocol commands
    //**********************************************************
    SPI_CMD_LOAD_ADDRESS                    = 0x06, //! Load address
    SPI_CMD_ENTER_PROGMODE                  = 0x10, //! Enter programming mode
    SPI_CMD_LEAVE_PROGMODE                  = 0x11, //! Leave programming mode
    SPI_CMD_CHIP_ERASE                      = 0x12, //! Erase device
    SPI_CMD_PROGRAM_FLASH                   = 0x13, //! Program flash data
    SPI_CMD_READ_FLASH                      = 0x14, //! Read flash data
    SPI_CMD_PROGRAM_EEPROM                  = 0x15, //! Program EEPROM data
    SPI_CMD_READ_EEPROM                     = 0x16, //! Read EEPROM data
    SPI_CMD_PROGRAM_FUSE                    = 0x17, //! Program a fuse byte
    SPI_CMD_READ_FUSE                       = 0x18, //! Read a fuse byte
    SPI_CMD_PROGRAM_LOCK                    = 0x19, //! Program lock bits
    SPI_CMD_READ_LOCK                       = 0x1A, //! Read lock bits
    SPI_CMD_READ_SIGNATURE                  = 0x1B, //! Read a signature byte
    SPI_CMD_READ_OSCCAL                     = 0x1C, //! Read an OSCCAL byte
    SPI_CMD_SET_BAUD                        = 0x1D, //! Set baud rate
    SPI_CMD_GET_BAUD                        = 0x1E  //! Read baud rate
};

enum SpiProgrammingProtocolResponses {
    //**********************************************************
    // Protocol responses
    //**********************************************************
    // Success
    SPI_STATUS_CMD_OK                       = 0x00, //! All OK

    // Warnings
    SPI_STATUS_CMD_TOUT                     = 0x80, //! Command timeout
    SPI_STATUS_RDY_BSY_TOUT                 = 0x81, //! Device busy

    // Errors
    SPI_STATUS_CMD_FAILED                   = 0xC0, //! Command failed
    SPI_STATUS_CMD_UNKNOWN                  = 0xC9, //! Unknown error
    SPI_STATUS_PHY_ERROR                    = 0xCB, //! Physical error
    SPI_STATUS_CLOCK_ERROR                  = 0xCC, //! Speed error
    SPI_STATUS_BAUD_INVALID                 = 0xCD  //! Baud value error
};
