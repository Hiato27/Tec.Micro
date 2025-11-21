#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

//  CONSTANTES SERVOS 
// Servomotor derecho (ancho de pulso en microsegundos)
#define DER_POS_CENTRO      1200    // Posición neutra
#define DER_POS_AFUERA      800     // Flipper hacia afuera
#define DER_POS_ADENTRO     1600    // Flipper hacia adentro (disparo)

// Servomotor izquierdo (ancho de pulso en microsegundos)
#define IZQ_POS_CENTRO      1500    // Posición neutra
#define IZQ_POS_AFUERA      1000    // Flipper hacia afuera
#define IZQ_POS_ADENTRO     2000    // Flipper hacia adentro (disparo)

// Cantidad de repeticiones de pulsos para asegurar el movimiento del servo
#define SERVO_REPETICIONES  12

//  PINES MOTORES DC
#define MOTOR_IZQ_IN1   PD4     // Dirección motor izquierdo
#define MOTOR_IZQ_IN2   PB0     // Dirección motor izquierdo
#define MOTOR_IZQ_PWM   PD6     // PWM (OC0A) motor izquierdo

#define MOTOR_DER_IN1   PD7     // Dirección motor derecho
#define MOTOR_DER_IN2   PB5     // Dirección motor derecho
#define MOTOR_DER_PWM   PD5     // PWM (OC0B) motor derecho

//  PINES SERVOS 
#define SERVO_DER_PIN   PD2     // Servo derecho (flipper derecho)
#define SERVO_IZQ_PIN   PB4     // Servo izquierdo (flipper izquierdo)

//  PINES SENSORES Y BUZZER 
#define SENSOR_IZQ      PB1     // Sensor de línea izquierdo
#define SENSOR_DER      PD3     // Sensor de línea derecho
#define BUZZER_PIN      PC2     // Buzzer (aviso al salir de la línea)

//  PINES HC-05 (BIT-BANG)
#define HC05_RX_PIN     PB2     // Entrada RX software (data desde HC-05)
#define HC05_TX_PIN     PB3     // Salida TX software (data hacia HC-05)
#define HC05_BIT_DELAY  26      // Delay aproximado por bit (ajustado a baud)

//  UART DEBUG HARDWARE 
#define DEBUG_BAUD 9600
#define DEBUG_UBRR ((F_CPU/16/DEBUG_BAUD)-1)  // Valor para UBRR0 (9600 baudios)

//  VELOCIDADES 
#define VEL_AVANCE  200    // Velocidad por defecto de avance
#define VEL_GIRO    150    // Velocidad usada para giros sobre eje

//  VARIABLES GLOBALES 
char receivedCommand = 0;                 // Último comando recibido por Bluetooth
uint8_t velocidad_actual = VEL_AVANCE;    // Velocidad actual configurada

//  PROTOTIPOS 
// UART debug
void Debug_Init(void);
void Debug_PrintString(const char* str);
void Debug_Print(unsigned char data);

// HC-05 bit-bang
void HC05_Init(void);
void HC05_Write(unsigned char data);
void HC05_WriteString(const char* str);
uint8_t HC05_Read(char* data);

// PWM y motores
void init_pwm(void);
void set_motor_izq(int16_t velocidad);
void set_motor_der(int16_t velocidad);
void coast_motor_izq(void);
void coast_motor_der(void);

// Movimientos del auto
void detener(void);
void avanzar(void);
void retroceder(void);
void girar_izquierda(void);
void girar_derecha(void);
void diagonal_adelante_izq(void);
void diagonal_adelante_der(void);
void diagonal_atras_izq(void);
void diagonal_atras_der(void);

// Sensores de línea y comandos
void check_line_sensors(void);
void processCommand(char cmd);

// GPIO general y servos
void GPIO_Init(void);
void servo_der_pulse(uint16_t high_us);
void servo_der_move(uint16_t high_us, uint8_t rep);
void servo_izq_pulse(uint16_t high_us);
void servo_izq_move(uint16_t high_us, uint8_t rep);
void flipper_out(uint8_t es_derecho);
void flippers_shot_in(void);

// IMPLEMENTACIÓN UART DEBUG 

// Inicializa UART hardware para mensajes de depuración (9600 8N1, solo TX)
void Debug_Init(void) {
    UBRR0H = (unsigned char)(DEBUG_UBRR >> 8);
    UBRR0L = (unsigned char)DEBUG_UBRR;
    UCSR0B = (1 << TXEN0);                    // Habilita transmisión
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);   // Trama 8 bits, sin paridad, 1 stop
}

// Envia un byte por UART debug
void Debug_Print(unsigned char data) {
    while (!(UCSR0A & (1 << UDRE0)));  // Espera a que el buffer esté vacío
    UDR0 = data;
}

// Envia una cadena por UART debug
void Debug_PrintString(const char* str) {
    while (*str) Debug_Print(*str++);
}

//  IMPLEMENTACIÓN HC-05 (SOFTWARE UART)

// Configura pines para UART software con HC-05
void HC05_Init(void) {
    DDRB  |= (1 << HC05_TX_PIN);   // TX como salida
    PORTB |= (1 << HC05_TX_PIN);   // Línea en alto (reposo)
    DDRB  &= ~(1 << HC05_RX_PIN);  // RX como entrada
    PORTB |=  (1 << HC05_RX_PIN);  // Pull-up en RX
}

// Envia 1 byte al HC-05 usando bit-banging a la velocidad configurada
void HC05_Write(unsigned char data) {
    cli();                                      // Deshabilita interrupciones para timing estable
    PORTB &= ~(1 << HC05_TX_PIN);              // Start bit (LOW)
    _delay_us(HC05_BIT_DELAY);

    for (uint8_t i = 0; i < 8; i++) {          // 8 bits de datos, LSB primero
        if (data & 0x01) PORTB |= (1 << HC05_TX_PIN);
        else             PORTB &= ~(1 << HC05_TX_PIN);
        data >>= 1;
        _delay_us(HC05_BIT_DELAY);
    }

    PORTB |= (1 << HC05_TX_PIN);               // Stop bit (HIGH)
    _delay_us(HC05_BIT_DELAY);
    sei();                                     // Rehabilita interrupciones
}

// Envia una cadena de texto al HC-05
void HC05_WriteString(const char* str) {
    while (*str) HC05_Write(*str++);
}

// Lee 1 byte desde el HC-05 usando bit-banging.
// Retorna 1 si se leyó un byte válido, 0 si hubo timeout o error.
uint8_t HC05_Read(char* data) {
    uint16_t timeout = 0;

    // Espera start bit (línea pasa de HIGH a LOW)
    while ((PINB & (1 << HC05_RX_PIN)) && timeout < 50000) timeout++;
    if (timeout >= 50000) return 0;            // Timeout sin recibir nada

    _delay_us(HC05_BIT_DELAY / 2);             // Se posiciona al centro del start bit
    if (PINB & (1 << HC05_RX_PIN)) return 0;   // Si volvió a HIGH, no era start

    uint8_t byte = 0;
    // Lee 8 bits (MSB construido desplazando hacia la derecha)
    for (uint8_t i = 0; i < 8; i++) {
        _delay_us(HC05_BIT_DELAY);
        byte >>= 1;
        if (PINB & (1 << HC05_RX_PIN)) byte |= 0x80;
    }

    _delay_us(HC05_BIT_DELAY);                 // Espera stop bit
    if (!(PINB & (1 << HC05_RX_PIN))) return 0; // Stop bit debe ser HIGH

    *data = byte;
    return 1;
}

//  PWM Y CONTROL DE MOTORES 

// Configura Timer0 en Fast PWM para controlar los dos motores (OC0A y OC0B)
void init_pwm(void) {
    DDRD |= (1 << MOTOR_IZQ_PWM) | (1 << MOTOR_DER_PWM);   // Pines PWM como salida
    TCCR0A = (1 << WGM00) | (1 << WGM01) |                 // Fast PWM 8 bits
             (1 << COM0A1) | (1 << COM0B1);                // Salida no invertida en OC0A/OC0B
    TCCR0B = (1 << CS01);                                  // Prescaler /8
    OCR0A = 0;
    OCR0B = 0;
}

// Control de motor izquierdo con velocidad signed:
//  >0  adelante, <0 atrás, 0 freno activo (coast = otra función)
void set_motor_izq(int16_t velocidad) {
    if (velocidad > 0) {
        PORTD |=  (1 << MOTOR_IZQ_IN1);
        PORTB &= ~(1 << MOTOR_IZQ_IN2);
        OCR0A = (velocidad > 255) ? 255 : velocidad;
    } else if (velocidad < 0) {
        PORTD &= ~(1 << MOTOR_IZQ_IN1);
        PORTB |=  (1 << MOTOR_IZQ_IN2);
        OCR0A = (velocidad < -255) ? 255 : -velocidad;
    } else {
        // Freno activo: ambos transistores en conducción
        PORTD |= (1 << MOTOR_IZQ_IN1);
        PORTB |= (1 << MOTOR_IZQ_IN2);
        OCR0A = 255;
    }
}

// Control de motor derecho (misma lógica que el izquierdo)
void set_motor_der(int16_t velocidad) {
    if (velocidad > 0) {
        PORTD |=  (1 << MOTOR_DER_IN1);
        PORTB &= ~(1 << MOTOR_DER_IN2);
        OCR0B = (velocidad > 255) ? 255 : velocidad;
    } else if (velocidad < 0) {
        PORTD &= ~(1 << MOTOR_DER_IN1);
        PORTB |=  (1 << MOTOR_DER_IN2);
        OCR0B = (velocidad < -255) ? 255 : -velocidad;
    } else {
        // Freno activo
        PORTD |= (1 << MOTOR_DER_IN1);
        PORTB |= (1 << MOTOR_DER_IN2);
        OCR0B = 255;
    }
}

// Modo "coast" (libre) para motor izquierdo: apaga transistores y PWM
void coast_motor_izq(void) {
    PORTD &= ~(1 << MOTOR_IZQ_IN1);
    PORTB &= ~(1 << MOTOR_IZQ_IN2);
    OCR0A = 0;
}

// Modo "coast" (libre) para motor derecho
void coast_motor_der(void) {
    PORTD &= ~(1 << MOTOR_DER_IN1);
    PORTB &= ~(1 << MOTOR_DER_IN2);
    OCR0B = 0;
}

//  CONTROL DE SERVOS

// Genera un pulso de ancho high_us (µs) para servo derecho
void servo_der_pulse(uint16_t high_us) {
    PORTD |= (1 << SERVO_DER_PIN);
    for (uint16_t i = 0; i < high_us; i++) _delay_us(1);
    PORTD &= ~(1 << SERVO_DER_PIN);
    _delay_ms(18);     // Completa período ~20 ms
}

// Mueve servo derecho repetidamente a la posición indicada
void servo_der_move(uint16_t high_us, uint8_t rep) {
    for (uint8_t r = 0; r < rep; r++) servo_der_pulse(high_us);
}

// Genera un pulso de ancho high_us (µs) para servo izquierdo
void servo_izq_pulse(uint16_t high_us) {
    PORTB |= (1 << SERVO_IZQ_PIN);
    for (uint16_t i = 0; i < high_us; i++) _delay_us(1);
    PORTB &= ~(1 << SERVO_IZQ_PIN);
    _delay_ms(18);     // Período total ~20 ms
}

// Mueve servo izquierdo repetidamente a la posición indicada
void servo_izq_move(uint16_t high_us, uint8_t rep) {
    for (uint8_t r = 0; r < rep; r++) servo_izq_pulse(high_us);
}

// Mueve uno de los flippers hacia afuera y lo devuelve al centro
// es_derecho = 1 -> flipper derecho, 0 -> izquierdo
void flipper_out(uint8_t es_derecho) {
    if (es_derecho) {
        servo_der_move(DER_POS_AFUERA, SERVO_REPETICIONES);
        servo_der_move(DER_POS_CENTRO, 5);
    } else {
        servo_izq_move(IZQ_POS_AFUERA, SERVO_REPETICIONES);
        servo_izq_move(IZQ_POS_CENTRO, 5);
    }
}

// Disparo hacia adentro con ambos flippers y luego regreso al centro
void flippers_shot_in(void) {
    servo_der_move(DER_POS_ADENTRO, SERVO_REPETICIONES);
    servo_izq_move(IZQ_POS_ADENTRO, SERVO_REPETICIONES);

    servo_der_move(DER_POS_CENTRO, 5);
    servo_izq_move(IZQ_POS_CENTRO, 5);
}

//  FUNCIONES DE MOVIMIENTO DEL AUTO 

void detener(void) {
    set_motor_izq(0);
    set_motor_der(0);
}

void avanzar(void) {
    set_motor_izq(velocidad_actual);
    set_motor_der(velocidad_actual);
}

void retroceder(void) {
    set_motor_izq(-velocidad_actual);
    set_motor_der(-velocidad_actual);
}

void girar_izquierda(void) {
    set_motor_izq(-VEL_GIRO);
    set_motor_der(VEL_GIRO);
}

void girar_derecha(void) {
    set_motor_izq(VEL_GIRO);
    set_motor_der(-VEL_GIRO);
}

void diagonal_adelante_izq(void) {
    coast_motor_izq();
    set_motor_der(velocidad_actual);
}

void diagonal_adelante_der(void) {
    set_motor_izq(velocidad_actual);
    coast_motor_der();
}

void diagonal_atras_izq(void) {
    set_motor_izq(-velocidad_actual);
    coast_motor_der();
}

void diagonal_atras_der(void) {
    coast_motor_izq();
    set_motor_der(-velocidad_actual);
}

//  SENSORES DE LÍNEA Y BUZZER 

// Lee sensores de línea y enciende el buzzer si alguno detecta línea
void check_line_sensors(void) {
    uint8_t izq_low = !(PINB & (1 << SENSOR_IZQ));  // Activo en nivel bajo
    uint8_t der_low = !(PIND & (1 << SENSOR_DER));  // Activo en nivel bajo
    if (izq_low || der_low) PORTC |=  (1 << BUZZER_PIN);
    else                    PORTC &= ~(1 << BUZZER_PIN);
}

// PROCESAMIENTO DE COMANDOS HC-05 

// Procesa un comando recibido por Bluetooth y ejecuta la acción correspondiente
void processCommand(char cmd) {
    Debug_PrintString("CMD: ");
    Debug_Print(cmd);
    Debug_PrintString("\r\n");

    switch(cmd) {
        // Movimiento básico
        case 'F': avanzar();             HC05_WriteString("OK:F\r\n"); break;
        case 'B': retroceder();          HC05_WriteString("OK:B\r\n"); break;
        case 'L': girar_izquierda();     HC05_WriteString("OK:L\r\n"); break;
        case 'R': girar_derecha();       HC05_WriteString("OK:R\r\n"); break;
        case 'S': detener();             HC05_WriteString("OK:S\r\n"); break;

        // Diagonales
        case 'Q': diagonal_adelante_izq(); HC05_WriteString("OK:Q\r\n"); break;
        case 'E': diagonal_adelante_der(); HC05_WriteString("OK:E\r\n"); break;
        case 'Z': diagonal_atras_izq();    HC05_WriteString("OK:Z\r\n"); break;
        case 'C': diagonal_atras_der();    HC05_WriteString("OK:C\r\n"); break;

        // Flippers: M/N hacia afuera, X disparo hacia adentro
        case 'M':
            HC05_WriteString("OK:M\r\n");
            flipper_out(1);      // Flipper derecho
            break;
        case 'N':
            HC05_WriteString("OK:N\r\n");
            flipper_out(0);      // Flipper izquierdo
            break;
        case 'X':
            HC05_WriteString("OK:X\r\n");
            flippers_shot_in();  // Ambos hacia adentro
            break;

        // Cambios de velocidad
        case '0': velocidad_actual = 0;   HC05_WriteString("OK:0\r\n"); break;
        case '1': velocidad_actual = 50;  HC05_WriteString("OK:1\r\n"); break;
        case '2': velocidad_actual = 100; HC05_WriteString("OK:2\r\n"); break;
        case '3': velocidad_actual = 150; HC05_WriteString("OK:3\r\n"); break;
        case '4': velocidad_actual = 200; HC05_WriteString("OK:4\r\n"); break;
        case '5': velocidad_actual = 255; HC05_WriteString("OK:5\r\n"); break;
    }
}

//  CONFIGURACIÓN DE GPIO 

// Configura todos los pines usados: motores, servos, sensores y buzzer
void GPIO_Init(void) {
    // Direcciones motores y servo derecho
    DDRD |= (1 << MOTOR_IZQ_IN1) | (1 << MOTOR_DER_IN1) | (1 << SERVO_DER_PIN);
    // Direcciones motores y servo izquierdo
    DDRB |= (1 << MOTOR_IZQ_IN2) | (1 << MOTOR_DER_IN2) | (1 << SERVO_IZQ_PIN);

    // Sensores de línea como entrada con pull-up
    DDRB &= ~(1 << SENSOR_IZQ);  PORTB |= (1 << SENSOR_IZQ);
    DDRD &= ~(1 << SENSOR_DER);  PORTD |= (1 << SENSOR_DER);

    // Buzzer como salida (apagado)
    DDRC |= (1 << BUZZER_PIN);
    PORTC &= ~(1 << BUZZER_PIN);

    // Asegura servos en LOW al inicio
    PORTD &= ~(1 << SERVO_DER_PIN);
    PORTB &= ~(1 << SERVO_IZQ_PIN);
}

//  FUNCIÓN PRINCIPAL

int main(void) {
    GPIO_Init();      // Configura pines
    Debug_Init();     // UART debug
    HC05_Init();      // UART software HC-05
    init_pwm();       // PWM para motores

    _delay_ms(500);   // Pequeño delay de arranque

    Debug_PrintString("Robot Iniciado - PWM Configurable\r\n");
    HC05_WriteString("Ready\r\n");

    // Bucle principal: leer sensores y comandos Bluetooth
    while (1) {
        check_line_sensors();                      // Actualiza buzzer según sensores de línea

        if (HC05_Read(&receivedCommand)) {         // Si llega un comando por BT
            processCommand(receivedCommand);       // Ejecuta acción correspondiente
            _delay_ms(10);                         // Pequeño delay para evitar rebotes de lectura
        }
    }
    return 0;
}
