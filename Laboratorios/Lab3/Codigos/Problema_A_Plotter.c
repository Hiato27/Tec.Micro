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

static inline void pausa_corta(uint16_t ciclos) {
    _delay_loop_2(ciclos);
}

// Configuración inicial de los puertos y pines
void inicializar_hardware(void) {
    DDRB |= (1 << PIN_PASO_X) | (1 << PIN_DIR_X) | (1 << PIN_HAB_X);
    
    DDRC |= (1 << PIN_PASO_Y) | (1 << PIN_DIR_Y) | (1 << PIN_HAB_Y) | (1 << PIN_PLUMA);
    PORTB |= (1 << PIN_HAB_X);
    PORTC |= (1 << PIN_HAB_Y);
    PORTC &= ~(1 << PIN_PLUMA);
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

void mover_eje(uint8_t identificador_eje, uint8_t sentido, uint16_t cantidad_pasos) {
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

// Función principal
int main(void) {
    inicializar_hardware();

    while (1) {
        bajar_pluma();

        mover_eje(0, 1, 200); 
        mover_eje(1, 1, 200); 
        mover_eje(0, 0, 200); 
        mover_eje(1, 0, 200); 

        subir_pluma();
        _delay_ms(1000);
    }
}
