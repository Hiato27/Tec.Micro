#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdbool.h>

// Funcionamientos de los LEDs 
static inline void leds_init(void){
    DDRD |= LED_MASK;     // Salida
    PORTD &= ~LED_MASK;   // Apagados
}
static inline void leds_on(void){  PORTD |=  LED_MASK; }
static inline void leds_off(void){ PORTD &= ~LED_MASK; }

// WDT a 1 segundo en modo interrupción 
static inline void wdt_setup_1s_interrupt(void){
    cli();
    
    WDTCSR = (1<<WDCE)|(1<<WDE);
    WDTCSR = (1<<WDIE) | (1<<WDP2) | (1<<WDP1);  // 1.0 s
    sei();
}

// Dormir en segundos en 'mode' usando WDT en 1s 
static void sleep_for_seconds(uint8_t mode, uint8_t seg, bool keep_adc)
{
    // Gestion de ADC según el modo para minimizar consumo en power-down
    if (keep_adc) ADCSRA |=  (1<<ADEN);
    else          ADCSRA &= ~(1<<ADEN);

    set_sleep_mode(mode);

    for(uint8_t s=0; s<seg; s++){
        wdt_ticks = 0;
        wdt_setup_1s_interrupt();

        // Bajar BOD antes de dormir en modos profundos
        // Minimiza consumo durante sleep
        sleep_enable();
        #ifdef sleep_bod_disable
        sleep_bod_disable();
        #endif

        sleep_cpu();      // despierta la ISR del WDT

        sleep_disable();  
        
        if(!wdt_ticks) _delay_ms(5);
    }
}

// Secuencia: 5s encendidos - 30s dormidos 
static void ciclo_completo(void){
    leds_on();
    _delay_ms(5000);

    leds_off();
    // 10 s en IDLE   
    sleep_for_seconds(SLEEP_MODE_IDLE, 10, false);
    // 10 s en ADC Noise Reduction 
    sleep_for_seconds(SLEEP_MODE_ADC, 10, true);
    // 10 s en POWER-DOWN 
    sleep_for_seconds(SLEEP_MODE_PWR_DOWN, 10, false);
}

//  MAIN 
int main(void){
    // Deshabilitar ADC al inicio
    ADCSRA &= ~(1<<ADEN);

    leds_init();
    sei();

    while(1){
        ciclo_completo();
    }
}

// ISR WDT: despierta y cuenta 1 
ISR(WDT_vect){
    wdt_ticks++;
    
}

