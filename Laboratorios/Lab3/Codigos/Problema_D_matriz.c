#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdbool.h>

// --- Definiciones de Hardware ---
#define LED_PORT     PORTD
#define LED_DDR      DDRD
#define LED_PIN      PD2              // Pin de datos para WS2812B
#define LED_MASK     (1<<LED_PIN)

#define ANCHO        8
#define ALTO         8
#define N_LEDS       (ANCHO*ALTO)

// --- Variables de Estado Globales ---
// Definición de la matriz RGB (GRB para WS2812)
static uint8_t matriz_rgb[N_LEDS][3];

// **ESTADO INICIAL CORREGIDO:** '3' (Apagar matriz)
volatile uint8_t animacion_actual = '3';

volatile uint8_t frame_actual = 0;
volatile uint16_t contador_frames = 0;   // Contador para el ritmo de la animación
// Ritmo en ciclos del loop principal (ajustar para cambiar la velocidad de la animación)
#define RITMO_ANIMACION 100

// --- Funciones Auxiliares ---
static uint8_t idx_xy(uint8_t x, uint8_t y){
	return (y * ANCHO) + x;
}

// --- Funciones WS2812 ---

void ws2812_init(void) {
	LED_DDR |= (1 << LED_PIN);
	LED_PORT &= ~(1 << LED_PIN);
}

// Función de envío de un byte (manteniendo el timing de 16MHz con ensamblador)
void ws2812_send_byte(uint8_t byte) {
	for(uint8_t i = 0; i < 8; i++) {
		if(byte & (1 << (7 - i))) {
			// Bit 1: T_1H > 0.8us, T_1L < 0.45us
			LED_PORT |= LED_MASK;  // HIGH
			asm volatile (
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			::);
			LED_PORT &= ~LED_MASK; // LOW
			asm volatile (
			"nop\n\t" "nop\n\t"
			::);
			} else {
			// Bit 0: T_0H > 0.4us, T_0L < 0.85us
			LED_PORT |= LED_MASK;  // HIGH
			asm volatile (
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			::);
			LED_PORT &= ~LED_MASK; // LOW
			asm volatile (
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			::);
		}
	}
}

// Actualiza la matriz enviando todos los datos (GRB)
void ws2812_update(void) {
	cli(); // Deshabilitar interrupciones para el timing crítico
	for(uint8_t i = 0; i < N_LEDS; i++) {
		ws2812_send_byte(matriz_rgb[i][0]);  // Enviar verde (G)
		ws2812_send_byte(matriz_rgb[i][1]);  // Enviar rojo (R)
		ws2812_send_byte(matriz_rgb[i][2]);  // Enviar azul (B)
	}
	sei(); // Habilitar interrupciones
	_delay_us(60);  // Tiempo de reset
}

// Establece un color en el buffer de la matriz (RGB -> GRB)
void ws2812_set_color(uint8_t led_num, uint8_t r, uint8_t g, uint8_t b) {
	matriz_rgb[led_num][0] = g;
	matriz_rgb[led_num][1] = r;
	matriz_rgb[led_num][2] = b;
}

// Pinta toda la matriz de un solo color y actualiza
void ws2812_set_all(uint8_t r, uint8_t g, uint8_t b) {
	for(uint8_t i = 0; i < N_LEDS; i++) {
		ws2812_set_color(i, r, g, b);
	}
	// No se llama a ws2812_update() aquí, ya que se llama en manejar_animacion()
	// Esto es crucial para el test de colores no bloqueante
}

// --- Funciones UART (Con Interrupciones) ---

void uart_print(const char* str) {
	while (*str) {
		// Espera hasta que el buffer esté listo para recibir datos
		while (!(UCSR0A & (1<<UDRE0)));
		UDR0 = *str++;
	}
}

void uart_init(void) {
	unsigned int ubrr = F_CPU/16/9600-1;
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;

	// Habilitar receptor (RXEN0), transmisor (TXEN0) e Interrupción de Recepción (RXCIE0)
	UCSR0B = (1<<RXEN0) | (1<<TXEN0) | (1<<RXCIE0);

	// Formato de frame: 8 bits de datos
	UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);
}

// Interrupción de Recepción UART (NO BLOQUEANTE)
ISR(USART_RX_vect) {
	char recibido = UDR0;
	
	if (recibido >= '0' && recibido <= '3') {
		// Cambio inmediato de estado
		animacion_actual = recibido;
		frame_actual = 0;
		contador_frames = 0;
		
		// Feedback
		uart_print("Comando recibido: ");
		if (recibido == '0') uart_print("Test de Colores\r\n");
		else if (recibido == '1') uart_print("Animacion 1 (Sonrisa)\r\n");
		else if (recibido == '2') uart_print("Animacion 2 (Corazon)\r\n");
		else if (recibido == '3') uart_print("Apagar\r\n");
		} else {
		uart_print("Comando no reconocido\r\n");
	}
}

// --- Definiciones de Animación (Frames) ---

// Animación 1: Cara Sonriente (Amarillo - RGB: 255, 255, 0)
const uint8_t SMILE_FRAME1[N_LEDS][3] = {
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {255,255,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {255,255,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {255,255,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {255,255,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {255,255,0}, {255,255,0}, {255,255,0}, {255,255,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};
const uint8_t NUM_FRAMES_SMILE = 1; // Un solo frame de ejemplo

// Animación 2: Corazón Palpitante (Rojo)
// Frame 1: Corazón completo (Rojo Fuerte: 255, 0, 0)
const uint8_t HEART_FRAME1[N_LEDS][3] = {
	{0,0,0}, {255,0,0}, {255,0,0}, {0,0,0}, {0,0,0}, {255,0,0}, {255,0,0}, {0,0,0},
	{255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0},
	{255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0},
	{0,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {255,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {255,0,0}, {255,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

// Frame 2: Corazón atenuado (Rojo Suave: 100, 0, 0)
const uint8_t HEART_FRAME2[N_LEDS][3] = {
	{0,0,0}, {100,0,0}, {100,0,0}, {0,0,0}, {0,0,0}, {100,0,0}, {100,0,0}, {0,0,0},
	{100,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {100,0,0},
	{100,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {100,0,0},
	{0,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {100,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {100,0,0}, {100,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
	{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};
const uint8_t NUM_FRAMES_HEART = 2;


// --- Lógica de la Animación No Bloqueante ---

// Función para copiar un frame al buffer de la matriz
void copiar_frame(const uint8_t frame[N_LEDS][3]) {
	for(uint8_t i = 0; i < N_LEDS; i++) {
		// Los frames ya están definidos en RGB, por lo que los pasamos directamente
		ws2812_set_color(i, frame[i][0], frame[i][1], frame[i][2]);
	}
}

// Función principal de manejo de animaciones (NO BLOQUEANTE)
void manejar_animacion(void) {
	
	// 1. Manejo del Ritmo (Timing)
	if (contador_frames >= RITMO_ANIMACION) {
		frame_actual++;
		contador_frames = 0; // Reiniciar el contador
	}
	
	// 2. Lógica de selección y frames
	if (animacion_actual == '1') {
		// Animación 1: Cara Sonriente (Cicla el frame)
		if (frame_actual >= NUM_FRAMES_SMILE) {
			frame_actual = 0;
		}
		copiar_frame(SMILE_FRAME1);
		
		} else if (animacion_actual == '2') {
		// Animación 2: Corazón Palpitante (Cicla los 2 frames)
		if (frame_actual >= NUM_FRAMES_HEART) {
			frame_actual = 0;
		}
		
		if (frame_actual == 0) {
			copiar_frame(HEART_FRAME1); // Rojo Fuerte
			} else if (frame_actual == 1) {
			copiar_frame(HEART_FRAME2); // Rojo Suave
		}
		
		} else if (animacion_actual == '0') {
		// Test de colores de inicialización (3 frames: R, G, B)
		if (frame_actual == 0) {
			ws2812_set_all(255, 0, 0); // Rojo
			} else if (frame_actual == 1) {
			ws2812_set_all(0, 255, 0); // Verde
			} else if (frame_actual == 2) {
			ws2812_set_all(0, 0, 255); // Azul
			} else {
			// Después del ciclo de colores, volver a la Animación 1 por defecto
			animacion_actual = '1';
			frame_actual = 0;
			// No incrementamos el contador_frames para asegurar el cambio inmediato
			contador_frames = 0;
		}
		
		} else if (animacion_actual == '3') {
		// Estado Apagado (por defecto o por comando)
		ws2812_set_all(0, 0, 0);
		// Si está apagado, no hay frames que avanzar
		frame_actual = 0;
		contador_frames = 0;
	}
	
	// 3. Actualización Visual y Ritmo
	ws2812_update();
	contador_frames++;
}

// --- Bucle Principal ---

int main(void) {
	// 1. Inicialización de periféricos
	uart_init();
	ws2812_init();
	sei(); // Habilitar las interrupciones globales

	// **INICIALIZACIÓN DE ESTADO:** Apagar la matriz inmediatamente al inicio
	ws2812_set_all(0, 0, 0);

	// 2. Mensaje inicial por UART
	uart_print("Sistema de Animaciones Matriz LED RGB (AVR)\r\n");
	uart_print("Comandos: 0 (Test Colores), 1 (Animacion 1), 2 (Animacion 2), 3 (Apagar)\r\n");
	
	// 3. Loop principal (NO BLOQUEANTE)
	while (1) {
		// El loop llama repetidamente a la función de manejo de animaciones
		// La recepción de comandos es asíncrona (por ISR)
		manejar_animacion();
	}

	return 0;
}
