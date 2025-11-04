#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

/* Nuestras libs */
#include "scr/SPI.h"
#include "scr/UART.h"
#include "scr/RC522.h"

/*  I2C + LCD 16x2 (PCF8574)  */
#define LCD_ADDR 0x27     // Cambia a 0x3F si tu backpack lo usa
#define LCD_BL   0x08

static void i2c_init(void){
	// 100 kHz @16MHz: prescaler=1, TWBR=72
	TWSR = 0x00;
	TWBR = 72;
}
static uint8_t i2c_start(uint8_t addr_rw){
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
	TWDR = addr_rw;
	TWCR = (1<<TWINT)|(1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
	return 0;
}
static void i2c_write(uint8_t d){
	TWDR = d;
	TWCR = (1<<TWINT)|(1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
}
static void i2c_stop(void){
	TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
}

static uint8_t lcd_bl = LCD_BL;
static void lcd_i2c_send(uint8_t v){
	i2c_start((LCD_ADDR<<1)|0);
	i2c_write(v | lcd_bl);
	i2c_stop();
}
static void lcd_pulse(uint8_t data){
	lcd_i2c_send(data | 0x04);  // EN=1
	_delay_us(1);
	lcd_i2c_send(data & ~0x04); // EN=0
	_delay_us(50);
}
static void lcd_write4(uint8_t nibble, uint8_t rs){
	uint8_t data = (nibble & 0xF0) | (rs?0x01:0x00);
	lcd_pulse(data);
}
static void lcd_cmd(uint8_t c){
	lcd_write4(c & 0xF0, 0);
	lcd_write4((c<<4)&0xF0, 0);
}
static void lcd_data(uint8_t d){
	lcd_write4(d & 0xF0, 1);
	lcd_write4((d<<4)&0xF0, 1);
}
static void lcd_init(void){
	i2c_init();
	_delay_ms(40);
	lcd_write4(0x30,0); _delay_ms(5);
	lcd_write4(0x30,0); _delay_us(150);
	lcd_write4(0x20,0);
	lcd_cmd(0x28);  // 4-bit, 2 líneas
	lcd_cmd(0x0C);  // Display ON, cursor OFF
	lcd_cmd(0x06);  // Entry mode
	lcd_cmd(0x01);  // Clear
	_delay_ms(2);
}
static void lcd_clear(void){ lcd_cmd(0x01); _delay_ms(2); }
static void lcd_set(uint8_t col, uint8_t row){
	static const uint8_t base[] = {0x00,0x40,0x14,0x54};
	lcd_cmd(0x80 | (base[row] + col));
}
static void lcd_print(const char *s){ while(*s) lcd_data(*s++); }

/*  EEPROM (tarjeta almacenada) */
#define EE_MAGIC        0xA5
#define EE_ADDR_MAGIC   0
#define EE_ADDR_LEN     1
#define EE_ADDR_UID     2   // 10 bytes

static bool ee_has_card(void){
	uint8_t m = eeprom_read_byte((uint8_t*)EE_ADDR_MAGIC);
	uint8_t l = eeprom_read_byte((uint8_t*)EE_ADDR_LEN);
	return (m==EE_MAGIC && l>0 && l<=10);
}
static void ee_clear_card(void){
	eeprom_write_byte((uint8_t*)EE_ADDR_MAGIC, 0xFF);
	eeprom_write_byte((uint8_t*)EE_ADDR_LEN,   0x00);
}
static void ee_write_card(const uint8_t *uid, uint8_t len){
	eeprom_write_byte((uint8_t*)EE_ADDR_MAGIC, EE_MAGIC);
	eeprom_write_byte((uint8_t*)EE_ADDR_LEN,   len);
	for(uint8_t i=0;i<len;i++)
	eeprom_write_byte((uint8_t*)(EE_ADDR_UID+i), uid[i]);
}
static uint8_t ee_read_card(uint8_t *uid_out){
	uint8_t len = eeprom_read_byte((uint8_t*)EE_ADDR_LEN);
	for(uint8_t i=0;i<len;i++)
	uid_out[i] = eeprom_read_byte((uint8_t*)(EE_ADDR_UID+i));
	return len;
}

/*  GPIO: LEDs / botones */
#define LED_VERDE   PD6
#define LED_ROJO    PD7


#define BTN_ACT     PD2   // actualizar/registrar (a GND, pull-up interno)
#define BTN_DEL     PD3   // borrar (a GND, pull-up interno)

static void gpio_init(void){
	DDRD |= (1<<LED_VERDE)|(1<<LED_ROJO)|(1<<RELE_PIN);
	PORTD &= ~((1<<LED_VERDE)|(1<<LED_ROJO)|(1<<RELE_PIN));

	DDRD &= ~((1<<BTN_ACT)|(1<<BTN_DEL));
	PORTD |=  (1<<BTN_ACT)|(1<<BTN_DEL);  // pull-up interno
}
static bool btn_pressed(uint8_t pin){
	if(!(PIND & (1<<pin))){
		_delay_ms(20);
		if(!(PIND & (1<<pin))){
			while(!(PIND & (1<<pin)));
			_delay_ms(10);
			return true;
		}
	}
	return false;
}

/*  Utiles  */
static bool uid_equals(const uint8_t *a, const uint8_t *b, uint8_t len){
	for(uint8_t i=0;i<len;i++) if(a[i]!=b[i]) return false;
	return true;
}
static void acceso_ok(void){
	PORTD |=  (1<<LED_VERDE);
	PORTD &= ~(1<<LED_ROJO);
	PORTD |=  (1<<RELE_PIN);     // abrir
	_delay_ms(800);
	PORTD &= ~(1<<RELE_PIN);
	PORTD &= ~(1<<LED_VERDE);
}
static void acceso_denegado(void){
	for(uint8_t i=0;i<3;i++){
		PORTD |=  (1<<LED_ROJO); _delay_ms(150);
		PORTD &= ~(1<<LED_ROJO); _delay_ms(120);
	}
}

/*  Main  */
#define BAUD       9600
#define UBRR_VALUE (F_CPU/16/BAUD - 1)

int main(void){
	uart_init(UBRR_VALUE);
	uart_print("\r\n== Sistema RFID iniciado ==\r\n");

	gpio_init();
	lcd_init();
	spi_init();
	mfrc522_init();

	lcd_clear(); lcd_print("Bienvenido RFID");
	lcd_set(0,1); lcd_print("Acerque tarjeta");

	uint8_t mem_uid[10]; uint8_t mem_len=0;
	if(ee_has_card()){
		mem_len = ee_read_card(mem_uid);
		uart_print("Tarjeta registrada: ");
		uart_print_hex_array(mem_uid, mem_len);
		}else{
		uart_print("Sin tarjeta registrada.\r\n");
	}

	uint8_t uid[10], uid_len;

	while(1){
		/* Borrar tarjeta */
		if(btn_pressed(BTN_DEL)){
			ee_clear_card(); mem_len=0;
			lcd_clear(); lcd_print("Tarjeta borrada");
			uart_print("Tarjeta borrada.\r\n");
			_delay_ms(900);
			lcd_clear(); lcd_print("Acerque tarjeta");
		}

		/* Registrar/Actualizar */
		if(btn_pressed(BTN_ACT)){
			lcd_clear(); lcd_print("Modo registro");
			lcd_set(0,1); lcd_print("Acerque tarjeta");
			uart_print("Modo registro...\r\n");
			bool ok=false;
			for(uint16_t to=4000; to>0; to--){
				mfrc522_standard(uid);
				if(uid[0]||uid[1]||uid[2]||uid[3]){
					// por defecto vienen 5 bytes: 4 UID + BCC
					uid_len = 5;
					ee_write_card(uid, uid_len);
					memcpy(mem_uid, uid, uid_len); mem_len=uid_len;
					lcd_clear(); lcd_print("Nueva tarjeta");
					lcd_set(0,1); lcd_print("registrada");
					uart_print("Nueva: "); uart_print_hex_array(uid, uid_len);
					ok=true; break;
				}
				_delay_ms(1);
			}
			if(!ok){
				lcd_clear(); lcd_print("No detectada");
				uart_print("Registro: timeout.\r\n");
			}
			_delay_ms(900);
			lcd_clear(); lcd_print("Acerque tarjeta");
		}

		/* Verificación normal */
		mfrc522_standard(uid);
		if(uid[0]||uid[1]||uid[2]||uid[3]){
			uart_print("UID: "); uart_print_hex_array(uid, 5);
			if(mem_len>0 && uid_equals(uid, mem_uid, mem_len)){
				lcd_clear(); lcd_print("Acceso permitido");
				acceso_ok();
				lcd_clear(); lcd_print("Acerque tarjeta");
				}else{
				lcd_clear(); lcd_print("Acceso denegado");
				acceso_denegado();
				lcd_clear(); lcd_print("Acerque tarjeta");
			}
			_delay_ms(300);
		}
	}
}
