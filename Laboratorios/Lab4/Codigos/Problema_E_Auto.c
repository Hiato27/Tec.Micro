#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define BAUD 9600UL
#define UBRR_VAL ((F_CPU / (16UL * BAUD)) - 1)

#define PIN_1 PD7
#define PIN_2 PD6
#define PIN_3 PD5
#define PIN_4 PD4

void uart_init(void) {
	UBRR0H = (uint8_t)(UBRR_VAL >> 8);
	UBRR0L = (uint8_t)(UBRR_VAL & 0xFF);

	UCSR0B = (1 << TXEN0) | (1 << RXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

char uart_read(void) {
	while (!(UCSR0A & (1 << RXC0)));
	return UDR0;
}

void pwm_init(void) {
	DDRB |= (1 << PB1) | (1 << PB2);

	TCCR1A = (1 << WGM10);
	TCCR1B = (1 << WGM12);

	TCCR1A |= (1 << COM1A1) | (1 << COM1B1);

	TCCR1B |= (1 << CS11) | (1 << CS10);

	OCR1A = 0;
	OCR1B = 0;
}

void motor_izq(uint8_t duty) {
	OCR1A = duty;
}

void motor_der(uint8_t duty) {
	OCR1B = duty;
}

void direccion_adelante(void) {
	PORTD |=  (1 << PIN_1) | (1 << PIN_3);
	PORTD &= ~((1 << PIN_2) | (1 << PIN_4));
}

void direccion_atras(void) {
	PORTD |=  (1 << PIN_2) | (1 << PIN_4);
	PORTD &= ~((1 << PIN_1) | (1 << PIN_3));
}

void direccion_init(void) {
	DDRD |= (1 << PIN_1) | (1 << PIN_2) | (1 << PIN_3) | (1 << PIN_4);
	direccion_adelante();
}

void ejecutar_comando(char c) {
	switch (c) {
		case 'F':
		direccion_adelante();
		motor_izq(255);
		motor_der(255);
		break;

		case 'I':
		direccion_adelante();
		motor_izq(255);
		motor_der(255 / 2);
		break;

		case 'R':
		direccion_adelante();
		motor_izq(255);
		motor_der(0);
		break;

		case 'H':
		direccion_adelante();
		motor_der(255);
		motor_izq(255 / 2);
		break;

		case 'L':
		direccion_adelante();
		motor_der(255);
		motor_izq(0);
		break;

		case 'G':
		direccion_atras();
		motor_izq(255);
		motor_der(255);
		break;

		case 'K':
		direccion_atras();
		motor_izq(255);
		motor_der(255 / 2);
		break;

		case 'J':
		direccion_atras();
		motor_der(255);
		motor_izq(255 / 2);
		break;

		case 'S':
		motor_izq(0);
		motor_der(0);
		break;

		default:
		break;
	}
}

int main(void) {
	uart_init();
	pwm_init();
	direccion_init();

	while (1) {
		char c = uart_read();
		ejecutar_comando(c);
	}
}
