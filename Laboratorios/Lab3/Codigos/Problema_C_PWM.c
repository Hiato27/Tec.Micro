//librerias
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

// UART 9600
static void uart_init(uint32_t baud){
    uint16_t ubrr = (F_CPU/16/baud) - 1;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr);
    UCSR0B = (1<<TXEN0);                         // Solo TX
    UCSR0C = (1<<UCSZ01) | (1<<UCSZ00);         // 8N1
}
static void uart_tx(char c){ while(!(UCSR0A & (1<<UDRE0))); UDR0 = c; }
static void uart_str(const char *s){ while(*s) uart_tx(*s++); }
static void uart_u16(uint16_t v){
    char b[6]; uint8_t i=0; if(!v){uart_tx('0');return;}
    while(v){ b[i++] = '0' + (v%10); v/=10; }
    while(i--) uart_tx(b[i]);
}

//ADC: AVcc ref
static void adc_init(void){
    ADMUX  = (1<<REFS0);                                    // Referencia AVcc (=5V)
    ADCSRA = (1<<ADEN) | (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);  // Habilitar, prescaler 128
}
static uint16_t adc_read(uint8_t ch){
    ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);   // Seleccionar canal 0..7
    ADCSRA |= (1<<ADSC);                    // Start
    while(ADCSRA & (1<<ADSC));              // Esperar fin
    return ADC;                              // 10 bits
}

//promedio de muestra
#define ADC_AVG 8
static uint16_t adc_read_avg(uint8_t ch){
    uint32_t acc = 0;
    for(uint8_t i=0;i<ADC_AVG;i++){
        acc += adc_read(ch);
        _delay_ms(1);
    }
    return (uint16_t)(acc / ADC_AVG);
}

//convertir ADC a voltios
static uint16_t adc_to_mV(uint16_t adc){
    // mV = adc * 5000 / 1023
    uint32_t num = (uint32_t)adc * 5000UL + 511UL;
    return (uint16_t)(num / 1023UL);
}

int main(void){
    uart_init(9600);
    adc_init();

    uart_str("\n=== Test Potenciometros A0/A1 @9600 ===\n");
    uart_str("CSV: t_ms, adc0, mv0, adc1, mv1\n");

    uint32_t tms = 0;

    while(1){
        uint16_t a0 = adc_read_avg(0);          // POT_ref en A0
        uint16_t a1 = adc_read_avg(1);          // POT_meas en A1
        uint16_t m0 = adc_to_mV(a0);
        uint16_t m1 = adc_to_mV(a1);

        // Línea CSV para terminal 
        uart_str("CSV:");
        uart_u16((uint16_t)(tms & 0xFFFF)); uart_tx(',');
        uart_u16(a0); uart_tx(',');
        uart_u16(m0); uart_tx(',');
        uart_u16(a1); uart_tx(',');
        uart_u16(m1); uart_tx('\n');

        _delay_ms(100);
        tms += 100;
    }
}
