## STC15W408AS 6-Way Mode Switch — Build & Flash
##
## Usage:
##   make          — compile to .ihx and .hex
##   make flash    — compile + flash via stcgal (power-cycle target when prompted)
##   make clean    — remove build artifacts

CC      = sdcc
CFLAGS  = -mmcs51 --model-small --std-c11
PORT   ?= /dev/ttyUSB0
FREQ    = 11059

TARGET  = main

.PHONY: all flash clean

all: $(TARGET).ihx $(TARGET).hex

$(TARGET).ihx: $(TARGET).c
	$(CC) $(CFLAGS) $< -o $@

$(TARGET).hex: $(TARGET).ihx
	packihx $< > $@

flash: $(TARGET).ihx
	@echo "==> Power-cycle the STC15 board NOW to enter bootloader..."
	stcgal -p $(PORT) -t $(FREQ) --option reset_pin_enabled=0 $<

clean:
	rm -f *.ihx *.hex *.lk *.map *.mem *.rel *.rst *.sym *.lst *.asm
