#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

#define PIN_1 PD7
#define PIN_2 PD6
#define PIN_3 PD5
#define PIN_4 PD4

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
		break;

		case 'G':
		direccion_atras();
		break;

		default:
		break;
	}
}

int main(void) {
	direccion_init();

	while (1) {
		char c = 'F'; // Ejemplo de comando
		ejecutar_comando(c);
	}
}
