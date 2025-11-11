#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define LED_PORT PORTD
#define LED_DDR  DDRD
#define LED_PIN  PD2
#define LED_MASK (1<<LED_PIN)

#define ANCHO 8
#define ALTO  8
#define N_LEDS (ANCHO*ALTO)
#define SERPENTINE 0
#define BRILLO_SHIFT 0

static uint8_t matriz_rgb[N_LEDS][3];
static uint8_t pos_x = 3, pos_y = 3;
static uint8_t col_r=255, col_g=0, col_b=0;

static uint8_t idx_xy(uint8_t x, uint8_t y){
#if SERPENTINE
    if (y & 1) return (y * ANCHO) + (ANCHO - 1 - x);
    else       return (y * ANCHO) + x;
#else
    return (y * ANCHO) + x;
#endif
}

static void actualizar_buf(void){
    for(uint8_t i=0;i<N_LEDS;i++){ matriz_rgb[i][0]=0; matriz_rgb[i][1]=0; matriz_rgb[i][2]=0; }
    uint8_t k = idx_xy(pos_x,pos_y);
    matriz_rgb[k][0] = col_g >> BRILLO_SHIFT;
    matriz_rgb[k][1] = col_r >> BRILLO_SHIFT;
    matriz_rgb[k][2] = col_b >> BRILLO_SHIFT;
}

static inline void ws2812_send_byte(uint8_t dato){
    asm volatile(
        "ldi  r18, 8\n\t"
        "1:\n\t"
        "sbi  %[port], %[bit]\n\t"
        "sbrs %[dato], 7\n\t"
        "cbi  %[port], %[bit]\n\t"
        "nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t"
        "cbi  %[port], %[bit]\n\t"
        "lsl  %[dato]\n\t"
        "dec  r18\n\t"
        "brne 1b\n\t"
        : [dato] "+r"(dato)
        : [port] "I" (_SFR_IO_ADDR(PORTD)), [bit] "I"(LED_PIN)
        : "r18"
    );
}

static void enviar_matriz(void){
    uint8_t sreg = SREG; cli();
    for(uint8_t i=0;i<N_LEDS;i++){
        ws2812_send_byte(matriz_rgb[i][0]);
        ws2812_send_byte(matriz_rgb[i][1]);
        ws2812_send_byte(matriz_rgb[i][2]);
    }
    SREG = sreg;
    _delay_us(70);
}

int main(void){
    LED_DDR |= LED_MASK;
    LED_PORT &= ~LED_MASK;
    actualizar_buf();
    while(1){
        enviar_matriz();
        _delay_ms(30);
    }
}
