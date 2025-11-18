#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

#define PIN_1 PD7
#define PIN_2 PD6
#define PIN_3 PD5
#define PIN_4 PD4

void pwm_init(void) {
	DDRB |= (1 << PB1) | (1 << PB2); // Configuramos los pines de salida para PWM
	TCCR1A = (1 << WGM10);
	TCCR1B = (1 << WGM12);
	TCCR1A |= (1 << COM1A1) | (1 << COM1B1);
	TCCR1B |= (1 << CS11) | (1 << CS10); // Prescaler 64
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

		case 'G':
		direccion_atras();
		motor_izq(255);
		motor_der(255);
		break;

		default:
		break;
	}
}

int main(void) {
	pwm_init();
	direccion_init();

	while (1) {
		char c = 'F'; // Ejemplo de comando
		ejecutar_comando(c);
	}
}
