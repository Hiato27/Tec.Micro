#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>

#define LED_PORT     PORTD
#define LED_DDR      DDRD
#define LED_PIN      PD2
#define LED_MASK     (1<<LED_PIN)

#define ADC_MEDIO    512
#define ZONA_MUERTA  80

#define ANCHO        8
#define ALTO         8
#define N_LEDS       (ANCHO*ALTO)

static uint8_t matriz_rgb[N_LEDS][3];

static uint8_t pos_x = 3;
static uint8_t pos_y = 3;

static uint8_t col_rojo  = 255;
static uint8_t col_verde = 0;
static uint8_t col_azul  = 0;

static uint8_t idx_xy(uint8_t x, uint8_t y){
    if(y % 2 == 0){
        return y*ANCHO + x;
    } else {
        return y*ANCHO + (ANCHO-1-x);
    }
}

static void actualizar_buf(void){
    for(uint8_t i=0; i<N_LEDS; i++){
        matriz_rgb[i][0] = 0;
        matriz_rgb[i][1] = 0;
        matriz_rgb[i][2] = 0;
    }
    uint8_t idx = idx_xy(pos_x, pos_y);
    matriz_rgb[idx][0] = col_verde;
    matriz_rgb[idx][1] = col_rojo;
    matriz_rgb[idx][2] = col_azul;
}

static inline void enviar_bit(uint8_t bit_val){
    if(bit_val){
        LED_PORT |=  LED_MASK;
        __asm__ __volatile__ (
            "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
            "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
            "nop\n\t""nop\n\t"
        );
        LED_PORT &= ~LED_MASK;
        __asm__ __volatile__ (
            "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
            "nop\n\t"
        );
    } else {
        LED_PORT |=  LED_MASK;
        __asm__ __volatile__ (
            "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
        );
        LED_PORT &= ~LED_MASK;
        __asm__ __volatile__ (
            "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
            "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
            "nop\n\t""nop\n\t"
        );
    }
}

static inline void enviar_byte(uint8_t dato){
    for(uint8_t i=0; i<8; i++){
        enviar_bit(dato & (1<<(7-i)));
    }
}

static void enviar_matriz(void){
    cli();
    for(uint8_t i=0; i<N_LEDS; i++){
        enviar_byte(matriz_rgb[i][0]);
        enviar_byte(matriz_rgb[i][1]);
        enviar_byte(matriz_rgb[i][2]);
    }
    sei();
    _delay_us(60);
}

static void adc_init(void){
    ADMUX  = (1<<REFS0); 
    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0); 
}

static uint16_t adc_leer(uint8_t canal){
    ADMUX = (ADMUX & 0xF0) | (canal & 0x0F);
    ADCSRA |= (1<<ADSC);
    while(ADCSRA & (1<<ADSC)){;}
    return ADC;
}

int main(void){
    LED_DDR  |= LED_MASK;
    LED_PORT &= ~LED_MASK;

    adc_init();

    while(1){
        uint16_t val_x = adc_leer(1); // VRx en A1
        uint16_t val_y = adc_leer(0); // VRy en A0

        int8_t mov_x = 0;
        int8_t mov_y = 0;

        if(val_x > (ADC_MEDIO + ZONA_MUERTA)) mov_x = +1;
        else if(val_x < (ADC_MEDIO - ZONA_MUERTA)) mov_x = -1;

        if(val_y > (ADC_MEDIO + ZONA_MUERTA)) mov_y = -1; // arriba
        else if(val_y < (ADC_MEDIO - ZONA_MUERTA)) mov_y = +1; // abajo

        // Actualizar posición 
        int16_t nueva_x = pos_x + mov_x;
        int16_t nueva_y = pos_y + mov_y;

        if(nueva_x < 0) nueva_x = 0;
        if(nueva_x > 7) nueva_x = 7;
        if(nueva_y < 0) nueva_y = 0;
        if(nueva_y > 7) nueva_y = 7;

        pos_x = (uint8_t)nueva_x;
        pos_y = (uint8_t)nueva_y;

        actualizar_buf();
        enviar_matriz();

        _delay_ms(120); 
    }
}
