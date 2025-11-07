#define F_CPU 1000000UL

#include <avr/io.h>
#include <util/delay_basic.h>
#include <util/delay.h>

//Pines Eje X
#define PIN_PASO_X   PB3
#define PIN_DIR_X    PB4
#define PIN_HAB_X    PB5

//Pines Eje Y
#define PIN_PASO_Y   PC3
#define PIN_DIR_Y    PC4
#define PIN_HAB_Y    PC5

//Pin de la Pluma
#define PIN_PLUMA    PC0

#define PASOS_POR_SEGUNDO 50.0 
#define DELAY_TRI_MS    16000.0 
#define DELAY_CRUZ_MS   14000.0 
#define DELAY_1S_MS     250.0
#define DELAY_2S_MS     500.0
#define DELAY_3S_MS     750.0
#define DELAY_10S_MS    2500.0
#define DELAY_25S_MS_REAL 4000.0 
#define CALCULAR_PASOS(ms) (uint16_t)((ms / 1000.0) * PASOS_POR_SEGUNDO)

static inline void pausa_corta(uint16_t ciclos) {
	_delay_loop_2(ciclos);
}

void inicializar_hardware(void) {
	DDRB |= (1 << PIN_PASO_X) | (1 << PIN_DIR_X) | (1 << PIN_HAB_X);
	
	DDRC |= (1 << PIN_PASO_Y) | (1 << PIN_DIR_Y) | (1 << PIN_HAB_Y) | (1 << PIN_PLUMA);
	PORTB |= (1 << PIN_HAB_X);
	PORTC |= (1 << PIN_HAB_Y);
	PORTC |= (1 << PIN_PLUMA);
}

void accionar_motor(volatile uint8_t *puerto_direccion, uint8_t pin_direccion,
volatile uint8_t *puerto_paso, uint8_t pin_paso,
uint8_t sentido_mov, uint16_t num_pasos) {
	
	if (sentido_mov) {
		*puerto_direccion |= (1 << pin_direccion);
		} else {
		*puerto_direccion &= ~(1 << pin_direccion);
	}
	pausa_corta(50);
	for (uint16_t i = 0; i < num_pasos; i++) {
		*puerto_paso |= (1 << pin_paso);
		pausa_corta(300);
		*puerto_paso &= ~(1 << pin_paso);
		pausa_corta(300);
	}
}

// Mueve un eje específico (X o Y)
void mover_eje(uint8_t identificador_eje, uint8_t sentido, uint16_t cantidad_pasos) {
	if (cantidad_pasos == 0) return;
	if (identificador_eje == 0) {
		accionar_motor(&PORTB, PIN_DIR_X, &PORTB, PIN_PASO_X, sentido, cantidad_pasos);
		} else if (identificador_eje == 1) {
		accionar_motor(&PORTC, PIN_DIR_Y, &PORTC, PIN_PASO_Y, sentido, cantidad_pasos);
	}
}

void mover_diagonal(uint8_t sentido_x, uint8_t sentido_y, uint16_t cantidad_pasos) {
	if (cantidad_pasos == 0) return;

	if (sentido_x) { PORTB |= (1 << PIN_DIR_X); }
	else { PORTB &= ~(1 << PIN_DIR_X); }
	if (sentido_y) { PORTC |= (1 << PIN_DIR_Y); }
	else { PORTC &= ~(1 << PIN_DIR_Y); }
	
	pausa_corta(50);

	for (uint16_t i = 0; i < cantidad_pasos; i++) {
		PORTB |= (1 << PIN_PASO_X);
		PORTC |= (1 << PIN_PASO_Y);
		pausa_corta(300);
		PORTB &= ~(1 << PIN_PASO_X);
		PORTC &= ~(1 << PIN_PASO_Y);
		pausa_corta(300);
	}
}


// Baja la pluma para dibujar
void bajar_pluma(void) {
	PORTC &= ~(1 << PIN_PLUMA);
	_delay_ms(100);
}

// Levanta la pluma
void subir_pluma(void) {
	PORTC |= (1 << PIN_PLUMA);
	_delay_ms(100);
}

/*TRIANGULO */
static void run_sequence(void) {
	bajar_pluma();
	_delay_ms(900);
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_TRI_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_TRI_MS));
	mover_diagonal(0, 0, CALCULAR_PASOS(DELAY_TRI_MS));

	subir_pluma();
	_delay_ms(900);
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_TRI_MS));
	mover_eje(0, 0, CALCULAR_PASOS(500));
	mover_eje(1, 0, CALCULAR_PASOS(1500));
}

/*CRUZ */
static void run_sequence2(void) {
	bajar_pluma();
	_delay_ms(900);
	mover_diagonal(0, 0, CALCULAR_PASOS(DELAY_CRUZ_MS));
	subir_pluma();
	_delay_ms(900);
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_CRUZ_MS));
	bajar_pluma();
	mover_diagonal(0, 1, CALCULAR_PASOS(DELAY_CRUZ_MS));
	_delay_ms(900);
	subir_pluma();
	_delay_ms(900);
	mover_eje(0, 0, CALCULAR_PASOS(2000));
	mover_eje(1, 0, CALCULAR_PASOS(2000));
}

/*CIRCULO */
static void run_sequence3(void) {
	bajar_pluma();
	_delay_ms(900);
	
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_10S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS)); 
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_3S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_3S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_10S_MS));
	
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_3S_MS)); 
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_3S_MS));
	mover_eje(0, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_10S_MS));

	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_3S_MS)); 
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_3S_MS));
	mover_eje(1, 0, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_10S_MS));
	
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS)); 
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_3S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_2S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_3S_MS));
	mover_eje(0, 1, CALCULAR_PASOS(DELAY_1S_MS));
	mover_eje(1, 1, CALCULAR_PASOS(DELAY_3S_MS));

	subir_pluma();
	_delay_ms(900);
}

//conejo 
static void run_conejo(void) {
	mover_eje(0, 0, CALCULAR_PASOS(2000));
	mover_eje(1, 0, CALCULAR_PASOS(15000));
	bajar_pluma();

	mover_eje(1, 1, CALCULAR_PASOS(3000));
	mover_eje(0, 1, CALCULAR_PASOS(3000));
	mover_eje(1, 0, CALCULAR_PASOS(1500));
	mover_eje(0, 0, CALCULAR_PASOS(3000));
	mover_eje(1, 1, CALCULAR_PASOS(250));
	mover_eje(0, 1, CALCULAR_PASOS(2750));
	mover_eje(1, 1, CALCULAR_PASOS(1000));
	mover_eje(0, 0, CALCULAR_PASOS(2750));
	mover_eje(1, 1, CALCULAR_PASOS(250));
	mover_eje(0, 0, CALCULAR_PASOS(6000));
	mover_eje(1, 0, CALCULAR_PASOS(6000));
	mover_eje(0, 1, CALCULAR_PASOS(9000));
	mover_eje(1, 1, CALCULAR_PASOS(1500));
	mover_eje(0, 0, CALCULAR_PASOS(3000));
	mover_eje(1, 0, CALCULAR_PASOS(250));
	mover_eje(0, 1, CALCULAR_PASOS(2750));
	mover_eje(1, 0, CALCULAR_PASOS(1000));
	mover_eje(0, 0, CALCULAR_PASOS(2750));
	mover_eje(1, 0, CALCULAR_PASOS(250));
	mover_eje(1, 1, CALCULAR_PASOS(3000));
	mover_eje(1, 1, CALCULAR_PASOS(3000));
	mover_eje(1, 0, CALCULAR_PASOS(3000));

	subir_pluma();
	mover_eje(1, 1, CALCULAR_PASOS(2250));
	mover_eje(0, 0, CALCULAR_PASOS(750));

	bajar_pluma();

	mover_eje(0, 0, CALCULAR_PASOS(1500));
	mover_eje(1, 0, CALCULAR_PASOS(1500));
	mover_eje(0, 1, CALCULAR_PASOS(1500));
	mover_eje(1, 1, CALCULAR_PASOS(1500));
	mover_eje(1, 0, CALCULAR_PASOS(1500));

	subir_pluma();
	
	mover_eje(1, 1, CALCULAR_PASOS(300));
	mover_eje(0, 0, CALCULAR_PASOS(300));
	
	bajar_pluma();
	
	mover_eje(1, 1, CALCULAR_PASOS(800));
	mover_eje(0, 0, CALCULAR_PASOS(800));
	mover_eje(1, 0, CALCULAR_PASOS(800));
	mover_eje(0, 1, CALCULAR_PASOS(800));
	
	subir_pluma();
	
	mover_eje(1, 0, CALCULAR_PASOS(300));
	mover_eje(0, 1, CALCULAR_PASOS(300));
	
	subir_pluma();
	
	mover_eje(1, 0, CALCULAR_PASOS(1500));

	bajar_pluma();

	mover_eje(0, 0, CALCULAR_PASOS(1500));
	mover_eje(1, 0, CALCULAR_PASOS(1500));
	mover_eje(0, 1, CALCULAR_PASOS(1500));
	mover_eje(1, 1, CALCULAR_PASOS(1500));
	mover_eje(1, 0, CALCULAR_PASOS(1500));

	subir_pluma();

	mover_eje(1, 1, CALCULAR_PASOS(300));
	mover_eje(0, 0, CALCULAR_PASOS(300));
	
	bajar_pluma();
	
	mover_eje(1, 1, CALCULAR_PASOS(800));
	mover_eje(0, 0, CALCULAR_PASOS(800));
	mover_eje(1, 0, CALCULAR_PASOS(800));
	mover_eje(0, 1, CALCULAR_PASOS(800));
	
	subir_pluma();
	
	mover_eje(1, 0, CALCULAR_PASOS(300));
	mover_eje(0, 1, CALCULAR_PASOS(300));

	mover_eje(0, 0, CALCULAR_PASOS(500));
	mover_eje(1, 1, CALCULAR_PASOS(1500));
	mover_eje(0, 0, CALCULAR_PASOS(1500));

	bajar_pluma();

	mover_eje(1, 1, CALCULAR_PASOS(1600));

	for (int i = 0; i < 8; i++) {
		mover_eje(0, 0, CALCULAR_PASOS(100));
		mover_eje(1, 0, CALCULAR_PASOS(100));
	}
	for (int i = 0; i < 8; i++) {
		mover_eje(0, 1, CALCULAR_PASOS(100));
		mover_eje(1, 0, CALCULAR_PASOS(100));
	}
	for (int i = 0; i < 8; i++) {
		mover_eje(0, 0, CALCULAR_PASOS(100));
		mover_eje(1, 1, CALCULAR_PASOS(100));
	}

	mover_eje(0, 0, CALCULAR_PASOS(1500));
	mover_eje(1, 1, CALCULAR_PASOS(1500));
	mover_eje(0, 1, CALCULAR_PASOS(1500));

	mover_eje(0, 0, CALCULAR_PASOS(1500));
	mover_eje(1, 0, CALCULAR_PASOS(1500));

	mover_eje(1, 0, CALCULAR_PASOS(1500));
	mover_eje(0, 1, CALCULAR_PASOS(1500));

	subir_pluma();

	mover_eje(1, 0, CALCULAR_PASOS(400));
	mover_eje(0, 0, CALCULAR_PASOS(900));

	bajar_pluma();

	mover_eje(1, 0, CALCULAR_PASOS(1500));
	mover_eje(1, 1, CALCULAR_PASOS(1500));

	subir_pluma();

	mover_eje(0, 1, CALCULAR_PASOS(750));

	bajar_pluma();

	mover_eje(1, 0, CALCULAR_PASOS(1500));

	subir_pluma();

	mover_eje(0, 1, CALCULAR_PASOS(750));

	bajar_pluma();

	mover_eje(1, 1, CALCULAR_PASOS(1500));

	subir_pluma();

	mover_eje(1, 1, CALCULAR_PASOS(3900));

	bajar_pluma();

	mover_eje(1, 1, CALCULAR_PASOS(1500));

	subir_pluma();

	mover_eje(0, 0, CALCULAR_PASOS(750));

	bajar_pluma();

	mover_eje(1, 0, CALCULAR_PASOS(1200));

	subir_pluma();
	
	mover_eje(0, 0, CALCULAR_PASOS(800));

	bajar_pluma();

	mover_eje(1, 1, CALCULAR_PASOS(1200));

	subir_pluma();
	
	mover_eje(0, 0, CALCULAR_PASOS(10000));
}


/* Murcielago */
static void run_murcielago(void) {

	bajar_pluma();
	//arriba
	mover_eje(1, 1, CALCULAR_PASOS(400));    
	for (int i = 0; i < 8; i++) {
		mover_eje(0, 1, CALCULAR_PASOS(300));
		mover_eje(1, 1, CALCULAR_PASOS(300));
	}
	for (int i = 0; i < 8; i++) {
		mover_eje(1, 1, CALCULAR_PASOS(300));
		mover_eje(0, 0, CALCULAR_PASOS(300));
	}
	mover_eje(1, 1, CALCULAR_PASOS(1600));
	for (int i = 0; i < 8; i++) {
		mover_eje(0, 1, CALCULAR_PASOS(300));
		mover_eje(1, 1, CALCULAR_PASOS(300));
	}
	for (int i = 0; i < 8; i++) {
		mover_eje(1, 1, CALCULAR_PASOS(300));
		mover_eje(0, 0, CALCULAR_PASOS(300)); 
	}
	mover_eje(1, 1, CALCULAR_PASOS(400));
	
	//derecha
	mover_eje(0, 0, CALCULAR_PASOS(1000)); 
	for (int i = 0; i < 8; i++) {
		mover_eje(1, 1, CALCULAR_PASOS(300));
		mover_eje(0, 1, CALCULAR_PASOS(300));
	}
	
	mover_eje(1, 1, CALCULAR_PASOS(200));
	mover_eje(0, 0, CALCULAR_PASOS(6000)); 
	
	for (int i = 0; i < 8; i++) {
		mover_eje(1, 0, CALCULAR_PASOS(300)); 
		mover_eje(0, 0, CALCULAR_PASOS(300)); 
	}
	mover_eje(1, 0, CALCULAR_PASOS(200)); 
	mover_eje(0, 0, CALCULAR_PASOS(1000)); 
	
	//abajo
	mover_eje(1, 0, CALCULAR_PASOS(12000)); 
	
	//izquierda
	mover_eje(0, 1, CALCULAR_PASOS(1000));
	for (int i = 0; i < 8; i++) {
		mover_eje(1, 0, CALCULAR_PASOS(300)); 
		mover_eje(0, 1, CALCULAR_PASOS(300));
	}
	mover_eje(1, 0, CALCULAR_PASOS(200)); 
	mover_eje(0, 1, CALCULAR_PASOS(6000));
	for (int i = 0; i < 8; i++) {
		mover_eje(1, 1, CALCULAR_PASOS(300));
		mover_eje(0, 0, CALCULAR_PASOS(300));
	}
	mover_eje(1, 1, CALCULAR_PASOS(200));
	mover_eje(0, 1, CALCULAR_PASOS(1000));
	mover_eje(1, 1, CALCULAR_PASOS(300));
	
	subir_pluma();
	
	//ojos
	mover_eje(0, 0, CALCULAR_PASOS(1000)); 
	mover_eje(1, 1, CALCULAR_PASOS(2200));
	
	bajar_pluma();
	
	mover_eje(1, 1, CALCULAR_PASOS(1800));
	mover_eje(0, 0, CALCULAR_PASOS(1800)); 
	mover_eje(1, 0, CALCULAR_PASOS(1800)); 
	mover_eje(0, 1, CALCULAR_PASOS(1800));
	
	subir_pluma();
	
	mover_eje(1, 1, CALCULAR_PASOS(6000));
	
	bajar_pluma();
	
	mover_eje(1, 1, CALCULAR_PASOS(1800));
	mover_eje(0, 0, CALCULAR_PASOS(1800)); 
	mover_eje(1, 0, CALCULAR_PASOS(1800)); 
	mover_eje(0, 1, CALCULAR_PASOS(1800));
	mover_eje(1, 1, CALCULAR_PASOS(1800));
	
	subir_pluma();
	
	//boca
	mover_eje(0, 0, CALCULAR_PASOS(3000));
	
	bajar_pluma();
	
	mover_eje(1, 0, CALCULAR_PASOS(9600)); 
	mover_eje(1, 1, CALCULAR_PASOS(9600));
	mover_eje(1, 0, CALCULAR_PASOS(2000)); 
	
	for (int i = 0; i < 8; i++) {
		mover_eje(0, 0, CALCULAR_PASOS(140));
		mover_eje(1, 0, CALCULAR_PASOS(140)); 
	}
	for (int i = 0; i < 8; i++) {
		mover_eje(0, 1, CALCULAR_PASOS(140));
		mover_eje(1, 0, CALCULAR_PASOS(140)); 
	}
	mover_eje(1, 0, CALCULAR_PASOS(1400)); 
	for (int i = 0; i < 8; i++) {
		mover_eje(0, 0, CALCULAR_PASOS(140)); 
		mover_eje(1, 0, CALCULAR_PASOS(140));
	}
	for (int i = 0; i < 8; i++) {
		mover_eje(0, 1, CALCULAR_PASOS(140));
		mover_eje(1, 0, CALCULAR_PASOS(140)); 
	}
	
	subir_pluma();
	

}
int main(void) {
	inicializar_hardware();
	subir_pluma();
	_delay_ms(500);
	
	run_sequence();
	subir_pluma();

	
	run_sequence2();
	subir_pluma();

	
	run_sequence3();
	subir_pluma();
	
	run_conejo();
	subir_pluma();

	run_murcielago();
	subir_pluma();

	_delay_ms(2000); 
	mover_eje(1, 1, CALCULAR_PASOS(50000.0));
	
	_delay_ms(500); 
	
	PORTB |= (1 << PIN_HAB_X);
	PORTC |= (1 << PIN_HAB_Y);
	
	while (1) {
	}
	

}
