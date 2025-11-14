
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

#define MOSI_PIN  PB3
#define MISO_PIN  PB4
#define SCK_PIN   PB5
#define SS_PIN    PB2

#define LED_SLAVE_PIN  PD6

static void spi_slave_init(void){
    DDRB &= ~((1<<MOSI_PIN)|(1<<SCK_PIN)|(1<<SS_PIN));
    DDRB |=  (1<<MISO_PIN);
    SPCR = (1<<SPE);
}

static void led_init(void){
    DDRD |= (1<<LED_SLAVE_PIN);
    PORTD &= ~(1<<LED_SLAVE_PIN);
}

static inline void led_on(void){  PORTD |=  (1<<LED_SLAVE_PIN); }
static inline void led_off(void){ PORTD &= ~(1<<LED_SLAVE_PIN); }

int main(void){
    spi_slave_init();
    led_init();

    uint8_t cmd;
    SPDR = 0x00;

    while(1){
        while(!(SPSR & (1<<SPIF)));
        cmd = SPDR;

        if(cmd == 0xA5){
            SPDR = 0x5A;
            led_on();
        } else {
            SPDR = 0x00;
            led_off();
        }
    }
}
