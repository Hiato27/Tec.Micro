//Control de temperatura mediante PWM

//Librerias
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h>

// PWM para el calefactor
// Fast PWM 8-bit
static inline void heater_pwm_init(void){
    DDRD  |= (1<<DDD5);                 // D5 como salida digital
    TCCR0A = (1<<COM0B1)                // habilita OC0B en modo no invertido
           | (1<<WGM01) | (1<<WGM00);   // Fast PWM (TOP=0xFF)
    TCCR0B = (1<<CS01) | (1<<CS00);     // reloj de Timer0 = F_CPU/64
    OCR0B  = 0;                         // arranca apagado 
}

// Fija el ciclo útil del heater de 0 a 255
static inline void heater_set_pwm(uint8_t duty){
    OCR0B = duty;                       // escribe el duty en el comparador B
}

//Main
int main(void){
    heater_pwm_init();                  // configura el PWM en D5

    const uint8_t DUTY_HEATER = 255;    // Configura la potencia al maximo
    heater_set_pwm(DUTY_HEATER);        // aplica la potencia deseada

    // Bucle de calentado
    while(1){
        _delay_ms(1000);                // espera pasiva
    }
}
