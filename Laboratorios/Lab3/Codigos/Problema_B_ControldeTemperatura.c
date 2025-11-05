//Control de temperatura mediante PWM

//Librerias
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h>

// UART serial 9600 
static void uart_init(uint32_t baud){
	// Calcula y carga el divisor de baudrate
	uint16_t ubrr = (F_CPU/16/baud) - 1;
	UBRR0H = (uint8_t)(ubrr>>8);
	UBRR0L = (uint8_t)(ubrr);
	// Habilita TX y RX
	UCSR0B = (1<<TXEN0) | (1<<RXEN0);
	// 8 bits, sin paridad
	UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);
}

// Envía un carácter
static void uart_tx(char c){ while(!(UCSR0A & (1<<UDRE0))); UDR0=c; }

// Envía una cadena terminada en '\0'
static void uart_tx_str(const char *s){ while(*s) uart_tx(*s++); }

// Envía un entero sin signo 
static void uart_tx_u16(uint16_t v){
	char b[6]; uint8_t i=0; if(!v){uart_tx('0');return;}
	while(v){ b[i++]='0'+(v%10); v/=10; } while(i--) uart_tx(b[i]);
}

// Lee una línea por UART 
static int16_t uart_readline(char *dst, uint8_t maxlen){
	uint8_t i=0;
	while(UCSR0A & (1<<RXC0)){
		char c=UDR0; if(c=='\r') continue;           // ignora CR
		if(c=='\n'){ dst[i]=0; return (int16_t)i; }  // termina la línea
		if(i<maxlen-1) dst[i++]=c;                   // acumula
	}
	return -1;
}

//ADC LM35 en A0 
static void adc_init(void){
	ADMUX  = (1<<REFS0);                              // Referencia AVcc
	ADCSRA = (1<<ADEN) | (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0); // Prescaler /128
}

// Lee el ADC a 10 bits del canal 'ch'
static uint16_t adc_read_10bit(uint8_t ch){
	ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);            // selecciona canal
	ADCSRA |= (1<<ADSC);                              // inicia conversión
	while(ADCSRA & (1<<ADSC));                        // espera fin
	return ADC;                                       // devuelve 0..1023
}

// Convierte lectura del LM35 a °C
static int16_t lm35_tempC_from_adc(uint16_t adc){
	uint32_t num = (uint32_t)adc * 500UL + 511UL;     // suma para redondear
	return (int16_t)(num / 1023UL);
}

// PWM calefactor 
static void pwm_heater_init(void){
	DDRD  |= (1<<DDD5);                               // D5 como salida
	TCCR0A = (1<<COM0B1) | (1<<WGM01) | (1<<WGM00);   // Fast PWM, no inversor
	TCCR0B = (1<<CS01) | (1<<CS00);                   // prescaler /64 (~976 Hz)
	OCR0B  = 0;                                       // arranca apagado
}
// Cambia el duty del calefactor (0..255)
static inline void heater_set_pwm(uint8_t duty){ OCR0B = duty; }

// PWM ventilador 
static void pwm_fan_init(void){
	DDRB  |= (1<<DDB3);                               // D11 como salida
	TCCR2A = (1<<COM2A1) | (1<<WGM21) | (1<<WGM20);   // Fast PWM, no inversor
	TCCR2B = (1<<CS22);                               // prescaler /64 (~976 Hz)
	OCR2A  = 0;                                       // arranca apagado
}
// Cambia el duty del ventilador (0..255)
static inline void fan_set_pwm(uint8_t duty){ OCR2A = duty; }

//punto medio por defecto
static int16_t punto_medio = 26;                     // °C }

// Muestra comandos disponibles por serie
static void help_menu(void){
	uart_tx_str("\nComandos:\n");
	uart_tx_str("  m=<entero>   -> fija punto medio (°C). Ej: m=27\n");
	uart_tx_str("  h            -> ayuda\n");
	uart_tx_str("Banda ideal: pm-3 .. pm+3\n");
}

// control
static void apply_control(int16_t tempC){
	if(tempC <= (punto_medio - 15)){
		heater_set_pwm(255);                           // heater encendido
		fan_set_pwm(0);
		uart_tx_str("ACT:HEAT_100;");
	} else if(tempC <= (punto_medio - 10)){
		heater_set_pwm(255);                           // heater encendido
		fan_set_pwm(0);
		uart_tx_str("ACT:HEAT_70;");
	} else if(tempC <= (punto_medio - 4)){
		heater_set_pwm(255);                           // heater encendido
		fan_set_pwm(0);
		uart_tx_str("ACT:HEAT_40;");
	} else if(tempC <= (punto_medio + 4)){
		heater_set_pwm(255);                           // banda muerta (sin ventilar)
		fan_set_pwm(0);
		uart_tx_str("ACT:IDLE;");
	} else if(tempC <= (punto_medio + 14)){
		heater_set_pwm(0);
		fan_set_pwm(100);                              // fan velocidad baja
		uart_tx_str("ACT:FAN_LOW;");
	} else if(tempC <= (punto_medio + 24)){
		heater_set_pwm(0);
		fan_set_pwm(170);                              // fan velocidad media
		uart_tx_str("ACT:FAN_MED;");
	} else {
		heater_set_pwm(0);
		fan_set_pwm(240);                              // fan velocidad alta
		uart_tx_str("ACT:FAN_HIGH;");
	}
}

// Procesa una línea de comando recibida por UART
static void process_line(char *line){
	if(line[0]=='h' || line[0]=='H'){ help_menu(); return; } // ayuda

	// m=XX -> cambia el punto medio (con límites)
	if(line[0]=='m' && line[1]=='='){
		int16_t v=0; bool neg=false; uint8_t i=2;
		if(line[i]=='-'){ neg=true; i++; }            // acepta signo
		for(; line[i]; ++i){
			if(line[i]<'0'||line[i]>'9') break;       // solo dígitos
			v=(int16_t)(v*10+(line[i]-'0'));
		}
		if(neg) v=-v;
		if(v>=5 && v<=80){                            // rango razonable
			punto_medio=v;
			uart_tx_str("OK punto_medio=");
			uart_tx_u16((uint16_t)punto_medio);
			uart_tx('\n');
		} else {
			uart_tx_str("ERR: rango 5..80\n");// debug
		}
		return;
	}

	uart_tx_str("Comando no valido. Use 'h'.\n");     // debug
}

// main
int main(void){
	uart_init(9600);                                  // UART a 9600 bps
	adc_init();                                       // ADC listo
	pwm_heater_init();                                // PWM calefactor
	pwm_fan_init();                                   // PWM ventilador
	sei();                                            // habilita interrupciones

	// Mensaje de arranque por serie
	uart_tx_str("\n=== Control Temp (Heater PWM D5 / Fan PWM D11) ===\n");
	uart_tx_str("LM35 en A0. Punto medio = ");
	uart_tx_u16((uint16_t)punto_medio);
	uart_tx_str(" C\n");
	help_menu();
	uart_tx_str("CSV: t(s), TempC, HeaterPWM(0-255), FanPWM(0-255)\n");

	uint32_t t=0; char line[32];                      // contador y buffer RX

	while(1){
		// Mide temperatura
		uint16_t adc = adc_read_10bit(0);             // canal A0
		int16_t  tempC = lm35_tempC_from_adc(adc);    // pasa a °C

		// Aplica control según la temperatura
		apply_control(tempC);

		// Log en formato CSV
		uart_tx_str("CSV:");
		uart_tx_u16((uint16_t)t);  uart_tx(',');
		uart_tx_u16((uint16_t)tempC); uart_tx(',');
		uart_tx_u16(OCR0B);        uart_tx(',');      // duty calefactor
		uart_tx_u16(OCR2A);        uart_tx('\n');     // duty ventilador

		// Lee uart
		int16_t n = uart_readline(line, sizeof(line));
		if(n>=0) process_line(line);

		_delay_ms(1000);                               // 1 segundo
		t++;                                          // tiempo en segundos
	}
}




