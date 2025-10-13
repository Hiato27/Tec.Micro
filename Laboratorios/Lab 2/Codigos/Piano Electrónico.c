#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>

// pin del buzzer
#define PIN_BUZZER PB1

// Botones 
#define T_DO_PC0   PC0  // A0
#define T_RE_PC1   PC1  // A1
#define T_MI_PC2   PC2  // A2
#define T_FA_PC3   PC3  // A3
#define T_SOL_PC4  PC4  // A4
#define T_LA_PC5   PC5  // A5
#define T_SI_PD2   PD2  // D2
#define T_DO2_PD3  PD3  // D3

// UART 9600 8N1
#define UART_UBRR_9600 103u

// Notas musicales
static const uint16_t ocr_nota[8] = {
	3821, // Do
	3404, // Re
	3033, // Mi
	2862, // Fa
	2550, // Sol
	2272, // La
	2024, // Si
	1910  // Do+
};
