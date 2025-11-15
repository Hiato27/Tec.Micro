#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>

#define SLAVE_ADDR 0x12
#define LED_PIN PD6

static void led_init(void){
    DDRD |= (1<<LED_PIN);
    PORTD &= ~(1<<LED_PIN);
}
static inline void led_on(void){ PORTD |= (1<<LED_PIN); }
static inline void led_off(void){ PORTD &= ~(1<<LED_PIN); }

static void i2c_init(void){
    TWSR = 0x00;
    TWBR = 72;
    TWCR = (1<<TWEN);
}
static void i2c_start(void){
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
    while(!(TWCR & (1<<TWINT)));
}
static void i2c_stop(void){
    TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
}
static void i2c_write(uint8_t b){
    TWDR = b;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while(!(TWCR & (1<<TWINT)));
}
static uint8_t i2c_read_nack(void){
    TWCR = (1<<TWINT)|(1<<TWEN);
    while(!(TWCR & (1<<TWINT)));
    return TWDR;
}

static bool i2c_handshake(void){
    uint8_t resp;
    i2c_start();
    i2c_write((SLAVE_ADDR<<1) | 0);
    i2c_write(0xA5);
    i2c_stop();

    _delay_ms(2);

    i2c_start();
    i2c_write((SLAVE_ADDR<<1) | 1);
    resp = i2c_read_nack();
    i2c_stop();

    return (resp == 0x5A);
}

int main(void){
    i2c_init();
    led_init();

    while(1){
        if(i2c_handshake()){
            led_on();
        }else{
            led_off();
            _delay_ms(100);
        }
        _delay_ms(400);
    }
}
