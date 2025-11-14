
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

#define SS_PIN    PB2
#define MOSI_PIN  PB3
#define MISO_PIN  PB4
#define SCK_PIN   PB5

#define LED_OK_PIN   PD6

static void spi_master_init(void){
    DDRB |= (1<<MOSI_PIN) | (1<<SCK_PIN) | (1<<SS_PIN);
    DDRB &= ~(1<<MISO_PIN);
    SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0);
    SPSR = 0;
    PORTB |= (1<<SS_PIN);
}

static uint8_t spi_transfer(uint8_t data){
    SPDR = data;
    while(!(SPSR & (1<<SPIF)));
    return SPDR;
}

static void led_init(void){
    DDRD |= (1<<LED_OK_PIN);
    PORTD &= ~(1<<LED_OK_PIN);
}

static inline void led_on(void){  PORTD |=  (1<<LED_OK_PIN); }
static inline void led_off(void){ PORTD &= ~(1<<LED_OK_PIN); }

static bool spi_handshake(void){
    uint8_t resp;
    PORTB &= ~(1<<SS_PIN);
    (void)spi_transfer(0xA5);
    resp = spi_transfer(0x00);
    PORTB |= (1<<SS_PIN);
    return (resp == 0x5A);
}

int main(void){
    spi_master_init();
    led_init();

    while(1){
        if(spi_handshake()){
            led_on();
        }else{
            led_off();
            _delay_ms(100);
        }
        _delay_ms(400);
    }
}
