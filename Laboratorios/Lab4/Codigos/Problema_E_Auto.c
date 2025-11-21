#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

#define DER_POS_CENTRO      1200 
#define DER_POS_AFUERA      800
#define DER_POS_ADENTRO     1600

#define IZQ_POS_CENTRO      1500
#define IZQ_POS_AFUERA      1000
#define IZQ_POS_ADENTRO     2000

#define SERVO_REPETICIONES  12

#define MOTOR_IZQ_IN1 PD4
#define MOTOR_IZQ_IN2 PB0
#define MOTOR_IZQ_PWM PD6

#define MOTOR_DER_IN1 PD7
#define MOTOR_DER_IN2 PB5
#define MOTOR_DER_PWM PD5

#define SERVO_DER_PIN PD2
#define SERVO_IZQ_PIN PB4

#define SENSOR_IZQ    PB1
#define SENSOR_DER    PD3
#define BUZZER_PIN    PC2

#define HC05_RX_PIN PB2
#define HC05_TX_PIN PB3
#define HC05_BIT_DELAY 26

#define DEBUG_BAUD 9600
#define DEBUG_UBRR ((F_CPU/16/DEBUG_BAUD)-1)

#define VEL_AVANCE 200
#define VEL_GIRO    150

char receivedCommand = 0;
uint8_t velocidad_actual = VEL_AVANCE;

void Debug_Init(void);
void Debug_PrintString(const char* str);
void Debug_Print(unsigned char data);
void HC05_Init(void);
void HC05_Write(unsigned char data);
void HC05_WriteString(const char* str);
uint8_t HC05_Read(char* data);
void init_pwm(void);
void set_motor_izq(int16_t velocidad);
void set_motor_der(int16_t velocidad);
void coast_motor_izq(void);
void coast_motor_der(void);
void detener(void);
void avanzar(void);
void retroceder(void);
void girar_izquierda(void);
void girar_derecha(void);
void diagonal_adelante_izq(void);
void diagonal_adelante_der(void);
void diagonal_atras_izq(void);
void diagonal_atras_der(void);
void check_line_sensors(void);
void processCommand(char cmd);
void GPIO_Init(void);
void servo_der_pulse(uint16_t high_us);
void servo_der_move(uint16_t high_us, uint8_t rep);
void servo_izq_pulse(uint16_t high_us);
void servo_izq_move(uint16_t high_us, uint8_t rep);
void flipper_out(uint8_t es_derecho);
void flippers_shot_in(void);

void Debug_Init(void) {
	UBRR0H = (unsigned char)(DEBUG_UBRR >> 8);
	UBRR0L = (unsigned char)DEBUG_UBRR;
	UCSR0B = (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void Debug_Print(unsigned char data) {
	while (!(UCSR0A & (1 << UDRE0)));
	UDR0 = data;
}

void Debug_PrintString(const char* str) {
	while (*str) Debug_Print(*str++);
}

void HC05_Init(void) {
	DDRB  |= (1 << HC05_TX_PIN);
	PORTB |= (1 << HC05_TX_PIN);
	DDRB  &= ~(1 << HC05_RX_PIN);
	PORTB |=  (1 << HC05_RX_PIN);
}

void HC05_Write(unsigned char data) {
	cli();
	PORTB &= ~(1 << HC05_TX_PIN);
	_delay_us(HC05_BIT_DELAY);
	for (uint8_t i = 0; i < 8; i++) {
		if (data & 0x01) PORTB |= (1 << HC05_TX_PIN);
		else PORTB &= ~(1 << HC05_TX_PIN);
		data >>= 1;
		_delay_us(HC05_BIT_DELAY);
	}
	PORTB |= (1 << HC05_TX_PIN);
	_delay_us(HC05_BIT_DELAY);
	sei();
}

void HC05_WriteString(const char* str) {
	while (*str) HC05_Write(*str++);
}

uint8_t HC05_Read(char* data) {
	uint16_t timeout = 0;
	while ((PINB & (1 << HC05_RX_PIN)) && timeout < 50000) timeout++;
	if (timeout >= 50000) return 0;
	_delay_us(HC05_BIT_DELAY / 2);
	if (PINB & (1 << HC05_RX_PIN)) return 0;
	uint8_t byte = 0;
	for (uint8_t i = 0; i < 8; i++) {
		_delay_us(HC05_BIT_DELAY);
		byte >>= 1;
		if (PINB & (1 << HC05_RX_PIN)) byte |= 0x80;
	}
	_delay_us(HC05_BIT_DELAY);
	if (!(PINB & (1 << HC05_RX_PIN))) return 0;
	*data = byte;
	return 1;
}

void init_pwm(void) {
	DDRD |= (1 << MOTOR_IZQ_PWM) | (1 << MOTOR_DER_PWM);
	TCCR0A = (1 << WGM00) | (1 << WGM01) | (1 << COM0A1) | (1 << COM0B1);
	TCCR0B = (1 << CS01);
	OCR0A = 0; OCR0B = 0;
}

void set_motor_izq(int16_t velocidad) {
	if (velocidad > 0) { PORTD |= (1 << MOTOR_IZQ_IN1); PORTB &= ~(1 << MOTOR_IZQ_IN2); OCR0A = (velocidad > 255) ? 255 : velocidad; }
	else if (velocidad < 0) { PORTD &= ~(1 << MOTOR_IZQ_IN1); PORTB |= (1 << MOTOR_IZQ_IN2); OCR0A = (velocidad < -255) ? 255 : -velocidad; }
	else { PORTD |= (1 << MOTOR_IZQ_IN1); PORTB |= (1 << MOTOR_IZQ_IN2); OCR0A = 255; }
}

void set_motor_der(int16_t velocidad) {
	if (velocidad > 0) { PORTD |= (1 << MOTOR_DER_IN1); PORTB &= ~(1 << MOTOR_DER_IN2); OCR0B = (velocidad > 255) ? 255 : velocidad; }
	else if (velocidad < 0) { PORTD &= ~(1 << MOTOR_DER_IN1); PORTB |= (1 << MOTOR_DER_IN2); OCR0B = (velocidad < -255) ? 255 : -velocidad; }
	else { PORTD |= (1 << MOTOR_DER_IN1); PORTB |= (1 << MOTOR_DER_IN2); OCR0B = 255; }
}

void coast_motor_izq(void) { PORTD &= ~(1 << MOTOR_IZQ_IN1); PORTB &= ~(1 << MOTOR_IZQ_IN2); OCR0A = 0; }
void coast_motor_der(void) { PORTD &= ~(1 << MOTOR_DER_IN1); PORTB &= ~(1 << MOTOR_DER_IN2); OCR0B = 0; }

void servo_der_pulse(uint16_t high_us) {
	PORTD |= (1 << SERVO_DER_PIN);
	for (uint16_t i = 0; i < high_us; i++) _delay_us(1);
	PORTD &= ~(1 << SERVO_DER_PIN);
	_delay_ms(18);
}

void servo_der_move(uint16_t high_us, uint8_t rep) {
	for (uint8_t r = 0; r < rep; r++) servo_der_pulse(high_us);
}

void servo_izq_pulse(uint16_t high_us) {
	PORTB |= (1 << SERVO_IZQ_PIN);
	for (uint16_t i = 0; i < high_us; i++) _delay_us(1);
	PORTB &= ~(1 << SERVO_IZQ_PIN);
	_delay_ms(18);
}

void servo_izq_move(uint16_t high_us, uint8_t rep) {
	for (uint8_t r = 0; r < rep; r++) servo_izq_pulse(high_us);
}

void flipper_out(uint8_t es_derecho) {
	if (es_derecho) {
		servo_der_move(DER_POS_AFUERA, SERVO_REPETICIONES);
		servo_der_move(DER_POS_CENTRO, 5);
	} else {
		servo_izq_move(IZQ_POS_AFUERA, SERVO_REPETICIONES);
		servo_izq_move(IZQ_POS_CENTRO, 5);
	}
}

void flippers_shot_in(void) {
	servo_der_move(DER_POS_ADENTRO, SERVO_REPETICIONES);
	servo_izq_move(IZQ_POS_ADENTRO, SERVO_REPETICIONES);
	
	servo_der_move(DER_POS_CENTRO, 5);
	servo_izq_move(IZQ_POS_CENTRO, 5);
}

void detener(void) { set_motor_izq(0); set_motor_der(0); }
void avanzar(void) { set_motor_izq(velocidad_actual); set_motor_der(velocidad_actual); }
void retroceder(void) { set_motor_izq(-velocidad_actual); set_motor_der(-velocidad_actual); }
void girar_izquierda(void) { set_motor_izq(-VEL_GIRO); set_motor_der(VEL_GIRO); }
void girar_derecha(void) { set_motor_izq(VEL_GIRO); set_motor_der(-VEL_GIRO); }
void diagonal_adelante_izq(void) { coast_motor_izq(); set_motor_der(velocidad_actual); }
void diagonal_adelante_der(void) { set_motor_izq(velocidad_actual); coast_motor_der(); }
void diagonal_atras_izq(void) { set_motor_izq(-velocidad_actual); coast_motor_der(); }
void diagonal_atras_der(void) { coast_motor_izq(); set_motor_der(-velocidad_actual); }

void check_line_sensors(void) {
	uint8_t izq_low = !(PINB & (1 << SENSOR_IZQ));
	uint8_t der_low = !(PIND & (1 << SENSOR_DER));
	if (izq_low || der_low) PORTC |= (1 << BUZZER_PIN);
	else PORTC &= ~(1 << BUZZER_PIN);
}

void processCommand(char cmd) {
	Debug_PrintString("CMD: "); Debug_Print(cmd); Debug_PrintString("\r\n");
	switch(cmd) {
		case 'F': avanzar(); HC05_WriteString("OK:F\r\n"); break;
		case 'B': retroceder(); HC05_WriteString("OK:B\r\n"); break;
		case 'L': girar_izquierda(); HC05_WriteString("OK:L\r\n"); break;
		case 'R': girar_derecha(); HC05_WriteString("OK:R\r\n"); break;
		case 'S': detener(); HC05_WriteString("OK:S\r\n"); break;
		case 'Q': diagonal_adelante_izq(); HC05_WriteString("OK:Q\r\n"); break;
		case 'E': diagonal_adelante_der(); HC05_WriteString("OK:E\r\n"); break;
		case 'Z': diagonal_atras_izq(); HC05_WriteString("OK:Z\r\n"); break;
		case 'C': diagonal_atras_der(); HC05_WriteString("OK:C\r\n"); break;
		
		case 'M': 
		HC05_WriteString("OK:M\r\n");
		flipper_out(1); 
		break;
		case 'N': 
		HC05_WriteString("OK:N\r\n");
		flipper_out(0); 
		break;
		case 'X': 
		HC05_WriteString("OK:X\r\n");
		flippers_shot_in();
		break;
		
		case '0': velocidad_actual = 0;    HC05_WriteString("OK:0\r\n"); break;
		case '1': velocidad_actual = 50;  HC05_WriteString("OK:1\r\n"); break;
		case '2': velocidad_actual = 100; HC05_WriteString("OK:2\r\n"); break;
		case '3': velocidad_actual = 150; HC05_WriteString("OK:3\r\n"); break;
		case '4': velocidad_actual = 200; HC05_WriteString("OK:4\r\n"); break;
		case '5': velocidad_actual = 255; HC05_WriteString("OK:5\r\n"); break;
	}
}

void GPIO_Init(void) {
	DDRD |= (1 << MOTOR_IZQ_IN1) | (1 << MOTOR_DER_IN1) | (1 << SERVO_DER_PIN);
	DDRB |= (1 << MOTOR_IZQ_IN2) | (1 << MOTOR_DER_IN2) | (1 << SERVO_IZQ_PIN);
	DDRB &= ~(1 << SENSOR_IZQ); PORTB |= (1 << SENSOR_IZQ);
	DDRD &= ~(1 << SENSOR_DER); PORTD |= (1 << SENSOR_DER);
	DDRC |= (1 << BUZZER_PIN); PORTC &= ~(1 << BUZZER_PIN);
	PORTD &= ~(1 << SERVO_DER_PIN);
	PORTB &= ~(1 << SERVO_IZQ_PIN);
}

int main(void) {
	GPIO_Init();
	Debug_Init();
	HC05_Init();
	init_pwm();
	
	_delay_ms(500);
	
	Debug_PrintString("Robot Iniciado - PWM Configurable\r\n");
	HC05_WriteString("Ready\r\n");
	
	while (1) {
		check_line_sensors();
		if (HC05_Read(&receivedCommand)) {
			processCommand(receivedCommand);
			_delay_ms(10);
		}
	}
	return 0;
}
