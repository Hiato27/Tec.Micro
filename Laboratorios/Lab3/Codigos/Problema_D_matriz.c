#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdbool.h>  // Añadir esta línea para usar 'bool', 'true' y 'false'

// Definición de pines
#define LED_PORT     PORTD
#define LED_DDR      DDRD
#define LED_PIN      PD2               // Datos WS2812B
#define LED_MASK     (1<<LED_PIN)

#define BTN_PINREG   PIND
#define BTN_DDR      DDRD
#define BTN_PORT     PORTD
#define BTN_PIN      PD3               // Botón del joystick (activo en LOW)

#define ADC_MEDIO     512
#define ZONA_MUERTA   80

#define ANCHO         8
#define ALTO          8
#define N_LEDS       (ANCHO*ALTO)

static uint8_t matriz_rgb[N_LEDS][3];

static uint8_t pos_x = 3;
static uint8_t pos_y = 3;

// Color actual del píxel encendido
static uint8_t col_rojo  = 255;
static uint8_t col_verde = 0;
static uint8_t col_azul  = 0;

static uint16_t sem_azar = 0xACE1u;

static uint16_t azar16(void){
	uint16_t bit = ((sem_azar >> 0u) ^ (sem_azar >> 2u) ^
	(sem_azar >> 3u) ^ (sem_azar >> 5u)) & 1u;
	sem_azar = (sem_azar >> 1u) | (bit << 15u);
	return sem_azar;
}

// Generar color nuevo aleatorio (evitar negro total)
static void color_aleatorio(void){
	uint16_t r = azar16();
	uint16_t g = azar16();
	uint16_t b = azar16();

	col_rojo  = (uint8_t)(r & 0xFF);
	col_verde = (uint8_t)(g & 0xFF);
	col_azul  = (uint8_t)(b & 0xFF);

	if(col_rojo < 20 && col_verde < 20 && col_azul < 20){
		col_rojo += 40;
	}
}

static uint8_t idx_xy(uint8_t x, uint8_t y){
	return (y * ANCHO) + x;
}

static void actualizar_buf(void){
	// Apagar todo
	for(uint8_t i=0; i<N_LEDS; i++){
		matriz_rgb[i][0] = 0;
		matriz_rgb[i][1] = 0;
		matriz_rgb[i][2] = 0;
	}

	// Encender solo el LED activo
	uint8_t idx = idx_xy(pos_x, pos_y);
	matriz_rgb[idx][0] = col_verde;
	matriz_rgb[idx][1] = col_rojo;
	matriz_rgb[idx][2] = col_azul;
}

// Funciones UART
void uart_init(void);
void uart_transmit(unsigned char data);
unsigned char uart_receive(void);
void uart_print(const char* str);
void uart_tx(char c);

void uart_init(void) {
	unsigned int ubrr = F_CPU/16/9600-1; // Configuración para 9600 baudios
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;
	UCSR0B = (1<<RXEN0) | (1<<TXEN0); // Habilitar receptor y transmisor
	UCSR0C = (1<<UCSZ01) | (1<<UCSZ00); // 8 bits de datos
}

void uart_transmit(unsigned char data) {
	while (!(UCSR0A & (1<<UDRE0))) {
		// Espera hasta que el buffer esté listo para recibir datos
	}
	UDR0 = data;
}

unsigned char uart_receive(void) {
	while (!(UCSR0A & (1<<RXC0))) {
		// Espera hasta recibir el dato
	}
	return UDR0;
}

void uart_print(const char* str) {
	while (*str) {
		uart_transmit(*str++);
	}
}

void uart_tx(char c) {
	uart_transmit(c);
}

// Funciones WS2812
void ws2812_send_byte(uint8_t byte) {
	for(uint8_t i = 0; i < 8; i++) {
		if(byte & (1 << (7 - i))) {
			LED_PORT |= LED_MASK;  // HIGH
			_delay_us(0.8);
			LED_PORT &= ~LED_MASK; // LOW
			_delay_us(0.4);
			} else {
			LED_PORT |= LED_MASK;  // HIGH
			_delay_us(0.4);
			LED_PORT &= ~LED_MASK; // LOW
			_delay_us(0.8);
		}
	}
}

void ws2812_update(void) {
	for(uint8_t i = 0; i < N_LEDS; i++) {
		ws2812_send_byte(matriz_rgb[i][0]);  // Enviar verde
		ws2812_send_byte(matriz_rgb[i][1]);  // Enviar rojo
		ws2812_send_byte(matriz_rgb[i][2]);  // Enviar azul
	}
	_delay_us(60);  // Tiempo de reset de la señal
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
	ws2812_update();
}

void ws2812_clear(void) {
	ws2812_set_all(0, 0, 0);  // Apagar todos los LEDs
	ws2812_update();
}

// Funciones de Animaciones
void show_animation(uint8_t anim_number) {
	if (anim_number == 1) {
		// Animación 1: Cara sonriente
		for(uint8_t i = 0; i < N_LEDS; i++) {
			ws2812_set_color(i, 255, 255, 0);  // Amarillo
		}
		ws2812_update();
		_delay_ms(500);
		} else if (anim_number == 2) {
		// Animación 2: Corazón palpitante
		for(uint8_t i = 0; i < N_LEDS; i++) {
			ws2812_set_color(i, 255, 0, 0);  // Rojo
		}
		ws2812_update();
		_delay_ms(500);
	}
}

void test_colors(void) {
	ws2812_set_all(255, 0, 0);  // Rojo
	ws2812_update();
	_delay_ms(500);
	ws2812_set_all(0, 255, 0);  // Verde
	ws2812_update();
	_delay_ms(500);
	ws2812_set_all(0, 0, 255);  // Azul
	ws2812_update();
	_delay_ms(500);
}

int main(void) {
	// Inicialización de pines
	LED_DDR  |= LED_MASK;        // Configura el pin de datos como salida
	LED_PORT &= ~LED_MASK;       // Inicializa el pin en bajo

	BTN_DDR  &= ~(1<<BTN_PIN);   // Configura el pin del botón como entrada
	BTN_PORT |=  (1<<BTN_PIN);   // Activa la resistencia pull-up en el botón

	uart_init();                 // Inicializa la UART

	pos_x      = 3;
	pos_y      = 3;
	col_rojo   = 255;
	col_verde  = 0;
	col_azul   = 0;

	actualizar_buf();
	ws2812_update();

	while (1) {
		char c = uart_receive();  // Recibe comando de UART

		if (c == '0') {
			uart_print("Probando colores\r\n");
			test_colors();  // Prueba de colores
			} else if (c == '1') {
			uart_print("Mostrando Animación 1\r\n");
			show_animation(1);  // Animación 1
			} else if (c == '2') {
			uart_print("Mostrando Animación 2\r\n");
			show_animation(2);  // Animación 2
			} else {
			uart_print("Comando no reconocido\r\n");
		}

		// Verificar si se presiona el botón para cambiar color
		bool btn_ahora = (BTN_PINREG & (1<<BTN_PIN)) == 0;  // Botón presionado si es LOW
		if (btn_ahora) {
			color_aleatorio();
			actualizar_buf();
			ws2812_update();
			_delay_ms(150);  // Espera un poco para evitar rebotes
		}

		_delay_ms(20);  // Retardo para evitar un uso excesivo de CPU
	}

	return 0;
}
