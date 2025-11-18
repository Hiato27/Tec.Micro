#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define SLAVE_ADDR 0x12

#define MOTOR_IN1     PD2
#define MOTOR_IN2     PD3
#define MOTOR_PWM_PIN PD6
#define LED_PWM_PIN   PD5
#define BUZZER_PIN    PB1

volatile uint8_t reg_sel=0xFF, val_buf=0, have_pair=0;

static inline void motor_set_dir(uint8_t dir){
    switch(dir){
        case 1:
            PORTD |=  (1<<MOTOR_IN1);
            PORTD &= ~(1<<MOTOR_IN2);
            break;
        case 2:
            PORTD &= ~(1<<MOTOR_IN1);
            PORTD |=  (1<<MOTOR_IN2);
            break;
        default:
            PORTD &= ~((1<<MOTOR_IN1)|(1<<MOTOR_IN2));
            break;
    }
}
static inline void buzzer_on(void){ TCCR1A |=  (1<<COM1A0); }
static inline void buzzer_off(void){
    TCCR1A &= ~(1<<COM1A0);
    PORTB  &= ~(1<<BUZZER_PIN);
}
static inline void apply_pair(uint8_t r, uint8_t v){
    switch(r){
        case 0x01: motor_set_dir(v); break;
        case 0x02: OCR0B = v;       break;
        case 0x04: if(v) buzzer_on(); else buzzer_off(); break;
        default: break;
    }
}

static void io_pwm_init(void){
    DDRD |= (1<<MOTOR_IN1) | (1<<MOTOR_IN2);
    PORTD &= ~((1<<MOTOR_IN1)|(1<<MOTOR_IN2));

    DDRD |= (1<<MOTOR_PWM_PIN) | (1<<LED_PWM_PIN);
    TCCR0A = (1<<COM0A1)|(1<<COM0B1)|(1<<WGM01)|(1<<WGM00);
    TCCR0B = (1<<CS01)|(1<<CS00);
    OCR0A = 200;
    OCR0B = 0;

    DDRB  |= (1<<BUZZER_PIN);
    TCCR1A = 0;
    TCCR1B = 0;
    OCR1A  = 499;
    TCCR1B = (1<<WGM12)|(1<<CS11);
    buzzer_off();
}

static inline void i2c_slave_init(uint8_t addr7){
    TWAR = (addr7<<1);
    TWCR = (1<<TWEA)|(1<<TWEN)|(1<<TWIE);
}

ISR(TWI_vect){
    uint8_t st = TWSR & 0xF8;
    switch(st){
        case 0x60: case 0x68:
            reg_sel=0xFF; have_pair=0;
            TWCR=(1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;

        case 0x80:
        {
            uint8_t d = TWDR;
            if(reg_sel==0xFF) reg_sel = d;
            else { val_buf = d; have_pair = 1; }
            TWCR=(1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
        } break;

        case 0xA0:
            if(have_pair){ apply_pair(reg_sel, val_buf); have_pair=0; }
            TWCR=(1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;

        case 0xA8: case 0xB0:
            TWDR = 0x00;
            TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWIE);
            break;

        case 0xC0: case 0xC8: default:
            TWCR=(1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            break;
    }
}

int main(void){
    io_pwm_init();
    i2c_slave_init(SLAVE_ADDR);
    sei();
    for(;;){ }
}
