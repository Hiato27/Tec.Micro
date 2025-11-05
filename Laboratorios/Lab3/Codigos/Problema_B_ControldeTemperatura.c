//Control de temperatura mediante PWM

//Librerias
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h>

// PWM para el calefactor
// Fast PWM 8-bit
static inline void heater_pwm_init(void){
    DDRD  |= (1<<DDD5);                 // D5 como salida digital
    TCCR0A = (1<<COM0B1)                // habilita OC0B en modo no invertido
           | (1<<WGM01) | (1<<WGM00);   // Fast PWM (TOP=0xFF)
    TCCR0B = (1<<CS01) | (1<<CS00);     // reloj de Timer0 = F_CPU/64
    OCR0B  = 0;                         // arranca apagado 
}

// Fija el ciclo útil del heater de 0 a 255
static inline void heater_set_pwm(uint8_t duty){
    OCR0B = duty;                       // escribe el duty en el comparador B
}


static void uart_init(uint32_t baud){
	uint16_t ubrr = (F_CPU/16/baud) - 1;
	UBRR0H = (uint8_t)(ubrr>>8);
	UBRR0L = (uint8_t)(ubrr);
	UCSR0B = (1<<TXEN0) | (1<<RXEN0);
	UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);
}
static void uart_tx(char c){ while(!(UCSR0A & (1<<UDRE0))); UDR0=c; }
static void uart_tx_str(const char *s){ while(*s) uart_tx(*s++); }
static void uart_tx_u16(uint16_t v){
	char b[6]; uint8_t i=0; if(!v){uart_tx('0');return;}
	while(v){ b[i++]='0'+(v%10); v/=10; } while(i--) uart_tx(b[i]);
}
static int16_t uart_readline(char *dst, uint8_t maxlen){
	uint8_t i=0;
	while(UCSR0A & (1<<RXC0)){
		char c=UDR0; if(c=='\r') continue;
		if(c=='\n'){ dst[i]=0; return (int16_t)i; }
		if(i<maxlen-1) dst[i++]=c;
	}
	return -1;
}

static void adc_init(void){
	ADMUX  = (1<<REFS0);
	ADCSRA = (1<<ADEN) | (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
}
static uint16_t adc_read_10bit(uint8_t ch){
	ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);
	ADCSRA |= (1<<ADSC);
	while(ADCSRA & (1<<ADSC));
	return ADC;
}
static int16_t lm35_tempC_from_adc(uint16_t adc){
	uint32_t num = (uint32_t)adc * 500UL + 511UL;
	return (int16_t)(num / 1023UL);
}


static void pwm_fan_init(void){
	DDRB  |= (1<<DDB3);
	TCCR2A = (1<<COM2A1) | (1<<WGM21) | (1<<WGM20);
	TCCR2B = (1<<CS22);
	OCR2A  = 0;
}
static inline void fan_set_pwm(uint8_t duty){ OCR2A = duty; }

static int16_t punto_medio = 26;

static void help_menu(void){
	uart_tx_str("\nComandos:\n");
	uart_tx_str("  m=<entero>   -> fija punto medio (°C). Ej: m=27\n");
	uart_tx_str("  h            -> ayuda\n");
	uart_tx_str("Banda ideal: pm-3 .. pm+3\n");
}

static void apply_control(int16_t tempC){
	if(tempC <= (punto_medio - 15)){
		heater_set_pwm(255);
		fan_set_pwm(0);
		uart_tx_str("ACT:HEAT_100;");
		} else if(tempC <= (punto_medio - 10)){
		heater_set_pwm(255);
		fan_set_pwm(0);
		uart_tx_str("ACT:HEAT_70;");
		} else if(tempC <= (punto_medio - 4)){
		heater_set_pwm(255);
		fan_set_pwm(0);
		uart_tx_str("ACT:HEAT_40;");
		} else if(tempC <= (punto_medio + 4)){
		heater_set_pwm(255);
		fan_set_pwm(0);
		uart_tx_str("ACT:IDLE;");
		} else if(tempC <= (punto_medio + 14)){
		heater_set_pwm(0);
		fan_set_pwm(100);
		uart_tx_str("ACT:FAN_LOW;");
		} else if(tempC <= (punto_medio + 24)){
		heater_set_pwm(0);
		fan_set_pwm(170);
		uart_tx_str("ACT:FAN_MED;");
		} else {
		heater_set_pwm(0);
		fan_set_pwm(240);
		uart_tx_str("ACT:FAN_HIGH;");
	}
}

static void process_line(char *line){
	if(line[0]=='h' || line[0]=='H'){ help_menu(); return; }
	if(line[0]=='m' && line[1]=='='){
		int16_t v=0; bool neg=false; uint8_t i=2;
		if(line[i]=='-'){ neg=true; i++; }
		for(; line[i]; ++i){
			if(line[i]<'0'||line[i]>'9') break;
			v=(int16_t)(v*10+(line[i]-'0'));
		}
		if(neg) v=-v;
		if(v>=5 && v<=80){
			punto_medio=v;
			uart_tx_str("OK punto_medio=");
			uart_tx_u16((uint16_t)punto_medio);
			uart_tx('\n');
			} else {
			uart_tx_str("ERR: rango 5..80\n");
		}
		return;
	}
	uart_tx_str("Comando no valido. Use 'h'.\n");
}

int main(void){
	uart_init(9600);
	adc_init();
	pwm_heater_init();
	pwm_fan_init();
	sei();

	uart_tx_str("\n=== Control Temp (Heater PWM D5 / Fan PWM D11) ===\n");
	uart_tx_str("LM35 en A0. Punto medio = ");
	uart_tx_u16((uint16_t)punto_medio);
	uart_tx_str(" C\n");
	help_menu();
      	uart_tx_str("CSV: t(s), TempC, HeaterPWM(0-255), FanPWM(0-255)\n");

	uint32_t t=0; char line[32];

	while(1){
		uint16_t adc = adc_read_10bit(0);
		int16_t  tempC = lm35_tempC_from_adc(adc);

		apply_control(tempC);

		uart_tx_str("CSV:");
		uart_tx_u16((uint16_t)t);  uart_tx(',');
		uart_tx_u16((uint16_t)tempC); uart_tx(',');
		uart_tx_u16(OCR0B);        uart_tx(',');
		uart_tx_u16(OCR2A);        uart_tx('\n');

		int16_t n = uart_readline(line, sizeof(line));
		if(n>=0) process_line(line);

		_delay_ms(1000);
		t++;
	}
}


