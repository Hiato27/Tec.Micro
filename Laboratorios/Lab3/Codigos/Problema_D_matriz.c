#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>

//Definición de pines 
#define LED_PORT     PORTD
#define LED_DDR      DDRD
#define LED_PIN      PD2               // Datos WS2812B
#define LED_MASK     (1<<LED_PIN)

#define BTN_PINREG   PIND
#define BTN_DDR      DDRD
#define BTN_PORT     PORTD
#define BTN_PIN      PD3               // Botón del joystick (activo en LOW)

#define ADC_MEDIO     512
#define ZONA_MUERTA   80

#define ANCHO         8
#define ALTO          8
#define N_LEDS       (ANCHO*ALTO)

static uint8_t matriz_rgb[N_LEDS][3];

static uint8_t pos_x = 3;
static uint8_t pos_y = 3;

// Color actual del píxel encendido
static uint8_t col_rojo  = 255;
static uint8_t col_verde = 0;
static uint8_t col_azul  = 0;

static uint16_t sem_azar = 0xACE1u;


static uint16_t azar16(void){
    uint16_t bit = ((sem_azar >> 0u) ^ (sem_azar >> 2u) ^
                    (sem_azar >> 3u) ^ (sem_azar >> 5u)) & 1u;
    sem_azar = (sem_azar >> 1u) | (bit << 15u);
    return sem_azar;
}

// Generar color nuevo aleatorio (evitar negro total)
static void color_aleatorio(void){
    uint16_t r = azar16();
    uint16_t g = azar16();
    uint16_t b = azar16();

    col_rojo  = (uint8_t)(r & 0xFF);
    col_verde = (uint8_t)(g & 0xFF);
    col_azul  = (uint8_t)(b & 0xFF);

    if(col_rojo < 20 && col_verde < 20 && col_azul < 20){
        col_rojo += 40;
    }
}

static uint8_t idx_xy(uint8_t x, uint8_t y){
    return (y * ANCHO) + x;
}


static void actualizar_buf(void){
    // Apagar todo
    for(uint8_t i=0; i<N_LEDS; i++){
        matriz_rgb[i][0] = 0;
        matriz_rgb[i][1] = 0; 
        matriz_rgb[i][2] = 0; 
    }

    // Encender solo el LED activo
    uint8_t idx = idx_xy(pos_x, pos_y);
    matriz_rgb[idx][0] = col_verde; 
    matriz_rgb[idx][1] = col_rojo;
    matriz_rgb[idx][2] = col_azul;
}

static inline void enviar_bit(uint8_t bit_val){
    if(bit_val){
        LED_PORT |=  LED_MASK;
        __asm__ __volatile__ (
            "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
            "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
            "nop\n\t""nop\n\t"
        );
        LED_PORT &= ~LED_MASK;
        __asm__ __volatile__ (
            "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
            "nop\n\t"
        );
    } else {
        LED_PORT |=  LED_MASK;
        __asm__ __volatile__ (
            "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
        );
        LED_PORT &= ~LED_MASK;
        __asm__ __volatile__ (
            "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
            "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
            "nop\n\t""nop\n\t"
        );
    }
}

static inline void enviar_byte(uint8_t dato){
    for(uint8_t i=0; i<8; i++){
        enviar_bit(dato & (1<<(7-i)));
    }
}

static void enviar_matriz(void){
    cli();

    for(uint8_t i=0; i<N_LEDS; i++){
        enviar_byte(matriz_rgb[i][0]); 
        enviar_byte(matriz_rgb[i][1]); 
        enviar_byte(matriz_rgb[i][2]); 
    }

    sei();
    _delay_us(60); 
}


// ADC
static void adc_init(void){
    ADMUX  = (1<<REFS0);
    ADCSRA = (1<<ADEN) | (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
}

static uint16_t adc_leer(uint8_t canal){
    ADMUX = (ADMUX & 0xF0) | (canal & 0x0F); 
    ADCSRA |= (1<<ADSC);                     
    while(ADCSRA & (1<<ADSC)){;}             
    return ADC;                             
}

static void leer_joy(int8_t *mov_x, int8_t *mov_y){
    uint16_t val_x = adc_leer(1); // A1
    uint16_t val_y = adc_leer(0); // A0

    int16_t crudo_x = 0;
    int16_t crudo_y = 0;

    // Eje X
    if(val_x > (ADC_MEDIO + ZONA_MUERTA)) crudo_x = +1;
    else if(val_x < (ADC_MEDIO - ZONA_MUERTA)) crudo_x = -1;
    else crudo_x = 0;

    // Eje Y
    if(val_y > (ADC_MEDIO + ZONA_MUERTA)) crudo_y = -1; 
    else if(val_y < (ADC_MEDIO - ZONA_MUERTA)) crudo_y = +1; 
    else crudo_y = 0;

    int16_t ax = (crudo_x<0)? -crudo_x : crudo_x;
    int16_t ay = (crudo_y<0)? -crudo_y : crudo_y;

    int8_t dx = 0;
    int8_t dy = 0;

    if(ax > ay){
        dx = (crudo_x>0)? +1 : (crudo_x<0? -1:0);
        dy = 0;
    }else if(ay > ax){
        dy = (crudo_y>0)? +1 : (crudo_y<0? -1:0);
        dx = 0;
    }else{
        dx = 0;
        dy = 0;
    }

    *mov_x = dx;
    *mov_y = dy;
}

static bool boton_apretado(void){
    return ( (BTN_PINREG & (1<<BTN_PIN)) == 0 );
}


int main(void){
    LED_DDR  |= LED_MASK;
    LED_PORT &= ~LED_MASK;

    BTN_DDR  &= ~(1<<BTN_PIN); 
    BTN_PORT |=  (1<<BTN_PIN);  

    adc_init();

    pos_x      = 3;
    pos_y      = 3;
    col_rojo   = 255;
    col_verde  = 0;
    col_azul   = 0;

    actualizar_buf();
    enviar_matriz();

    bool btn_ant     = false;
    bool listo_mover = true;   // 1 paso por inclinación

    while(1){
        // Movimiento
        int8_t mov_x, mov_y;
        leer_joy(&mov_x, &mov_y);

        if(listo_mover){
            if(mov_x != 0 || mov_y != 0){
                int16_t nueva_x = (int16_t)pos_x + mov_x;
                int16_t nueva_y = (int16_t)pos_y + mov_y;

                // Limites
                if(nueva_x < 0) nueva_x = 0;
                if(nueva_x > 7) nueva_x = 7;
                if(nueva_y < 0) nueva_y = 0;
                if(nueva_y > 7) nueva_y = 7;

                pos_x = (uint8_t)nueva_x;
                pos_y = (uint8_t)nueva_y;

                actualizar_buf();
                enviar_matriz();
                listo_mover = false;
            }
        } else {
            if(mov_x == 0 && mov_y == 0){
                listo_mover = true;
            }
        }

        //Cambio de color
        bool btn_ahora = boton_apretado();
        if(btn_ahora && !btn_ant){
            color_aleatorio();
            actualizar_buf();
            enviar_matriz();
            _delay_ms(150);
        }
        btn_ant = btn_ahora;

        _delay_ms(20);
    }

    return 0;
}
