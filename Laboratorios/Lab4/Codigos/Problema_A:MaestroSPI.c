#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define DHT_PIN         PD4
#define BTN_CCW         PD3
#define BTN_BUZZ        PD7
#define BTN_CW_BIT      PB0

#define SS_PIN          PB2
#define MOSI_PIN        PB3
#define MISO_PIN        PB4
#define SCK_PIN         PB5

static inline void spi_master_init(void) {
    DDRB |= (1<<MOSI_PIN) | (1<<SCK_PIN) | (1<<SS_PIN);
    DDRB &= ~(1<<MISO_PIN);
    SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0);
    SPSR = 0;
    PORTB |= (1<<SS_PIN);
}
static inline uint8_t spi_transfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1<<SPIF)));
    return SPDR;
}
static inline void spi_send_packet(uint8_t cmd, uint8_t val) {
    const uint8_t SYNC = 0xAA;
    uint8_t chk = SYNC ^ cmd ^ val;
    PORTB &= ~(1<<SS_PIN);
    spi_transfer(SYNC);
    spi_transfer(cmd);
    spi_transfer(val);
    spi_transfer(chk);
    PORTB |= (1<<SS_PIN);
}

static inline void adc_init(void){
    ADMUX  = (1<<REFS0);
    ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1);
}
static inline uint16_t adc_read(uint8_t ch){
    ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
    ADCSRA |= (1<<ADSC);
    while (ADCSRA & (1<<ADSC));
    return ADC;
}
static inline uint8_t map_10b_to_8b(uint16_t v){ return (uint8_t)((v*255UL)/1023UL); }

static inline void dht_pin_output(void){ DDRD |=  (1<<DHT_PIN); }
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
    *hum  = data[0];
    *temp = data[2];
    return 1;
}

static inline void io_init_inputs(void){
    DDRD  &= ~((1<<BTN_CCW) | (1<<BTN_BUZZ));
    PORTD |=  ((1<<BTN_CCW) | (1<<BTN_BUZZ));
    DDRB  &= ~(1<<BTN_CW_BIT);
    PORTB |=  (1<<BTN_CW_BIT);
    dht_pin_input_pullup();
}

#define LCD_ADDR 0x27

static void I2C_Init(void) {
    TWSR = 0x00;
    TWBR = 0x48;
    TWCR = (1 << TWEN);
}
static void I2C_Start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}
static void I2C_Stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    _delay_us(100);
}
static void I2C_Write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

static void LCD_SendByte(uint8_t data, uint8_t mode) {
    uint8_t high_n = data & 0xF0;
    uint8_t low_n  = (data << 4) & 0xF0;

    uint8_t d_h = high_n | mode | 0x08;
    uint8_t d_l = low_n  | mode | 0x08;

    I2C_Start();
    I2C_Write(LCD_ADDR << 1);

    I2C_Write(d_h | 0x04); _delay_us(1);
    I2C_Write(d_h);        _delay_us(50);

    I2C_Write(d_l | 0x04); _delay_us(1);
    I2C_Write(d_l);        _delay_us(50);

    I2C_Stop();
}

static void LCD_Write4(uint8_t high_n, uint8_t mode){
    uint8_t d = (high_n & 0xF0) | mode | 0x08;
    I2C_Start(); I2C_Write(LCD_ADDR << 1);
    I2C_Write(d | 0x04); _delay_us(1);
    I2C_Write(d);        _delay_us(50);
    I2C_Stop();
}

static void LCD_Clear(void) {
    LCD_SendByte(0x01, 0);
    _delay_ms(2);
}
static void LCD_Init(void) {
    _delay_ms(50);

    LCD_Write4(0x30, 0); _delay_ms(5);
    LCD_Write4(0x30, 0); _delay_us(150);
    LCD_Write4(0x30, 0); _delay_us(150);
    LCD_Write4(0x20, 0); _delay_us(80);

    LCD_SendByte(0x28, 0);
    LCD_SendByte(0x0C, 0);
    LCD_SendByte(0x06, 0);
    LCD_Clear();
}
static void LCD_SetCursor(uint8_t row, uint8_t col) {
    uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
    LCD_SendByte(addr, 0);
}
static void LCD_Print(const char* str) {
    while (*str) LCD_SendByte(*str++, 1);
}
static void LCD_PrintPadded(const char* s, uint8_t width){
    uint8_t i=0;
    while (*s && i<width){ LCD_SendByte(*s++,1); i++; }
    while (i<width){ LCD_SendByte(' ',1); i++; }
}

int main(void){
    spi_master_init();
    adc_init();
    io_init_inputs();
    I2C_Init();
    LCD_Init();

    LCD_Clear();
    LCD_Print("SPI Maestro OK");
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
        uint8_t cw   = (PINB & (1<<BTN_CW_BIT)) ? 0 : 1;
        uint8_t ccw  = (PIND & (1<<BTN_CCW))    ? 0 : 1;
        buzz_on      = (PIND & (1<<BTN_BUZZ))    ? 0 : 1;

        if (cw && !ccw)      dir = 1;
        else if (ccw && !cw) dir = 2;
        else                 dir = 0;

        pwm_led = map_10b_to_8b(adc_read(0));

        static uint16_t tick=0;
        if (++tick >= 500){
            tick=0;
            (void)dht_read(&hum, &temp);
        }

        spi_send_packet(0x01, dir);
        _delay_us(200);
        spi_send_packet(0x02, pwm_led);
        _delay_us(200);
        spi_send_packet(0x04, buzz_on ? 1 : 0);

        ui_ms += 2;
        uint8_t need_update = 0;
        if (dir!=last_dir || pwm_led!=last_pwm || temp!=last_t || hum!=last_h || buzz_on!=last_b){
            need_update = 1;
        }
        if (ui_ms >= 100 || need_update){
            ui_ms = 0;
            last_dir=dir; last_pwm=pwm_led; last_t=temp; last_h=hum; last_b=buzz_on;

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
