// Ultrasonido para medir distancia 10 Intencidades de LED

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

//pines
#define TRIG_PORT PORTB
#define TRIG_DDR  DDRB
#define TRIG_BIT  PB0     // D8

#define ECHO_PIN  PIND
#define ECHO_DDR  DDRD
#define ECHO_BIT  PD2     // D2

//Pwm en pin 9
static void pwm_iniciar(void){
	DDRB  |= (1<<PB1);                         // D9 salida
	TCCR1A = (1<<WGM10) | (1<<COM1A1);         // Fast PWM 8-bit
	TCCR1B = (1<<WGM12) | (1<<CS11) | (1<<CS10);// ~976 Hz
	OCR1A  = 0;
}
static inline void pwm_habilitar(void){
	TCCR1A = (TCCR1A & ~((1<<COM1A1)|(1<<COM1A0))) | (1<<COM1A1);
}
static inline void pwm_apagar_total(void){
	TCCR1A &= ~((1<<COM1A1)|(1<<COM1A0));  // Apagar competamente si distancia Mayor a 40cm
	DDRB   |=  (1<<PB1);
	PORTB  &= ~(1<<PB1);                   
}

//intencidades pwm
static void pwm_nivel(uint8_t nivel){
	if(nivel==0){ pwm_apagar_total(); return; }
	if(nivel>10) nivel=10;
	pwm_habilitar();
	uint16_t duty = (nivel*255u)/10u;      
	if(duty==0) duty=1; if(duty>=255) duty=254;
	OCR1A = (uint8_t)duty;
}

//Radar ultrasonico
static void sonar_iniciar(void){
	TRIG_DDR |=  (1<<TRIG_BIT);   // TRIG out
	ECHO_DDR &= ~(1<<ECHO_BIT);   // ECHO in
	TRIG_PORT &= ~(1<<TRIG_BIT);  // TRIG bajo

	// Timer
	TCCR0A = 0x00;
	TCCR0B = (1<<CS01);
}

static void sonar_trigger(void){
	TRIG_PORT &= ~(1<<TRIG_BIT);
	_delay_us(2);
	TRIG_PORT |=  (1<<TRIG_BIT);
	_delay_us(10);
	TRIG_PORT &= ~(1<<TRIG_BIT);
}

static uint32_t medir_pulso_us(void){
	// espera subida 
	uint32_t to = 60000UL;
	while(((ECHO_PIN&(1<<ECHO_BIT))==0) && to){ to--; }
	if(!to) return 0;

	// mide altura
	TCNT0=0; TIFR0=(1<<TOV0);
	uint16_t desb=0; to=120000UL;
	while((ECHO_PIN&(1<<ECHO_BIT)) && to){
		if(TIFR0&(1<<TOV0)){ TIFR0=(1<<TOV0); desb++; }
		to--;
	}
	if(!to) return 0;

	uint32_t ticks = ((uint32_t)desb<<8) + (uint32_t)TCNT0; 
	return ticks/2; 
}
//us a cm
static uint16_t us_a_cm(uint32_t us){
	if(us==0) return 9999;  
	return (uint16_t)(us/58UL);
}

//4cm por paso
static uint8_t distancia_a_nivel(uint16_t cm){
	if(cm>=40) return 0;
	return (uint8_t)(10 - (cm/4)); // 0..10
}
//main
int main(void){
	pwm_iniciar();
	sonar_iniciar();
	_delay_ms(100);

	while(1){
		sonar_trigger();
		uint32_t t_us = medir_pulso_us();
		uint16_t cm   = us_a_cm(t_us);
		uint8_t nivel = distancia_a_nivel(cm);
		pwm_nivel(nivel);
		_delay_ms(100);
	}
}
