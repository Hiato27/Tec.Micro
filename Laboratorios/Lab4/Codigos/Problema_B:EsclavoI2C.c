// Control de motor, PWM, buzzer y comunicación I2C (Esclavo)

// Librerías
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>

// Definición de pines
#define SLAVE_ADDR      0x12       // Dirección del dispositivo esclavo I2C
#define MOTOR_IN1       PD2        // Pin para el control de dirección del motor
#define MOTOR_IN2       PD3        // Pin para el control de dirección del motor
#define MOTOR_PWM_PIN   PD6        // Pin para el PWM del motor
#define LED_PWM_PIN     PD5        // Pin para el PWM del LED
#define BUZZER_PIN      PB1        // Pin para el buzzer

// Variables globales
volatile uint8_t reg_sel = 0xFF, val_buf = 0, have_pair = 0; // Variables de registro y valor para comunicación I2C

// Función para establecer la dirección del motor
static inline void motor_set_dir(uint8_t dir){
    switch(dir){
        case 1:
            PORTD |=  (1<<MOTOR_IN1);   // Establece la dirección 1
            PORTD &= ~(1<<MOTOR_IN2);
            break;
        case 2:
            PORTD &= ~(1<<MOTOR_IN1);   // Establece la dirección 2
            PORTD |=  (1<<MOTOR_IN2);
            break;
        default:
            PORTD &= ~((1<<MOTOR_IN1) | (1<<MOTOR_IN2)); // Detiene el motor
            break;
    }
}

// Función para encender el buzzer
static inline void buzzer_on(void){ 
    TCCR1A |=  (1<<COM1A0);  // Activa el buzzer mediante PWM
}

// Función para apagar el buzzer
static inline void buzzer_off(void){
    TCCR1A &= ~(1<<COM1A0);  // Desactiva el buzzer
    PORTB  &= ~(1<<BUZZER_PIN);  // Apaga el buzzer
}

// Función para aplicar el par de registros de control (motor, PWM, buzzer)
static inline void apply_pair(uint8_t r, uint8_t v){
    switch(r){
        case 0x01: motor_set_dir(v); break;  // Control de dirección del motor
        case 0x02: OCR0B = v;       break;  // Control de PWM del motor
        case 0x04: if(v) buzzer_on(); else buzzer_off(); break;  // Control del buzzer
        default: break;
    }
}

// Inicializa los pines de control PWM y buzzer
static void io_pwm_init(void){
    DDRD |= (1<<MOTOR_IN1) | (1<<MOTOR_IN2); // Configura los pines de dirección del motor como salida
    PORTD &= ~((1<<MOTOR_IN1) | (1<<MOTOR_IN2)); // Detiene el motor al inicio

    DDRD |= (1<<MOTOR_PWM_PIN) | (1<<LED_PWM_PIN); // Configura los pines de PWM como salida
    TCCR0A = (1<<COM0A1) | (1<<COM0B1) | (1<<WGM01) | (1<<WGM00); // Configura el modo PWM (Fast PWM)
    TCCR0B = (1<<CS01) | (1<<CS00);  // Configura el prescaler de 64 (~976 Hz)
    OCR0A = 200;                      // Establece el valor de PWM para el motor
    OCR0B = 0;                        // Establece el valor de PWM para el LED (apagado)

    DDRB  |= (1<<BUZZER_PIN);         // Configura el pin del buzzer como salida
    TCCR1A = 0;                       // Configura el temporizador 1 en modo normal
    TCCR1B = 0;                       // Configura el temporizador 1 en modo normal
    OCR1A  = 499;                     // Establece el valor para generar una frecuencia
    TCCR1B = (1<<WGM12) | (1<<CS11);  // Configura el temporizador para CTC con prescaler de 8
    buzzer_off();                     // Apaga el buzzer al inicio
}

// Inicializa la comunicación I2C en modo esclavo
static inline void i2c_slave_init(uint8_t addr7){
    TWAR = (addr7 << 1);  // Establece la dirección del esclavo en el registro de dirección
    TWCR = (1<<TWEA) | (1<<TWEN) | (1<<TWIE); // Habilita la interrupción de I2C y la respuesta de dirección
}

// Interrupción de comunicación I2C (función de manejo del protocolo)
ISR(TWI_vect){
    uint8_t st = TWSR & 0xF8;  // Lee el estado del registro de estado de I2C
    switch(st){
        case 0x60: case 0x68:  // Dirección de esclavo recibida correctamente
            reg_sel = 0xFF;     // Restablece el registro seleccionado
            have_pair = 0;      // Restablece el par de valores
            TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN) | (1<<TWIE);  // Acepta la transmisión y espera más datos
            break;

        case 0x80:  // Datos recibidos
        {
            uint8_t d = TWDR;  // Lee el dato recibido
            if(reg_sel == 0xFF) 
                reg_sel = d;  // Si no se ha seleccionado un registro, selecciona el registro
            else { 
                val_buf = d;  // Si ya se ha seleccionado un registro, guarda el valor
                have_pair = 1; // Indica que se tiene un par de datos válido
            }
            TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN) | (1<<TWIE);  // Acepta el dato y espera más
        } break;

        case 0xA0:  // Finalización de la transmisión de datos
            if(have_pair){ 
                apply_pair(reg_sel, val_buf);  // Aplica el par de valores recibidos
                have_pair = 0;                 // Restablece la bandera de par de datos
            }
            TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN) | (1<<TWIE);  // Espera más datos
            break;

        case 0xA8: case 0xB0:  // No hay más datos disponibles
            TWDR = 0x00;  // Envía un 0 como respuesta
            TWCR = (1<<TWINT) | (1<<TWEN) | (1<<TWIE);  // Responde con 0 y espera más datos
            break;

        case 0xC0: case 0xC8: default:
            TWCR = (1<<TWINT) | (1<<TWEA) | (1<<TWEN) | (1<<TWIE);  // Responde a la solicitud con la dirección correcta
            break;
    }
}

// Función principal
int main(void){
    io_pwm_init();        // Inicializa los pines de control PWM y buzzer
    i2c_slave_init(SLAVE_ADDR);  // Inicializa I2C en modo esclavo
    sei();                // Habilita las interrupciones globales

    for(;;){ }  // Bucle infinito, esperando a que las interrupciones manejen la comunicación I2C
}
