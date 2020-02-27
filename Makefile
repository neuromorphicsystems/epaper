device = /dev/ttyACM0
baudrate = 115200
microcontroller = atmega328p
programmer = arduino
frequency = 16000000
flags = -DF_CPU=$(frequency) -mmcu=$(microcontroller) -Wall -Wextra -O3

.PHONY: flash clean

all: build/epaper.hex

build/epaper.o: source/epaper.c
	mkdir -p build
	avr-gcc $(flags) -c $< -o $@

build/epaper.elf: build/epaper.o
	avr-gcc $(flags) -o $@ $^

build/epaper.hex: build/epaper.elf
	avr-objcopy -j .text -j .data -O ihex $^ $@

flash: build/epaper.hex
	avrdude -p$(microcontroller) -c$(programmer) -P$(device) -b$(baudrate) -D -Uflash:w:$<:i

clean:
	rm -rf build
