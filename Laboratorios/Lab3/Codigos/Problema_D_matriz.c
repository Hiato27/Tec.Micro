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
#define LED_PIN      PD2              // Datos WS2812B
#define LED_MASK     (1<<LED_PIN)

#define ANCHO        8
#define ALTO         8
#define N_LEDS       (ANCHO*ALTO)

// --- Variables de Estado Globales ---
// Definición de la matriz RGB (GRB para WS2812)
static uint8_t matriz_rgb[N_LEDS][3];

// Estado de la animación
volatile uint8_t animacion_actual = '1';  // Estado inicial: Animación 1
volatile uint8_t frame_actual = 0;       // Frame actual de la animación
volatile uint16_t contador_frames = 0;   // Contador para el ritmo de la animación
#define RITMO_ANIMACION 100              // Ritmo en ciclos del loop principal (ajustar según sea necesario)

// --- Funciones Auxiliares ---

// Función de mapeo (utilizada en el código original, mantenida)
static uint8_t idx_xy(uint8_t x, uint8_t y){
	// Mapeo simple: Fila (y) * Ancho + Columna (x)
	return (y * ANCHO) + x;
}

// --- Funciones WS2812 ---

void ws2812_init(void) {
	LED_DDR |= (1 << LED_PIN);  // Configura el pin como salida
	LED_PORT &= ~(1 << LED_PIN); // Inicializa el pin en bajo
}

// Función de envío de un byte (manteniendo el timing de 16MHz)
// NOTA: Se recomienda usar ensamblador para un timing preciso en produccion,
// pero se mantiene la version C por simplicidad y compatibilidad con el código original.
void ws2812_send_byte(uint8_t byte) {
	for(uint8_t i = 0; i < 8; i++) {
		if(byte & (1 << (7 - i))) {
			// Bit 1: T_1H > 0.8us, T_1L < 0.45us
			LED_PORT |= LED_MASK;  // HIGH
			asm volatile (
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t" // ~0.75us - 0.8us con 16MHz y optimizacion O0/O1
			::);
			LED_PORT &= ~LED_MASK; // LOW
			asm volatile (
			"nop\n\t" "nop\n\t" // ~0.18us - 0.2us
			::);
			} else {
			// Bit 0: T_0H > 0.4us, T_0L < 0.85us
			LED_PORT |= LED_MASK;  // HIGH
			asm volatile (
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t" // ~0.37us - 0.4us
			::);
			LED_PORT &= ~LED_MASK; // LOW
			asm volatile (
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t"
			"nop\n\t" "nop\n\t" "nop\n\t" "nop\n\t" // ~0.75us - 0.8us
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
	_delay_us(60);  // Tiempo de reset (requerido >50us)
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
	ws2812_update();
}

// --- Funciones UART (Con Interrupciones) ---

// Buffer para la transmisión (se mantiene la función de impresion)
void uart_print(const char* str) {
	while (*str) {
		// Espera hasta que el buffer esté listo para recibir datos
		while (!(UCSR0A & (1<<UDRE0)));
		UDR0 = *str++;
	}
}

void uart_init(void) {
	// Configuración para 9600 baudios (ajustado para 16MHz)
	unsigned int ubrr = F_CPU/16/9600-1;
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;

	// Habilitar receptor (RXEN0), transmisor (TXEN0)
	// Habilitar Interrupción de Recepción (RXCIE0)
	UCSR0B = (1<<RXEN0) | (1<<TXEN0) | (1<<RXCIE0);

	// Formato de frame: 8 bits de datos, sin paridad, 1 bit de parada
	UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);
}

// Interrupción de Recepción UART (NO BLOQUEANTE)
ISR(USART_RX_vect) {
	char recibido = UDR0;
	// La variable global 'animacion_actual' es 'volatile' y se actualiza inmediatamente
	if (recibido >= '0' && recibido <= '3') {
		animacion_actual = recibido;
		frame_actual = 0; // Reiniciar el frame al cambiar de animación
		contador_frames = 0; // Reiniciar el contador de tiempo
		
		// Responder al comando para feedback (opcional, pero útil)
		uart_print("Comando recibido: ");
		// No se puede usar `uart_transmit` simple dentro de ISR, por lo que imprimimos el caracter
		// si se requiere, pero para evitar complicaciones, mejor solo imprimir el mensaje.
		
		if (recibido == '0') uart_print("Test de Colores\r\n");
		else if (recibido == '1') uart_print("Animacion 1 (Sonrisa)\r\n");
		else if (recibido == '2') uart_print("Animacion 2 (Corazon)\r\n");
		else if (recibido == '3') uart_print("Apagar\r\n");
		} else {
		uart_print("Comando no reconocido\r\n");
	}
}

// --- Definiciones de Animación (Frames) ---

// Frame de 8x8 (Rojo, Verde, Azul)
// Para Ahorrar memoria y simplificar, se usan 8 bytes por frame donde cada bit
// representa si el LED debe estar encendido o apagado, y se define un color base.
// Alternativamente, se define directamente un color (R, G, B) para cada LED.
// Usaremos la alternativa para mayor flexibilidad.

// Ejemplo Animación 1: Cara Sonriente (3 frames)
// Byte: R, G, B
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
// El resto de frames de la sonrisa se omiten por espacio, pero la idea es cambiar la posicion/forma
// ...

// Ejemplo Animación 2: Corazón Palpitante (2 frames)
// Se define solo para los LEDs que están encendidos para ahorrar espacio
// Frame 1: Corazón completo (Rojo Fuerte)
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

// Frame 2: Corazón atenuado (Rojo Suave)
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

const uint8_t NUM_FRAMES_SMILE = 1; // Solo se definió 1 frame para el ejemplo
const uint8_t NUM_FRAMES_HEART = 2;


// --- Lógica de la Animación No Bloqueante ---

// Función para copiar un frame al buffer de la matriz
void copiar_frame(const uint8_t frame[N_LEDS][3]) {
	for(uint8_t i = 0; i < N_LEDS; i++) {
		ws2812_set_color(i, frame[i][0], frame[i][1], frame[i][2]);
	}
}

// Función principal de manejo de animaciones (NO BLOQUEANTE)
void manejar_animacion(void) {
	// Si el contador de frames alcanza el ritmo deseado, avanzar el frame
	if (contador_frames >= RITMO_ANIMACION) {
		frame_actual++;
		contador_frames = 0; // Reiniciar el contador
	}
	
	// Lógica para seleccionar el frame a mostrar
	if (animacion_actual == '1') {
		// Animación 1: Cara Sonriente
		// (En un caso real con múltiples frames, usar un switch/if)
		if (frame_actual >= NUM_FRAMES_SMILE) {
			frame_actual = 0; // Ciclar la animación
		}
		copiar_frame(SMILE_FRAME1); // Mostrar el frame (solo 1 frame de ejemplo)
		
		} else if (animacion_actual == '2') {
		// Animación 2: Corazón Palpitante
		if (frame_actual >= NUM_FRAMES_HEART) {
			frame_actual = 0; // Ciclar la animación
		}
		
		if (frame_actual == 0) {
			copiar_frame(HEART_FRAME1);
			} else if (frame_actual == 1) {
			copiar_frame(HEART_FRAME2);
		}
		
		} else if (animacion_actual == '0') {
		// Test de colores de inicialización
		// Lo dividimos en "frames" para que no bloquee con delays
		if (frame_actual == 0) {
			ws2812_set_all(255, 0, 0); // Rojo
			} else if (frame_actual == 1) {
			ws2812_set_all(0, 255, 0); // Verde
			} else if (frame_actual == 2) {
			ws2812_set_all(0, 0, 255); // Azul
			} else {
			// Después del ciclo de colores, volver a la animación 1
			animacion_actual = '1';
			frame_actual = 0;
		}
		
		} else if (animacion_actual == '3') {
		// Apagar matriz
		ws2812_set_all(0, 0, 0);
	}
	
	// Enviar el frame actual al hardware (actualización visual)
	ws2812_update();
	
	// Incrementar el contador de tiempo
	contador_frames++;
}

// --- Bucle Principal ---

int main(void) {
	// 1. Inicialización de periféricos
	uart_init();
	ws2812_init();
	sei(); // Habilitar las interrupciones globales

	// 2. Mensaje inicial por UART
	uart_print("Sistema de Animaciones Matriz LED RGB\r\n");
	uart_print("Comandos: 0 (Test Colores), 1 (Animacion 1), 2 (Animacion 2), 3 (Apagar)\r\n");
	
	// 3. Loop principal (NO BLOQUEANTE)
	while (1) {
		// Toda la lógica de la animación se ejecuta continuamente en el loop principal.
		// La recepción de comandos se maneja por interrupción, que *interrumpe*
		// esta función *solo* para cambiar el valor de `animacion_actual` y `frame_actual`.
		
		manejar_animacion();
		
		// Se puede añadir un pequeño delay de espera para regular la velocidad del loop,
		// ajustando RITMO_ANIMACION. Aquí se mantiene un loop rápido.
		// _delay_ms(5); // Por ejemplo, si RITMO_ANIMACION es 20, seria 20 frames/seg
	}

	return 0;
}
