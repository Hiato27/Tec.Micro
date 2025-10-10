#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdint.h>

/*Pinout
LCD: RS=PB0(D8), E=PB1(D9), D4..D7=PD4..PD7(D4..D7)
TECLADO: Filas=PB2..PB5(D10..D13), Columnas=PC0..PC3(A0..A3)
LEDs: Verde=PD2(D2), Rojo=PD3(D3)
Zumbador: PC4(A4)
*/

/*EEPROM*/
static void eeprom_escribir_byte(uint16_t dir, uint8_t dato){
    while(EECR & (1<<EEPE));
    EEAR = dir;
    EEDR = dato;
    EECR |= (1<<EEMPE);
    EECR |= (1<<EEPE);
}
static uint8_t eeprom_leer_byte(uint16_t dir){
    while(EECR & (1<<EEPE));
    EEAR = dir;
    EECR |= (1<<EERE);
    return EEDR;
}

#define EE_MAGIA_DIR  0
#define EE_LONG_DIR   1
#define EE_DATOS_DIR  2
#define EE_MAGIA_VAL  0xA5

/*LCD 4-bit*/
static void lcd_pulso_e(void){
    PORTB |= (1<<PB1);
    _delay_us(1);
    PORTB &= ~(1<<PB1);
    _delay_us(50);
}
static void lcd_escribir4(uint8_t v){ // en PD7..PD4
    uint8_t p = PORTD & 0x0F;
    PORTD = p | (v<<4);
    lcd_pulso_e();
}
static void lcd_comando(uint8_t c){
    PORTB &= ~(1<<PB0); // RS=0
    lcd_escribir4(c>>4);
    lcd_escribir4(c & 0x0F);
    if(c==0x01 || c==0x02) _delay_ms(2);
}
static void lcd_dato(uint8_t d){
    PORTB |= (1<<PB0); // RS=1
    lcd_escribir4(d>>4);
    lcd_escribir4(d & 0x0F);
}
static void lcd_iniciar(void){
    _delay_ms(40);
    PORTB &= ~((1<<PB0)|(1<<PB1)); // RS=E=0
    lcd_escribir4(0x03); _delay_ms(5);
    lcd_escribir4(0x03); _delay_us(150);
    lcd_escribir4(0x03); _delay_us(150);
    lcd_escribir4(0x02); _delay_us(150);
    lcd_comando(0x28); // 4-bit, 2 líneas
    lcd_comando(0x0C); // display on
    lcd_comando(0x06); // entry mode
    lcd_comando(0x01); // clear
}
static void lcd_pos(uint8_t col, uint8_t fila){
    uint8_t dir = (fila?0x40:0x00) + col;
    lcd_comando(0x80 | dir);
}
static void lcd_imprimir(const char* s){
    while(*s) lcd_dato(*s++);
}

static void io_iniciar(void){
    // LCD
    DDRB |= (1<<PB0)|(1<<PB1);     // RS,E
    DDRD |= (1<<PD4)|(1<<PD5)|(1<<PD6)|(1<<PD7); // D4..D7
    // Teclado
    DDRB |= (1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5); 
    DDRC &= ~((1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3)); 
    PORTC |= (1<<PC0)|(1<<PC1)|(1<<PC2)|(1<<PC3);  
    // LEDs
    DDRD |= (1<<PD2)|(1<<PD3);
    // Zumbador
    DDRC |= (1<<PC4);
}

/*Teclado 4x4*/
static const char mapa_teclas_final[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};
static void filas_poner_baja(uint8_t fila_baja){ 
    PORTB |= (1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5); 
    switch(fila_baja){
        case 0: PORTB &= ~(1<<PB2); break;
        case 1: PORTB &= ~(1<<PB3); break;
        case 2: PORTB &= ~(1<<PB4); break;
        case 3: PORTB &= ~(1<<PB5); break;
        default: break;
    }
}
static int8_t leer_columna_final(void){
    uint8_t c = PINC;
    if(!(c & (1<<PC0))) return 0;
    if(!(c & (1<<PC1))) return 1;
    if(!(c & (1<<PC2))) return 2;
    if(!(c & (1<<PC3))) return 3;
    return -1;
}
static char teclado_leer_bloqueante_final(void){
    while(1){
        for(uint8_t f=0;f<4;f++){
            filas_poner_baja(f);
            _delay_us(50);
            int8_t col = leer_columna_final();
            if(col>=0){
                _delay_ms(15);
                if(leer_columna_final()==col){
                    while(leer_columna_final()==col);
                    return mapa_teclas_final[f][col];
                }
            }
        }
    }
}

/*Indicadores*/
static void leds_verde(void){ PORTD |= (1<<PD2); PORTD &= ~(1<<PD3); }
static void leds_rojo(void){  PORTD |= (1<<PD3); PORTD &= ~(1<<PD2); }
static void leds_apagar(void){ PORTD &= ~((1<<PD2)|(1<<PD3)); }
static void zumbador_on(void){ PORTC |= (1<<PC4); }
static void zumbador_off(void){ PORTC &= ~(1<<PC4); }

#define CLAVE_MAX 6
#define CLAVE_MIN 4

static void eeprom_cargar_o_por_defecto_final(uint8_t *longitud, char *buf){
    uint8_t m = eeprom_leer_byte(EE_MAGIA_DIR);
    if(m != EE_MAGIA_VAL){
        // default "1234"
        eeprom_escribir_byte(EE_MAGIA_DIR, EE_MAGIA_VAL);
        eeprom_escribir_byte(EE_LONG_DIR, 4);
        eeprom_escribir_byte(EE_DATOS_DIR+0, '1');
        eeprom_escribir_byte(EE_DATOS_DIR+1, '2');
        eeprom_escribir_byte(EE_DATOS_DIR+2, '3');
        eeprom_escribir_byte(EE_DATOS_DIR+3, '4');
    }
    *longitud = eeprom_leer_byte(EE_LONG_DIR);
    if(*longitud < CLAVE_MIN || *longitud > CLAVE_MAX) *longitud = 4;
    for(uint8_t i=0;i<*longitud;i++) buf[i] = eeprom_leer_byte(EE_DATOS_DIR+i);
}

static void eeprom_guardar_clave_final(uint8_t longitud, const char* buf){
    eeprom_escribir_byte(EE_MAGIA_DIR, EE_MAGIA_VAL);
    eeprom_escribir_byte(EE_LONG_DIR, longitud);
    for(uint8_t i=0;i<longitud;i++) eeprom_escribir_byte(EE_DATOS_DIR+i, buf[i]);
}

/* Muestra **** al escribir y gestiona *, # */
static uint8_t ingresar_clave_mascarada_final(char* destino, uint8_t objetivo_min, uint8_t objetivo_max){
    uint8_t n = 0;
    while(1){
        char t = teclado_leer_bloqueante_final();
        if(t>='0' && t<='9'){
            if(n<objetivo_max){
                destino[n++] = t;
                lcd_dato('*');
            }
        }else if(t=='*'){ // borrar
            if(n>0){
                n--;
                lcd_pos(n,1);
                lcd_dato(' ');
                lcd_pos(n,1);
            }
        }else if(t=='#'){ // enter
            if(n>=objetivo_min) return n;
        }else if(t=='A'){ 
        }
    }
}

static bool claves_iguales_final(const char* a, uint8_t la, const char* b, uint8_t lb){
    if(la!=lb) return false;
    for(uint8_t i=0;i<la;i++) if(a[i]!=b[i]) return false;
    return true;
}

/*Pantallas*/
static void pantalla_bienvenida(void){
    lcd_comando(0x01);
    lcd_pos(0,0); lcd_imprimir("Ingrese clave:");
}

static void pantalla_menu_cambio(void){
    lcd_comando(0x01);
    lcd_pos(0,0); lcd_imprimir("A: Cambiar clave");
    lcd_pos(0,1); lcd_imprimir("#: Continuar");
}

int main(void){
    io_iniciar();
    lcd_iniciar();
    leds_apagar(); zumbador_off();

    uint8_t longitud_clave=0;
    char clave[CLAVE_MAX];
    eeprom_cargar_o_por_defecto_final(&longitud_clave, clave);

    uint8_t reintentos = 0;

    while(1){
        pantalla_menu_cambio();
        // Tecla para entrar al cambio: 'A'; con '#' seguimos a login
        char opcion = teclado_leer_bloqueante_final();
        if(opcion=='A'){
            // Verificar clave actual
            lcd_comando(0x01);
            lcd_pos(0,0); lcd_imprimir("Clave actual:");
            lcd_pos(0,1);
            char ingreso[CLAVE_MAX];
            uint8_t n_ing = ingresar_clave_mascarada_final(ingreso, longitud_clave, longitud_clave);
            if(claves_iguales_final(clave, longitud_clave, ingreso, n_ing)){
                // Elegir 4 a 6 digitos
                lcd_comando(0x01);
                lcd_pos(0,0); lcd_imprimir("4 a 6 digitos:");
                uint8_t nueva_longitud=0;
                while(1){
                    char c = teclado_leer_bloqueante_final();
                    if(c>='4' && c<='6'){ nueva_longitud = (uint8_t)(c - '0'); break; }
                }
                // Ingresar nueva + confirmar
                lcd_comando(0x01);
                lcd_pos(0,0); lcd_imprimir("Nueva clave:");
                lcd_pos(0,1);
                char nueva[CLAVE_MAX];
                uint8_t n_nueva = ingresar_clave_mascarada_final(nueva, nueva_longitud, nueva_longitud);

                lcd_comando(0x01);
                lcd_pos(0,0); lcd_imprimir("Confirmar:");
                lcd_pos(0,1);
                char confirma[CLAVE_MAX];
                uint8_t n_conf = ingresar_clave_mascarada_final(confirma, nueva_longitud, nueva_longitud);

                if(claves_iguales_final(nueva, n_nueva, confirma, n_conf)){
                    eeprom_guardar_clave_final(n_nueva, nueva);
                    longitud_clave = n_nueva; for(uint8_t i=0;i<n_nueva;i++) clave[i]=nueva[i];
                    lcd_comando(0x01);
                    lcd_pos(0,0); lcd_imprimir("Clave guardada");
                    leds_verde(); _delay_ms(800); leds_apagar();
                }else{
                    lcd_comando(0x01);
                    lcd_pos(0,0); lcd_imprimir("No coincide");
                    leds_rojo(); _delay_ms(1000); leds_apagar();
                }
            }else{
                lcd_comando(0x01);
                lcd_pos(0,0); lcd_imprimir("Clave invalida");
                leds_rojo(); _delay_ms(1000); leds_apagar();
            }
            continue; // vuelve al menú
        }

        if(opcion=='#'){
            // Proceso de ingreso normal
            pantalla_bienvenida();
            lcd_pos(0,1);
            char ingreso[CLAVE_MAX];
            uint8_t n_ing = ingresar_clave_mascarada_final(ingreso, longitud_clave, longitud_clave);

            if(claves_iguales_final(clave, longitud_clave, ingreso, n_ing)){
                lcd_comando(0x01);
                lcd_pos(0,0); lcd_imprimir("Acceso permitido");
                leds_verde();
                reintentos = 0;
                _delay_ms(1500);
                leds_apagar();
            }else{
                reintentos++;
                lcd_comando(0x01);
                lcd_pos(0,0); lcd_imprimir("Clave incorrecta");
                lcd_pos(0,1); lcd_imprimir("Intento ");
                lcd_dato('0'+reintentos); lcd_imprimir("/3");
                leds_rojo(); _delay_ms(1200); leds_apagar();

                if(reintentos>=3){
                    lcd_comando(0x01);
                    lcd_pos(0,0); lcd_imprimir("ALERTA BLOQUEO");
                    zumbador_on(); leds_rojo();
                    _delay_ms(4000);
                    zumbador_off(); leds_apagar();
                    reintentos = 0;
                }
            }
        }
    }
    return 0;
}
