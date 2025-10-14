#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

/* Tipo para canciones */
typedef struct {
	uint16_t f;   /* frecuencia en Hz */
	uint16_t ms;  /* duración en milisegundos */
} nota_t;

#define FIN_CANCION 0xFFFF  /* usar {FIN_CANCION,0} como final */

/*  Pines */
#define BUZZER_DDR   DDRB
#define BUZZER_PORT  PORTB
#define BUZZER_PIN   PB1             

/* Pulsadores */
static inline void teclas_ini(void){
	DDRC  &= ~0x3F;                   
	PORTC |=  0x3F;                   // pull-up
	DDRD  &= ~((1<<PD2)|(1<<PD3));    // D2/D3 entrada
	PORTD |=  ((1<<PD2)|(1<<PD3));    // pull-up
}

/* Devuelve índice 0 a 7 o -1 si ninguna  */
static int8_t leer_tecla(void){
	if(!(PINC & (1<<PC0))) return 0;  // A0 -> Do
	if(!(PINC & (1<<PC1))) return 1;  // A1 -> Re
	if(!(PINC & (1<<PC2))) return 2;  // A2 -> Mi
	if(!(PINC & (1<<PC3))) return 3;  // A3 -> Fa
	if(!(PINC & (1<<PC4))) return 4;  // A4 -> Sol
	if(!(PINC & (1<<PC5))) return 5;  // A5 -> La
	if(!(PIND & (1<<PD2))) return 6;  // D2 -> Si
	if(!(PIND & (1<<PD3))) return 7;  // D3 -> Sol
	return -1;
}

/* UART 9600 8N1 */
static void uart_ini(uint16_t ubrr){
	UBRR0H = (uint8_t)(ubrr>>8);
	UBRR0L = (uint8_t)ubrr;           // 16 MHz, 9600 -> UBRR=103
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);   // RX/TX
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00); // 8N1
}
static inline void uart_tx(char c){ while(!(UCSR0A & (1<<UDRE0))); UDR0 = c; }
static void uart_print(const char* s){ while(*s) uart_tx(*s++); }
static inline bool uart_rx_disponible(void){ return (UCSR0A & (1<<RXC0)); }
static inline char uart_rx(void){ return UDR0; }

/* Tonos con Timer1 CTC toggle en OC1A */
static void tono_iniciar(uint16_t freq_hz){
	if(freq_hz == 0){
		TCCR1A = 0; TCCR1B = 0;
		PORTB &= ~(1<<BUZZER_PIN);
		return;
	}

	BUZZER_DDR |= (1<<BUZZER_PIN);

	struct Opt { uint16_t presc; uint8_t csbits; } opts[3] = {
	{1,  (1<<CS10)},                 // clk/1
	{8,  (1<<CS11)},                 // clk/8
	{64, (1<<CS11)|(1<<CS10)}        // clk/64
};

uint32_t best_err = 0xFFFFFFFFUL;
uint16_t best_ocr = 0;
uint8_t  best_cs  = (1<<CS11);      // default seguro

for (uint8_t i=0; i<3; ++i){
	uint32_t presc = opts[i].presc;

	// OCR = round(F_CPU / (2*presc*freq)) - 1
	uint32_t num = (uint32_t)F_CPU;
	uint32_t den = 2UL * presc * (uint32_t)freq_hz;
	if (den == 0) continue;

	uint32_t ocr_calc = (num + den/2) / den;   // redondeo al entero más cercano
	if (ocr_calc == 0) ocr_calc = 1;           // evita 0
	uint32_t ocr = ocr_calc - 1;

	if (ocr > 65535UL) continue;               // fuera de rango

	// Frecuencia real lograda
	uint32_t f_real = (uint32_t)F_CPU / (2UL * presc * (ocr + 1UL));
	uint32_t err = (f_real > freq_hz) ? (f_real - freq_hz) : (freq_hz - f_real);

	if (err < best_err){
		best_err = err;
		best_ocr = (uint16_t)ocr;
		best_cs  = opts[i].csbits;
	}
}

TCCR1A = (1<<COM1A0);             // toggle OC1A
TCCR1B = (1<<WGM12) | best_cs;    // CTC + mejor prescaler
OCR1A  = best_ocr;
}
static inline void tono_detener(void){
	TCCR1A = 0; TCCR1B = 0;
	PORTB &= ~(1<<BUZZER_PIN);
}

// Notas en Hz
#define NOTA_DO        261   // C4
#define NOTA_RE        293   // D4
#define NOTA_MI        329   // E4
#define NOTA_FA        349   // F4
#define NOTA_SOL       392   // G4
#define NOTA_LA        440   // A4
#define NOTA_SI        494   // B4
#define NOTA_DO_AGUDO  523   // C5

// Mapa del piano
static const uint16_t notas[8] = {
	NOTA_DO, NOTA_RE, NOTA_MI, NOTA_FA,
	NOTA_SOL, NOTA_LA, NOTA_SI, NOTA_SOL
};

// Velocidad del volumen 
#define TEMPO_NUM   160   
#define TEMPO_DEN   100
#define GATE_PCT     82   

static inline uint16_t escalar_ms(uint16_t ms){
	// redondeo para tiempos cortos
	return (uint16_t)(((uint32_t)ms * TEMPO_NUM + (TEMPO_DEN/2)) / TEMPO_DEN);
}

//  Canciones 
