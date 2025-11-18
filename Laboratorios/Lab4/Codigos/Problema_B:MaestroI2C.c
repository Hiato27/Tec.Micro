#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define DHT_PIN         PD4
#define BTN_CCW         PD3
#define BTN_BUZZ        PD7
#define BTN_CW_BIT      PB0

#define I2C_FREQ    100000UL
#define SLAVE_ADDR  0x12
#define LCD_ADDR    0x27

static inline void adc_init(void){
    ADMUX  = (1<<REFS0);
    ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1);
}
static inline uint16_t adc_read(uint8_t ch){
    ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
    ADCSRA |= (1<<ADSC);
    while (ADCSRA & (1<<ADSC));
    return ADC;
}
static inline uint8_t map_10b_to_8b(uint16_t v){ return (uint8_t)((v*255UL)/1023UL); }

static inline void dht_pin_output(void){ DDRD |=  (1<<DHT_PIN); }
static inline void dht_pin_input_pullup(void){ DDRD &= ~(1<<DHT_PIN); PORTD |= (1<<DHT_PIN); }

static uint8_t dht_read(uint8_t *hum, uint8_t *temp){
    uint8_t data[5]={0};
    dht_pin_output();
    PORTD &= ~(1<<DHT_PIN);
    _delay_ms(18);
    PORTD |= (1<<DHT_PIN);
    _delay_us(30);
    dht_pin_input_pullup();

    uint16_t t=0;
    while ((PIND & (1<<DHT_PIN))) { if (++t>20000) return 0; }
    t=0; while (!(PIND & (1<<DHT_PIN))) { if (++t>20000) return 0; }
    t=0; while ((PIND & (1<<DHT_PIN))) { if (++t>20000) return 0; }

    for (uint8_t i=0;i<40;i++){
        t=0; while (!(PIND & (1<<DHT_PIN))) { if (++t>20000) return 0; }
        _delay_us(30);
        if (PIND & (1<<DHT_PIN)){
            data[i/8] |= (1<<(7-(i%8)));
            t=0; while ((PIND & (1<<DHT_PIN))) { if (++t>20000) break; }
        }
    }
    uint8_t sum = data[0]+data[1]+data[2]+data[3];
    if (sum != data[4]) return 0;
    *hum  = data[0];
    *temp = data[2];
    return 1;
}

static inline void io_init_inputs(void){
    DDRD  &= ~((1<<BTN_CCW) | (1<<BTN_BUZZ));
    PORTD |=  ((1<<BTN_CCW) | (1<<BTN_BUZZ));
    DDRB  &= ~(1<<BTN_CW_BIT);
    PORTB |=  (1<<BTN_CW_BIT);
    dht_pin_input_pullup();
}

static inline void I2C_Init(void){
    TWSR = 0x00;
    TWBR = (uint8_t)((F_CPU/I2C_FREQ - 16)/2);
    TWCR = (1<<TWEN);
}
static inline void I2C_Start(void){
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
    while(!(TWCR & (1<<TWINT)));
}
static inline void I2C_Stop(void){
    TWCR = (1<<TWINT)|(1<<TWSTO)|(1<<TWEN);
}
static inline void I2C_Write(uint8_t b){
    TWDR = b;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while(!(TWCR & (1<<TWINT)));
}
static inline void i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t val){
    I2C_Start();
    I2C_Write((addr7<<1) | 0);
    I2C_Write(reg);
    I2C_Write(val);
    I2C_Stop();
}

#define LCD_RS 0x01
#define LCD_RW 0x02
#define LCD_E  0x04
#define LCD_BL 0x08

static inline void LCD_I2C_Send(uint8_t x){ I2C_Write(x); }
static void LCD_Pulse(uint8_t data){
    LCD_I2C_Send(data | LCD_E);   _delay_us(1);
    LCD_I2C_Send(data & ~LCD_E);  _delay_us(50);
}
static void LCD_Write4(uint8_t nibble, uint8_t rs){
    uint8_t base = (nibble & 0xF0) | LCD_BL | (rs ? LCD_RS : 0);
    I2C_Start(); I2C_Write(LCD_ADDR<<1);
    LCD_Pulse(base);
    I2C_Stop();
}
static void LCD_SendByte(uint8_t data, uint8_t rs){
    uint8_t high_n = data & 0xF0;
    uint8_t low_n  = (data << 4) & 0xF0;
    I2C_Start(); I2C_Write(LCD_ADDR<<1);
    LCD_Pulse(high_n | LCD_BL | (rs?LCD_RS:0));
    LCD_Pulse(low_n  | LCD_BL | (rs?LCD_RS:0));
    I2C_Stop();
}
static inline void LCD_Cmd(uint8_t c){ LCD_SendByte(c, 0); }
static inline void LCD_Clear(void){ LCD_Cmd(0x01); _delay_ms(2); }
static void LCD_Init(void){
    _delay_ms(50);
    LCD_Write4(0x30, 0); _delay_ms(5);
    LCD_Write4(0x30, 0); _delay_us(150);
    LCD_Write4(0x30, 0); _delay_us(150);
    LCD_Write4(0x20, 0); _delay_us(80);

    LCD_Cmd(0x28);
    LCD_Cmd(0x0C);
    LCD_Cmd(0x06);
    LCD_Clear();
}
static inline void LCD_SetCursor(uint8_t row, uint8_t col){
    uint8_t addr = (row==0) ? (0x80 + col) : (0xC0 + col);
    LCD_Cmd(addr);
}
static void LCD_Print(const char* s){ while(*s) LCD_SendByte(*s++, 1); }
static void LCD_PrintPadded(const char* s, uint8_t width){
    uint8_t i=0;
    while (*s && i<width){ LCD_SendByte(*s++,1); i++; }
    while (i<width){ LCD_SendByte(' ',1); i++; }
}

int main(void){
    I2C_Init();
    LCD_Init();
    adc_init();
    io_init_inputs();

    LCD_Clear();
    LCD_Print("I2C Maestro OK");
    LCD_SetCursor(1,0);
    LCD_Print("Inicializando...");
    _delay_ms(800);

    uint8_t hum=0, temp=0;
    uint8_t dir=0;
    uint8_t pwm_led=0;
    uint8_t buzz_on=0;

    uint16_t ui_ms = 0;
    uint8_t last_dir=255, last_pwm=255, last_t=255, last_h=255, last_b=255;

    while (1){
        uint8_t cw   = (PINB & (1<<BTN_CW_BIT)) ? 0 : 1;
        uint8_t ccw  = (PIND & (1<<BTN_CCW))     ? 0 : 1;
        buzz_on      = (PIND & (1<<BTN_BUZZ))    ? 0 : 1;

        if (cw && !ccw)      dir = 1;
        else if (ccw && !cw) dir = 2;
        else                 dir = 0;

        pwm_led = map_10b_to_8b(adc_read(0));

        static uint16_t tick=0;
        if (++tick >= 500){
            tick=0;
            (void)dht_read(&hum, &temp);
        }

        if (dir != last_dir){
            i2c_write_reg(SLAVE_ADDR, 0x01, dir);
            last_dir = dir;
        }

        if (last_pwm==255 || (pwm_led > last_pwm+3) || (pwm_led+3 < last_pwm)){
            i2c_write_reg(SLAVE_ADDR, 0x02, pwm_led);
            last_pwm = pwm_led;
        }

        if (buzz_on != last_b){
            i2c_write_reg(SLAVE_ADDR, 0x04, buzz_on ? 1 : 0);
            last_b = buzz_on;
        }

        ui_ms += 2;
        uint8_t need_update = 0;
        if (dir!=last_dir || pwm_led!=last_pwm || temp!=last_t || hum!=last_h || buzz_on!=last_b){
            need_update = 1;
        }
        if (ui_ms >= 100 || need_update){
            ui_ms = 0;
            last_t=temp; last_h=hum;
            char line[17];

            LCD_SetCursor(0,0);
            snprintf(line, sizeof(line), "T:%2uC H:%2u%%", temp, hum);
            LCD_PrintPadded(line, 16);

            LCD_SetCursor(1,0);
            const char* dstr = (dir==1)?"H":(dir==2)?"AH":"STOP";
            snprintf(line, sizeof(line), "M:%s L:%3u B:%c", dstr, pwm_led, buzz_on?'1':'0');
            LCD_PrintPadded(line, 16);
        }

        _delay_ms(2);
    }
}
