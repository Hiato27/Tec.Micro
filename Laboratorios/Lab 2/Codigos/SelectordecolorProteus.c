#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


#define NSAMPLES   16
#define PERIOD_MS  150

#define T_ROJO      680
#define T_AZUL      700
#define T_VERDE     780
#define T_AMARILLO  800


#define R_FIXED_OHM 10000UL   

#define PIN_R   PD5
#define PIN_G   PD4
#define PIN_B   PD3


#define SERVO_DDR  DDRB
#define SERVO_PORT PORTB
#define SERVO_PINB PB1   


#define BAUD 9600UL
#define UBRR_VALUE ((F_CPU/16/BAUD)-1)


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


void uart_init(void){
	UBRR0H = (uint8_t)(UBRR_VALUE>>8);
	UBRR0L = (uint8_t)(UBRR_VALUE & 0xFF);
	UCSR0B = (1<<TXEN0);
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00); 
}
void uart_tx(char c){ while(!(UCSR0A & (1<<UDRE0))); UDR0 = c; }
void uart_print(const char* s){ while(*s) uart_tx(*s++); }
void uart_sp(void){ uart_tx(' '); }
void uart_nl(void){ uart_tx('\r'); uart_tx('\n'); }

static void uart_print_zeropad(uint32_t v, uint8_t digits){
	char buf[10]; for(int8_t i=digits-1;i>=0;i--){ buf[i] = '0'+(v%10); v/=10; }
	for(uint8_t i=0;i<digits;i++) uart_tx(buf[i]);
}
void uart_print_uint(uint32_t v){
	char buf[11]; uint8_t i=0;
	if(v==0){ uart_tx('0'); return; }
	while(v>0 && i<10){ buf[i++] = '0'+(v%10); v/=10; }
	while(i--) uart_tx(buf[i]);
}
void uart_print_mv(uint32_t mv){             
	uart_print_uint(mv/1000);
	uart_tx('.');
	uart_print_zeropad(mv%1000,3);
}
void uart_print_kohm_tenths(uint32_t k10){  
	uart_print_uint(k10/10);
	uart_tx('.');
	uart_tx('0'+(k10%10));
}


void adc_init(void){
	ADMUX  = (1<<REFS0); 
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0); 
}
uint16_t adc_read(uint8_t ch){
	ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
	ADCSRA |= (1<<ADSC);
	while(ADCSRA & (1<<ADSC));
	return ADC;
}
uint16_t adc_avg(uint8_t ch){
	uint32_t s=0;
	for(uint8_t i=0;i<NSAMPLES;i++) s += adc_read(ch);
	return (uint16_t)(s/NSAMPLES);
}


void gpio_init_rgb(void){
	DDRD |= (1<<PIN_R)|(1<<PIN_G)|(1<<PIN_B);
	PORTD &= ~((1<<PIN_R)|(1<<PIN_G)|(1<<PIN_B)); 
}
void rgb_set(uint8_t r_on, uint8_t g_on, uint8_t b_on){
	if(r_on) PORTD |=  (1<<PIN_R); else PORTD &= ~(1<<PIN_R);
	if(g_on) PORTD |=  (1<<PIN_G); else PORTD &= ~(1<<PIN_G);
	if(b_on) PORTD |=  (1<<PIN_B); else PORTD &= ~(1<<PIN_B);
}


void servo_init(void){
	SERVO_DDR |= (1<<SERVO_PINB);        
	TCCR1A = (1<<COM1A1)|(1<<WGM11);
	TCCR1B = (1<<WGM13)|(1<<WGM12)|(1<<CS11);
	ICR1   = 40000;                       
	servo_set_angle(0);                    
}
void servo_set_angle(uint8_t angle){
	if(angle>180) angle=180;
	uint16_t counts = 2000 + (uint32_t)angle*2000/180;
	OCR1A = counts;
}

const char* color_from_raw(uint16_t raw, uint16_t* vEst){
	if(raw < T_ROJO){ *vEst = T_ROJO; return "Rojo"; }
	if(raw < T_AZUL){ *vEst = T_AZUL; return "Azul"; }
	if(raw < T_VERDE){ *vEst = T_VERDE; return "Verde"; }
	*vEst = T_AMARILLO; return "Amarillo";
}


int main(void){
	uart_init();
	adc_init();
	gpio_init_rgb();
	servo_init();

	_delay_ms(200);
	uart_print("raw volt kOhm color valor_establecido diferencia"); uart_nl();

	while(1){
		uint16_t raw = adc_avg(0);

		uint32_t vout_mV = (uint32_t)raw * 5000UL / 1023UL;
		uint32_t r_ohm   = (vout_mV>0) ? (R_FIXED_OHM * (5000UL - vout_mV)) / vout_mV : 1000000000UL;
		uint32_t k10     = (r_ohm*10UL + 500UL)/1000UL;

		uint16_t vEst=0;
		const char* col = color_from_raw(raw,&vEst);

		if      (strcmp(col,"Rojo")==0)      servo_set_angle(0);
		else if (strcmp(col,"Amarillo")==0)  servo_set_angle(45);
		else if (strcmp(col,"Verde")==0)     servo_set_angle(90);
		else if (strcmp(col,"Azul")==0)      servo_set_angle(180);

		if      (strcmp(col,"Rojo")==0)      rgb_set(1,0,0);
		else if (strcmp(col,"Amarillo")==0)  rgb_set(1,1,0);
		else if (strcmp(col,"Verde")==0)     rgb_set(0,1,0);
		else if (strcmp(col,"Azul")==0)      rgb_set(0,0,1);
		else                                  rgb_set(0,0,0);

		uint16_t diff = (raw>vEst)? (raw-vEst) : (vEst-raw);

		uart_print_uint(raw);   uart_sp();
		uart_print_mv(vout_mV); uart_sp();
		uart_print_kohm_tenths(k10); uart_sp();
		uart_print(col);        uart_sp();
		uart_print_uint(vEst);  uart_sp();
		uart_print_uint(diff);  uart_nl();

		_delay_ms(PERIOD_MS);
	}
}
