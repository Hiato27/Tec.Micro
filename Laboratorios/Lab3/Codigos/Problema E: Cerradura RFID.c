// ============================================================================
// Cerradura RFID con RC522 + LCD I2C + EEPROM + UART + LEDs + Botones (+Relé)
// ATmega328P @16MHz | AVR-GCC | Microchip Studio
// ----------------------------------------------------------------------------
// Pines:
// RC522 (3.3V): RST->D9(PB1), SS/SDA->D10(PB2), MOSI->D11(PB3), MISO->D12(PB4), SCK->D13(PB5)
// LCD I2C (5V): SDA->A4(PC4), SCL->A5(PC5) (0x27/0x3F/0x20)
// LEDs: Verde->D6(PD6), Rojo->D7(PD7)
// Botones (a GND, pull-up): ACTUALIZAR->D2(PD2), BORRAR->D3(PD3)
// Relé: D5(PD5) con NPN + diodo (o módulo)
// EEPROM: Byte0=0xA5 (flag), Bytes1..4: UID[0..3]
// ============================================================================

#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#define UART_BAUD 9600

#include <avr/io.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ================= UART ================= */
#define UBRR_VAL ((F_CPU/16UL/((unsigned long)UART_BAUD))-1UL)
static void uart_init(void){
	UBRR0H = (uint8_t)(UBRR_VAL>>8);
	UBRR0L = (uint8_t)UBRR_VAL;
	UCSR0B = (1<<TXEN0);
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);  // 8N1
}
static void uart_tx(uint8_t d){ while(!(UCSR0A&(1<<UDRE0))); UDR0=d; }
static void uart_print(const char*s){ while(*s) uart_tx((uint8_t)*s++); }
static void uprintf(const char*fmt,...){ char b[192]; va_list ap; va_start(ap,fmt); vsnprintf(b,sizeof(b),fmt,ap); va_end(ap); uart_print(b); }

/* ================= LEDS ================= */
#define LED_OK_DDR DDRD
#define LED_OK_PORT PORTD
#define LED_OK_PIN PD6
#define LED_ER_DDR DDRD
#define LED_ER_PORT PORTD
#define LED_ER_PIN PD7
static inline void led_ok_on(void){ LED_OK_PORT |= (1<<LED_OK_PIN); }
static inline void led_ok_off(void){ LED_OK_PORT &= ~(1<<LED_OK_PIN); }
static inline void led_er_on(void){ LED_ER_PORT |= (1<<LED_ER_PIN); }
static inline void led_er_off(void){ LED_ER_PORT &= ~(1<<LED_ER_PIN); }

/* ================ I2C / LCD ================= */
#define TWI_FREQ 100000UL
static void twi_init(void){
	TWSR = 0x00; // prescaler=1
	TWBR = (uint8_t)((F_CPU/TWI_FREQ - 16) / 2);
}
static uint8_t twi_start_write(uint8_t addr7){
	TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
	while(!(TWCR&(1<<TWINT)));
	TWDR = (addr7<<1); // write
	TWCR = (1<<TWINT)|(1<<TWEN);
	while(!(TWCR&(1<<TWINT)));
	return ((TWSR & 0xF8) == 0x18) ? 0 : 1; // 0=ACK
}
static void twi_write(uint8_t d){ TWDR=d; TWCR=(1<<TWINT)|(1<<TWEN); while(!(TWCR&(1<<TWINT))); }
static void twi_stop(void){ TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWSTO); }

// LCD PCF8574 (4-bit)
#define EN 0x04
#define RS 0x01
static uint8_t LCD_ADDR = 0x27;
static uint8_t BL_MASK = 0x08; // backlight activo alto
static void pcf_send(uint8_t v){ if (twi_start_write(LCD_ADDR)==0){ twi_write(v | BL_MASK); } twi_stop(); }
static void lcd_pulse(uint8_t d){ pcf_send(d|EN); _delay_us(1); pcf_send(d&~EN); _delay_us(50); }
static void lcd_send4(uint8_t n){ lcd_pulse(n); }
static void lcd_send(uint8_t v, uint8_t mode){ uint8_t hi=v&0xF0, lo=(v<<4)&0xF0; lcd_send4(hi|mode); lcd_send4(lo|mode); }
static void lcd_cmd(uint8_t c){ lcd_send(c,0x00); }
static void lcd_data(uint8_t d){ lcd_send(d,RS); }
static void lcd_clear(void){ lcd_cmd(0x01); _delay_ms(2); }
static void lcd_set(uint8_t c,uint8_t r){ static const uint8_t A[2]={0x00,0x40}; lcd_cmd(0x80 | (A[r&1] + (c&0x0F))); }
static void lcd_print(const char*s){ while(*s) lcd_data((uint8_t)*s++); }
static bool lcd_try_init_with(uint8_t addr, bool bl_high){
	LCD_ADDR=addr; BL_MASK= bl_high?0x08:0x00;
	if (twi_start_write(LCD_ADDR)!=0){ twi_stop(); return false; }
	twi_stop(); _delay_ms(40);
	pcf_send(0x00);
	lcd_send4(0x30); _delay_ms(5);
	lcd_send4(0x30); _delay_us(150);
	lcd_send4(0x30); _delay_us(150);
	lcd_send4(0x20); _delay_us(150);
	lcd_cmd(0x28); // 4-bit, 2 líneas, 5x8
	lcd_cmd(0x0C); // display ON, cursor OFF
	lcd_cmd(0x06); // entry mode
	lcd_clear();
	return true;
}

/* ================= SPI ================= */
static inline void spi_set_mode_speed(uint8_t mode, uint8_t spr1, uint8_t spr0, uint8_t spi2x){
	DDRB |= (1<<PB2)|(1<<PB3)|(1<<PB5);
	DDRB &= ~(1<<PB4);
	PORTB |= (1<<PB2); // SS HIGH
	SPCR = (1<<SPE)|(1<<MSTR) | (spr1? (1<<SPR1):0) | (spr0? (1<<SPR0):0);
	switch(mode){
		case 0: SPCR &= ~((1<<CPOL)|(1<<CPHA)); break;
		case 1: SPCR = (SPCR & ~(1<<CPOL)) | (1<<CPHA); break;
		case 2: SPCR = (SPCR & ~(1<<CPHA)) | (1<<CPOL); break;
		case 3: SPCR |= (1<<CPOL)|(1<<CPHA); break;
	}
	if (spi2x) SPSR |= (1<<SPI2X); else SPSR &= ~(1<<SPI2X);
}
static inline void SS_LOW(void){ PORTB &= ~(1<<PB2); }
static inline void SS_HIGH(void){ PORTB |=  (1<<PB2); }
static inline uint8_t spi_tr(uint8_t d){ SPDR=d; while(!(SPSR&(1<<SPIF))); return SPDR; }

/* ================= RC522 ================= */
#define RST_PIN PB1
// Registros
#define VersionReg    0x37
#define CommandReg    0x01
#define CommIEnReg    0x02
#define CommIrqReg    0x04
#define ErrorReg      0x06
#define FIFODataReg   0x09
#define FIFOLevelReg  0x0A
#define ControlReg    0x0C
#define BitFramingReg 0x0D
#define ModeReg       0x11
#define TxControlReg  0x14
#define TxASKReg      0x15
#define RFCfgReg      0x26
#define TModeReg      0x2A
#define TPrescalerReg 0x2B
#define TReloadRegH   0x2C
#define TReloadRegL   0x2D
// Comandos
#define PCD_TRANSCEIVE 0x0C
#define PCD_SOFTRESET  0x0F
#define PICC_REQIDL        0x26
#define PICC_ANTICOLL_CL1  0x93

static inline void rc_write_addr(uint8_t reg){ spi_tr((reg<<1)&0x7E); }
static inline void rc_read_addr (uint8_t reg){ spi_tr(((reg<<1)&0x7E)|0x80); }
static void rc_write(uint8_t reg,uint8_t val){ SS_LOW(); _delay_us(2); rc_write_addr(reg); spi_tr(val); _delay_us(1); SS_HIGH(); }
static uint8_t rc_read(uint8_t reg){ uint8_t v; SS_LOW(); _delay_us(2); rc_read_addr(reg); v=spi_tr(0x00); _delay_us(1); SS_HIGH(); return v; }
static void rc_setBitMask(uint8_t reg, uint8_t mask){ rc_write(reg, rc_read(reg)|mask); }
static void rc_clrBitMask(uint8_t reg, uint8_t mask){ rc_write(reg, rc_read(reg)&(~mask)); }

static void rc_reset_pin_init(void){ DDRB|=(1<<RST_PIN); PORTB&=~(1<<RST_PIN); _delay_ms(10); PORTB|=(1<<RST_PIN); _delay_ms(50); }
static void rc_softreset(void){ rc_write(CommandReg, PCD_SOFTRESET); _delay_ms(50); }
static void rc_init_core(void){
	rc_softreset();
	rc_write(TModeReg, 0x8D);
	rc_write(TPrescalerReg, 0x3E);
	rc_write(TReloadRegL, 0x1E);
	rc_write(TReloadRegH, 0x00);
	rc_write(TxASKReg, 0x40);   // 100% ASK
	rc_write(ModeReg,  0x3D);   // CRC preset
	rc_write(RFCfgReg, 0x7F);   // ganancia alta
	uint8_t v = rc_read(TxControlReg);
	if ((v & 0x03) != 0x03) rc_write(TxControlReg, v | 0x03); // antena ON
}

// Autodetección SPI (Modo 0..3 y /128,/64,/32,/16). Devuelve true si hay versión válida.
static bool rc522_autoprobe(uint8_t *outVer){
	const uint8_t sp[4][3] = {{1,1,0},{1,0,0},{0,1,0},{0,0,0}}; // /128,/64,/32,/16
	for (uint8_t mode=0; mode<4; mode++){
		for (uint8_t s=0; s<4; s++){
			spi_set_mode_speed(mode, sp[s][0], sp[s][1], sp[s][2]);
			_delay_ms(5);
			rc_softreset();
			uint8_t v = rc_read(VersionReg);
			if (v==0x91 || v==0x92 || v==0x88){ *outVer=v; return true; }
		}
	}
	*outVer = rc_read(VersionReg);
	return false;
}

// TX/RX
static uint8_t rc_transceive(uint8_t *tx, uint8_t txLen, uint8_t *rx, uint8_t *rxBits){
	uint8_t waitIrq = 0x30; // RxIRq | IdleIrq
	rc_write(CommIEnReg, waitIrq | 0x80);
	rc_clrBitMask(CommIrqReg, 0x80);
	rc_setBitMask(FIFOLevelReg, 0x80);
	for (uint8_t i=0;i<txLen;i++) rc_write(FIFODataReg, tx[i]);
	rc_write(CommandReg, PCD_TRANSCEIVE);
	rc_setBitMask(BitFramingReg, 0x80); // StartSend
	uint16_t i=3000; uint8_t n;
	do { n=rc_read(CommIrqReg); i--; } while(i && !(n&0x01) && !(n&0x30));
	rc_clrBitMask(BitFramingReg, 0x80);
	if (i==0) return 1;                      // timeout
	if (rc_read(ErrorReg) & 0x13) return 2;  // errores
	uint8_t fifo = rc_read(FIFOLevelReg);
	for(uint8_t j=0;j<fifo;j++) rx[j]=rc_read(FIFODataReg);
	uint8_t lastBits = rc_read(ControlReg) & 0x07;
	*rxBits = (fifo ? (fifo-1)*8 + lastBits : 0);
	return 0;
}

// Lee UID de 4 bytes (CL1)
static uint8_t rc_read_uid(uint8_t *uid4){
	uint8_t rx[16]; uint8_t bits;
	rc_write(BitFramingReg, 0x07);           // REQA: 7 bits
	uint8_t reqa[1] = {PICC_REQIDL};
	if (rc_transceive(reqa, 1, rx, &bits) != 0 || bits != 16){ rc_write(BitFramingReg, 0x00); return 0; }
	rc_write(BitFramingReg, 0x00);           // bytes completos
	uint8_t anticoll[] = {PICC_ANTICOLL_CL1, 0x20};
	if (rc_transceive(anticoll, sizeof(anticoll), rx, &bits) != 0 || bits < 5*8) return 0;
	for (uint8_t i=0;i<4;i++) uid4[i] = rx[i];
	return 4;
}

/* ========= BOTONES / EEPROM / RELÉ ========= */
#define RELAY_ACTIVE_LOW 0         // poné 1 si tu módulo de relé es activo-bajo
#define RELAY_DDR DDRD
#define RELAY_PORT PORTD
#define RELAY_PIN PD5
static inline void relay_on(void){
	#if RELAY_ACTIVE_LOW
	RELAY_PORT &= ~(1<<RELAY_PIN);
	#else
	RELAY_PORT |=  (1<<RELAY_PIN);
	#endif
}
static inline void relay_off(void){
	#if RELAY_ACTIVE_LOW
	RELAY_PORT |=  (1<<RELAY_PIN);
	#else
	RELAY_PORT &= ~(1<<RELAY_PIN);
	#endif
}

// Botones (pull-up)
#define BTN_UPD_PINP PIND
#define BTN_UPD_PORT PORTD
#define BTN_UPD_DDR DDRD
#define BTN_UPD_PIN PD2 // ACTUALIZAR
#define BTN_DEL_PINP PIND
#define BTN_DEL_PORT PORTD
#define BTN_DEL_DDR DDRD
#define BTN_DEL_PIN PD3 // BORRAR
static bool button_pressed(volatile uint8_t *pinx, uint8_t bit){
	if (((*pinx)&(1<<bit))==0){ _delay_ms(20); return (((*pinx)&(1<<bit))==0); }
	return false;
}

// EEPROM
#define EEPROM_FLAG_ADDR ((uint8_t*)0)
#define EEPROM_UID_ADDR  ((uint8_t*)1)
#define EEPROM_FLAG_VALUE (0xA5)
static bool uid_eq(const uint8_t*a,const uint8_t*b){ return a[0]==b[0] && a[1]==b[1] && a[2]==b[2] && a[3]==b[3]; }
static bool eeprom_read_uid(uint8_t uid4[4]){
	uint8_t flag=eeprom_read_byte(EEPROM_FLAG_ADDR);
	if(flag!=EEPROM_FLAG_VALUE) return false;
	for(uint8_t i=0;i<4;i++) uid4[i]=eeprom_read_byte(EEPROM_UID_ADDR+i);
	return true;
}
static void eeprom_write_uid(const uint8_t uid4[4]){
	eeprom_write_byte(EEPROM_FLAG_ADDR, EEPROM_FLAG_VALUE);
	for(uint8_t i=0;i<4;i++) eeprom_write_byte(EEPROM_UID_ADDR+i, uid4[i]);
}
static void eeprom_clear_uid(void){
	eeprom_write_byte(EEPROM_FLAG_ADDR, 0xFF);
	for(uint8_t i=0;i<4;i++) eeprom_write_byte(EEPROM_UID_ADDR+i, 0xFF);
}

/* ============ UI: máquina de estados de LCD (para evitar parpadeo) ============ */
typedef enum {
	UI_WELCOME,
	UI_INITING,
	UI_RC522_OK,
	UI_RC522_FAIL,
	UI_NO_CARD_STORED,
	UI_PROMPT_CARD,
	UI_ACCESS_OK,
	UI_ACCESS_DENY,
	UI_REG_NEW,
	UI_REG_SAVED,
	UI_ERASED,
	UI_TIMEOUT
} ui_state_t;

static ui_state_t ui_last = (ui_state_t)255;

static void ui_show(ui_state_t st, uint8_t verOpt){
	if (st == ui_last) return; // no actualizar si es la misma pantalla
	ui_last = st;
	switch(st){
		case UI_WELCOME:
		lcd_clear(); lcd_set(0,0); lcd_print("Bienvenido al");
		lcd_set(0,1); lcd_print("sistema RFID");
		break;
		case UI_INITING:
		lcd_clear(); lcd_set(0,0); lcd_print("Inicializando...");
		break;
		case UI_RC522_OK:{
			char hex[5]; snprintf(hex,sizeof(hex),"%02X",verOpt);
			lcd_clear(); lcd_set(0,0); lcd_print("RC522 Ver:0x"); lcd_print(hex);
			lcd_set(0,1); lcd_print("SPI OK");
		} break;
		case UI_RC522_FAIL:
		lcd_clear(); lcd_set(0,0); lcd_print("RC522 sin resp.");
		lcd_set(0,1); lcd_print("Rev 3V3/RST/SS");
		break;
		case UI_NO_CARD_STORED:
		lcd_clear(); lcd_set(0,0); lcd_print("Sin tarjeta");
		lcd_set(0,1); lcd_print("Pres ACTUAL");
		break;
		case UI_PROMPT_CARD:
		lcd_clear(); lcd_set(0,0); lcd_print("Acerque su");
		lcd_set(0,1); lcd_print("tarjeta...");
		break;
		case UI_ACCESS_OK:
		lcd_clear(); lcd_set(0,0); lcd_print("Acceso permitido");
		lcd_set(0,1); lcd_print("Bienvenido");
		break;
		case UI_ACCESS_DENY:
		lcd_clear(); lcd_set(0,0); lcd_print("Acceso denegado");
		lcd_set(0,1); lcd_print("UID distinto");
		break;
		case UI_REG_NEW:
		lcd_clear(); lcd_set(0,0); lcd_print("Registro nuevo");
		lcd_set(0,1); lcd_print("Acerque tarjeta");
		break;
		case UI_REG_SAVED:
		lcd_clear(); lcd_set(0,0); lcd_print("Nueva tarjeta");
		lcd_set(0,1); lcd_print("registrada");
		break;
		case UI_ERASED:
		lcd_clear(); lcd_set(0,0); lcd_print("Tarjeta borrada");
		lcd_set(0,1); lcd_print("Acerque nueva");
		break;
		case UI_TIMEOUT:
		lcd_clear(); lcd_set(0,0); lcd_print("Tiempo agotado");
		lcd_set(0,1); lcd_print("Intente de nuevo");
		break;
		default: break;
	}
}

/* ================= MAIN ================= */
int main(void){
	// --- Diagnóstico reset + apagar Watchdog ---
	uint8_t cause = MCUSR;   // guarda causa
	MCUSR = 0;               // limpia flags
	wdt_disable();           // apaga WDT por si venía activo

	// LEDs
	LED_OK_DDR |= (1<<LED_OK_PIN);
	LED_ER_DDR |= (1<<LED_ER_PIN);
	led_ok_off(); led_er_off();

	// Relé
	RELAY_DDR |= (1<<RELAY_PIN);
	relay_off();

	// Botones: entradas con pull-up
	BTN_UPD_DDR &= ~(1<<BTN_UPD_PIN); BTN_UPD_PORT |= (1<<BTN_UPD_PIN);
	BTN_DEL_DDR &= ~(1<<BTN_DEL_PIN); BTN_DEL_PORT |= (1<<BTN_DEL_PIN);

	// UART (para ver causa de reset y logs)
	uart_init();
	uprintf("\r\n== RFID LOCK ==  Reset cause=0x%02X  (PORF=%d BORF=%d EXTRF=%d WDRF=%d)\r\n",
	cause, (cause>>0)&1, (cause>>2)&1, (cause>>1)&1, (cause>>3)&1);

	// I2C + LCD
	twi_init();
	bool lcd_ok=false; const uint8_t addrs[]={0x27,0x3F,0x20};
	for(uint8_t ia=0; ia<sizeof(addrs); ia++){
		if (lcd_ok) break;
		if (lcd_try_init_with(addrs[ia], true))  { lcd_ok=true; break; }
		if (lcd_try_init_with(addrs[ia], false)) { lcd_ok=true; break; }
	}
	if(lcd_ok) ui_show(UI_WELCOME, 0), _delay_ms(900), ui_show(UI_INITING, 0);

	// RC522: RST y AUTODETECCIÓN SPI
	rc_reset_pin_init();
	uint8_t ver=0x00;
	bool ok = rc522_autoprobe(&ver);
	if(lcd_ok) ui_show(ok? UI_RC522_OK : UI_RC522_FAIL, ver);
	uprintf("RC522 VersionReg=0x%02X (%s)\r\n", ver, ok? "OK":"NO");
	if(!ok){
		while(1){ _delay_ms(500); } // queda indicando error en LCD
	}

	// Inicialización completa del RC522
	rc_init_core();

	// EEPROM: cargar UID maestro
	uint8_t master_uid[4]; bool has_master = eeprom_read_uid(master_uid);
	if(lcd_ok) ui_show(has_master? UI_PROMPT_CARD : UI_NO_CARD_STORED, 0);

	uint8_t uid[10]={0};

	// ----------- LOOP PRINCIPAL -----------
	while(1){
		// BOTON BORRAR
		if (button_pressed(&BTN_DEL_PINP, BTN_DEL_PIN)){
			eeprom_clear_uid(); has_master=false; led_er_on(); led_ok_off(); relay_off();
			if(lcd_ok) ui_show(UI_ERASED, 0);
			uart_print("EEPROM: tarjeta borrada\r\n"); _delay_ms(800); led_er_off();
		}

		// BOTON ACTUALIZAR (REGISTRAR NUEVA)
		if (button_pressed(&BTN_UPD_PINP, BTN_UPD_PIN)){
			if(lcd_ok) ui_show(UI_REG_NEW, 0);
			uart_print("Registro: acerque nueva tarjeta...\r\n");
			uint16_t tout=4000; bool grabado=false;
			while(tout--){
				memset(uid,0,sizeof(uid));
				if (rc_read_uid(uid)==4){
					eeprom_write_uid(uid); memcpy(master_uid,uid,4); has_master=true;
					if(lcd_ok) ui_show(UI_REG_SAVED, 0);
					uprintf("Nueva UID: %02X:%02X:%02X:%02X\r\n", uid[0],uid[1],uid[2],uid[3]);
					grabado=true; _delay_ms(900);
					break;
				}
				_delay_ms(1);
			}
			if(!grabado){ if(lcd_ok) ui_show(UI_TIMEOUT, 0); uart_print("Registro cancelado por timeout.\r\n"); _delay_ms(900); }
			if(lcd_ok) ui_show(UI_PROMPT_CARD, 0);
		}

		// MODO NORMAL
		if(lcd_ok && ui_last!=UI_PROMPT_CARD && ui_last!=UI_ACCESS_OK && ui_last!=UI_ACCESS_DENY)
		ui_show(UI_PROMPT_CARD, 0);

		memset(uid,0,sizeof(uid));
		if (rc_read_uid(uid)==4){
			uprintf("Leida: %02X:%02X:%02X:%02X\r\n", uid[0],uid[1],uid[2],uid[3]);
			if (has_master && uid_eq(uid, master_uid)){
				led_ok_on(); led_er_off(); relay_on();
				if(lcd_ok) ui_show(UI_ACCESS_OK, 0);
				uart_print("ACCESO PERMITIDO\r\n");
				_delay_ms(1500);
				relay_off(); led_ok_off();
				if(lcd_ok) ui_show(UI_PROMPT_CARD, 0);
				} else {
				led_er_on(); led_ok_off(); relay_off();
				if(lcd_ok) ui_show(UI_ACCESS_DENY, 0);
				uart_print("DENEGADO\r\n");
				_delay_ms(1200);
				led_er_off();
				if(lcd_ok) ui_show(UI_PROMPT_CARD, 0);
			}
		}

		_delay_ms(120);
	}
}
