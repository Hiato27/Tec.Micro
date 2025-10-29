#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>


//  UART 

static void uart_init(uint16_t ubrr){
	UBRR0H = (uint8_t)(ubrr>>8);
	UBRR0L = (uint8_t)ubrr;
	UCSR0B = (1<<TXEN0);
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00); // 8N1
}
static void uart_putc(char c){
	while(!(UCSR0A & (1<<UDRE0)));
	UDR0 = c;
}
static void uart_print(const char *s){
	while(*s) uart_putc(*s++);
}
static void uart_print_hex8(uint8_t v){
	static const char HEX[]="0123456789ABCDEF";
	uart_putc(HEX[(v>>4)&0xF]);
	uart_putc(HEX[v&0xF]);
}
static void uart_print_hex_array(const uint8_t *buf, uint8_t n){
	for(uint8_t i=0;i<n;i++){
		uart_print_hex8(buf[i]);
		if(i<n-1) uart_putc(':');
	}
}


// SPI 

static void spi_init(void){
	// SS(PB2), MOSI(PB3), SCK(PB5) salidas; MISO(PB4) entrada
	DDRB |= (1<<PB2)|(1<<PB3)|(1<<PB5);
	DDRB &= ~(1<<PB4);
	// SPI enable, Master, Mode0, fosc/8
	SPCR = (1<<SPE)|(1<<MSTR)|(0<<CPOL)|(0<<CPHA)|(0<<SPR1)|(0<<SPR0);
	SPSR = (1<<SPI2X);
	// SS alto por defecto
	PORTB |= (1<<PB2);
}
static uint8_t spi_transfer(uint8_t data){
	SPDR = data;
	while(!(SPSR & (1<<SPIF)));
	return SPDR;
}


// I2C 

static void i2c_init(void){
	TWSR = 0x00; // prescaler = 1
	TWBR = 72;   // ~100 kHz @ 16 MHz
}
static uint8_t i2c_start(uint8_t addr_rw){
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
	TWDR = addr_rw;
	TWCR = (1<<TWINT)|(1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
	return 0;
}
static void i2c_write(uint8_t data){
	TWDR = data;
	TWCR = (1<<TWINT)|(1<<TWEN);
	while(!(TWCR & (1<<TWINT)));
}
static void i2c_stop(void){
	TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
}


// LCD I2C 

#ifndef LCD_I2C_ADDR
#define LCD_I2C_ADDR 0x27   // Cambiar a 0x3F si tu backpack lo requiere
#endif
// PCF8574: P7..P0 = D7 D6 D5 D4 BL EN RW RS
#define LCD_BL 0x08
static uint8_t lcd_bl = LCD_BL;

static void lcd_i2c_send(uint8_t v){
	i2c_start((LCD_I2C_ADDR<<1)|0);
	i2c_write(v | lcd_bl);
	i2c_stop();
}
static void lcd_pulse(uint8_t data){
	lcd_i2c_send(data | 0x04); // EN=1
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
	lcd_write4((c<<4) & 0xF0, 0);
}
static void lcd_data(uint8_t d){
	lcd_write4(d & 0xF0, 1);
	lcd_write4((d<<4) & 0xF0, 1);
}
static void lcd_init(void){
	i2c_init();
	_delay_ms(40);
	lcd_write4(0x30,0); _delay_ms(5);
	lcd_write4(0x30,0); _delay_us(150);
	lcd_write4(0x20,0);
	lcd_cmd(0x28); // 4-bit, 2 líneas
	lcd_cmd(0x0C); // display ON, cursor off
	lcd_cmd(0x06); // entry mode
	lcd_cmd(0x01); _delay_ms(2);
}
static void lcd_clear(void){
	lcd_cmd(0x01); _delay_ms(2);
}
static void lcd_set_cursor(uint8_t col, uint8_t row){
	static const uint8_t base[] = {0x00,0x40,0x14,0x54};
	lcd_cmd(0x80 | (base[row] + col));
}
static void lcd_print(const char *s){
	while(*s) lcd_data(*s++);
}


//  EEPROM UTIL 

#define EE_MAGIC       0xA5
#define EE_ADDR_MAGIC  0
#define EE_ADDR_LEN    1
#define EE_ADDR_UID    2   // hasta 10 bytes

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


//  RC522

// Pines
#define RC522_SS_PORT  PORTB
#define RC522_SS_DDR   DDRB
#define RC522_SS_PIN   PB2
#define RC522_RST_PORT PORTB
#define RC522_RST_DDR  DDRB
#define RC522_RST_PIN  PB1

#define SS_LOW()   (RC522_SS_PORT &= ~(1<<RC522_SS_PIN))
#define SS_HIGH()  (RC522_SS_PORT |=  (1<<RC522_SS_PIN))

// Registros
#define CommandReg     0x01
#define CommIrqReg     0x04
#define ErrorReg       0x06
#define FIFODataReg    0x09
#define FIFOLevelReg   0x0A
#define ControlReg     0x0C
#define BitFramingReg  0x0D
#define ModeReg        0x11
#define TxControlReg   0x14
#define TxASKReg       0x15
#define CRCResultRegH  0x21
#define CRCResultRegL  0x22
#define TModeReg       0x2A
#define TPrescalerReg  0x2B
#define TReloadRegH    0x2C
#define TReloadRegL    0x2D

// Comandos
#define PCD_IDLE       0x00
#define PCD_TRANSCEIVE 0x0C
#define PCD_SOFTRESET  0x0F

// PICC
#define PICC_REQIDL    0x26
#define PICC_ANTICOLL  0x93
#define PICC_HALT      0x50

static uint8_t rc522_read(uint8_t reg){
	uint8_t addr = ((reg<<1)&0x7E) | 0x80; // read
	SS_LOW();
	spi_transfer(addr);
	uint8_t val = spi_transfer(0x00);
	SS_HIGH();
	return val;
}
static void rc522_write(uint8_t reg, uint8_t val){
	uint8_t addr = ((reg<<1)&0x7E); // write
	SS_LOW();
	spi_transfer(addr);
	spi_transfer(val);
	SS_HIGH();
}
static void rc522_setBitMask(uint8_t reg, uint8_t mask){
	rc522_write(reg, rc522_read(reg) | mask);
}
static void rc522_clearBitMask(uint8_t reg, uint8_t mask){
	rc522_write(reg, rc522_read(reg) & (~mask));
}
static void mfrc522_resetPinInit(void){
	RC522_RST_DDR |= (1<<RC522_RST_PIN);
	RC522_SS_DDR  |= (1<<RC522_SS_PIN);
	RC522_RST_PORT &= ~(1<<RC522_RST_PIN);
	_delay_ms(10);
	RC522_RST_PORT |=  (1<<RC522_RST_PIN);
	_delay_ms(50);
	SS_HIGH();
}
static void mfrc522_reset(void){
	rc522_write(CommandReg, PCD_SOFTRESET);
	_delay_ms(50);
}
static void mfrc522_init(void){
	mfrc522_reset();
	// Temporizadores/modulación 106 kbps
	rc522_write(TModeReg,      0x8D);
	rc522_write(TPrescalerReg, 0x3E);
	rc522_write(TReloadRegL,   30);
	rc522_write(TReloadRegH,   0);
	rc522_write(TxASKReg, 0x40); // 100% ASK
	rc522_write(ModeReg,  0x3D); // CRC ON, MSB first
	// Antena ON
	uint8_t v = rc522_read(TxControlReg);
	if(!(v & 0x03)) rc522_write(TxControlReg, v | 0x03);
	_delay_ms(5);
}
// Lee UID de 4 bytes (anticolisión nivel 1). Devuelve true si ok.
static bool mfrc522_read_uid(uint8_t *uid, uint8_t *uid_len){
	*uid_len = 0;
	// REQA (7 bits)
	rc522_write(FIFOLevelReg, 0x80);
	rc522_write(BitFramingReg, 0x07);
	rc522_write(FIFODataReg, PICC_REQIDL);
	rc522_write(CommandReg, PCD_TRANSCEIVE);
	rc522_setBitMask(BitFramingReg, 0x80);
	uint16_t t=1000; uint8_t irq;
	do { irq = rc522_read(CommIrqReg); t--; } while(!(irq & 0x30) && t);
	rc522_clearBitMask(BitFramingReg, 0x80);
	if(t==0) return false;

	// Anticolisión
	rc522_write(BitFramingReg, 0x00);
	rc522_write(FIFOLevelReg, 0x80);
	rc522_write(FIFODataReg, PICC_ANTICOLL);
	rc522_write(FIFODataReg, 0x20);
	rc522_write(CommandReg, PCD_TRANSCEIVE);
	rc522_setBitMask(BitFramingReg, 0x80);
	t=1000;
	do { irq = rc522_read(CommIrqReg); t--; } while(!(irq & 0x30) && t);
	rc522_clearBitMask(BitFramingReg, 0x80);
	if(t==0) return false;

	uint8_t n = rc522_read(FIFOLevelReg);
	if(n < 5) return false;

	for(uint8_t i=0;i<5;i++){
		uint8_t v = rc522_read(FIFODataReg);
		if(i<4) uid[i] = v; // BCC se descarta
	}
	*uid_len = 4;

	// HALT
	rc522_write(FIFOLevelReg, 0x80);
	rc522_write(FIFODataReg,  PICC_HALT);
	rc522_write(FIFODataReg,  0x00);
	rc522_write(CommandReg,   PCD_TRANSCEIVE);
	rc522_setBitMask(BitFramingReg, 0x80);
	_delay_ms(1);
	rc522_clearBitMask(BitFramingReg, 0x80);

	return true;
}


//  APLICACIÓN 

// Pines de usuario (tu pinout)
#define LED_VERDE_PIN  PD6
#define LED_ROJO_PIN   PD7
#define RELE_PIN       PD5   // opcional

#define BTN_ACT_PIN    PD2   // actualizar/registrar
#define BTN_DEL_PIN    PD3   // borrar

static void gpio_init(void){
	// LEDs + RELÉ salidas
	DDRD |= (1<<LED_VERDE_PIN)|(1<<LED_ROJO_PIN)|(1<<RELE_PIN);
	PORTD &= ~((1<<LED_VERDE_PIN)|(1<<LED_ROJO_PIN)|(1<<RELE_PIN));
	// Botones con pull-up
	DDRD &= ~((1<<BTN_ACT_PIN)|(1<<BTN_DEL_PIN));
	PORTD |=  (1<<BTN_ACT_PIN)|(1<<BTN_DEL_PIN);
}

static bool btn_pressed(uint8_t pin){
	if(!(PIND & (1<<pin))){
		_delay_ms(20);
		if(!(PIND & (1<<pin))){
			while(!(PIND & (1<<pin))); // esperar soltar
			_delay_ms(10);
			return true;
		}
	}
	return false;
}
static bool uid_equals(const uint8_t *a, const uint8_t *b, uint8_t len){
	for(uint8_t i=0;i<len;i++) if(a[i]!=b[i]) return false;
	return true;
}
static void acceso_ok(void){
	PORTD |=  (1<<LED_VERDE_PIN);
	PORTD &= ~(1<<LED_ROJO_PIN);
	PORTD |=  (1<<RELE_PIN);          // abrir
	_delay_ms(800);
	PORTD &= ~(1<<RELE_PIN);
	PORTD &= ~(1<<LED_VERDE_PIN);
}
static void acceso_denegado(void){
	for(uint8_t i=0;i<3;i++){
		PORTD |=  (1<<LED_ROJO_PIN);
		_delay_ms(150);
		PORTD &= ~(1<<LED_ROJO_PIN);
		_delay_ms(120);
	}
}

int main(void){
	// UART 9600
	#define BAUD 9600
	#define MY_UBRR (F_CPU/16/BAUD-1)
	uart_init(MY_UBRR);

	gpio_init();
	lcd_init();
	spi_init();
	mfrc522_resetPinInit();
	mfrc522_init();

	lcd_clear();
	lcd_print("Bienvenido RFID");
	lcd_set_cursor(0,1); lcd_print("Acerque tarjeta");
	uart_print("\r\n== Sistema RFID iniciado ==\r\n");

	uint8_t uid[10], uid_len;
	uint8_t mem_uid[10]; uint8_t mem_len = 0;

	if(ee_has_card()){
		mem_len = ee_read_card(mem_uid);
		uart_print("Tarjeta registrada: ");
		uart_print_hex_array(mem_uid, mem_len);
		uart_print("\r\n");
		} else {
		uart_print("Sin tarjeta registrada.\r\n");
	}

	while(1){
		// Borrar tarjeta (BTN DEL)
		if(btn_pressed(BTN_DEL_PIN)){
			ee_clear_card();
			mem_len = 0;
			lcd_clear();
			lcd_print("Tarjeta borrada");
			lcd_set_cursor(0,1); lcd_print("Registre nueva");
			uart_print("Tarjeta borrada.\r\n");
			_delay_ms(900);
			lcd_clear(); lcd_print("Acerque tarjeta");
		}

		// Registrar/Actualizar (BTN ACT)
		if(btn_pressed(BTN_ACT_PIN)){
			lcd_clear(); lcd_print("Modo registro");
			lcd_set_cursor(0,1); lcd_print("Acerque tarjeta");
			uart_print("Modo registro: acerque tarjeta...\r\n");
			bool ok=false;
			for(uint16_t to=3000; to>0; to--){
				if(mfrc522_read_uid(uid, &uid_len) && uid_len>=4){
					ee_write_card(uid, uid_len);
					memcpy(mem_uid, uid, uid_len); mem_len = uid_len;
					lcd_clear(); lcd_print("Nueva tarjeta");
					lcd_set_cursor(0,1); lcd_print("registrada");
					uart_print("Nueva tarjeta: ");
					uart_print_hex_array(uid, uid_len); uart_print("\r\n");
					ok=true; break;
				}
				_delay_ms(1);
			}
			if(!ok){
				lcd_clear(); lcd_print("No detectada");
				uart_print("Tiempo de registro agotado.\r\n");
			}
			_delay_ms(900);
			lcd_clear(); lcd_print("Acerque tarjeta");
		}

		// Verificación normal
		if(mfrc522_read_uid(uid, &uid_len) && uid_len>=4){
			uart_print("UID leido: "); uart_print_hex_array(uid, uid_len); uart_print("\r\n");
			if(mem_len>0 && uid_equals(uid, mem_uid, mem_len)){
				lcd_clear(); lcd_print("Acceso permitido");
				acceso_ok();
				lcd_clear(); lcd_print("Acerque tarjeta");
				} else {
				lcd_clear(); lcd_print("Acceso denegado");
				acceso_denegado();
				lcd_clear(); lcd_print("Acerque tarjeta");
			}
			_delay_ms(300);
		}
	}
}
