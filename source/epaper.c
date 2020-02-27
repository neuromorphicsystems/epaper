#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <stddef.h>
#include <util/delay.h>

// serial baudrate
const uint32_t baudrate = 2000000;

// GPIOs
const uint8_t m1_cs_pin = DDD2;
const uint8_t s1_cs_pin = DDD3;
const uint8_t m2_cs_pin = DDC4;
const uint8_t s2_cs_pin = DDC0;
const uint8_t m1s1_dc_pin = DDD6;
const uint8_t m2s2_dc_pin = DDC3;
const uint8_t m1s1_reset_pin = DDD5;
const uint8_t m2s2_reset_pin = DDC2;
const uint8_t m1_busy_pin = DDD4;
const uint8_t s1_busy_pin = DDD7;
const uint8_t m2_busy_pin = DDC1;
const uint8_t s2_busy_pin = DDC5;
const uint8_t sram_cs1_pin = DDB2;
const uint8_t sram_cs2_pin = DDB1;
const uint8_t sram_cs3_pin = DDB0;

/// command bundles the parameters of an ePaper command.
struct command {
    enum {
        address_m1,
        address_s1,
        address_m2,
        address_s2,
    } address;
    uint8_t value;
};

/// serial_transfer sends a value over USART.
void serial_transfer(uint8_t value) {
    while (!((UCSR0A >> UDRE0) & 1)) {
    }
    UDR0 = value;
}

/// spi_transfer sends a value over SPI.
__attribute__((always_inline)) inline uint8_t spi_transfer(uint8_t value) {
    SPDR = value;
    while (!((SPSR >> SPIF) & 1)) {
    }
    return SPDR;
}

/// send_command sends a command and its data payload to the ePaper display.
#define send_command(port, dc_pin, cs_pin, value, data, size)                                                          \
    port &= ~((1 << cs_pin) | (1 << dc_pin));                                                                          \
    spi_transfer(value);                                                                                               \
    port |= (1 << cs_pin) | (1 << dc_pin);                                                                             \
    for (uint8_t index = 0; index < size; ++index) {                                                                   \
        port &= ~(1 << cs_pin);                                                                                        \
        spi_transfer(data[index]);                                                                                     \
        port |= (1 << cs_pin);                                                                                         \
    }
void send_command_m1(uint8_t value, const uint8_t* data, uint8_t size) {
    send_command(PORTD, m1s1_dc_pin, m1_cs_pin, value, data, size)
}
void send_command_s1(uint8_t value, const uint8_t* data, uint8_t size) {
    send_command(PORTD, m1s1_dc_pin, s1_cs_pin, value, data, size)
}
void send_command_m2(uint8_t value, const uint8_t* data, uint8_t size) {
    send_command(PORTC, m2s2_dc_pin, m2_cs_pin, value, data, size)
}
void send_command_s2(uint8_t value, const uint8_t* data, uint8_t size) {
    send_command(PORTC, m2s2_dc_pin, s2_cs_pin, value, data, size)
}
void send_command_m1m2(uint8_t value) {
    PORTC &= ~((1 << m2_cs_pin) | (1 << m2s2_dc_pin));
    PORTD &= ~((1 << m1_cs_pin) | (1 << m1s1_dc_pin));
    spi_transfer(value);
    PORTC |= (1 << m2_cs_pin) | (1 << m2s2_dc_pin);
    PORTD |= (1 << m1_cs_pin) | (1 << m1s1_dc_pin);
}
void send_command_all(uint8_t value, const uint8_t* data, uint8_t size) {
    PORTC &= ~((1 << m2_cs_pin) | (1 << s2_cs_pin) | (1 << m2s2_dc_pin));
    PORTD &= ~((1 << m1_cs_pin) | (1 << s1_cs_pin) | (1 << m1s1_dc_pin));
    spi_transfer(value);
    PORTC |= (1 << m2_cs_pin) | (1 << s2_cs_pin) | (1 << m2s2_dc_pin);
    PORTD |= (1 << m1_cs_pin) | (1 << s1_cs_pin) | (1 << m1s1_dc_pin);
    for (uint8_t index = 0; index < size; ++index) {
        PORTC &= ~((1 << m2_cs_pin) | (1 << s2_cs_pin));
        PORTD &= ~((1 << m1_cs_pin) | (1 << s1_cs_pin));
        spi_transfer(data[index]);
        PORTC |= (1 << m2_cs_pin) | (1 << s2_cs_pin);
        PORTD |= (1 << m1_cs_pin) | (1 << s1_cs_pin);
    }
}

/// forward pipes a frame from the USART to the SPI.
#define forward(port, dc_pin, cs_pin, address, maximum_count, state)                                                   \
    if ((UCSR0A >> RXC0) & 1) {                                                                                        \
        if (state.buffer_write != (state.buffer_read + 255) % 256) {                                                   \
            state.buffer[state.buffer_write] = UDR0;                                                                   \
            ++state.buffer_write;                                                                                      \
        }                                                                                                              \
    }                                                                                                                  \
    if (state.count == 0) {                                                                                            \
        port &= ~((1 << cs_pin) | (1 << dc_pin));                                                                      \
        SPDR = address;                                                                                                \
        ++state.count;                                                                                                 \
    } else if (state.count == 1) {                                                                                     \
        if (((SPSR >> SPIF) & 1)) {                                                                                    \
            SPDR;                                                                                                      \
            port |= (1 << cs_pin) | (1 << dc_pin);                                                                     \
            ++state.count;                                                                                             \
        }                                                                                                              \
    } else if (state.count == maximum_count + 2) {                                                                     \
        if (((SPSR >> SPIF) & 1)) {                                                                                    \
            SPDR;                                                                                                      \
            port |= (1 << cs_pin);                                                                                     \
            state.count = 0;                                                                                           \
            ++state.index;                                                                                             \
        }                                                                                                              \
    } else if (state.buffer_read != state.buffer_write) {                                                              \
        if (state.count == 2) {                                                                                        \
            port &= ~(1 << cs_pin);                                                                                    \
            SPDR = state.buffer[state.buffer_read];                                                                    \
            ++state.buffer_read;                                                                                       \
            ++state.count;                                                                                             \
        } else if ((SPSR >> SPIF) & 1) {                                                                               \
            SPDR;                                                                                                      \
            port |= (1 << cs_pin);                                                                                     \
            port &= ~(1 << cs_pin);                                                                                    \
            SPDR = state.buffer[state.buffer_read];                                                                    \
            ++state.buffer_read;                                                                                       \
            ++state.count;                                                                                             \
        }                                                                                                              \
    }                                                                                                                  \
    break

int main(void) {
    wdt_reset();
    wdt_disable();

    // configure pins
    DDRC |= (1 << m2_cs_pin) | (1 << s2_cs_pin) | (1 << m2s2_dc_pin) | (1 << m2s2_reset_pin);
    DDRC &= ~((1 << m2_busy_pin) | (1 << s2_busy_pin));
    PORTC |= (1 << m2_cs_pin) | (1 << s2_cs_pin) | (1 << m2s2_dc_pin);
    PORTC &= ~(1 << m2s2_reset_pin);
    DDRD |= (1 << m1_cs_pin) | (1 << s1_cs_pin) | (1 << m1s1_dc_pin) | (1 << m1s1_reset_pin);
    DDRD &= ~((1 << m1_busy_pin) | (1 << s1_busy_pin));
    PORTD |= (1 << m1_cs_pin) | (1 << s1_cs_pin) | (1 << m1s1_dc_pin);
    PORTD &= ~(1 << m1s1_reset_pin);
    DDRB |= (1 << sram_cs1_pin) | (1 << sram_cs2_pin) | (1 << sram_cs3_pin);
    PORTB |= (1 << sram_cs1_pin) | (1 << sram_cs2_pin) | (1 << sram_cs3_pin);

    // enable USART
    UBRR0H = ((F_CPU / 8) / baudrate - 1) >> 8;
    UBRR0L = ((F_CPU / 8) / baudrate - 1) & 0xff;
    UCSR0A |= (1 << U2X0);
    UCSR0B |= (1 << RXEN0) | (1 << TXEN0);
    UCSR0C |= (1 << UCSZ01) | (1 << UCSZ00);

    // enable SPI
    DDRB |= (1 << DDB2) | (1 << DDB3) | (1 << DDB5);
    DDRB &= ~(1 << DDB4);
    PORTB |= (1 << DDB3) | (1 << DDB5);
    SPCR = (1 << MSTR) | (1 << SPE);

    // initialise the display
    _delay_ms(200);
    PORTC |= (1 << m2s2_reset_pin);
    PORTD |= (1 << m1s1_reset_pin);
    _delay_ms(200);
    send_command_m1(0x00, (const uint8_t[]){0x2f}, 1);
    send_command_s1(0x00, (const uint8_t[]){0x2f}, 1);
    send_command_m2(0x00, (const uint8_t[]){0x23}, 1);
    send_command_s2(0x00, (const uint8_t[]){0x23}, 1);
    send_command_m1(0x01, (const uint8_t[]){0x07, 0x17, 0x3f, 0x3f, 0x0d}, 5);
    send_command_m2(0x01, (const uint8_t[]){0x07, 0x17, 0x3f, 0x3f, 0x0d}, 5);
    send_command_m1(0x06, (const uint8_t[]){0x17, 0x17, 0x39, 0x17}, 4);
    send_command_m2(0x06, (const uint8_t[]){0x17, 0x17, 0x39, 0x17}, 4);
    send_command_m1(0x61, (const uint8_t[]){0x02, 0x88, 0x01, 0xec}, 4);
    send_command_s1(0x61, (const uint8_t[]){0x02, 0x90, 0x01, 0xec}, 4);
    send_command_m2(0x61, (const uint8_t[]){0x02, 0x90, 0x01, 0xec}, 4);
    send_command_s2(0x61, (const uint8_t[]){0x02, 0x88, 0x01, 0xec}, 4);
    send_command_all(0x15, (const uint8_t[]){0x20}, 1);
    send_command_all(0x30, (const uint8_t[]){0x08}, 1);
    send_command_all(0x50, (const uint8_t[]){0x31, 0x07}, 2);
    send_command_all(0x60, (const uint8_t[]){0x22}, 1);
    send_command_m1(0xe0, (const uint8_t[]){0x01}, 1);
    send_command_m2(0xe0, (const uint8_t[]){0x01}, 1);
    send_command_all(0xe3, (const uint8_t[]){0x00}, 1);
    send_command_m1(0x82, (const uint8_t[]){0x1c}, 1);
    send_command_m2(0x82, (const uint8_t[]){0x1c}, 1);
    send_command_all(
        0x20,
        (const uint8_t[]){
            0x00, 0x10, 0x10, 0x01, 0x08, 0x01, 0x00, 0x06, 0x01, 0x06, 0x01, 0x05, 0x00, 0x08, 0x01,
            0x08, 0x01, 0x06, 0x00, 0x06, 0x01, 0x06, 0x01, 0x05, 0x00, 0x05, 0x01, 0x1e, 0x0f, 0x06,
            0x00, 0x05, 0x01, 0x1e, 0x0f, 0x01, 0x00, 0x04, 0x05, 0x08, 0x08, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        },
        60);
    send_command_all(
        0x21,
        (const uint8_t[]){
            0x91, 0x10, 0x10, 0x01, 0x08, 0x01, 0x04, 0x06, 0x01, 0x06, 0x01, 0x05, 0x84, 0x08, 0x01,
            0x08, 0x01, 0x06, 0x80, 0x06, 0x01, 0x06, 0x01, 0x05, 0x00, 0x05, 0x01, 0x1e, 0x0f, 0x06,
            0x00, 0x05, 0x01, 0x1e, 0x0f, 0x01, 0x08, 0x04, 0x05, 0x08, 0x08, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        },
        60);
    send_command_all(
        0x22,
        (const uint8_t[]){
            0xa8, 0x10, 0x10, 0x01, 0x08, 0x01, 0x84, 0x06, 0x01, 0x06, 0x01, 0x05, 0x84, 0x08, 0x01,
            0x08, 0x01, 0x06, 0x86, 0x06, 0x01, 0x06, 0x01, 0x05, 0x8c, 0x05, 0x01, 0x1e, 0x0f, 0x06,
            0x8c, 0x05, 0x01, 0x1e, 0x0f, 0x01, 0xf0, 0x04, 0x05, 0x08, 0x08, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        },
        60);
    send_command_all(
        0x23,
        (const uint8_t[]){
            0x91, 0x10, 0x10, 0x01, 0x08, 0x01, 0x04, 0x06, 0x01, 0x06, 0x01, 0x05, 0x84, 0x08, 0x01,
            0x08, 0x01, 0x06, 0x80, 0x06, 0x01, 0x06, 0x01, 0x05, 0x00, 0x05, 0x01, 0x1e, 0x0f, 0x06,
            0x00, 0x05, 0x01, 0x1e, 0x0f, 0x01, 0x08, 0x04, 0x05, 0x08, 0x08, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        },
        60);
    send_command_all(
        0x24,
        (const uint8_t[]){
            0x92, 0x10, 0x10, 0x01, 0x08, 0x01, 0x80, 0x06, 0x01, 0x06, 0x01, 0x05, 0x84, 0x08, 0x01,
            0x08, 0x01, 0x06, 0x04, 0x06, 0x01, 0x06, 0x01, 0x05, 0x00, 0x05, 0x01, 0x1e, 0x0f, 0x06,
            0x00, 0x05, 0x01, 0x1e, 0x0f, 0x01, 0x01, 0x04, 0x05, 0x08, 0x08, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        },
        60);
    send_command_all(
        0x25,
        (const uint8_t[]){
            0x91, 0x10, 0x10, 0x01, 0x08, 0x01, 0x04, 0x06, 0x01, 0x06, 0x01, 0x05, 0x84, 0x08, 0x01,
            0x08, 0x01, 0x06, 0x80, 0x06, 0x01, 0x06, 0x01, 0x05, 0x00, 0x05, 0x01, 0x1e, 0x0f, 0x06,
            0x00, 0x05, 0x01, 0x1e, 0x0f, 0x01, 0x08, 0x04, 0x05, 0x08, 0x08, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        },
        60);

    struct {
        uint8_t index;
        uint16_t count;
        uint8_t buffer[256];
        uint8_t buffer_read;
        uint8_t buffer_write;
    } state = {
        .index = 9,
        .count = 0,
        .buffer = {},
        .buffer_read = 0,
        .buffer_write = 0,
    };
    serial_transfer('r');
    for (;;) {
        switch (state.index) {
            case 0:
                forward(PORTD, m1s1_dc_pin, m1_cs_pin, 0x10, 81u * 492u, state);
            case 1:
                forward(PORTD, m1s1_dc_pin, m1_cs_pin, 0x13, 81u * 492u, state);
            case 2:
                forward(PORTD, m1s1_dc_pin, s1_cs_pin, 0x10, 82u * 492u, state);
            case 3:
                forward(PORTD, m1s1_dc_pin, s1_cs_pin, 0x13, 82u * 492u, state);
            case 4:
                forward(PORTC, m2s2_dc_pin, m2_cs_pin, 0x10, 82u * 492u, state);
            case 5:
                forward(PORTC, m2s2_dc_pin, m2_cs_pin, 0x13, 82u * 492u, state);
            case 6:
                forward(PORTC, m2s2_dc_pin, s2_cs_pin, 0x10, 81u * 492u, state);
            case 7:
                forward(PORTC, m2s2_dc_pin, s2_cs_pin, 0x13, 81u * 492u, state);
            case 8:
                if ((UCSR0A >> RXC0) & 1) {
                    UDR0;
                }
                send_command_m1m2(0x04);
                _delay_ms(300);
                send_command_all(0x12, NULL, 0);
                for (;;) {
                    send_command_m1(0x71, NULL, 0);
                    if ((PIND >> m1_busy_pin) & 1) {
                        break;
                    }
                }
                _delay_ms(300);
                send_command_all(0x02, NULL, 0);
                _delay_ms(300);
                send_command_all(0x07, (const uint8_t[]){0xa5}, 1);
                _delay_ms(300);
                serial_transfer('r');
                ++state.index;
                break;
            case 9:
                if ((UCSR0A >> RXC0) & 1) {
                    if (UDR0 == 'r') {
                        state.index = 0;
                        state.count = 0;
                        state.buffer_read = 0;
                        state.buffer_write = 0;
                    }
                }
                break;
            default:
                break;
        }
    }
}
