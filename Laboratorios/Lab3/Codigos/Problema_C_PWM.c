// Librerias
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

// Parámetros generales
#define ADC_AVG   10      // Lecturas promediamos del ADC
#define PWM_MAX   15    // Límite de 8 bits para el PWM
#define DEADZONE  30     // Zona muerta (rango de aceptacion)


// UART a 9600 
// Inicializo UART 
static void uart_init(uint32_t baud){
	uint16_t ubrr = (F_CPU/16/baud) - 1;
	UBRR0H = (uint8_t)(ubrr>>8);
	UBRR0L = (uint8_t)(ubrr);
	UCSR0B = (1<<TXEN0);                      // Solo transmisión
	UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);       // 8 bits, 1 stop, sin paridad
}
// Envío un char cuando el registro está listo
static void uart_tx(char c){ while(!(UCSR0A & (1<<UDRE0))); UDR0 = c; }
// Envío una cadena terminada en '\0'
static void uart_str(const char *s){ while(*s) uart_tx(*s++); }
// Envío un enter
static void uart_u16(uint16_t v){
	char b[6]; uint8_t i = 0; if (!v){ uart_tx('0'); return; }
	while (v) { b[i++] = '0' + (v % 10); v /= 10; }
	while (i--) uart_tx(b[i]);
}


// Habilito ADC para pin A0
static void adc_init(void) {
	ADMUX  = (1 << REFS0);                           // Referencia = AVcc (5V)
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Enable + /128
}
// Leo el pin 
static uint16_t adc_read(uint8_t ch){
	ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);           // Selecciono pin
	ADCSRA |= (1 << ADSC);                          // Inicio conversión
	while (ADCSRA & (1 << ADSC));                   // Espero fin
	return ADC;                                     // Devuelvo 0..1023
}
// Varias lecturas para realizar promedio
static uint16_t adc_read_avg(uint8_t ch){
	uint32_t acc = 0;
	for (uint8_t i = 0; i < ADC_AVG; i++) {
		acc += adc_read(ch);
		_delay_ms(1);                               // delay entre lecturas
	}
	return (uint16_t)(acc / ADC_AVG);
}

// D9 como salida PWM 
static void pwm_init(void) {
	DDRB  |= (1 << DDB1);                           // PB1 (D9) salida
	TCCR1A = (1 << COM1A1) | (1 << WGM10);          // No-inverting + Fast PWM 8-bit
	TCCR1B = (1 << WGM12)  | (1 << CS11);           // Prescaler /8 
	OCR1A  = 0;                                     // Arranca con duty = 0
}
// Escribo el duty 
static inline void pwm_set(uint8_t duty) { OCR1A = duty; }

//pines puente H
#define IN1_PORT  PORTD
#define IN1_DDR   DDRD
#define IN1_PIN   PD7

#define IN2_PORT  PORTB
#define IN2_DDR   DDRB
#define IN2_PIN   PB0

// Apaga el moto}
static inline void motor_stop(void) {
	IN1_PORT &= ~(1 << IN1_PIN);
	IN2_PORT &= ~(1 << IN2_PIN);
	pwm_set(0);
}
// Gira sentido horario (IN1=1, IN2=0)
static inline void motor_cw(uint8_t duty) {
	IN1_PORT |=  (1 << IN1_PIN);
	IN2_PORT &= ~(1 << IN2_PIN);
	pwm_set(duty);
}
// Gira sentido antihorario (IN1=0, IN2=1) 
static inline void motor_ccw(uint8_t duty) {
	IN1_PORT &= ~(1 << IN1_PIN);
	IN2_PORT |=  (1 << IN2_PIN);
	pwm_set(duty);
}

//Main
int main(void) {
	// Inicializo periféricos
	uart_init(9600);
	adc_init();
	pwm_init();

	// Configuro pines de dirección como salida y dejo todo apagado
	IN1_DDR |= (1 << IN1_PIN);
	IN2_DDR |= (1 << IN2_PIN);
	motor_stop();

	// Cabecera para el monitor serial
	uart_str("\n=== Control Motor (Pot_ref=A0, Pot_meas=A1 | ENA=D9 PWM, IN1=D7, IN2=D8) ===\n");
	uart_str("CSV: t(ms),ref_adc,meas_adc,pwm,dir(0=STOP,1=CW,2=CCW)\n");

	const uint16_t deadband = DEADZONE;   // rango de zona muerta
	uint32_t tms = 0;                     // Tiempo “lógico” en ms para el CSV

	while(1) {
		// Lee referencia (A0) y medición (A1) promediadas
		uint16_t ref  = adc_read_avg(0);
		uint16_t meas = adc_read_avg(1);

		// Error =
		int16_t  e    = (int16_t)ref - (int16_t)meas;
		uint8_t  dir  = 0;                                 
		uint16_t aerr = (e >= 0) ? e : (uint16_t)(-e);      // error

		// Si el error es chico esta dentro del rango mantengo parado
		if (aerr <= deadband) {
			motor_stop();
			dir = 0;
		} else {
			// Control proporcional 
			uint16_t duty = aerr >> 2;                    
			if (duty > PWM_MAX) duty = PWM_MAX;

			// Decide el sentido según el signo del error
			if (e > 0) {
				motor_cw((uint8_t)duty);                    // subir hacia la referencia
				dir = 1;
			} else {
				motor_ccw((uint8_t)duty);                   // bajar hacia la referencia
				dir = 2;
			}
		}

		// Log en formato CSV para Python y Excel:
		// t(ms), ref(ADC), meas(ADC), pwm(0..255), dir(0/1/2)
		uart_str("CSV:");
		uart_u16((uint16_t)(tms & 0xFFFF)); uart_tx(',');
		uart_u16(ref);  uart_tx(',');
		uart_u16(meas); uart_tx(',');
		uart_u16(OCR1A);uart_tx(',');
		uart_u16(dir);  uart_tx('\n');

		// 50 Hz de refesco
		_delay_ms(20);
		tms += 20;
	}
}
