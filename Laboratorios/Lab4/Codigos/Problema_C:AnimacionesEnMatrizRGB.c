#ifndef F_CPU
#define F_CPU 16000000UL
#endif

//  LIBRERÍAS 
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdbool.h>
#include <avr/pgmspace.h>

//  DEFINICIÓN DE PINES / MATRIZ 
// Pin de datos de la tira/matriz WS2812 (un solo pin)
#define LED_PORT      PORTD
#define LED_DDR       DDRD
#define LED_PIN       PD2
#define LED_MASK      (1<<LED_PIN)

// Tamaño de la matriz
#define ANCHO         8
#define ALTO          8
#define N_LEDS        (ANCHO*ALTO)

//  RITMOS / CANTIDAD DE FRAMES 
// Período de actualización (en "ticks" lógicos) para animación lenta
#define RITMO_ANIMACION_LENTO 100
// Ritmo más rápido para animación del rayo
#define RITMO_ANIMACION_RAYO  20

// Cantidad de frames de cada animación
#define NUM_FRAMES_ARBOL  4      // Luces del árbol
#define NUM_FRAMES_LLUVIA 4      // Lluvia (cíclica)
#define NUM_FRAMES_RAYO   8      // Rayo (caída + desaparición)

//  ESTADO DE LA MATRIZ / ANIMACIÓN 
// Buffer de color para cada LED [G,R,B] (formato WS2812)
static uint8_t matriz_rgb[N_LEDS][3];

// Animación seleccionada por UART ('0','1','2','3'). Arranca apagado.
volatile uint8_t animacion_actual = '3';

// Frame actual dentro de la animación seleccionada
volatile uint8_t  frame_actual    = 0;
// Contador de "ticks" para decidir cuándo avanzar frame
volatile uint16_t contador_frames = 0;

//  UTILIDAD: MAPEO (x,y) -> ÍNDICE LINEAL 
static uint8_t idx_xy(uint8_t x, uint8_t y){
    // Mapeo lineal: fila por fila de izquierda a derecha
    return (y * ANCHO) + x;
}


//  MANEJO DE WS2812B 

// Inicializa el pin de datos de la tira WS2812
void ws2812_init(void) {
    LED_DDR  |= (1 << LED_PIN);   // Pin como salida
    LED_PORT &= ~(1 << LED_PIN);  // Arranca en nivel bajo
}

// Envía un byte a la tira WS2812 usando "bit-banging" con NOPs para timing
void ws2812_send_byte(uint8_t byte) {
    for(uint8_t i = 0; i < 8; i++) {
        if(byte & (1 << (7 - i))) {
            // Bit '1': pulso alto más largo
            LED_PORT |= LED_MASK;
            asm volatile (
                "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
                "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
                "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
                ::);
            LED_PORT &= ~LED_MASK;
            asm volatile (
                "nop\n\t""nop\n\t"
                ::);
        } else {
            // Bit '0': pulso alto más corto
            LED_PORT |= LED_MASK;
            asm volatile (
                "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
                ::);
            LED_PORT &= ~LED_MASK;
            asm volatile (
                "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
                "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
                ::);
        }
    }
}

// Recorre el buffer y lo envía completo a la matriz WS2812
void ws2812_update(void) {
    cli();  // Deshabilita interrupciones para no romper el timing
    for(uint8_t i = 0; i < N_LEDS; i++) {
        // Formato GRB
        ws2812_send_byte(matriz_rgb[i][0]); // G
        ws2812_send_byte(matriz_rgb[i][1]); // R
        ws2812_send_byte(matriz_rgb[i][2]); // B
    }
    sei();              // Rehabilita interrupciones
    _delay_us(60);      // Tiempo de reset (>50 µs)
}

// Setea el color de un LED individual
void ws2812_set_color(uint8_t led_num, uint8_t r, uint8_t g, uint8_t b) {
    matriz_rgb[led_num][0] = g;
    matriz_rgb[led_num][1] = r;
    matriz_rgb[led_num][2] = b;
}

// Rellena toda la matriz con un mismo color
void ws2812_set_all(uint8_t r, uint8_t g, uint8_t b) {
    for(uint8_t i = 0; i < N_LEDS; i++) {
        ws2812_set_color(i, r, g, b);
    }
}

// Copia un frame desde PROGMEM al buffer de la matriz
void copiar_frame(const uint8_t frame[N_LEDS][3]) {
    for(uint8_t i = 0; i < N_LEDS; i++) {
        // Lee R,G,B desde memoria de programa
        uint8_t r = pgm_read_byte(&frame[i][0]);
        uint8_t g = pgm_read_byte(&frame[i][1]);
        uint8_t b = pgm_read_byte(&frame[i][2]);
        ws2812_set_color(i, r, g, b);
    }
}


//  UART (COMANDOS) 

// Envía una cadena por UART (bloqueante)
void uart_print(const char* str) {
    while (*str) {
        while (!(UCSR0A & (1<<UDRE0))); // Espera buffer TX libre
        UDR0 = *str++;
    }
}

// Inicializa UART a 9600 8N1 con interrupción de RX
void uart_init(void) {
    unsigned int ubrr = F_CPU/16/9600-1;
    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;

    UCSR0B = (1<<RXEN0) | (1<<TXEN0) | (1<<RXCIE0);   // RX/TX + IRQ RX
    UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);               // 8 bits, sin paridad
}

// Interrupción: recepción de un byte por UART
ISR(USART_RX_vect) {
    char recibido = UDR0;

    // Acepta sólo comandos '0'..'3'
    if (recibido >= '0' && recibido <= '3') {
        animacion_actual = recibido;
        frame_actual     = 0;
        contador_frames  = 0;

        // Mensaje de debug al recibir comando válido
        uart_print("Comando recibido: ");
        if (recibido == '0') uart_print("Test de Colores\r\n");
        else if (recibido == '1') uart_print("Animacion 1 (Arbol de Navidad GRANDE)\r\n");
        else if (recibido == '2') uart_print("Animacion 2 (TORMENTA: NUBE, RAYO Y LLUVIA)\r\n");
        else if (recibido == '3') uart_print("Apagar\r\n");
    } else {
        uart_print("Comando no reconocido\r\n");
    }
}


//  DEFINICIÓN DE COLORES 

// Colores "genéricos" para luces del árbol
#define C_ROJO        255, 0,   0
#define C_VERDE       0,   255, 0
#define C_AZUL        0,   0,   255
#define C_AMARILLO    255, 255, 0
#define C_MAGENTA     255, 0,   255
#define C_CYAN        0,   255, 255

// Colores para fondo, tronco, árbol y estrella
#define B_FONDO           0,   0,  30
#define T_TRONCO          100, 50, 0
#define ARBOL_VERDE       0,   150,0
#define ESTRELLA_AMARILLA 255, 255,0


//  ÁRBOL BASE + LUCES NAVIDAD 

// Árbol de navidad "estático" (sin luces), en PROGMEM
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

// Luces del árbol, frame 1 (rojo y azul)
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

// Luces del árbol, frame 2 (verde y amarillo)
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

// Luces del árbol, frame 3 (magenta y cian)
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

// Luces del árbol, frame 4 (amarillo y rojo, patrón inverso)
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

// Combina el árbol base con un frame de luces (anula color base donde hay luz)
void combinar_luces(const uint8_t luces_frame[N_LEDS][3]) {
    for(uint8_t i = 0; i < N_LEDS; i++) {
        // Árbol base
        uint8_t r_base = pgm_read_byte(&ARBOL_BASE[i][0]);
        uint8_t g_base = pgm_read_byte(&ARBOL_BASE[i][1]);
        uint8_t b_base = pgm_read_byte(&ARBOL_BASE[i][2]);
        ws2812_set_color(i, r_base, g_base, b_base);

        // Luces en este LED
        uint8_t r_luz = pgm_read_byte(&luces_frame[i][0]);
        uint8_t g_luz = pgm_read_byte(&luces_frame[i][1]);
        uint8_t b_luz = pgm_read_byte(&luces_frame[i][2]);

        // Si hay luz distinta de negro, pisa el color del árbol
        if (r_luz > 0 || g_luz > 0 || b_luz > 0) {
            ws2812_set_color(i, r_luz, g_luz, b_luz);
        }
    }
}


//  TORMENTA: NUBE, RAYO, LLUVIA 

// Colores para la nube, rayo, lluvia y fondo
#define C_NUBE_AZUL       50,  50, 100
#define C_RAYO_AMARILLO   255, 255, 0
#define C_LLUVIA          50,  100, 200
#define C_FONDO_CLARO     0,   0,   0

// Fondo base de la nube (sin rayo ni lluvia)
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

//  Frames del rayo (caída + desaparición) 

// Frame con rayo apagado
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

// Rayo iniciando (fila 4)
const uint8_t RAYO_F1[N_LEDS][3] PROGMEM = {
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},

    // Fila 4: inicio del rayo
    {0,0,0}, {0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

// Rayo extendiéndose a fila 5
const uint8_t RAYO_F2[N_LEDS][3] PROGMEM = {
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},

    // Fila 4
    {0,0,0}, {0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0},
    // Fila 5
    {0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

// Rayo cayendo a fila 6
const uint8_t RAYO_F3[N_LEDS][3] PROGMEM = {
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},

    // Fila 4
    {0,0,0}, {0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0},
    // Fila 5
    {0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    // Fila 6 (extensión final)
    {0,0,0}, {C_RAYO_AMARILLO}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

// Rayo extendiéndose hasta la fila 7
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
    // Fila 7 (punto final del rayo)
    {0,0,0}, {C_RAYO_AMARILLO}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

// Array de punteros a frames del rayo (8: caída + varios apagados)
const PGM_P RAYO_FRAMES[NUM_FRAMES_RAYO] = {
    (PGM_P)RAYO_F1,     // 0: inicio
    (PGM_P)RAYO_F2,     // 1
    (PGM_P)RAYO_F3,     // 2
    (PGM_P)RAYO_F4,     // 3
    (PGM_P)RAYO_F_OFF,  // 4: apagado
    (PGM_P)RAYO_F_OFF,  // 5
    (PGM_P)RAYO_F_OFF,  // 6
    (PGM_P)RAYO_F_OFF   // 7
};

//  Frames de la lluvia (4 frames cíclicos) 
#define L_C 50, 100, 200  // color de gota de lluvia
#define N_C 0,  0,   0    // fondo (sin lluvia)

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

// Array cíclico de frames de lluvia
const PGM_P LLUVIA_FRAMES[NUM_FRAMES_LLUVIA] = {
    (PGM_P)LLUVIA_FRAME_0,
    (PGM_P)LLUVIA_FRAME_1,
    (PGM_P)LLUVIA_FRAME_2,
    (PGM_P)LLUVIA_FRAME_3
};

// Combina nube base, rayo y lluvia en un solo frame de "tormenta"
void combinar_tormenta(uint8_t frame_lluvia_idx, uint8_t frame_rayo_idx) {

    // Frame actual del rayo desde la tabla de punteros
    const uint8_t (*rayo_frame)[3] PROGMEM   =
        (const uint8_t (*)[3])RAYO_FRAMES[frame_rayo_idx];

    // Frame de lluvia (cíclico)
    const uint8_t (*lluvia_frame)[3] PROGMEM =
        (const uint8_t (*)[3])LLUVIA_FRAMES[frame_lluvia_idx];

    for(uint8_t i = 0; i < N_LEDS; i++) {
        // 1) Dibuja la nube base
        uint8_t r_nube = pgm_read_byte(&NUBE_BASE[i][0]);
        uint8_t g_nube = pgm_read_byte(&NUBE_BASE[i][1]);
        uint8_t b_nube = pgm_read_byte(&NUBE_BASE[i][2]);
        ws2812_set_color(i, r_nube, g_nube, b_nube);

        // 2) Superpone rayo si hay pixel de rayo en ese LED
        uint8_t r_rayo = pgm_read_byte(&rayo_frame[i][0]);
        uint8_t g_rayo = pgm_read_byte(&rayo_frame[i][1]);
        uint8_t b_rayo = pgm_read_byte(&rayo_frame[i][2]);
        if (r_rayo > 0 || g_rayo > 0 || b_rayo > 0) {
            ws2812_set_color(i, r_rayo, g_rayo, b_rayo);
        }

        // 3) Superpone lluvia en filas inferiores
        uint8_t r_lluvia = pgm_read_byte(&lluvia_frame[i][0]);
        uint8_t g_lluvia = pgm_read_byte(&lluvia_frame[i][1]);
        uint8_t b_lluvia = pgm_read_byte(&lluvia_frame[i][2]);

        if (r_lluvia > 0 || g_lluvia > 0 || b_lluvia > 0) {
            // Sólo se dibuja lluvia a partir de cierta fila (parte baja)
            if (i / ANCHO >= 4) {
                ws2812_set_color(i, r_lluvia, g_lluvia, b_lluvia);
            }
        }
    }
}


//  LÓGICA DE ANIMACIONES 

// Actualiza la animación actual según animacion_actual y avanza frames
void manejar_animacion(void) {

    // Velocidad base de animación
    uint16_t ritmo_actual = RITMO_ANIMACION_LENTO;

    // Animación 2 (tormenta) usa ritmo más rápido para el rayo
    if (animacion_actual == '2') {
        ritmo_actual = RITMO_ANIMACION_RAYO;
    }

    // Avanza frame cuando el contador supera el ritmo seleccionado
    if (contador_frames >= ritmo_actual) {
        frame_actual++;
        contador_frames = 0;
    }

    //  Animación 1: Árbol de navidad 
    if (animacion_actual == '1') {

        // Frame actual va de 0..NUM_FRAMES_ARBOL-1
        if (frame_actual >= NUM_FRAMES_ARBOL) {
            frame_actual = 0;
        }

        if      (frame_actual == 0) combinar_luces(LUCES_FRAME1);
        else if (frame_actual == 1) combinar_luces(LUCES_FRAME2);
        else if (frame_actual == 2) combinar_luces(LUCES_FRAME3);
        else if (frame_actual == 3) combinar_luces(LUCES_FRAME4);

    //  Animación 2: Tormenta (nube+rayo+lluvia) 
    } else if (animacion_actual == '2') {

        // Frame del rayo corre de 0..7
        if (frame_actual >= NUM_FRAMES_RAYO) {
            frame_actual = 0;
        }

        // La lluvia cicla 0..3 independientemente
        uint8_t frame_lluvia_idx = frame_actual % NUM_FRAMES_LLUVIA;
        // El rayo usa el mismo índice de frame_actual (0..7)
        uint8_t frame_rayo_idx   = frame_actual;

        combinar_tormenta(frame_lluvia_idx, frame_rayo_idx);

    //  Animación 0: Test de colores 
    } else if (animacion_actual == '0') {

        // 3 frames: rojo, verde, azul
        if (frame_actual >= 3) {
            frame_actual = 0;
        }

        if      (frame_actual == 0) ws2812_set_all(255, 0,   0);
        else if (frame_actual == 1) ws2812_set_all(0,   255, 0);
        else if (frame_actual == 2) ws2812_set_all(0,   0,   255);

    //  Animación 3: Apagar 
    } else if (animacion_actual == '3') {

        ws2812_set_all(0, 0, 0);     // Todo apagado
        frame_actual    = 0;
        contador_frames = 0;
    }

    // Aplica los cambios a la tira
    ws2812_update();
    contador_frames++;
}


//  MAIN 


int main(void) {

    // Inicializa UART y WS2812
    uart_init();
    ws2812_init();
    sei();  // Habilita interrupciones globales (para UART RX)

    // Apaga la matriz al inicio
    ws2812_set_all(0, 0, 0);

    // Mensajes iniciales por serie
    uart_print("Sistema de Animaciones de la Matriz LED RGB\r\n");
    uart_print("Comandos: 0 (Test Colores), 1 (Animacion 1 - Arbol Navidad), 2 (Animacion 2 - tormenta), 3 (Apagar)\r\n");

    // Bucle principal: sólo delega en manejar_animacion()
    while (1) {
        manejar_animacion();
    }

    return 0;
}
