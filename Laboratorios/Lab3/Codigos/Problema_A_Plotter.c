// Frecuencia del cristal (1MHz)
#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay_basic.h>
#include <util/delay.h>

// --- Pines Eje X (Puerto B) ---
#define PIN_PASO_X   PB3
#define PIN_DIR_X    PB4
#define PIN_HAB_X    PB5

// --- Pines Eje Y (Puerto C) ---
#define PIN_PASO_Y   PC3
#define PIN_DIR_Y    PC4
#define PIN_HAB_Y    PC5

// --- Pin de la Pluma (Puerto C) ---
#define PIN_PLUMA    PC0

// --- ¡¡AJUSTA ESTE VALOR!! ---
// Define cuántos pasos debe dar el motor por cada 1000ms (1 segundo)
// del código original. Prueba subiendo o bajando este número.
#define PASOS_POR_SEGUNDO 100.0

// Función de conversión
// (Calcula los pasos basado en los milisegundos del código original)
#define CALCULAR_PASOS(ms) (uint16_t)((ms / 1000.0) * PASOS_POR_SEGUNDO)

// Retardo corto
static inline void pausa_corta(uint16_t ciclos) {
    _delay_loop_2(ciclos);
}

// Configuración inicial de los puertos y pines
void inicializar_hardware(void) {
    DDRB |= (1 << PIN_PASO_X) | (1 << PIN_DIR_X) | (1 << PIN_HAB_X);
    DDRC |= (1 << PIN_PASO_Y) | (1 << PIN_DIR_Y) | (1 << PIN_HAB_Y) | (1 << PIN_PLUMA);

    PORTB |= (1 << PIN_HAB_X);
    PORTC |= (1 << PIN_HAB_Y);
    PORTC |= (1 << PIN_PLUMA); // Pluma arriba por defecto
}

// Función genérica para mover un motor paso a paso
void accionar_motor(volatile uint8_t *puerto_direccion, uint8_t pin_direccion,
                    volatile uint8_t *puerto_paso, uint8_t pin_paso,
                    uint8_t sentido_mov, uint16_t num_pasos) {
    
    if (sentido_mov) {
        *puerto_direccion |= (1 << pin_direccion);
    } else {
        *puerto_direccion &= ~(1 << pin_direccion);
    }

    pausa_corta(50);

    for (uint16_t i = 0; i < num_pasos; i++) {
        *puerto_paso |= (1 << pin_paso);
        pausa_corta(300);
        *puerto_paso &= ~(1 << pin_paso);
        pausa_corta(300);
    }
}

// Mueve un eje específico (X o Y)
void mover_eje(uint8_t identificador_eje, uint8_t sentido, uint16_t cantidad_pasos) {
    if (cantidad_pasos == 0) return;

    if (identificador_eje == 0) { // Eje X
        accionar_motor(&PORTB, PIN_DIR_X, &PORTB, PIN_PASO_X, sentido, cantidad_pasos);
    } else if (identificador_eje == 1) { // Eje Y
        accionar_motor(&PORTC, PIN_DIR_Y, &PORTC, PIN_PASO_Y, sentido, cantidad_pasos);
    }
}

// Baja la pluma
void bajar_pluma(void) {
    PORTC &= ~(1 << PIN_PLUMA);
    _delay_ms(100);
}

// Levanta la pluma
void subir_pluma(void) {
    PORTC |= (1 << PIN_PLUMA);
    _delay_ms(100);
}


/* --- SECUENCIA CONEJO (Traducida sin posicionamiento inicial) --- */
static void run_conejo(void) {
    // Ya no hay movimiento inicial. La figura comienza donde esté la pluma.

    bajar_pluma();

    mover_eje(0, 0, CALCULAR_PASOS(1500)); // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(1500)); // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(750));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(1500)); // X_POS
    mover_eje(0, 0, CALCULAR_PASOS(125));  // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(1375)); // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(500));  // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(1375)); // X_POS
    mover_eje(0, 0, CALCULAR_PASOS(125));  // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(3000)); // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(3000)); // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(4500)); // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(750));  // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(1500)); // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(125));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(1375)); // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(500));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(1375)); // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(125));  // Y_POS
    mover_eje(0, 0, CALCULAR_PASOS(1500)); // X_NEG
    mover_eje(0, 0, CALCULAR_PASOS(1500)); // X_NEG
    mover_eje(1, 1, CALCULAR_PASOS(1500)); // Y_POS

    subir_pluma();
    
    mover_eje(0, 0, CALCULAR_PASOS(1125)); // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(375));  // X_POS

    bajar_pluma();

    mover_eje(0, 1, CALCULAR_PASOS(750));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(750));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(750));  // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(750));  // X_NEG
    mover_eje(1, 1, CALCULAR_PASOS(750));  // Y_POS

    subir_pluma();
    
    mover_eje(0, 0, CALCULAR_PASOS(150)); // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(150)); // X_POS
    
    bajar_pluma();
    
    mover_eje(0, 0, CALCULAR_PASOS(400)); // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(400)); // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(400)); // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(400)); // Y_NEG
    
    subir_pluma();
        
    mover_eje(1, 1, CALCULAR_PASOS(150)); // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(150)); // Y_NEG
    
    subir_pluma();
    
    mover_eje(1, 1, CALCULAR_PASOS(750));  // Y_POS

    bajar_pluma();

    mover_eje(0, 1, CALCULAR_PASOS(750));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(750));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(750));  // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(750));  // X_NEG
    mover_eje(1, 1, CALCULAR_PASOS(750));  // Y_POS

    subir_pluma();

    mover_eje(0, 0, CALCULAR_PASOS(150)); // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(150)); // X_POS
    
    bajar_pluma();
    
    mover_eje(0, 0, CALCULAR_PASOS(400)); // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(400)); // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(400)); // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(400)); // Y_NEG
    
    subir_pluma();
    
    mover_eje(1, 1, CALCULAR_PASOS(150)); // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(150)); // Y_NEG

    mover_eje(0, 1, CALCULAR_PASOS(250)); // X_POS
    mover_eje(0, 0, CALCULAR_PASOS(750)); // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(750)); // X_POS

    bajar_pluma();

    mover_eje(0, 0, CALCULAR_PASOS(800)); // X_NEG

    for (int i = 0; i < 8; i++) {
        mover_eje(0, 1, CALCULAR_PASOS(50)); // X_POS
        mover_eje(1, 1, CALCULAR_PASOS(50)); // Y_POS
    }
    for (int i = 0; i < 8; i++) {
        mover_eje(1, 0, CALCULAR_PASOS(50)); // Y_NEG
        mover_eje(1, 1, CALCULAR_PASOS(50)); // Y_POS
    }
    for (int i = 0; i < 8; i++) {
        mover_eje(0, 1, CALCULAR_PASOS(50)); // X_POS
        mover_eje(0, 0, CALCULAR_PASOS(50)); // X_NEG
    }

    mover_eje(0, 1, CALCULAR_PASOS(750)); // X_POS
    mover_eje(0, 0, CALCULAR_PASOS(750)); // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(750)); // Y_NEG

    mover_eje(0, 1, CALCULAR_PASOS(750)); // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(750)); // Y_POS

    mover_eje(1, 1, CALCULAR_PASOS(750)); // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(750)); // Y_NEG

    subir_pluma();

    mover_eje(1, 1, CALCULAR_PASOS(200)); // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(450)); // X_POS

    bajar_pluma();

    mover_eje(1, 1, CALCULAR_PASOS(750)); // Y_POS
    mover_eje(0, 0, CALCULAR_PASOS(750)); // X_NEG

    subir_pluma();

    mover_eje(1, 0, CALCULAR_PASOS(375)); // Y_NEG

    bajar_pluma();

    mover_eje(1, 1, CALCULAR_PASOS(750)); // Y_POS

    subir_pluma();

    mover_eje(1, 0, CALCULAR_PASOS(375)); // Y_NEG

    bajar_pluma();

    mover_eje(0, 0, CALCULAR_PASOS(750)); // X_NEG

    subir_pluma();

    mover_eje(0, 0, CALCULAR_PASOS(1950)); // X_NEG

    bajar_pluma();

    mover_eje(0, 0, CALCULAR_PASOS(750)); //dibuja X_NEG

    subir_pluma();

    mover_eje(0, 1, CALCULAR_PASOS(375)); //posicion X_POS

    bajar_pluma();

    mover_eje(1, 1, CALCULAR_PASOS(600)); // Y_POS

    subir_pluma();
    
    mover_eje(0, 1, CALCULAR_PASOS(400)); //posicion X_POS

    bajar_pluma();

    mover_eje(0, 0, CALCULAR_PASOS(600)); // X_NEG

    subir_pluma();
        
    mover_eje(1, 0, CALCULAR_PASOS(6500)); // Y_NEG
}


/* --- SECUENCIA MURCIELAGO (Traducida sin posicionamiento inicial) --- */
static void run_murcielago(void) {
    // Ya no hay movimiento inicial. La figura comienza donde esté la pluma.

    bajar_pluma();
    //arriba
    mover_eje(0, 0, CALCULAR_PASOS(200)); // X_NEG
    for (int i = 0; i < 8; i++) {
        mover_eje(1, 0, CALCULAR_PASOS(150)); // Y_NEG
        mover_eje(0, 0, CALCULAR_PASOS(150)); // X_NEG
    }
    for (int i = 0; i < 8; i++) {
        mover_eje(0, 0, CALCULAR_PASOS(150)); // X_NEG
        mover_eje(0, 1, CALCULAR_PASOS(150)); // X_POS
    }
    mover_eje(0, 0, CALCULAR_PASOS(800)); // X_NEG
    for (int i = 0; i < 8; i++) {
        mover_eje(1, 0, CALCULAR_PASOS(150)); // Y_NEG
        mover_eje(0, 0, CALCULAR_PASOS(150)); // X_NEG
    }
    for (int i = 0; i < 8; i++) {
        mover_eje(0, 0, CALCULAR_PASOS(150)); // X_NEG
        mover_eje(0, 1, CALCULAR_PASOS(150)); // X_POS
    }
    mover_eje(0, 0, CALCULAR_PASOS(200)); // X_NEG
    
    //derecha
    mover_eje(0, 1, CALCULAR_PASOS(500)); // X_POS
    for (int i = 0; i < 8; i++) {
        mover_eje(0, 0, CALCULAR_PASOS(150)); // X_NEG
        mover_eje(1, 0, CALCULAR_PASOS(150)); // Y_NEG
    }
    
    mover_eje(0, 0, CALCULAR_PASOS(100)); // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(3000)); // X_POS
    
    for (int i = 0; i < 8; i++) {
        mover_eje(1, 1, CALCULAR_PASOS(150)); // Y_POS
        mover_eje(0, 1, CALCULAR_PASOS(150)); // X_POS
    }
    mover_eje(1, 1, CALCULAR_PASOS(100)); // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(500)); // X_POS
    
    //abajo 
    mover_eje(1, 1, CALCULAR_PASOS(6000)); // Y_POS
    
    //izquierda
    mover_eje(1, 0, CALCULAR_PASOS(500)); // Y_NEG
    for (int i = 0; i < 8; i++) {
        mover_eje(1, 1, CALCULAR_PASOS(150)); // Y_POS
        mover_eje(1, 0, CALCULAR_PASOS(150)); // Y_NEG
    }
    mover_eje(1, 1, CALCULAR_PASOS(100)); // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(3000)); // Y_NEG
    for (int i = 0; i < 8; i++) {
        mover_eje(0, 0, CALCULAR_PASOS(150)); // X_NEG
        mover_eje(0, 1, CALCULAR_PASOS(150)); // X_POS
    }
    mover_eje(0, 0, CALCULAR_PASOS(100)); // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(500)); // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(150)); // X_NEG
    
    subir_pluma();
    
    //ojos
    mover_eje(0, 1, CALCULAR_PASOS(500)); // X_POS
    mover_eje(0, 0, CALCULAR_PASOS(1100)); // X_NEG
    
    bajar_pluma();
    
    mover_eje(0, 0, CALCULAR_PASOS(900)); // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(900)); // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(900)); // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(900)); // Y_NEG
    
    subir_pluma();
    
    mover_eje(0, 0, CALCULAR_PASOS(3000)); // X_NEG
    
    bajar_pluma();
    
    mover_eje(0, 0, CALCULAR_PASOS(900)); // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(900)); // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(900)); // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(900)); // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(900)); // X_NEG
    
    subir_pluma();
    
    //boca
    mover_eje(0, 1, CALCULAR_PASOS(1500)); // X_POS
    
    bajar_pluma();
    
    mover_eje(1, 1, CALCULAR_PASOS(4800)); // Y_POS
    mover_eje(0, 0, CALCULAR_PASOS(4800)); // X_NEG
    mover_eje(1, 1, CALCULAR_PASOS(1000)); // Y_POS
    
    for (int i = 0; i < 8; i++) {
        mover_eje(0, 1, CALCULAR_PASOS(70)); // X_POS
        mover_eje(1, 1, CALCULAR_PASOS(70)); // Y_POS
    }
    for (int i = 0; i < 8; i++) {
        mover_eje(1, 0, CALCULAR_PASOS(70)); // Y_NEG
        mover_eje(1, 1, CALCULAR_PASOS(70)); // Y_POS
    }
    mover_eje(1, 1, CALCULAR_PASOS(700)); // Y_POS
    for (int i = 0; i < 8; i++) {
        mover_eje(0, 1, CALCULAR_PASOS(70)); // X_POS
        mover_eje(1, 1, CALCULAR_PASOS(70)); // Y_POS
    }
    for (int i = 0; i < 8; i++) {
        mover_eje(1, 0, CALCULAR_PASOS(70)); // Y_NEG
        mover_eje(1, 1, CALCULAR_PASOS(70)); // Y_POS
    }
        
    subir_pluma();
        
    // Volver al origen
    mover_eje(1, 0, CALCULAR_PASOS(12000)); // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(18000)); // X_NEG
}

/* --- SECUENCIA CIRCULO (Traducida) --- */
static void run_circulo(void) {
    // Ya no hay movimiento inicial. La figura comienza donde esté la pluma.

    bajar_pluma();

    mover_eje(1, 1, CALCULAR_PASOS(25 * 8)); // Y_POS (25s * 8)

    subir_pluma();
    _delay_ms(1000); // delay_1s

    // Estas secuencias de movimientos en X y Y parecen intentar trazar curvas/círculos
    // pero al ser solo movimientos en un eje a la vez, el resultado será una serie de líneas rectas
    // moviendo alternadamente X y luego Y.
    mover_eje(0, 0, CALCULAR_PASOS(10000)); // X_NEG (delay_10s)
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS (delay_1s)
    mover_eje(0, 0, CALCULAR_PASOS(3000));  // X_NEG (delay_3s)
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS (delay_1s)
    mover_eje(0, 0, CALCULAR_PASOS(2000));  // X_NEG (delay_2s)
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS (delay_1s)
    mover_eje(0, 0, CALCULAR_PASOS(2000));  // X_NEG (delay_2s)
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS (delay_1s)
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG (delay_1s)
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS (delay_1s)
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG (delay_1s)
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS (delay_1s)
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG (delay_1s)
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS (delay_1s)
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG (delay_1s)
    mover_eje(0, 1, CALCULAR_PASOS(2000));  // X_POS (delay_2s)
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG (delay_1s)
    mover_eje(0, 1, CALCULAR_PASOS(2000));  // X_POS (delay_2s)
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG (delay_1s)
    mover_eje(0, 1, CALCULAR_PASOS(3000));  // X_POS (delay_3s)
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG (delay_1s)
    mover_eje(0, 1, CALCULAR_PASOS(10000)); // X_POS (delay_10s)

    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(3000));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(2000));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(2000));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(2000));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(2000));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(3000));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(1000));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(10000)); // Y_POS

    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(3000));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(2000));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(2000));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(2000));  // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(2000));  // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(3000));  // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(1000));  // Y_POS
    mover_eje(1, 0, CALCULAR_PASOS(10000)); // Y_NEG

    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(3000));  // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(2000));  // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(2000));  // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(1000));  // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(2000));  // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(2000));  // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(3000));  // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(1000));  // Y_NEG
    mover_eje(0, 0, CALCULAR_PASOS(3000));  // X_NEG

    subir_pluma();
    
    mover_eje(0, 0, CALCULAR_PASOS(7000)); // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(5000)); // Y_NEG
}


// Función principal
int main(void) {
    inicializar_hardware();
    subir_pluma(); // Asegurarse de que la pluma esté arriba al iniciar
    _delay_ms(500); // Pequeño retardo inicial

    while (1) {
        // --- Dibuja el Conejo ---
        run_conejo();
        subir_pluma(); // Asegura pluma arriba
        mover_eje(0, 1, CALCULAR_PASOS(3000)); // Mueve 3 segundos de X+ para separar

        // --- Dibuja el Murciélago ---
        run_murcielago();
        subir_pluma(); // Asegura pluma arriba
        mover_eje(0, 1, CALCULAR_PASOS(3000)); // Mueve 3 segundos de X+ para separar

        // --- Dibuja el Círculo ---
        run_circulo();
        subir_pluma(); // Asegura pluma arriba
        mover_eje(0, 0, CALCULAR_PASOS(10000)); // Mueve 10 segundos de X- para volver un poco y separar
        mover_eje(1, 0, CALCULAR_PASOS(10000)); // Mueve 10 segundos de Y- para volver y separar
        
        // Espera 5 segundos antes de repetir todo el ciclo
        _delay_ms(5000); 
    }
}
