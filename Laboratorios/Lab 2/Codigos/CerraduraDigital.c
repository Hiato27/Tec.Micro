#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>

/* Pinout:
LCD: RS=PB0, E=PB1, D4..D7=PD4..PD7
TECLADO: Filas=PB2..PB5, Columnas=PC0..PC3
*/

static void lcd_pulso_e(void){
    PORTB |=  (1<<PB1);
    _delay_us(1);
    PORTB &= ~(1<<PB1);
    _delay_us(50);
}
static void lcd_escribir4(uint8_t valor){
    uint8_t preserva = PORTD & 0x0F;
    PORTD = preserva | (valor<<4);
    lcd_pulso_e();
}
static void lcd_comando(uint8_t cmd){
    PORTB &= ~(1<<PB0);        // RS=0
    lcd_escribir4(cmd>>4);
    lcd_escribir4(cmd & 0x0F);
    if(cmd==0x01 || cmd==0x02) _delay_ms(2);
}
static void lcd_dato(uint8_t dato){
    PORTB |= (1<<PB0);         // RS=1
    lcd_escribir4(dato>>4);
    lcd_escribir4(dato & 0x0F);
}
static void lcd_iniciar(void){
    _delay_ms(40);
    PORTB &= ~((1<<PB0)|(1<<PB1));
    lcd_escribir4(0x03); _delay_ms(5);
    lcd_escribir4(0x03); _delay_us(150);
    lcd_escribir4(0x03); _delay_us(150);
    lcd_escribir4(0x02); _delay_us(150);
    lcd_comando(0x28);
    lcd_comando(0x0C);
    lcd_comando(0x06);
    lcd_comando(0x01);
}
static void lcd_pos(uint8_t col, uint8_t fila){
    uint8_t dir = (fila?0x40:0x00) + col;
    lcd_comando(0x80 | dir);
}
static void lcd_imprimir(const char* txt){
    while(*txt) lcd_dato(*txt++);
}

/* Teclado 4x4 */
static const char mapa_teclas[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};
static void filas_poner(uint8_t fila_baja){
    PORTB |= (1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5);
    if(fila_baja<4) PORTB &= ~(1<<(PB2+fila_baja));
}
static int8_t leer_columna(void){
    uint8_t pin = PINC;
    if(!(pin&(1<<PC0))) return 0;
    if(!(pin&(1<<PC1))) return 1;
    if(!(pin&(1<<PC2))) return 2;
    if(!(pin&(1<<PC3))) return 3;
    return -1;
}
static char teclado_leer_bloqueante(void){
    while(1){
        for(uint8_t f=0; f<4; f++){
            filas_poner(f);
            _delay_us(50);
            int8_t c = leer_columna();
            if(c>=0){
                _delay_ms(15);
                if(leer_columna()==c){
                    while(leer_columna()==c);
                    return mapa_teclas[f][c];
                }
            }
        }
    }
}

/* IO */
static void io_iniciar(void){
    DDRB |= (1<<PB0)|(1<<PB1);                     // LCD RS,E
    DDRD |= (1<<PD4)|(1<<PD5)|(1<<PD6)|(1<<PD7);   // LCD D4..D7
    DDRB |= (1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5);   // Filas OUT
    DDRC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3));// Cols IN
    PORTC |=  (1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3); // Pull-ups
}

int main(void){
    io_iniciar();
    lcd_iniciar();

    lcd_pos(0,0); lcd_imprimir("Teclado listo");
    lcd_pos(0,1); lcd_imprimir("Tecla: ");

    while(1){
        char tecla = teclado_leer_bloqueante();
        lcd_pos(7,1);
        lcd_dato(tecla);
        lcd_imprimir("  ");
    }
}
