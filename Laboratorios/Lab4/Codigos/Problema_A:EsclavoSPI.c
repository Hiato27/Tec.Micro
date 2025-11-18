// Control de motor, PWM, buzzer y comunicación SPI /Esclavo)

// Librerías
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>

// Definición de pines
#define MOTOR_IN1       PD2        // Pin para el control de dirección del motor
#define MOTOR_IN2       PD3        // Pin para el control de dirección del motor
#define MOTOR_PWM_PIN   PD6        // Pin para el PWM del motor
#define LED_PWM_PIN     PD5        // Pin para el PWM del LED
#define BUZZER_PIN      PB1        // Pin para el buzzer

// Inicializa SPI en modo esclavo
static inline void spi_slave_init(void){
    DDRB &= ~((1<<PB3)|(1<<PB5)|(1<<PB2));  // Configura los pines de SPI como entrada
    DDRB |=  (1<<PB4);                       // Configura el pin MISO como salida
    SPCR = (1<<SPE);                         // Habilita el SPI en modo esclavo
}

// Recibe un byte por SPI
static inline uint8_t spi_recv(void){
    while(!(SPSR & (1<<SPIF)));  // Espera a que el byte se haya recibido completamente
    return SPDR;                  // Retorna el byte recibido
}

// Inicializa los pines para los PWM
static inline void pwm_init(void){
    DDRD |= (1<<MOTOR_PWM_PIN) | (1<<LED_PWM_PIN); // Configura los pines de PWM como salida
    TCCR0A = (1<<COM0A1) | (1<<COM0B1) | (1<<WGM01) | (1<<WGM00); // Configura el modo PWM (Fast PWM)
    TCCR0B = (1<<CS01) | (1<<CS00);  // Configura el prescaler de 64 (~976 Hz)
    OCR0A = 200;                      // Establece el valor de PWM para el motor
    OCR0B = 0;                        // Establece el valor de PWM para el LED (apagado)
}

// Inicializa los pines para controlar la dirección del motor
static inline void motor_gpio_init(void){
    DDRD |= (1<<MOTOR_IN1) | (1<<MOTOR_IN2);  // Configura los pines de dirección del motor como salida
    PORTD &= ~((1<<MOTOR_IN1)|(1<<MOTOR_IN2)); // Apaga el motor al inicio
}

// Establece la dirección del motor
static inline void motor_set_dir(uint8_t dir){
    switch(dir){
        case 1:
            PORTD |=  (1<<MOTOR_IN1);    // Activa la dirección 1
            PORTD &= ~(1<<MOTOR_IN2);
            break;
        case 2:
            PORTD &= ~(1<<MOTOR_IN1);    // Activa la dirección 2
            PORTD |=  (1<<MOTOR_IN2);
            break;
        default:
            PORTD &= ~((1<<MOTOR_IN1)|(1<<MOTOR_IN2));  // Apaga el motor
            break;
    }
}

// Inicializa el buzzer para emitir sonido
static inline void buzzer_init(void){
    DDRB |= (1<<BUZZER_PIN);  // Configura el pin del buzzer como salida
    TCCR1A = 0;               // Configura el temporizador 1 en modo normal
    TCCR1B = 0;
    OCR1A  = 499;             // Establece el valor para generar una frecuencia
    TCCR1A &= ~(1<<COM0A0);   // Desactiva la salida en el buzzer
    PORTB  &= ~(1<<BUZZER_PIN);  // Apaga el buzzer al inicio
    TCCR1B = (1<<WGM12) | (1<<CS11);  // Configura el temporizador para CTC con prescaler de 8
}

// Enciende el buzzer
static inline void buzzer_on(void){
    TCCR1A |= (1<<COM1A0);  // Activa el buzzer
}

// Apaga el buzzer
static inline void buzzer_off(void){
    TCCR1A &= ~(1<<COM1A0);  // Desactiva el buzzer
    PORTB  &= ~(1<<BUZZER_PIN);  // Apaga el buzzer
}

// Verifica la validez del paquete SPI (comprobación de checksum)
static inline bool packet_ok(uint8_t s, uint8_t c, uint8_t v, uint8_t k){
    return (s == 0xAA) && ((s ^ c ^ v) == k);  // Comprueba si el checksum es correcto
}

// Función principal
int main(void){
    spi_slave_init();  // Inicializa SPI en modo esclavo
    pwm_init();        // Inicializa los pines de PWM
    motor_gpio_init(); // Inicializa los pines de control del motor
    buzzer_init();     // Inicializa el buzzer

    uint8_t sync, cmd, val, chk;

    while(1){
        sync = spi_recv();  // Recibe el byte de sincronización
        cmd  = spi_recv();  // Recibe el comando
        val  = spi_recv();  // Recibe el valor asociado al comando
        chk  = spi_recv();  // Recibe el checksum

        if(!packet_ok(sync, cmd, val, chk)){  // Verifica la validez del paquete
            continue;  // Si el paquete no es válido, lo descarta
        }

        if(cmd == 0x01){  // Comando para controlar la dirección del motor
            motor_set_dir(val);  // Establece la dirección del motor
        } else if(cmd == 0x02){  // Comando para controlar el PWM del motor
            OCR0B = val;  // Ajusta el ciclo de trabajo del motor
        } else if(cmd == 0x04){  // Comando para controlar el buzzer
            if(val) buzzer_on();  // Si el valor es 1, enciende el buzzer
            else    buzzer_off(); // Si el valor es 0, apaga el buzzer
        }
    }
    return 0;  // Fin de la ejecución
}
