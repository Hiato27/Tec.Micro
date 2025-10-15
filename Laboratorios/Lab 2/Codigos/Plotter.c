#define F_CPU 16000000UL
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

/* UART */
#define UBRR_9600 103u  /* 16 MHz, 9600 bps */

/* Pines (PORTD)
   PD2, PD3: auxiliares
   PD4: X+
   PD5: Y-
   PD6: X-
   PD7: Y+
*/
#define PD_X_POS  PD4
#define PD_Y_NEG  PD5
#define PD_X_NEG  PD6
#define PD_Y_POS  PD7

/* Escalas */
#define DELAY_TRI_SEG   16u
#define DELAY_CRUZ_SEG  14u

/* Prototipos UART */
static void uart_init(void);
static void uart_tx(uint8_t c);
static uint8_t uart_rx(void);
static void print_crlf(void);
static void print_str(const char *s);

/* Delays base */
static void delay_1s(void);
static void delay_2s(void);
static void delay_3s(void);
static void delay_10s(void);
static void delay_25s(void);

/* Delays dedicados */
static void delay_tri(void);
static void delay_cruz(void);

/* Helpers de salida */
static inline void out_delay(uint8_t mask, void (*delay_fn)(void)) {
    PORTD = mask;
    delay_fn();
}
static inline void out_delay_ms(uint8_t mask, uint16_t ms) {
    PORTD = mask;
    while (ms--) _delay_ms(1);
}

/* Figuras */
static void run_sequence(void);   /* Triangulo */
static void run_sequence2(void);  /* Cruz */
static void run_sequence3(void);  /* Circulo */
static void run_conejo(void);     /* Conejo */
static void run_murcielago(void);     /* Murcielago */

/* Menu */
static void print_menu(void);

/* Textos */
static const char STR_BANNER[]  = "===== MENU PRINCIPAL =====\r\n";
static const char STR_M1[]      = "1) Dibujar Triangulo\r\n";
static const char STR_M2[]      = "2) Dibujar Circulo\r\n";
static const char STR_M3[]      = "3) Dibujar Cruz\r\n";
static const char STR_M5[]      = "4) Dibujar Conejo\r\n";
static const char STR_M6[]      = "5) Dibujar Murcielago\r\n";
static const char STR_M4[]      = "T) Dibujar todas las Figuras\r\n";
static const char STR_PROMPT[]  = "Seleccione (1, 2, 3, 4, 5 o T): ";
static const char STR_INV[]     = "\r\nOpcion invalida. Intente de nuevo.\r\n";
static const char STR_H1[]      = "[Opcion 1: Iniciando Triangulo]";
static const char STR_H2[]      = "[Opcion 3: Iniciando Cruz]";
static const char STR_H3[]      = "[Opcion 2: Iniciando Circulo]";
static const char STR_H5[]      = "[Opcion 4: Iniciando Conejo]";
static const char STR_H6[]      = "[Opcion 5: Iniciando Conejo]";
static const char STR_H4[]      = "[Opcion T: Iniciando todas las figuras]";

/* INICIO */
int main(void) {
    uart_init();

    /* PD2..PD7 salida, LOW */
    DDRD  = (1u<<PD2)|(1u<<PD3)|(1u<<PD4)|(1u<<PD5)|(1u<<PD6)|(1u<<PD7);
    PORTD = 0x00;

    for (;;) {
        print_menu();

        uint8_t key;
        for (;;) {
            key = uart_rx();
            if (key == 0x0D || key == 0x0A) continue;
            break;
        }
        uart_tx(key);
        print_crlf();

        if (key == '1') {
            print_str(STR_H1); print_crlf();
            out_delay((1u<<PD_X_POS), delay_tri);
            out_delay((1u<<PD_X_POS), delay_tri);
            run_sequence();     /* Triangulo */

        } else if (key == '2') {
            print_str(STR_H3); print_crlf();
            out_delay((1u<<PD_X_POS), delay_25s);
            out_delay((1u<<PD_X_POS), delay_25s);
            out_delay((1u<<PD_X_POS), delay_25s);
            out_delay((1u<<PD_X_POS), delay_25s);
            out_delay((1u<<PD_X_POS), delay_25s);
            out_delay((1u<<PD_Y_POS), delay_25s);
            run_sequence3();    /* Circulo */

        } else if (key == '3') {
            print_str(STR_H2); print_crlf();
            out_delay((1u<<PD_Y_POS), delay_cruz);
            out_delay((1u<<PD_Y_POS), delay_cruz);
            out_delay((1u<<PD_Y_POS), delay_cruz);
            out_delay((1u<<PD_Y_POS), delay_cruz);
            out_delay((1u<<PD_X_POS), delay_cruz);
            out_delay((1u<<PD_X_POS), delay_cruz);
            out_delay((1u<<PD_X_POS), delay_cruz);
            out_delay((1u<<PD_X_POS), delay_cruz);
            run_sequence2();    /* Cruz */

        } else if (key == '4') {
            print_str(STR_H5); print_crlf();
            run_conejo();       /* Conejo */
			
		} else if (key == '5') {
			print_str(STR_H6); print_crlf();
			run_murcielago();       /* murcielago */

        } else if (key == 'T') {
            print_str(STR_H4); print_crlf();
            run_sequence();
            run_sequence3();
            run_sequence2();
			run_conejo();
			run_murcielago();

        } else {
            print_str(STR_INV);
        }
    }
}

/* UART */
static void uart_init(void) {
    UBRR0H = (uint8_t)(UBRR_9600 >> 8);
    UBRR0L = (uint8_t)(UBRR_9600 & 0xFF);
    UCSR0A = 0x00;                          /* U2X0=0 */
    UCSR0B = (1u<<RXEN0) | (1u<<TXEN0);     /* RX/TX */
    UCSR0C = (1u<<UCSZ01) | (1u<<UCSZ00);   /* 8N1 */
}
static void uart_tx(uint8_t c) {
    while (!(UCSR0A & (1u<<UDRE0))) { }
    UDR0 = c;
}
static uint8_t uart_rx(void) {
    while (!(UCSR0A & (1u<<RXC0))) { }
    return UDR0;
}
static void print_crlf(void) { uart_tx(0x0D); uart_tx(0x0A); }
static void print_str(const char *s) { while (*s) uart_tx((uint8_t)*s++); }

/* DELAYS */
static void delay_1s(void) {
    for (uint8_t i = 0; i < 4; i++) {
        for (uint8_t j = 0; j < 200; j++) {
            for (uint8_t k = 0; k < 250; k++) {
                __asm__ __volatile__("nop");
            }
        }
    }
}
static void delay_25s(void) { for (uint8_t n = 0; n < 8; n++) delay_1s(); }
static void delay_2s(void)  { delay_1s(); delay_1s(); }
static void delay_3s(void)  { delay_1s(); delay_1s(); delay_1s(); }
static void delay_10s(void) { for (uint8_t n=0;n<10;n++) delay_1s(); }

static void delay_tri(void)  { for (uint8_t n = 0; n < DELAY_TRI_SEG;  n++) delay_1s(); }
static void delay_cruz(void) { for (uint8_t n = 0; n < DELAY_CRUZ_SEG; n++) delay_1s(); }

/* OPCION 1: TRIANGULO */
static void run_sequence(void) {
    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD_X_POS), delay_tri);

    out_delay((1u<<PD_Y_POS), delay_tri);
    out_delay((1u<<PD_Y_POS), delay_tri);
    out_delay((1u<<PD_Y_POS), delay_10s);

    out_delay((1u<<PD2),      delay_1s);

    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD_Y_POS), delay_tri);

    out_delay((1u<<PD_X_NEG)|(1u<<PD_Y_NEG), delay_tri);

    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD3),      delay_1s);

	out_delay_ms((1u<<PD_X_NEG), 5000);
	out_delay_ms((1u<<PD_Y_NEG), 15000);
    PORTD = 0x00;
}

/* OPCION 3: CRUZ */
static void run_sequence2(void) {
    out_delay((1u<<PD_X_POS), delay_3s);
	out_delay((1u<<PD_X_POS), delay_3s);
	out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_X_POS), delay_cruz);
    out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_X_POS), delay_3s);
	out_delay((1u<<PD_X_POS), delay_3s);
	out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_Y_POS), delay_cruz);
    out_delay((1u<<PD_Y_POS), delay_cruz);
    out_delay((1u<<PD_X_POS), delay_cruz);
	out_delay((1u<<PD_X_POS), delay_cruz);

    out_delay((1u<<PD2),      delay_1s);

    out_delay((1u<<PD_X_NEG)|(1u<<PD_Y_NEG), delay_cruz);

    out_delay((1u<<PD3),      delay_1s);
    out_delay((1u<<PD_X_POS), delay_cruz);
    out_delay((1u<<PD2),      delay_1s);

    out_delay((1u<<PD_Y_POS)|(1u<<PD_Y_NEG), delay_cruz); /* (PD7|PD5) */

    out_delay((1u<<PD3),      delay_1s);
	
	out_delay_ms((1u<<PD_X_NEG), 15000);
	out_delay_ms((1u<<PD_Y_NEG), 15000);
	
    PORTD = 0x00;
}

/* OPCION 2: CIRCULO */
static void run_sequence3(void) {
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);

    out_delay((1u<<PD2),      delay_1s);

    out_delay((1u<<PD_X_NEG), delay_10s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_3s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_2s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_2s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_2s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_2s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_10s);

    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_2s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_2s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_2s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_2s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_3s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_10s);

    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_3s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_2s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_2s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_2s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_2s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_3s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_10s);

    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_3s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_2s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_2s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_2s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_2s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_3s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_3s);

    out_delay((1u<<PD3),      delay_1s);
	
	out_delay_ms((1u<<PD_X_NEG), 7000);
	out_delay_ms((1u<<PD_Y_NEG), 5000);
    PORTD = 0x00;
}

/* CONEJO */
static void run_conejo(void) {
    out_delay_ms((1u<<PD_X_POS), 10000);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_NEG), 1500);
    out_delay_ms((1u<<PD_Y_NEG), 1500);
    out_delay_ms((1u<<PD_Y_POS), 750);
    out_delay_ms((1u<<PD_X_POS), 1500);
    out_delay_ms((1u<<PD_X_NEG), 125);
    out_delay_ms((1u<<PD_Y_NEG), 1375);
    out_delay_ms((1u<<PD_X_NEG), 500);
    out_delay_ms((1u<<PD_X_POS), 1375);
    out_delay_ms((1u<<PD_X_NEG), 125);
    out_delay_ms((1u<<PD_X_POS), 3000);
    out_delay_ms((1u<<PD_Y_POS), 3000);
    out_delay_ms((1u<<PD_Y_NEG), 4500);
    out_delay_ms((1u<<PD_X_NEG), 750);
    out_delay_ms((1u<<PD_X_POS), 1500);
    out_delay_ms((1u<<PD_Y_POS), 125);
    out_delay_ms((1u<<PD_Y_NEG), 1375);
    out_delay_ms((1u<<PD_Y_POS), 500);
    out_delay_ms((1u<<PD_X_POS), 1375);
    out_delay_ms((1u<<PD_Y_POS), 125);
    out_delay_ms((1u<<PD_X_NEG), 1500);
    out_delay_ms((1u<<PD_X_NEG), 1500);
    out_delay_ms((1u<<PD_Y_POS), 1500);

    out_delay_ms((1u<<PD3), 100);
    out_delay_ms((1u<<PD_X_NEG), 1125);
    out_delay_ms((1u<<PD_X_POS), 375);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_POS), 750);
    out_delay_ms((1u<<PD_Y_POS), 750);
    out_delay_ms((1u<<PD_Y_NEG), 750);
    out_delay_ms((1u<<PD_X_NEG), 750);
    out_delay_ms((1u<<PD_Y_POS), 750);

    out_delay_ms((1u<<PD3), 100);
	
	out_delay_ms((1u<<PD_X_NEG), 150);
	out_delay_ms((1u<<PD_X_POS), 150);
	 
    out_delay_ms((1u<<PD2), 100); 
	
	out_delay_ms((1u<<PD_X_NEG), 400);
	out_delay_ms((1u<<PD_X_POS), 400);
	out_delay_ms((1u<<PD_Y_POS), 400);
	out_delay_ms((1u<<PD_Y_NEG), 400);
	
	out_delay_ms((1u<<PD3), 100);
		
	out_delay_ms((1u<<PD_Y_POS), 150);
	out_delay_ms((1u<<PD_Y_NEG), 150);
	
	out_delay_ms((1u<<PD3), 100);
	
    out_delay_ms((1u<<PD_Y_POS), 750);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_POS), 750);
    out_delay_ms((1u<<PD_Y_POS), 750);
    out_delay_ms((1u<<PD_Y_NEG), 750);
    out_delay_ms((1u<<PD_X_NEG), 750);
    out_delay_ms((1u<<PD_Y_POS), 750);

    out_delay_ms((1u<<PD3), 100);

	out_delay_ms((1u<<PD_X_NEG), 150);
	out_delay_ms((1u<<PD_X_POS), 150);
	
	out_delay_ms((1u<<PD2), 100);
	
	out_delay_ms((1u<<PD_X_NEG), 400);
	out_delay_ms((1u<<PD_X_POS), 400);
	out_delay_ms((1u<<PD_Y_POS), 400);
	out_delay_ms((1u<<PD_Y_NEG), 400);
	
	out_delay_ms((1u<<PD3), 100);
	
	out_delay_ms((1u<<PD_Y_POS), 150);
	out_delay_ms((1u<<PD_Y_NEG), 150);

    out_delay_ms((1u<<PD_X_POS), 250);
    out_delay_ms((1u<<PD_X_NEG), 750);
    out_delay_ms((1u<<PD_X_POS), 750);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_NEG), 800);

    for (int i = 0; i < 8; i++) {
        out_delay_ms((1u<<PD_X_POS), 50);
        out_delay_ms((1u<<PD_Y_POS), 50);
    }
    for (int i = 0; i < 8; i++) {
        out_delay_ms((1u<<PD_Y_NEG), 50);
        out_delay_ms((1u<<PD_Y_POS), 50);
    }
    for (int i = 0; i < 8; i++) {
        out_delay_ms((1u<<PD_X_POS), 50);
        out_delay_ms((1u<<PD_X_NEG), 50);
    }

    out_delay_ms((1u<<PD_X_POS), 750);
    out_delay_ms((1u<<PD_X_NEG), 750);
    out_delay_ms((1u<<PD_Y_NEG), 750);

    out_delay_ms((1u<<PD_X_POS), 750);
    out_delay_ms((1u<<PD_Y_POS), 750);

    out_delay_ms((1u<<PD_Y_POS), 750);
    out_delay_ms((1u<<PD_Y_NEG), 750);

    out_delay_ms((1u<<PD3), 100); 

    out_delay_ms((1u<<PD_Y_POS), 200); 
    out_delay_ms((1u<<PD_X_POS), 450);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_Y_POS), 750);
	out_delay_ms((1u<<PD_X_NEG), 750);

    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_Y_NEG), 375);

    out_delay_ms((1u<<PD2), 100);

	out_delay_ms((1u<<PD_Y_POS), 750);

    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_Y_NEG), 375);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_NEG), 750);

    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_X_NEG), 1950);

    out_delay_ms((1u<<PD2), 100); 

    out_delay_ms((1u<<PD_X_NEG), 750); 

    out_delay_ms((1u<<PD3), 100); 

    out_delay_ms((1u<<PD_X_POS), 375); 

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_Y_POS), 600);

    out_delay_ms((1u<<PD3), 100);
	
	   out_delay_ms((1u<<PD_X_POS), 400); 

	    out_delay_ms((1u<<PD2), 100);

	    out_delay_ms((1u<<PD_X_NEG), 600);

	    out_delay_ms((1u<<PD3), 100);
		
   out_delay_ms((1u<<PD_Y_NEG), 6500);

    PORTD = 0x00;
}

/* Murcielago */
static void run_murcielago(void) {

	
	  out_delay_ms((1u<<PD_X_POS), 2000);
	  out_delay_ms((1u<<PD_Y_POS), 16000);

	  out_delay_ms((1u<<PD2), 100);
	
	 out_delay_ms((1u<<PD_X_NEG), 200);
	for (int i = 0; i < 8; i++) {
	 out_delay_ms((1u<<PD_Y_NEG), 150);
	 out_delay_ms((1u<<PD_X_NEG), 150);
	}
	 for (int i = 0; i < 8; i++) {
	 out_delay_ms((1u<<PD_X_NEG), 150);
	 out_delay_ms((1u<<PD_X_POS), 150);
	 }
	  out_delay_ms((1u<<PD_X_NEG), 800);
	 for (int i = 0; i < 8; i++) {
		 out_delay_ms((1u<<PD_Y_NEG), 150);
		 out_delay_ms((1u<<PD_X_NEG), 150);
	 }
	 for (int i = 0; i < 8; i++) {
		 out_delay_ms((1u<<PD_X_NEG), 150);
		 out_delay_ms((1u<<PD_X_POS), 150);
	 }
	  out_delay_ms((1u<<PD_X_NEG), 200);
	 
	   out_delay_ms((1u<<PD_X_POS), 500);
	   for (int i = 0; i < 8; i++) {
		   out_delay_ms((1u<<PD_X_NEG), 150);
		   out_delay_ms((1u<<PD_Y_NEG), 150);
	   }
	   
		out_delay_ms((1u<<PD_X_NEG), 100);
		out_delay_ms((1u<<PD_X_POS), 3000);
		
		
	   for (int i = 0; i < 8; i++) {
		   out_delay_ms((1u<<PD_Y_POS), 150);
		   out_delay_ms((1u<<PD_X_POS), 150);
	   }
	   out_delay_ms((1u<<PD_Y_POS), 100);
	  	out_delay_ms((1u<<PD_X_POS), 500);
	  
		out_delay_ms((1u<<PD_Y_POS), 6000);
	  
	 
	    out_delay_ms((1u<<PD_Y_NEG), 500);
	    for (int i = 0; i < 8; i++) {
		    out_delay_ms((1u<<PD_Y_POS), 150);
		    out_delay_ms((1u<<PD_Y_NEG), 150);
	    }
		out_delay_ms((1u<<PD_Y_POS), 100);
		out_delay_ms((1u<<PD_Y_NEG), 3000);
	    for (int i = 0; i < 8; i++) {
		    out_delay_ms((1u<<PD_X_NEG), 150);
		    out_delay_ms((1u<<PD_X_POS), 150);
	    }
		out_delay_ms((1u<<PD_X_NEG), 100);
	    out_delay_ms((1u<<PD_Y_NEG), 500);
		out_delay_ms((1u<<PD_X_NEG), 150);
	out_delay_ms((1u<<PD3), 100);  
	
	out_delay_ms((1u<<PD_X_POS), 500);
	out_delay_ms((1u<<PD_X_NEG), 1100);
	out_delay_ms((1u<<PD2), 100);
	
    out_delay_ms((1u<<PD_X_NEG), 900);
    out_delay_ms((1u<<PD_X_POS), 900);
    out_delay_ms((1u<<PD_Y_POS), 900);
    out_delay_ms((1u<<PD_Y_NEG), 900);
   	out_delay_ms((1u<<PD3), 100);
	 
	out_delay_ms((1u<<PD_X_NEG), 3000 );  
	out_delay_ms((1u<<PD2), 100);
	
	out_delay_ms((1u<<PD_X_NEG), 900);
	out_delay_ms((1u<<PD_X_POS), 900);
	out_delay_ms((1u<<PD_Y_POS), 900);
	out_delay_ms((1u<<PD_Y_NEG), 900);
	out_delay_ms((1u<<PD_X_NEG), 900);
	out_delay_ms((1u<<PD3), 100);
	
	
	out_delay_ms((1u<<PD_X_POS), 1500);
	out_delay_ms((1u<<PD2), 100);
	out_delay_ms((1u<<PD_Y_POS), 4800);
	out_delay_ms((1u<<PD_X_NEG), 4800);
	out_delay_ms((1u<<PD_Y_POS), 1000);
	
	  for (int i = 0; i < 8; i++) {
		  out_delay_ms((1u<<PD_X_POS), 70);
		  out_delay_ms((1u<<PD_Y_POS), 70);
	  }
	  for (int i = 0; i < 8; i++) {
		  out_delay_ms((1u<<PD_Y_NEG), 70);
		  out_delay_ms((1u<<PD_Y_POS), 70);
	  }
	 out_delay_ms((1u<<PD_Y_POS), 700);
	 for (int i = 0; i < 8; i++) {
	 out_delay_ms((1u<<PD_X_POS), 70);
	 out_delay_ms((1u<<PD_Y_POS), 70);
	 }
	 for (int i = 0; i < 8; i++) {
	 out_delay_ms((1u<<PD_Y_NEG), 70);
	 out_delay_ms((1u<<PD_Y_POS), 70);
	 }
		
		out_delay_ms((1u<<PD3), 100);
		
	out_delay_ms((1u<<PD_Y_NEG), 12000);
	out_delay_ms((1u<<PD_X_NEG), 18000);
	PORTD = 0x00;
}
/* MENU */
static void print_menu(void) {
    print_str(STR_BANNER);
    print_str(STR_M1);
    print_str(STR_M2);
    print_str(STR_M3);
    print_str(STR_M4);
    print_str(STR_M5);
	print_str(STR_M6);
    print_str(STR_PROMPT);
}
