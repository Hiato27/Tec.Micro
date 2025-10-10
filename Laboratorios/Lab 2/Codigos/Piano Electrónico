#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

/* Pines  */
#define BUZZER_DDR   DDRB
#define BUZZER_PORT  PORTB
#define BUZZER_PIN   PB1              // OC1A (D9)

/* Pulsadores: */
static inline void teclas_ini(void){
    DDRC  &= ~0x3F;                   // PC0..PC5 entrada
    PORTC |=  0x3F;                   // pull-up
    DDRD  &= ~((1<<PD2)|(1<<PD3));    // D2/D3 entrada
    PORTD |=  ((1<<PD2)|(1<<PD3));    // pull-up
}

/* Devuelve índice 0..7 o -1 si ninguna */
static int8_t leer_tecla(void){
    if(!(PINC & (1<<PC0))) return 0;
    if(!(PINC & (1<<PC1))) return 1;
    if(!(PINC & (1<<PC2))) return 2;
    if(!(PINC & (1<<PC3))) return 3;
    if(!(PINC & (1<<PC4))) return 4;
    if(!(PINC & (1<<PC5))) return 5;
    if(!(PIND & (1<<PD2))) return 6;
    if(!(PIND & (1<<PD3))) return 7;
    return -1;
}

/*  UART 9600 8N1  */
static void uart_ini(uint16_t ubrr){
    UBRR0H = (uint8_t)(ubrr>>8);
    UBRR0L = (uint8_t)ubrr;           // 16 MHz, 9600 -> UBRR=103
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);   // RX/TX
    UCSR0C = (1<<UCSZ01)|(1<<UCSZ00); // 8N1
}
static inline void uart_tx(char c){ while(!(UCSR0A & (1<<UDRE0))); UDR0 = c; }
static void uart_print(const char* s){ while(*s) uart_tx(*s++); }
static inline bool uart_rx_disponible(void){ return (UCSR0A & (1<<RXC0)); }
static inline char uart_rx(void){ return UDR0; }

/*  Tonos con Timer1 CTC toggle en OC1A  */

static void tono_iniciar(uint16_t freq_hz){
    if(freq_hz == 0){
        TCCR1A = 0; TCCR1B = 0;
        PORTB &= ~(1<<BUZZER_PIN);
        return;
    }
    BUZZER_DDR  |= (1<<BUZZER_PIN);
    uint16_t ocr = (uint16_t)(F_CPU/(2UL*8UL*freq_hz) - 1UL);
    TCCR1A = (1<<COM1A0);             // toggle OC1A
    TCCR1B = (1<<WGM12) | (1<<CS11);  // CTC, presc 8
    OCR1A  = ocr;
}
static inline void tono_detener(void){
    TCCR1A = 0; TCCR1B = 0;
    PORTB &= ~(1<<BUZZER_PIN);
}

/*  Notas (Hz) — octava alta para mayor claridad --- */
#define NOTA_DO        523   // Do  (C5)
#define NOTA_RE        587   // Re  (D5)
#define NOTA_MI        659   // Mi  (E5)
#define NOTA_FA        698   // Fa  (F5)
#define NOTA_SOL       784   // Sol (G5)
#define NOTA_LA        880   // La  (A5)
#define NOTA_SI        988   // Si  (B5)
#define NOTA_DO_AGUDO 1047   // Do' (C6)

/* Mapa usado por el modo Piano */
static const uint16_t notas[8] = {
    NOTA_DO, NOTA_RE, NOTA_MI, NOTA_FA,
    NOTA_SOL, NOTA_LA, NOTA_SI, NOTA_DO_AGUDO
};
