//Frecuencias
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

//Librerias
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdbool.h>
#include <avr/pgmspace.h>

// Definiciones de pines
#define LED_PORT      PORTD
#define LED_DDR       DDRD
#define LED_PIN       PD2
#define LED_MASK      (1<<LED_PIN)

#define ANCHO         8
#define ALTO          8
#define N_LEDS        (ANCHO*ALTO)


#define RITMO_ANIMACION_LENTO 100
#define RITMO_ANIMACION_RAYO  20


#define NUM_FRAMES_ARBOL 4
#define NUM_FRAMES_LLUVIA 4        
#define NUM_FRAMES_RAYO 8           



static uint8_t matriz_rgb[N_LEDS][3];

// Estado inicial: '3' (Apagar matriz)
volatile uint8_t animacion_actual = '3';

volatile uint8_t frame_actual = 0;
volatile uint16_t contador_frames = 0;



static uint8_t idx_xy(uint8_t x, uint8_t y){
	
	return (y * ANCHO) + x;
}

// Funciones del WS2812

void ws2812_init(void) {
	LED_DDR |= (1 << LED_PIN);
	LED_PORT &= ~(1 << LED_PIN);
}

void ws2812_send_byte(uint8_t byte) {
	
	for(uint8_t i = 0; i < 8; i++) {
		if(byte & (1 << (7 - i))) {
			
			LED_PORT |= LED_MASK;
			asm volatile (
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			::);
			LED_PORT &= ~LED_MASK;
			asm volatile (
			"nop\n\t" "nop\n\t"
			::);
			} else {
			
			LED_PORT |= LED_MASK;
			asm volatile (
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			::);
			LED_PORT &= ~LED_MASK;
			asm volatile (
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			::);
		}
	}
}

void ws2812_update(void) {
	cli();
	for(uint8_t i = 0; i < N_LEDS; i++) {
		
		ws2812_send_byte(matriz_rgb[i][0]);
		ws2812_send_byte(matriz_rgb[i][1]);
		ws2812_send_byte(matriz_rgb[i][2]);
	}
	sei();
	_delay_us(60);
}

void ws2812_set_color(uint8_t led_num, uint8_t r, uint8_t g, uint8_t b) {
	
	matriz_rgb[led_num][0] = g;
	matriz_rgb[led_num][1] = r;
	matriz_rgb[led_num][2] = b;
}

void ws2812_set_all(uint8_t r, uint8_t g, uint8_t b) {
	for(uint8_t i = 0; i < N_LEDS; i++) {
		ws2812_set_color(i, r, g, b);
	}
}


void copiar_frame(const uint8_t frame[N_LEDS][3]) {
	for(uint8_t i = 0; i < N_LEDS; i++) {
		
		uint8_t r = pgm_read_byte(&frame[i][0]);
		uint8_t g = pgm_read_byte(&frame[i][1]);
		uint8_t b = pgm_read_byte(&frame[i][2]);
		ws2812_set_color(i, r, g, b);
	}
}

// Funciones UART

void uart_print(const char* str) {
	while (*str) {
		while (!(UCSR0A & (1<<UDRE0)));
		UDR0 = *str++;
	}
}

void uart_init(void) {
	unsigned int ubrr = F_CPU/16/9600-1;
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;

	UCSR0B = (1<<RXEN0) | (1<<TXEN0) | (1<<RXCIE0);
	UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);
}

ISR(USART_RX_vect) {
	char recibido = UDR0;
	
	
	if (recibido >= '0' && recibido <= '3') {
		animacion_actual = recibido;
		frame_actual = 0;
		contador_frames = 0;
		
		uart_print("Comando recibido: ");
		if (recibido == '0') uart_print("Test de Colores\r\n");
		else if (recibido == '1') uart_print("Animacion 1 (Arbol de Navidad GRANDE)\r\n");
		else if (recibido == '2') uart_print("Animacion 2 (TORMENTA: NUBE, RAYO Y LLUVIA)\r\n");
		else if (recibido == '3') uart_print("Apagar\r\n");
		} else {
		uart_print("Comando no reconocido\r\n");
	}
}



// Definiciones de los colores de las luces del árbol
#define C_ROJO    255, 0, 0
#define C_VERDE   0, 255, 0
#define C_AZUL    0, 0, 255
#define C_AMARILLO 255, 255, 0
#define C_MAGENTA 255, 0, 255
#define C_CYAN    0, 255, 255


#define B_FONDO   0, 0, 30
#define T_TRONCO  100, 50, 0
#define ARBOL_VERDE 0, 150, 0
#define ESTRELLA_AMARILLA 255, 255, 0


const uint8_t ARBOL_BASE[N_LEDS][3] PROGMEM = {
	{B_FONDO}, {B_FONDO}, {B_FONDO}, {ESTRELLA_AMARILLA}, {B_FONDO}, {B_FONDO}, {B_FONDO}, {B_FONDO},
	{B_FONDO}, {B_FONDO}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {B_FONDO}, {B_FONDO}, {B_FONDO},
	{B_FONDO}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {B_FONDO}, {B_FONDO},
	{B_FONDO}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {B_FONDO}, {B_FONDO},
	{ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {B_FONDO},
	{ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {ARBOL_VERDE}, {B_FONDO},
	{B_FONDO}, {B_FONDO}, {T_TRONCO}, {T_TRONCO}, {T_TRONCO}, {B_FONDO}, {B_FONDO}, {B_FONDO},
	{B_FONDO}, {B_FONDO}, {T_TRONCO}, {T_TRONCO}, {T_TRONCO}, {B_FONDO}, {B_FONDO}, {B_FONDO}
};


// Frames de las luces de colores

// Frame 1: Rojo y Azul
const uint8_t LUCES_FRAME1[N_LEDS][3] PROGMEM = {
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {C_ROJO}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {C_AZUL}, {0,0,0}, {0,0,0}, {0,0,0}, {C_ROJO}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {C_ROJO}, {0,0,0}, {C_AZUL}, {0,0,0}, {0,0,0}, {0,0,0},
	{C_AZUL}, {0,0,0}, {0,0,0}, {C_ROJO}, {0,0,0}, {0,0,0}, {C_AZUL}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

// Frame 2: Verde y Amarillo
const uint8_t LUCES_FRAME2[N_LEDS][3] PROGMEM = {
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {C_VERDE}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {C_AMARILLO}, {0,0,0}, {C_VERDE}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {C_VERDE}, {0,0,0}, {C_AMARILLO}, {0,0,0}, {C_VERDE}, {0,0,0}, {0,0,0},
	{0,0,0}, {C_VERDE}, {0,0,0}, {0,0,0}, {C_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

// Frame 3: Magenta y Cyan
const uint8_t LUCES_FRAME3[N_LEDS][3] PROGMEM = {
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {C_MAGENTA}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {C_CYAN}, {0,0,0}, {0,0,0}, {0,0,0}, {C_MAGENTA}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {C_MAGENTA}, {0,0,0}, {C_CYAN}, {0,0,0}, {0,0,0}, {0,0,0},
	{C_CYAN}, {0,0,0}, {0,0,0}, {C_MAGENTA}, {0,0,0}, {0,0,0}, {C_CYAN}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

// Frame 4: Amarillo y Rojo (Patrón inverso)
const uint8_t LUCES_FRAME4[N_LEDS][3] PROGMEM = {
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {C_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {C_ROJO}, {0,0,0}, {C_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {C_AMARILLO}, {0,0,0}, {C_ROJO}, {0,0,0}, {C_AMARILLO}, {0,0,0}, {0,0,0},
	{0,0,0}, {C_AMARILLO}, {0,0,0}, {0,0,0}, {C_ROJO}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};


// Función donde se combina el arbol de navidad con las luces
void combinar_luces(const uint8_t luces_frame[N_LEDS][3]) {
	for(uint8_t i = 0; i < N_LEDS; i++) {
		
		uint8_t r_base = pgm_read_byte(&ARBOL_BASE[i][0]);
		uint8_t g_base = pgm_read_byte(&ARBOL_BASE[i][1]);
		uint8_t b_base = pgm_read_byte(&ARBOL_BASE[i][2]);
		ws2812_set_color(i, r_base, g_base, b_base);
		
		
		uint8_t r_luz = pgm_read_byte(&luces_frame[i][0]);
		uint8_t g_luz = pgm_read_byte(&luces_frame[i][1]);
		uint8_t b_luz = pgm_read_byte(&luces_frame[i][2]);
		
		if (r_luz > 0 || g_luz > 0 || b_luz > 0) {
			ws2812_set_color(i, r_luz, g_luz, b_luz);
		}
	}
}


//  Definiciones de la nube de tormenta eléctrica y la lluvia


#define C_NUBE_AZUL       50, 50, 100
#define C_RAYO_AMARILLO 255, 255, 0
#define C_LLUVIA        50, 100, 200
#define C_FONDO_CLARO   0, 0, 0


const uint8_t NUBE_BASE[N_LEDS][3] PROGMEM = {
	{C_FONDO_CLARO}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_FONDO_CLARO}, {C_FONDO_CLARO},
	{C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL},
	{C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL},
	{C_FONDO_CLARO}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_NUBE_AZUL}, {C_FONDO_CLARO},
	{C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO},
	{C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO},
	{C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO},
	{C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}, {C_FONDO_CLARO}
};

// Frames del rayo para la caida y la desapación

// Rayo apagado
const uint8_t RAYO_F_OFF[N_LEDS][3] PROGMEM = {
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

// Solo el inicio de la fila 4
const uint8_t RAYO_F1[N_LEDS][3] PROGMEM = {
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	
	// Comienzo del rayo 
	{0,0,0}, {0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

//  Cae a la Fila 5
const uint8_t RAYO_F2[N_LEDS][3] PROGMEM = {
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	
	// La fila 4 del rayo se mantiene
	{0,0,0}, {0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0},
	// Fila 5: El rayo se extiende
	{0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

// El rayo cae de la fila 3 a la fila 6
const uint8_t RAYO_F3[N_LEDS][3] PROGMEM = {
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	
	// Fila 4
	{0,0,0}, {0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0},
	// Fila 5
	{0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	// Fila 6 ( ahí el rayo se extiende)
	{0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

// El rayo cae de la fila 4 a la fila 7
const uint8_t RAYO_F4[N_LEDS][3] PROGMEM = {
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	
	// Fila 4
	{0,0,0}, {0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0},
	// Fila 5
	{0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	// Fila 6
	{0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	// Fila 7 (el rayo termina de caer)
	{0,0,0}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

// Array de punteros a los frames del rayo (8 frames)
const PGM_P RAYO_FRAMES[NUM_FRAMES_RAYO] = {
	(PGM_P)RAYO_F1,     // Frame 0: Caída (Inicio)
	(PGM_P)RAYO_F2,     // Frame 1: Caída (Medio)
	(PGM_P)RAYO_F3,     // Frame 2: Caída (Casi completo)
	(PGM_P)RAYO_F4,     // Frame 3: Caída (Completo)
    (PGM_P)RAYO_F_OFF,  // Frame 4: Desaparición (Apagado)
    (PGM_P)RAYO_F_OFF,  // Frame 5: Desaparición (Pausa)
    (PGM_P)RAYO_F_OFF,  // Frame 6: Desaparición (Pausa)
    (PGM_P)RAYO_F_OFF   // Frame 7: Desaparición (Pausa)
};


// Frames de la lluvia
#define L_C 50, 100, 200
#define N_C 0, 0, 0


const uint8_t LLUVIA_FRAME_0[N_LEDS][3] PROGMEM = {
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	
	{N_C}, {L_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {L_C},
	
	{N_C}, {N_C}, {N_C}, {L_C}, {N_C}, {L_C}, {N_C}, {N_C},
	
	{L_C}, {N_C}, {L_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	
	{N_C}, {N_C}, {N_C}, {N_C}, {L_C}, {N_C}, {L_C}, {N_C}
};


const uint8_t LLUVIA_FRAME_1[N_LEDS][3] PROGMEM = {
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	
	{L_C}, {N_C}, {N_C}, {N_C}, {N_C}, {L_C}, {N_C}, {N_C},

	{N_C}, {L_C}, {N_C}, {N_C}, {L_C}, {N_C}, {N_C}, {L_C},
	
	{N_C}, {N_C}, {N_C}, {L_C}, {N_C}, {L_C}, {N_C}, {N_C},
	
	{L_C}, {N_C}, {L_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}
};


const uint8_t LLUVIA_FRAME_2[N_LEDS][3] PROGMEM = {
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	
	{N_C}, {N_C}, {L_C}, {N_C}, {N_C}, {N_C}, {L_C}, {N_C},
	
	{L_C}, {N_C}, {N_C}, {N_C}, {N_C}, {L_C}, {N_C}, {N_C},
	
	{N_C}, {L_C}, {N_C}, {N_C}, {L_C}, {N_C}, {N_C}, {L_C},
	
	{N_C}, {N_C}, {N_C}, {L_C}, {N_C}, {L_C}, {N_C}, {N_C}
};


const uint8_t LLUVIA_FRAME_3[N_LEDS][3] PROGMEM = {
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	{N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C}, {N_C},
	
	{N_C}, {N_C}, {N_C}, {L_C}, {N_C}, {L_C}, {N_C}, {N_C},
	
	{N_C}, {N_C}, {L_C}, {N_C}, {N_C}, {N_C}, {L_C}, {N_C},
	
	{L_C}, {N_C}, {N_C}, {N_C}, {N_C}, {L_C}, {N_C}, {N_C},
	
	{N_C}, {L_C}, {N_C}, {N_C}, {L_C}, {N_C}, {N_C}, {L_C}
};


const PGM_P LLUVIA_FRAMES[NUM_FRAMES_LLUVIA] = {
	(PGM_P)LLUVIA_FRAME_0,
	(PGM_P)LLUVIA_FRAME_1,
	(PGM_P)LLUVIA_FRAME_2,
	(PGM_P)LLUVIA_FRAME_3
};


// Función para combinar la nube con el rayo y junto con la lluvia
void combinar_tormenta(uint8_t frame_lluvia_idx, uint8_t frame_rayo_idx) {
	
	// Utiliza el array de frames de rayo para la animación de caída/desaparición
	const uint8_t (*rayo_frame)[3] PROGMEM = (const uint8_t (*)[3])RAYO_FRAMES[frame_rayo_idx];
	
	// La lluvia sigue ciclando de 0 a 3
	const uint8_t (*lluvia_frame)[3] PROGMEM = (const uint8_t (*)[3])LLUVIA_FRAMES[frame_lluvia_idx];
	
	for(uint8_t i = 0; i < N_LEDS; i++) {
		
		
		uint8_t r_nube = pgm_read_byte(&NUBE_BASE[i][0]);
		uint8_t g_nube = pgm_read_byte(&NUBE_BASE[i][1]);
		uint8_t b_nube = pgm_read_byte(&NUBE_BASE[i][2]);
		ws2812_set_color(i, r_nube, g_nube, b_nube);
		
		
		uint8_t r_rayo = pgm_read_byte(&rayo_frame[i][0]);
		uint8_t g_rayo = pgm_read_byte(&rayo_frame[i][1]);
		uint8_t b_rayo = pgm_read_byte(&rayo_frame[i][2]);

		if (r_rayo > 0 || g_rayo > 0 || b_rayo > 0) {
			ws2812_set_color(i, r_rayo, g_rayo, b_rayo);
		}

		
		uint8_t r_lluvia = pgm_read_byte(&lluvia_frame[i][0]);
		uint8_t g_lluvia = pgm_read_byte(&lluvia_frame[i][1]);
		uint8_t b_lluvia = pgm_read_byte(&lluvia_frame[i][2]);
		
		if (r_lluvia > 0 || g_lluvia > 0 || b_lluvia > 0) {
			
			if (i / ANCHO >= 4) {
				ws2812_set_color(i, r_lluvia, g_lluvia, b_lluvia);
			}
		}
	}
}


void manejar_animacion(void) {
	
	uint16_t ritmo_actual = RITMO_ANIMACION_LENTO;

	
	if (animacion_actual == '2') {
		ritmo_actual = RITMO_ANIMACION_RAYO; 
	}


	if (contador_frames >= ritmo_actual) {
		frame_actual++;
		contador_frames = 0;
	}
	
	
	if (animacion_actual == '1') {
		
		if (frame_actual >= NUM_FRAMES_ARBOL) {
			frame_actual = 0;
		}
		
		if (frame_actual == 0) {
			combinar_luces(LUCES_FRAME1);
			} else if (frame_actual == 1) {
			combinar_luces(LUCES_FRAME2);
			} else if (frame_actual == 2) {
			combinar_luces(LUCES_FRAME3);
			} else if (frame_actual == 3) {
			combinar_luces(LUCES_FRAME4);
		}
		
		} else if (animacion_actual == '2') {
		
		// El frame_actual corre de 0 a 7
		if (frame_actual >= NUM_FRAMES_RAYO) { 
			frame_actual = 0;
		}
		
		
		uint8_t frame_lluvia_idx = frame_actual % NUM_FRAMES_LLUVIA; 
		
		
		uint8_t frame_rayo_idx = frame_actual;

		combinar_tormenta(frame_lluvia_idx, frame_rayo_idx);
		
		} else if (animacion_actual == '0') {
		
		if (frame_actual >= 3) {
			frame_actual = 0;
		}
		
		if (frame_actual == 0) {
			ws2812_set_all(255, 0, 0);
			} else if (frame_actual == 1) {
			ws2812_set_all(0, 255, 0);
			} else if (frame_actual == 2) {
			ws2812_set_all(0, 0, 255);
		}
		
		} else if (animacion_actual == '3') {
		
		ws2812_set_all(0, 0, 0);
		frame_actual = 0;
		contador_frames = 0;
	}
	
	
	ws2812_update();
	contador_frames++;
}



int main(void) {
	
	uart_init();
	ws2812_init();
	sei();

	
	ws2812_set_all(0, 0, 0);

	
	uart_print("Sistema de Animaciones de la Matriz LED RGB \r\n");
	uart_print("Comandos: 0 (Test Colores), 1 (Animacion 1 - Arbol Navidad), 2 (Animacion 2 - tormenta), 3 (Apagar)\r\n");
	
	
	while (1) {
		manejar_animacion();
	}

	return 0;
}
