#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#define OUTPUT_LCD   1

#define LCD_RS_PORT PORTB
#define LCD_RS_DDR  DDRB
#define LCD_RS_PIN  PB0

#define LCD_E_PORT  PORTB
#define LCD_E_DDR   DDRB
#define LCD_E_PIN   PB1

#define LCD_DDR     DDRD
#define LCD_PORT    PORTD
#define LCD_D4      PD4
#define LCD_D5      PD5
#define LCD_D6      PD6
#define LCD_D7      PD7

#define ROW_DDR   DDRB
#define ROW_PORT  PORTB
#define ROW_PINS_MASK ((1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5))

#define COL_DDR   DDRC
#define COL_PORT  PORTC
#define COL_PINR  PINC
#define COL_PINS_MASK ((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3))

static void uart_init(uint32_t baud){
	uint16_t ubrr = (F_CPU/16/baud) - 1;      // 8N1
	UBRR0H = (uint8_t)(ubrr>>8);
	UBRR0L = (uint8_t)ubrr;
	UCSR0B = (1<<TXEN0);
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
}
static void uart_tx(char c){
	while(!(UCSR0A & (1<<UDRE0)));            // Espera buffer
	UDR0 = c;
}
static void uart_print(const char* s){
	while(*s) uart_tx(*s++);
}

static inline void lcd_pulse_e(void){
	LCD_E_PORT |=  (1<<LCD_E_PIN);            // Pulso E
	_delay_us(1);
	LCD_E_PORT &= ~(1<<LCD_E_PIN);
	_delay_us(40);
}
static inline void lcd_write4(uint8_t nib){
	uint8_t p = LCD_PORT & 0x0F;              // Preserva PD0..PD3
	p |= ((nib & 0x0F) << 4);                  // Carga D4..D7
	LCD_PORT = p;
	lcd_pulse_e();
}
static void lcd_cmd(uint8_t c){
	LCD_RS_PORT &= ~(1<<LCD_RS_PIN);          // RS=0 comando
	lcd_write4(c>>4);
	lcd_write4(c & 0x0F);
	if (c==0x01 || c==0x02) _delay_ms(2);     
}
static void lcd_data(uint8_t d){
	LCD_RS_PORT |= (1<<LCD_RS_PIN);           // RS=1 dato
	lcd_write4(d>>4);
	lcd_write4(d & 0x0F);
}
static void lcd_init(void){
	LCD_RS_DDR |= (1<<LCD_RS_PIN);
	LCD_E_DDR  |= (1<<LCD_E_PIN);
	LCD_DDR    |= (1<<LCD_D4)|(1<<LCD_D5)|(1<<LCD_D6)|(1<<LCD_D7);

	_delay_ms(20);                            // Power-on
	LCD_RS_PORT &= ~(1<<LCD_RS_PIN);
	lcd_write4(0x03); _delay_ms(5);
	lcd_write4(0x03); _delay_us(150);
	lcd_write4(0x03); _delay_us(150);
	lcd_write4(0x02);                         // 4 bits

	lcd_cmd(0x28);                            // 4b, 2 líneas
	lcd_cmd(0x08);                            // Display off
	lcd_cmd(0x01);                           
	lcd_cmd(0x06);                          
	lcd_cmd(0x0C);                            // Display on
}
static void lcd_gotoxy(uint8_t x, uint8_t y){
	uint8_t base = (y==0)?0x00:0x40;
	lcd_cmd(0x80 | (base + x));               
}
static void lcd_puts(const char* s){
	while(*s) lcd_data(*s++);
}

static const char keymap[4][4] = {
	{'1','2','3','A'},
	{'4','5','6','B'},
	{'7','8','9','C'},
	{'*','0','#','D'}
};

static void keypad_init(void){
	ROW_DDR  |= ROW_PINS_MASK;                // Filas salida
	ROW_PORT |= ROW_PINS_MASK;                // Filas en '1'
	COL_DDR  &= ~COL_PINS_MASK;               // Columnas input
	COL_PORT |=  COL_PINS_MASK;               // Pull-ups
}
static inline void rows_set_all_high(void){
	ROW_PORT |= ROW_PINS_MASK;
}
static void row_drive_low(uint8_t r){
	rows_set_all_high();                      
	ROW_PORT &= ~(1 << (PB2 + r));            // Activa fila r (0..3)
}
static int8_t read_col(void){
	uint8_t pins = COL_PINR;                  // Lee columnas
	if(!(pins & (1<<PC0))) return 0;
	if(!(pins & (1<<PC1))) return 1;
	if(!(pins & (1<<PC2))) return 2;
	if(!(pins & (1<<PC3))) return 3;
	return -1;
}
static char keypad_getkey_blocking(void){
	while(1){
		for(uint8_t r=0;r<4;r++){
			row_drive_low(r);
			_delay_us(5);                     
			int8_t c = read_col();
			if(c >= 0){
				_delay_ms(10);                
				if(read_col()==c){
					char k = keymap[r][c];
					while(read_col()==c) _delay_ms(5); // Espera soltar
					rows_set_all_high();
					return k;
				}
			}
		}
	}
}

int main(void){
	#ifdef OUTPUT_UART
	uart_init(9600);
	#endif
	#ifdef OUTPUT_LCD
	lcd_init();
	#endif
	keypad_init();
	sei();                                    

	#ifdef OUTPUT_LCD
	lcd_gotoxy(0,0); lcd_puts("Tecla:");
	lcd_gotoxy(0,1); lcd_puts("Esperando...");
	#endif
	#ifdef OUTPUT_UART
	uart_print("Keypad listo. Presiona teclas...\r\n");
	#endif

	while(1){
		char k = keypad_getkey_blocking();

		#ifdef OUTPUT_LCD
		lcd_cmd(0x01);
		lcd_gotoxy(0,0); lcd_puts("Tecla oprimida:");
		lcd_gotoxy(0,1); lcd_data(k);
		#endif
		#ifdef OUTPUT_UART
		uart_print("Key: ");
		uart_tx(k);
		uart_print("\r\n");
		#endif
	}
}
