#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay_basic.h>
#include <util/delay.h>

//Pines Eje X 
#define PIN_PASO_X   PB3
#define PIN_DIR_X    PB4
#define PIN_HAB_X    PB5

//Pines Eje Y 
#define PIN_PASO_Y   PC3
#define PIN_DIR_Y    PC4
#define PIN_HAB_Y    PC5

//Pin de la Pluma
#define PIN_PLUMA    PC0
#define PASOS_POR_SEGUNDO 100.0 // Valor de ejemplo
#define CALCULAR_PASOS(ms) (uint16_t)((ms / 1000.0) * PASOS_POR_SEGUNDO)

static inline void pausa_corta(uint16_t ciclos) {
    _delay_loop_2(ciclos);
}

void inicializar_hardware(void) {
    DDRB |= (1 << PIN_PASO_X) | (1 << PIN_DIR_X) | (1 << PIN_HAB_X);
    
    DDRC |= (1 << PIN_PASO_Y) | (1 << PIN_DIR_Y) | (1 << PIN_HAB_Y) | (1 << PIN_PLUMA);
    PORTB |= (1 << PIN_HAB_X);
    PORTC |= (1 << PIN_HAB_Y);
    PORTC |= (1 << PIN_PLUMA); 
}

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

void mover_eje(uint8_t identificador_eje, uint8_t sentido, uint16_t cantidad_pasos) {
    if (cantidad_pasos == 0) return;
    if (identificador_eje == 0) { 
        accionar_motor(&PORTB, PIN_DIR_X, &PORTB, PIN_PASO_X, sentido, cantidad_pasos);
    } else if (identificador_eje == 1) { 
        accionar_motor(&PORTC, PIN_DIR_Y, &PORTC, PIN_PASO_Y, sentido, cantidad_pasos);
    }
}

// Baja la pluma para dibujar
void bajar_pluma(void) {
    PORTC &= ~(1 << PIN_PLUMA); 
    _delay_ms(100);
}

// Levanta la pluma
void subir_pluma(void) {
    PORTC |= (1 << PIN_PLUMA); 
    _delay_ms(100);
}

/*SECUENCIA CONEJO*/
static void run_conejo(void) {
    bajar_pluma();
    mover_eje(0, 0, CALCULAR_PASOS(1500)); // X_NEG
    mover_eje(1, 0, CALCULAR_PASOS(1500)); // Y_NEG
    mover_eje(1, 1, CALCULAR_PASOS(750));  // Y_POS
    mover_eje(0, 1, CALCULAR_PASOS(1500)); // X_POS
    subir_pluma();
    mover_eje(0, 0, CALCULAR_PASOS(1125)); // X_NEG
    mover_eje(0, 1, CALCULAR_PASOS(375));  // X_POS
    bajar_pluma();
    mover_eje(0, 1, CALCULAR_PASOS(750));  // X_POS
    mover_eje(1, 1, CALCULAR_PASOS(750));  // Y_POS
    subir_pluma();
}

/*SECUENCIA MURCIELAGO */
static void run_murcielago(void) {
    bajar_pluma();
    mover_eje(0, 0, CALCULAR_PASOS(200)); // X_NEG
    
    for (int i = 0; i < 8; i++) {
        mover_eje(1, 0, CALCULAR_PASOS(150)); // Y_NEG
        mover_eje(0, 0, CALCULAR_PASOS(150)); // X_NEG
    subir_pluma();
}


// Función principal
int main(void) {
    inicializar_hardware();
    subir_pluma(); 
    _delay_ms(500); 

    while (1) {
        //Dibuja el Conejo
        run_conejo();
        subir_pluma(); 
        mover_eje(0, 1, CALCULAR_PASOS(3000)); 

        //Dibuja el Murciélago
        run_murcielago();
        subir_pluma(); 
        mover_eje(0, 1, CALCULAR_PASOS(3000)); 
        
        mover_eje(0, 0, CALCULAR_PASOS(6000)); 
        _delay_ms(5000); 
    }
}
