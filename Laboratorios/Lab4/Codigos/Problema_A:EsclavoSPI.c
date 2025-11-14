#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#define MOTOR_IN1       PD2
#define MOTOR_IN2       PD3
#define MOTOR_PWM_PIN   PD6
#define LED_PWM_PIN     PD5
#define BUZZER_PIN      PB1

static inline void spi_slave_init(void){
    DDRB &= ~((1<<PB3)|(1<<PB5)|(1<<PB2));
    DDRB |=  (1<<PB4);
    SPCR = (1<<SPE);
}
static inline uint8_t spi_recv(void){
    while(!(SPSR & (1<<SPIF)));
    return SPDR;
}

static inline void pwm_init(void){
    DDRD |= (1<<MOTOR_PWM_PIN) | (1<<LED_PWM_PIN);
    TCCR0A = (1<<COM0A1) | (1<<COM0B1) | (1<<WGM01) | (1<<WGM00);
    TCCR0B = (1<<CS01) | (1<<CS00);
    OCR0A = 200;
    OCR0B = 0;
}

static inline void motor_gpio_init(void){
    DDRD |= (1<<MOTOR_IN1) | (1<<MOTOR_IN2);
    PORTD &= ~((1<<MOTOR_IN1)|(1<<MOTOR_IN2));
}
static inline void motor_set_dir(uint8_t dir){
    switch(dir){
        case 1:
        PORTD |=  (1<<MOTOR_IN1);
        PORTD &= ~(1<<MOTOR_IN2);
        break;
        case 2:
        PORTD &= ~(1<<MOTOR_IN1);
        PORTD |=  (1<<MOTOR_IN2);
        break;
        default:
        PORTD &= ~((1<<MOTOR_IN1)|(1<<MOTOR_IN2));
        break;
    }
}

static inline void buzzer_init(void){
    DDRB |= (1<<BUZZER_PIN);
    TCCR1A = 0;
    TCCR1B = 0;
    OCR1A  = 499;
    TCCR1A &= ~(1<<COM0A0);
    PORTB  &= ~(1<<BUZZER_PIN);
    TCCR1B = (1<<WGM12) | (1<<CS11);
}
static inline void buzzer_on(void){
    TCCR1A |= (1<<COM1A0);
}
static inline void buzzer_off(void){
    TCCR1A &= ~(1<<COM1A0);
    PORTB  &= ~(1<<BUZZER_PIN);
}

static inline bool packet_ok(uint8_t s, uint8_t c, uint8_t v, uint8_t k){
    return (s==0xAA) && ((s ^ c ^ v) == k);
}

int main(void){
    spi_slave_init();
    pwm_init();
    motor_gpio_init();
    buzzer_init();

    uint8_t sync, cmd, val, chk;

    while(1){
        sync = spi_recv();
        cmd  = spi_recv();
        val  = spi_recv();
        chk  = spi_recv();

        if(!packet_ok(sync, cmd, val, chk)){
            continue;
        }

        if(cmd == 0x01){
            motor_set_dir(val);
        } else if(cmd == 0x02){
            OCR0B = val;
        } else if(cmd == 0x04){
            if(val) buzzer_on();
            else    buzzer_off();
        }
    }
    return 0;
}
