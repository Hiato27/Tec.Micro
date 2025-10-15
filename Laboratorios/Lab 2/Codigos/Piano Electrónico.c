#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

// Tipo de nota 
typedef struct {
	uint16_t f;   // frecuencia en Hz (0 = silencio)
	uint16_t ms;  // duración en milisegundos
} nota_t;

#define FIN_CANCION 0xFFFF  // usar {FIN_CANCION,0} como fin

// Pines 
#define BUZZER_DDR   DDRB
#define BUZZER_PORT  PORTB
#define BUZZER_PIN   PB1      // OC1A (D9)

// Botones
static inline void teclas_ini(void){
	DDRC  &= ~0x3F;                   // PC0..PC5 entrada (A0..A5)
	PORTC |=  0x3F;                   // pull-up
	DDRD  &= ~((1<<PD2)|(1<<PD3));    // D2/D3 entrada
	PORTD |=  ((1<<PD2)|(1<<PD3));    // pull-up
}

// Lee el estado de las teclas: 0..7 o -1 si ninguna 
static inline int8_t leer_tecla_cruda(void){
	if(!(PINC & (1<<PC0))) return 0;  // Do
	if(!(PINC & (1<<PC1))) return 1;  // Re
	if(!(PINC & (1<<PC2))) return 2;  // Mi
	if(!(PINC & (1<<PC3))) return 3;  // Fa
	if(!(PINC & (1<<PC4))) return 4;  // Sol
	if(!(PINC & (1<<PC5))) return 5;  // La  
	if(!(PIND & (1<<PD2))) return 6;  // Si
	if(!(PIND & (1<<PD3))) return 7;  // Sol
	return -1;
}

// Debounce robusto: exige estabilidad ~15 ms 
#define DB_CHECKS  15   // 15 muestras
#define DB_DELAYMS 1    // separadas 1 ms
static int8_t leer_tecla_estable(void){
	int8_t first = leer_tecla_cruda();
	for(uint8_t i=0; i<DB_CHECKS; ++i){
		_delay_ms(DB_DELAYMS);
		int8_t v = leer_tecla_cruda();
		if(v != first) return -1; // si cambia, descartamos (ruido)
	}
	return first;
}

// UART 9600 8N1 
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

// Generador de tono (Timer1 CTC, toggle OC1A) 
// Arranque “limpio”: apagar, cargar OCR, TCNT1=0, encender 
static void tono_iniciar(uint16_t freq_hz){
	// Siempre empezamos con salida en bajo y timer apagado
	TCCR1A = 0; TCCR1B = 0;
	BUZZER_DDR  |= (1<<BUZZER_PIN);
	BUZZER_PORT &= ~(1<<BUZZER_PIN);

	if(freq_hz == 0){
		return; // silencio
	}

	struct Opt { uint16_t presc; uint8_t csbits; } opts[3] = {
	{1,  (1<<CS10)},                 // clk/1
	{8,  (1<<CS11)},                 // clk/8
	{64, (1<<CS11)|(1<<CS10)}        // clk/64
};

uint32_t best_err = 0xFFFFFFFFUL;
uint16_t best_ocr = 0;
uint8_t  best_cs  = (1<<CS10);      // por defecto clk/1

for (uint8_t i=0; i<3; ++i){
	uint32_t presc = opts[i].presc;
	uint32_t den = 2UL * presc * (uint32_t)freq_hz;
	if (!den) continue;

	uint32_t ocr_calc = (F_CPU + den/2) / den; // redondeo
	if (ocr_calc == 0) ocr_calc = 1;
	uint32_t ocr = ocr_calc - 1;
	if (ocr > 65535UL) continue;

	uint32_t f_real = (uint32_t)F_CPU / (2UL * presc * (ocr + 1UL));
	uint32_t err = (f_real > freq_hz) ? (f_real - freq_hz) : (freq_hz - f_real);
	if (err < best_err){
		best_err = err; best_ocr = (uint16_t)ocr; best_cs = opts[i].csbits;
	}
}

OCR1A  = best_ocr;              // cargar primero
TCNT1  = 0;                     // reiniciar contador (evita primer pulso raro)
TCCR1A = (1<<COM1A0);           // toggle OC1A
TCCR1B = (1<<WGM12) | best_cs;  // CTC + prescaler
}
static inline void tono_detener(void){
	TCCR1A = 0; TCCR1B = 0;
	BUZZER_PORT &= ~(1<<BUZZER_PIN);
}

// Notas (Hz)
#define NOTA_DO        261 // C4
#define NOTA_RE        293 // D4
#define NOTA_MI        329 // E4
#define NOTA_FA        349 // F4
#define NOTA_SOL       392 // G4
#define NOTA_LA        440 // A4
#define NOTA_SI        494 // B4
#define NOTA_DO_AGUDO  523 // C5

// Mapa del piano 
static const uint16_t notas[8] = {
	NOTA_DO, NOTA_RE, NOTA_MI, NOTA_FA,
	NOTA_SOL, NOTA_LA, NOTA_SI, NOTA_SOL
};

// Tempo / Gate 
#define TEMPO_NUM   160
#define TEMPO_DEN   100
#define GATE_PCT     82

static inline uint16_t escalar_ms(uint16_t ms){
	return (uint16_t)(((uint32_t)ms * TEMPO_NUM + (TEMPO_DEN/2)) / TEMPO_DEN);
}

//Canciones
// C1: Star Wars 
static const nota_t cancion1[] = {
	{392,400},{392,400},{392,400},
	{311,300},{0,60},{466,150},{392,400},
	{311,300},{0,60},{466,150},{392,600},
	{587,400},{587,400},{587,400},
	{622,300},{0,60},{466,150},{370,400},
	{311,300},{0,60},{466,150},{392,600},
	{392,400},{392,400},{392,400},
	{311,300},{0,60},{466,150},{392,400},
	{311,300},{0,60},{466,150},{392,800},
	{FIN_CANCION,0}
};
// C2: Super Mario 
static const nota_t cancion2[] = {
	{659,150},{659,150},{0,75},{659,150},{0,150},
	{523,150},{659,150},{784,150},{0,225},
	{392,150},{0,225},
	{523,150},{0,150},{392,150},{0,150},{330,150},{0,150},
	{440,150},{494,150},{466,150},{440,150},
	{392,112},{659,112},{784,112},{880,150},{0,150},
	{698,112},{784,112},{0,112},{659,112},{0,112},
	{523,112},{587,112},{494,150},{0,225},
	{523,150},{0,150},{392,150},{0,150},{330,150},{0,150},
	{440,150},{494,150},{466,150},{440,150},
	{392,112},{659,112},{784,112},{880,150},{0,150},
	{698,112},{784,112},{0,112},{659,112},{0,112},
	{523,112},{587,112},{494,300},
	{784,150},{740,150},{698,150},{622,150},{659,300},{0,150},
	{523,150},{554,150},{587,150},{466,150},{494,300},{0,300},
	{FIN_CANCION,0}
};
