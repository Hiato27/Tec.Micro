#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define SLAVE_ADDR 0x12
#define LED_PIN PD6

volatile uint8_t resp = 0x00;

static void led_init(void){
    DDRD |= (1<<LED_PIN);
    PORTD &= ~(1<<LED_PIN);
}
static inline void led_on(void){ PORTD |= (1<<LED_PIN); }
static inline void led_off(void){ PORTD &= ~(1<<LED_PIN); }

static void i2c_slave_init(uint8_t addr7){
    TWAR = (addr7<<1);
    TWCR = (1<<TWEA)|(1<<TWEN)|(1<<TWIE)|(1<<TWINT);
}

ISR(TWI_vect){
    uint8_t st = TWSR & 0xF8;
    switch(st){
        case 0x60:
        case 0x68:
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;

        case 0x80:
        case 0x88:
        {
            uint8_t d = TWDR;
            if(d == 0xA5){
                resp = 0x5A;
                led_on();
            }else{
                resp = 0x00;
                led_off();
            }
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
        }
        break;

        case 0xA8:
        case 0xB0:
            TWDR = resp;
            TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWIE);
            break;

        case 0xC0:
        case 0xC8:
        case 0xA0:
        default:
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;
    }
}

int main(void){
    led_init();
    i2c_slave_init(SLAVE_ADDR);
    sei();
    while(1){}
}
