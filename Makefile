PRJ = Net
MCU = atmega328p
#CLK = 16000000  
# CLK usually defined in config.h
AVRPATH=c:\WinAVR-20100110\bin
COMPORT=COM3

CC = ${AVRPATH}\avr-gcc
AVRDUDE = avrdude
OBJCOPY = ${AVRPATH}\avr-objcopy
OBJDUMP = $(AVRPATH)\avr-objdump
SIZE    = $(AVRPATH)\avr-size --format=avr --mcu=$(MCU)
CFLAGS    = -Wall -Os -mmcu=$(MCU) -c -std=gnu99 -funsigned-char -funsigned-bitfields -ffunction-sections -fdata-sections -fpack-struct -fshort-enums -gdwarf-2
#-DF_CPU=$(CLK)
SRCS = application.c applicationCore.c applicationHelloWorld.c lfsr.c init.c isp.c mem23SRAM.c w25q.c main.c network.c linkENC28J60.c transport.c md5.c ripemd160.c sha1.c sha256.c power.c
#where.c

OBJS = $(patsubst %.c,obj/%.o,$(SRCS)) 
# object files in subdirectory obj, which will (below) be created if needed
DEPS := $(OBJS:.o=.d)
# Dependency files for e.g. recompile when header file changes

all: ${PRJ}.hex

${PRJ}.hex: ${PRJ}.elf
	${OBJCOPY} -O ihex $< $@
	${SIZE} -C ${PRJ}.elf

${PRJ}.elf: ${OBJS}
	${CC} -mmcu=${MCU} -Wl,--gc-sections -o $@ $^
# -Wl,--gc-sections removes unwanted code for space

install: ${PRJ}.hex
	avrdude -p $(MCU) -c STK500v2 -P $(COMPORT) -V -U flash:w:${PRJ}.hex

$(OBJS): | obj
  
obj:
	@mkdir -p $@  
  
-include $(DEPS)  
  
obj/%.o: %.c
	$(CC) $(CFLAGS) -MMD -MF $(patsubst %.o,%.d,$@) -o $@ $<
# -MMD and -MF make the .d dependency files to ensure we recompile when needed
  
clean:
	rm -f ${PRJ}.elf ${PRJ}.hex ${OBJS} ${DEPS}