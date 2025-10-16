#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <string.h>

#define NSAMPLES    16
#define PERIOD_MS   150
#define R_FIXED_OHM 10000UL

#define T_ROJO      466
#define T_AZUL      390
#define T_VERDE     505
#define T_AMARILLO  650

#define BAUD 9600UL
#define UBRR_VALUE ((F_CPU/16/BAUD)-1)

void uart_init(void){
	UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
	UBRR0L = (uint8_t)(UBRR_VALUE & 0xFF);
	UCSR0B = (1<<TXEN0);
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00); 
}

void uart_tx(char c){
	while (!(UCSR0A & (1<<UDRE0))) ;
	UDR0 = c;
}

void uart_print(const char* s){
	while (*s) uart_tx(*s++);
}

void uart_sp(void){ uart_tx(' '); }
void uart_nl(void){ uart_tx('\r'); uart_tx('\n'); }

void uart_print_uint(uint32_t v){
	char buf[11]; uint8_t i = 0;
	if(v == 0){ uart_tx('0'); return; }
	while(v > 0 && i < 10){ buf[i++] = '0' + (v % 10); v /= 10; }
	while(i--) uart_tx(buf[i]);
}

void uart_print_mv(uint32_t mv){
	uart_print_uint(mv / 1000); uart_tx('.');
	uint32_t f = mv % 1000;
	uart_tx('0' + (f / 100)); uart_tx('0' + ((f / 10) % 10)); uart_tx('0' + (f % 10));
}

void uart_print_kohm_tenths(uint32_t k10){
	uart_print_uint(k10 / 10); uart_tx('.'); uart_tx('0' + (k10 % 10));
}

void adc_init(void){
	ADMUX  = (1<<REFS0); 
	ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0); 
}

uint16_t adc_read(uint8_t ch){
	ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
	ADCSRA |= (1<<ADSC);
	while(ADCSRA & (1<<ADSC));
	return ADC;
}

uint16_t adc_avg(uint8_t ch){
	uint32_t s = 0;
	for (uint8_t i = 0; i < NSAMPLES; i++) s += adc_read(ch);
	return (uint16_t)(s / NSAMPLES);
}

const char* color_from_raw(uint16_t raw, uint16_t* vEst){
	if (raw < T_AZUL) { *vEst = T_AZUL; return "Azul"; }
	if (raw < T_ROJO) { *vEst = T_ROJO; return "Rojo"; }
	if (raw < T_VERDE){ *vEst = T_VERDE; return "Verde"; }
	*vEst = T_AMARILLO;
	return "Amarillo";
}

int main(void){
	uart_init();
	adc_init();
	_delay_ms(200);

	uart_print("raw volt kOhm color valor_establecido diferencia"); uart_nl();

	while (1) {
		uint16_t raw = adc_avg(0); // ADC0
		uint32_t vout_mV = (uint32_t)raw * 5000UL / 1023UL;
		uint32_t r_ohm   = (vout_mV > 0) ? (R_FIXED_OHM * (5000UL - vout_mV)) / vout_mV : 1000000000UL;
		uint32_t k10     = (r_ohm * 10UL + 500UL) / 1000UL;

		uint16_t vEst = 0;
		const char* col = color_from_raw(raw, &vEst);

		uint16_t diff = (raw > vEst) ? (raw - vEst) : (vEst - raw);

		uart_print_uint(raw); uart_sp();
		uart_print_mv(vout_mV); uart_sp();
		uart_print_kohm_tenths(k10); uart_sp();
		uart_print(col); uart_sp();
		uart_print_uint(vEst); uart_sp();
		uart_print_uint(diff); uart_nl();

		_delay_ms(PERIOD_MS);
	}
}

