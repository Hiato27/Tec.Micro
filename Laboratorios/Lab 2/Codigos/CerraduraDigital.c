#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>

/* Pinout:
LCD: RS=PB0, E=PB1, D4..D7=PD4..PD7
TECLADO: Filas=PB2..PB5, Columnas=PC0..PC3
*/

static void lcd_pulso_e(void){ PORTB|=(1<<PB1); _delay_us(1); PORTB&=~(1<<PB1); _delay_us(50); }
static void lcd_escribir4(uint8_t v){ uint8_t p=PORTD&0x0F; PORTD=p|(v<<4); lcd_pulso_e(); }
static void lcd_comando(uint8_t c){ PORTB&=~(1<<PB0); lcd_escribir4(c>>4); lcd_escribir4(c&0x0F); if(c==0x01||c==0x02)_delay_ms(2); }
static void lcd_dato(uint8_t d){ PORTB|=(1<<PB0); lcd_escribir4(d>>4); lcd_escribir4(d&0x0F); }
static void lcd_iniciar(void){
    _delay_ms(40); PORTB&=~((1<<PB0)|(1<<PB1));
    lcd_escribir4(0x03); _delay_ms(5); lcd_escribir4(0x03); _delay_us(150); lcd_escribir4(0x03); _delay_us(150);
    lcd_escribir4(0x02); _delay_us(150);
    lcd_comando(0x28); lcd_comando(0x0C); lcd_comando(0x06); lcd_comando(0x01);
}
static void lcd_pos(uint8_t c,uint8_t f){ lcd_comando(0x80 | ((f?0x40:0x00)+c)); }
static void lcd_imprimir(const char*s){ while(*s) lcd_dato(*s++); }

/* Teclado */
static void filas_poner(uint8_t f){ PORTB|=(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5); if(f<4) PORTB&=~(1<<(PB2+f)); }
static int8_t leer_columna(void){ uint8_t x=PINC; if(!(x&(1<<PC0)))return 0; if(!(x&(1<<PC1)))return 1; if(!(x&(1<<PC2)))return 2; if(!(x&(1<<PC3)))return 3; return -1; }
static char teclado_leer_bloqueante(void){
    while(1){
        for(uint8_t f=0; f<4; f++){
            filas_poner(f); _delay_us(50);
            int8_t c=leer_columna();
            if(c>=0){
                _delay_ms(15);
                if(leer_columna()==c){
                    while(leer_columna()==c);
                    static const char mapa[4][4]={{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
                    return mapa[f][c];
                }
            }
        }
    }
}

/* EEPROM */
static void eeprom_escribir_byte(uint16_t dir, uint8_t dato){
    while(EECR&(1<<EEPE)); EEAR=dir; EEDR=dato; EECR|=(1<<EEMPE); EECR|=(1<<EEPE);
}
static uint8_t eeprom_leer_byte(uint16_t dir){
    while(EECR&(1<<EEPE)); EEAR=dir; EECR|=(1<<EERE); return EEDR;
}
#define EE_MAGIA_DIR  0
#define EE_LONG_DIR   1
#define EE_DATOS_DIR  2
#define EE_MAGIA_VAL  0xA5

static void io_iniciar(void){
    DDRB |= (1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5);
    DDRD |= (1<<PD4)|(1<<PD5)|(1<<PD6)|(1<<PD7);
    DDRC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));
    PORTC |=  (1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3);
}

static uint8_t ingresar_clave_4(char *dest){
    uint8_t n=0;
    while(1){
        char t=teclado_leer_bloqueante();
        if(t>='0'&&t<='9'){
            if(n<4){ dest[n++]=t; lcd_dato('*'); }
        }else if(t=='*'){
            if(n>0){ n--; lcd_pos(n,1); lcd_dato(' '); lcd_pos(n,1); }
        }else if(t=='#'){
            if(n==4) return n;
        }
    }
}

int main(void){
    io_iniciar();
    lcd_iniciar();

    uint8_t magia = eeprom_leer_byte(EE_MAGIA_DIR);
    if(magia!=EE_MAGIA_VAL){
        eeprom_escribir_byte(EE_MAGIA_DIR, EE_MAGIA_VAL);
        eeprom_escribir_byte(EE_LONG_DIR, 4);
        eeprom_escribir_byte(EE_DATOS_DIR+0,'1');
        eeprom_escribir_byte(EE_DATOS_DIR+1,'2');
        eeprom_escribir_byte(EE_DATOS_DIR+2,'3');
        eeprom_escribir_byte(EE_DATOS_DIR+3,'4');
    }

    uint8_t longitud = eeprom_leer_byte(EE_LONG_DIR);
    if(longitud!=4) longitud=4;
    char clave[4];
    for(uint8_t i=0;i<4;i++) clave[i]=eeprom_leer_byte(EE_DATOS_DIR+i);

    while(1){
        lcd_comando(0x01);
        lcd_pos(0,0); lcd_imprimir("Ingrese clave:");
        lcd_pos(0,1);
        char ingreso[4];
        uint8_t cant = ingresar_clave_4(ingreso);

        bool coincide = (cant==4);
        for(uint8_t i=0;i<4 && coincide;i++) if(ingreso[i]!=clave[i]) coincide=false;

        lcd_comando(0x01);
        lcd_pos(0,0);
        if(coincide) lcd_imprimir("Acceso permitido");
        else         lcd_imprimir("Clave incorrecta");
        _delay_ms(800);
    }
}
