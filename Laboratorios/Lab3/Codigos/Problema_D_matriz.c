#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define LED_PORT     PORTD
#define LED_DDR      DDRD
#define LED_PIN      PD2
#define LED_MASK     (1<<LED_PIN)

#define N_LEDS 64  // matriz 8x8

// Buffer de LEDs en formato WS2812: G,R,B
static uint8_t matriz_rgb[N_LEDS][3];

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

int main(void){
    // PD2 salida
    LED_DDR  |= LED_MASK;
    LED_PORT &= ~LED_MASK;

    for(uint8_t i=0; i<N_LEDS; i++){
        matriz_rgb[i][0] = 0; 
        matriz_rgb[i][1] = 0; 
        matriz_rgb[i][2] = 0; 
    }

    matriz_rgb[0][0] = 0;   
    matriz_rgb[0][1] = 255; 
    matriz_rgb[0][2] = 0;   

    while(1){
        enviar_matriz();
        _delay_ms(100);
    }
}
