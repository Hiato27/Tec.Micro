#define F_CPU 16000000UL       // Frecuencia del reloj 
#include <avr/io.h>            // Librería para registros del ATmega328P
#include <util/delay.h>        // Librería para funciones de retardo (_delay_ms)
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ======== CONFIGURACIONES GENERALES ========

// Promedia 16 lecturas del ADC para obtener una medición estable
#define NSAMPLES   16
// Tiempo de espera entre lecturas en milisegundos
#define PERIOD_MS  150

// Umbrales de luminosidad (valores de ADC) para clasificar los colores
#define T_ROJO      680
#define T_AZUL      700
#define T_VERDE     780
#define T_AMARILLO  800

// Resistencia fija del divisor de tensión del LDR (10kΩ)
#define R_FIXED_OHM 10000UL   

// Pines RGB conectados al puerto D
#define PIN_R   PD5   // Rojo → pin digital 5
#define PIN_G   PD4   // Verde → pin digital 4
#define PIN_B   PD3   // Azul → pin digital 3

// Servo conectado al pin 9 (PB1 / OC1A)
#define SERVO_DDR  DDRB
#define SERVO_PORT PORTB
#define SERVO_PINB PB1   

// Configuración de UART a 9600 bps
#define BAUD 9600UL
#define UBRR_VALUE ((F_CPU/16/BAUD)-1)


// ======== PROTOTIPOS DE FUNCIONES ========
void uart_init(void);
void uart_tx(char c);
void uart_print(const char* s);
void uart_print_uint(uint32_t v);
void uart_print_mv(uint32_t mv);
void uart_print_kohm_tenths(uint32_t k10);
void uart_sp(void);
void uart_nl(void);

void adc_init(void);
uint16_t adc_read(uint8_t ch);
uint16_t adc_avg(uint8_t ch);

void gpio_init_rgb(void);
void rgb_set(uint8_t r_on, uint8_t g_on, uint8_t b_on);

void servo_init(void);
void servo_set_angle(uint8_t angle);

const char* color_from_raw(uint16_t raw, uint16_t* vEst);


// ======== UART (SERIAL) ========
// Inicializa la comunicación serial a 9600 bps
void uart_init(void){
	UBRR0H = (uint8_t)(UBRR_VALUE>>8);
	UBRR0L = (uint8_t)(UBRR_VALUE & 0xFF);
	UCSR0B = (1<<TXEN0);                        // Habilita transmisión
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);           // Formato: 8 bits, sin paridad, 1 bit stop
}

// Envía un solo carácter
void uart_tx(char c){ while(!(UCSR0A & (1<<UDRE0))); UDR0 = c; }

// Envía una cadena de texto
void uart_print(const char* s){ while(*s) uart_tx(*s++); }

// Envían caracteres de separación y salto de línea
void uart_sp(void){ uart_tx(' '); }
void uart_nl(void){ uart_tx('\r'); uart_tx('\n'); }

// Imprime ceros delante de un número para mantener formato
static void uart_print_zeropad(uint32_t v, uint8_t digits){
	char buf[10]; 
	for(int8_t i=digits-1;i>=0;i--){ buf[i] = '0'+(v%10); v/=10; }
	for(uint8_t i=0;i<digits;i++) uart_tx(buf[i]);
}

// Imprime un número entero sin signo
void uart_print_uint(uint32_t v){
	char buf[11]; uint8_t i=0;
	if(v==0){ uart_tx('0'); return; }
	while(v>0 && i<10){ buf[i++] = '0'+(v%10); v/=10; }
	while(i--) uart_tx(buf[i]);
}

// Imprime un valor en milivoltios (por ejemplo, 4761 -> “4.761”)
void uart_print_mv(uint32_t mv){             
	uart_print_uint(mv/1000);
	uart_tx('.');
	uart_print_zeropad(mv%1000,3);
}

// Imprime una resistencia en kiloohmios con un decimal (ej: 53 -> “5.3”)
void uart_print_kohm_tenths(uint32_t k10){  
	uart_print_uint(k10/10);
	uart_tx('.');
	uart_tx('0'+(k10%10));
}


// ======== ADC (CONVERSOR ANALÓGICO-DIGITAL) ========
// Configura el ADC con referencia en AVcc (5V)
void adc_init(void){
	ADMUX  = (1<<REFS0); // AVcc como referencia
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0); // Prescaler 128
}

// Lee un canal del ADC (0-7)
uint16_t adc_read(uint8_t ch){
	ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
	ADCSRA |= (1<<ADSC);                 // Inicia conversión
	while(ADCSRA & (1<<ADSC));           // Espera a que termine
	return ADC;                          // Devuelve resultado (0–1023)
}

// Promedia varias lecturas para mayor estabilidad
uint16_t adc_avg(uint8_t ch){
	uint32_t s=0;
	for(uint8_t i=0;i<NSAMPLES;i++) s += adc_read(ch);
	return (uint16_t)(s/NSAMPLES);
}


// ======== LED RGB ========
// Configura los pines RGB como salida
void gpio_init_rgb(void){
	DDRD |= (1<<PIN_R)|(1<<PIN_G)|(1<<PIN_B);
	PORTD &= ~((1<<PIN_R)|(1<<PIN_G)|(1<<PIN_B)); // Apaga todos al inicio
}

// Enciende o apaga los LEDs según los parámetros (1 = encendido)
void rgb_set(uint8_t r_on, uint8_t g_on, uint8_t b_on){
	if(r_on) PORTD |=  (1<<PIN_R); else PORTD &= ~(1<<PIN_R);
	if(g_on) PORTD |=  (1<<PIN_G); else PORTD &= ~(1<<PIN_G);
	if(b_on) PORTD |=  (1<<PIN_B); else PORTD &= ~(1<<PIN_B);
}


// ======== SERVO ========
// Inicializa Timer1 para generar una señal PWM de 20 ms (50 Hz)
void servo_init(void){
	SERVO_DDR |= (1<<SERVO_PINB);        // Configura PB1 como salida
	TCCR1A = (1<<COM1A1)|(1<<WGM11);     // Modo PWM con TOP en ICR1
	TCCR1B = (1<<WGM13)|(1<<WGM12)|(1<<CS11); // Prescaler 8
	ICR1   = 40000;                      // Periodo de 20 ms
	servo_set_angle(0);                  // Posición inicial 0°
}

// Ajusta el ángulo del servo (0–180°)
void servo_set_angle(uint8_t angle){
	if(angle>180) angle=180;
	uint16_t counts = 2000 + (uint32_t)angle*2000/180; // Convierte a pulsos
	OCR1A = counts; // Aplica el valor al registro del PWM
}


// ======== CLASIFICACIÓN DE COLOR ========
// Determina el color según el valor del LDR
const char* color_from_raw(uint16_t raw, uint16_t* vEst){
	if(raw < T_ROJO){ *vEst = T_ROJO; return "Rojo"; }
	if(raw < T_AZUL){ *vEst = T_AZUL; return "Azul"; }
	if(raw < T_VERDE){ *vEst = T_VERDE; return "Verde"; }
	*vEst = T_AMARILLO; return "Amarillo";
}


// ======== PROGRAMA PRINCIPAL ========
int main(void){
	uart_init();      // Inicia comunicación serial
	adc_init();       // Configura el ADC
	gpio_init_rgb();  // Inicializa los LEDs RGB
	servo_init();     // Configura el servo

	_delay_ms(200);   // Espera breve al iniciar

	uart_print("raw volt kOhm color valor_establecido diferencia");
	uart_nl();

	while(1){
		// --- LECTURA DEL LDR ---
		uint16_t raw = adc_avg(0); // Lee el canal ADC0

		// Calcula voltaje y resistencia del LDR
		uint32_t vout_mV = (uint32_t)raw * 5000UL / 1023UL;
		uint32_t r_ohm   = (vout_mV>0) ? (R_FIXED_OHM * (5000UL - vout_mV)) / vout_mV : 1000000000UL;
		uint32_t k10     = (r_ohm*10UL + 500UL)/1000UL; // Valor en kiloohmios con 1 decimal

		// Determina el color según el valor leído
		uint16_t vEst=0;
		const char* col = color_from_raw(raw,&vEst);

		// --- CONTROL DEL SERVO ---
		if      (strcmp(col,"Rojo")==0)      servo_set_angle(0);
		else if (strcmp(col,"Amarillo")==0)  servo_set_angle(45);
		else if (strcmp(col,"Verde")==0)     servo_set_angle(90);
		else if (strcmp(col,"Azul")==0)      servo_set_angle(180);

		// --- LED RGB ---
		if      (strcmp(col,"Rojo")==0)      rgb_set(1,0,0);   // Rojo
		else if (strcmp(col,"Amarillo")==0)  rgb_set(1,1,0);   // Amarillo
		else if (strcmp(col,"Verde")==0)     rgb_set(0,1,0);   // Verde
		else if (strcmp(col,"Azul")==0)      rgb_set(0,0,1);   // Azul
		else                                  rgb_set(0,0,0);  // Apagado

		// Calcula diferencia entre valor leído y valor estimado
		uint16_t diff = (raw>vEst)? (raw-vEst) : (vEst-raw);

		// --- ENVÍA DATOS AL SERIAL ---
		uart_print_uint(raw);   uart_sp();
		uart_print_mv(vout_mV); uart_sp();
		uart_print_kohm_tenths(k10); uart_sp();
		uart_print(col);        uart_sp();
		uart_print_uint(vEst);  uart_sp();
		uart_print_uint(diff);  uart_nl();

		// Espera antes de volver a medir
		_delay_ms(PERIOD_MS);
	}
}
