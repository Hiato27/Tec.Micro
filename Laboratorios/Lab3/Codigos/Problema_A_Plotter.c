#define F_CPU 16000000UL
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>
#include <stdbool.h>

/* PORTD:
   PD2= abajo, PD3= arriba,
   PD4=X+, PD5=Y-, PD6=X-, PD7=Y+
*/
#define PD_X_POS  PD4
#define PD_Y_NEG  PD5
#define PD_X_NEG  PD6
#define PD_Y_POS  PD7

static inline void io_init(void){
    DDRD  = (1u<<PD2)|(1u<<PD3)|(1u<<PD4)|(1u<<PD5)|(1u<<PD6)|(1u<<PD7);
    PORTD = 0x00;
}

static inline void fig_pen_down(void){ PORTD = (1u<<PD2); _delay_ms(1); PORTD=0; }
static inline void fig_pen_up(void){   PORTD = (1u<<PD3); _delay_ms(1); PORTD=0; }

static inline void fig_move_x_pos_ms(uint16_t ms){ PORTD=(1u<<PD_X_POS); while(ms--) _delay_ms(1); PORTD=0; }
static inline void fig_move_x_neg_ms(uint16_t ms){ PORTD=(1u<<PD_X_NEG); while(ms--) _delay_ms(1); PORTD=0; }
static inline void fig_move_y_pos_ms(uint16_t ms){ PORTD=(1u<<PD_Y_POS); while(ms--) _delay_ms(1); PORTD=0; }
static inline void fig_move_y_neg_ms(uint16_t ms){ PORTD=(1u<<PD_Y_NEG); while(ms--) _delay_ms(1); PORTD=0; }

static inline void fig_move_diag_ms(uint8_t mask, uint16_t ms){
    uint16_t half = ms/2;
    if(mask & (1u<<PD_X_POS)) fig_move_x_pos_ms(half);
    if(mask & (1u<<PD_X_NEG)) fig_move_x_neg_ms(half);
    if(mask & (1u<<PD_Y_POS)) fig_move_y_pos_ms(half);
    if(mask & (1u<<PD_Y_NEG)) fig_move_y_neg_ms(half);
}

static inline void out_delay(uint8_t mask, void (*delay_fn)(void)){
    if(delay_fn==NULL) return;
    extern void delay_1s(void);    extern void delay_2s(void);
    extern void delay_3s(void);    extern void delay_10s(void);
    extern void delay_25s(void);   extern void delay_tri(void);
    extern void delay_cruz(void);
    uint32_t ms=0;
    if(delay_fn==delay_1s) ms=1000;
    else if(delay_fn==delay_2s) ms=2000;
    else if(delay_fn==delay_3s) ms=3000;
    else if(delay_fn==delay_10s) ms=10000;
    else if(delay_fn==delay_25s) ms=25000;
    else if(delay_fn==delay_tri)  ms=16u*1000u; 
    else if(delay_fn==delay_cruz) ms=14u*1000u; 

    if(mask & (1u<<PD2)){ fig_pen_down(); }
    if(mask & (1u<<PD3)){ fig_pen_up(); }

    if(mask == (1u<<PD_X_POS))      fig_move_x_pos_ms(ms);
    else if(mask == (1u<<PD_X_NEG)) fig_move_x_neg_ms(ms);
    else if(mask == (1u<<PD_Y_POS)) fig_move_y_pos_ms(ms);
    else if(mask == (1u<<PD_Y_NEG)) fig_move_y_neg_ms(ms);
    else if(mask == ((1u<<PD_X_POS)|(1u<<PD_Y_POS)) ||
            mask == ((1u<<PD_X_POS)|(1u<<PD_Y_NEG)) ||
            mask == ((1u<<PD_X_NEG)|(1u<<PD_Y_POS)) ||
            mask == ((1u<<PD_X_NEG)|(1u<<PD_Y_NEG))){
        fig_move_diag_ms(mask, (uint16_t)ms);
    } else {
    }
}
static inline void out_delay_ms(uint8_t mask, uint16_t ms){
    if(mask & (1u<<PD2)){ fig_pen_down(); }
    if(mask & (1u<<PD3)){ fig_pen_up(); }
    if(mask == (1u<<PD_X_POS))      fig_move_x_pos_ms(ms);
    else if(mask == (1u<<PD_X_NEG)) fig_move_x_neg_ms(ms);
    else if(mask == (1u<<PD_Y_POS)) fig_move_y_pos_ms(ms);
    else if(mask == (1u<<PD_Y_NEG)) fig_move_y_neg_ms(ms);
    else if(mask == ((1u<<PD_X_POS)|(1u<<PD_Y_POS)) ||
            mask == ((1u<<PD_X_POS)|(1u<<PD_Y_NEG)) ||
            mask == ((1u<<PD_X_NEG)|(1u<<PD_Y_POS)) ||
            mask == ((1u<<PD_X_NEG)|(1u<<PD_Y_NEG))) {
        fig_move_diag_ms(mask, ms);
    } else { }
}

static void delay_1s(void){
    for (uint8_t i = 0; i < 4; i++){
        for (uint8_t j = 0; j < 200; j++){
            for (uint8_t k = 0; k < 250; k++){
                __asm__ __volatile__("nop");
            }
        }
    }
}
static void delay_25s(void) { for (uint8_t n = 0; n < 8; n++) delay_1s(); }
static void delay_2s(void)  { delay_1s(); delay_1s(); }
static void delay_3s(void)  { delay_1s(); delay_1s(); delay_1s(); }
static void delay_10s(void) { for (uint8_t n=0;n<10;n++) delay_1s(); }
static void delay_tri(void) { for (uint8_t n=0; n<16u; n++) delay_1s(); }
static void delay_cruz(void){ for (uint8_t n=0; n<14u; n++) delay_1s(); }

/*Figuras*/
static void run_sequence(void);   /* Triangulo */
static void run_sequence2(void);  /* Cruz */
static void run_sequence3(void);  /* Circulo */
static void run_conejo(void);     /* Conejo */
static void run_murcielago(void); /* Murcielago */

/*Todas las figuras*/
int main(void){
    io_init();
    run_sequence();
    run_sequence3();
    run_sequence2();
    run_conejo();
    run_murcielago();
    PORTD=0;
    for(;;){}
    return 0;
}

/*TRIANGULO*/
static void run_sequence(void){
    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD_X_POS), delay_tri);

    out_delay((1u<<PD_Y_POS), delay_tri);
    out_delay((1u<<PD_Y_POS), delay_tri);
    out_delay((1u<<PD_Y_POS), delay_10s);

    out_delay((1u<<PD2),      delay_1s);

    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD_Y_POS), delay_tri);

    out_delay((1u<<PD_X_NEG)|(1u<<PD_Y_NEG), delay_tri);

    out_delay((1u<<PD_X_POS), delay_tri);
    out_delay((1u<<PD3),      delay_1s);

    out_delay_ms((1u<<PD_X_NEG), 5000);
    out_delay_ms((1u<<PD_Y_NEG), 15000);
}

/*CRUZ*/
static void run_sequence2(void){
    out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_X_POS), delay_cruz);
    out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_Y_POS), delay_cruz);
    out_delay((1u<<PD_Y_POS), delay_cruz);
    out_delay((1u<<PD_X_POS), delay_cruz);
    out_delay((1u<<PD_X_POS), delay_cruz);

    out_delay((1u<<PD2),      delay_1s);

    out_delay((1u<<PD_X_NEG)|(1u<<PD_Y_NEG), delay_cruz);

    out_delay((1u<<PD3),      delay_1s);
    out_delay((1u<<PD_X_POS), delay_cruz);
    out_delay((1u<<PD2),      delay_1s);

    out_delay((1u<<PD_Y_POS)|(1u<<PD_Y_NEG), delay_cruz);

    out_delay((1u<<PD3),      delay_1s);

    out_delay_ms((1u<<PD_X_NEG), 15000);
    out_delay_ms((1u<<PD_Y_NEG), 15000);
}

/*CIRCULO*/
static void run_sequence3(void){
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);
    out_delay((1u<<PD_Y_POS), delay_25s);

    out_delay((1u<<PD2),      delay_1s);

    out_delay((1u<<PD_X_NEG), delay_10s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_3s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_2s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_2s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_2s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_2s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_X_POS), delay_10s);

    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_3s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_2s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_2s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_2s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_2s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_3s);
    out_delay((1u<<PD_X_POS), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_10s);

    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_3s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_2s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_2s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_2s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_2s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_3s);
    out_delay((1u<<PD_Y_POS), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_10s);

    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_3s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_2s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_2s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_1s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_2s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_2s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_3s);
    out_delay((1u<<PD_Y_NEG), delay_1s);
    out_delay((1u<<PD_X_NEG), delay_3s);

    out_delay((1u<<PD3),      delay_1s);

    out_delay_ms((1u<<PD_X_NEG), 7000);
    out_delay_ms((1u<<PD_Y_NEG), 5000);
}

/*CONEJO*/
static void run_conejo(void){
    out_delay_ms((1u<<PD_X_POS), 10000);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_NEG), 1500);
    out_delay_ms((1u<<PD_Y_NEG), 1500);
    out_delay_ms((1u<<PD_Y_POS), 750);
    out_delay_ms((1u<<PD_X_POS), 1500);
    out_delay_ms((1u<<PD_X_NEG), 125);
    out_delay_ms((1u<<PD_Y_NEG), 1375);
    out_delay_ms((1u<<PD_X_NEG), 500);
    out_delay_ms((1u<<PD_X_POS), 1375);
    out_delay_ms((1u<<PD_X_NEG), 125);
    out_delay_ms((1u<<PD_X_POS), 3000);
    out_delay_ms((1u<<PD_Y_POS), 3000);
    out_delay_ms((1u<<PD_Y_NEG), 4500);
    out_delay_ms((1u<<PD_X_NEG), 750);
    out_delay_ms((1u<<PD_X_POS), 1500);
    out_delay_ms((1u<<PD_Y_POS), 125);
    out_delay_ms((1u<<PD_Y_NEG), 1375);
    out_delay_ms((1u<<PD_Y_POS), 500);
    out_delay_ms((1u<<PD_X_POS), 1375);
    out_delay_ms((1u<<PD_Y_POS), 125);
    out_delay_ms((1u<<PD_X_NEG), 1500);
    out_delay_ms((1u<<PD_X_NEG), 1500);
    out_delay_ms((1u<<PD_Y_POS), 1500);

    out_delay_ms((1u<<PD3), 100);
    out_delay_ms((1u<<PD_X_NEG), 1125);
    out_delay_ms((1u<<PD_X_POS), 375);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_POS), 750);
    out_delay_ms((1u<<PD_Y_POS), 750);
    out_delay_ms((1u<<PD_Y_NEG), 750);
    out_delay_ms((1u<<PD_X_NEG), 750);
    out_delay_ms((1u<<PD_Y_POS), 750);

    out_delay_ms((1u<<PD3), 100);
    out_delay_ms((1u<<PD_X_NEG), 150);
    out_delay_ms((1u<<PD_X_POS), 150);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_NEG), 400);
    out_delay_ms((1u<<PD_X_POS), 400);
    out_delay_ms((1u<<PD_Y_POS), 400);
    out_delay_ms((1u<<PD_Y_NEG), 400);

    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_Y_POS), 150);
    out_delay_ms((1u<<PD_Y_NEG), 150);

    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_Y_POS), 750);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_POS), 750);
    out_delay_ms((1u<<PD_Y_POS), 750);
    out_delay_ms((1u<<PD_Y_NEG), 750);
    out_delay_ms((1u<<PD_X_NEG), 750);
    out_delay_ms((1u<<PD_Y_POS), 750);

    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_X_NEG), 150);
    out_delay_ms((1u<<PD_X_POS), 150);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_NEG), 400);
    out_delay_ms((1u<<PD_X_POS), 400);
    out_delay_ms((1u<<PD_Y_POS), 400);
    out_delay_ms((1u<<PD_Y_NEG), 400);

    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_Y_POS), 150);
    out_delay_ms((1u<<PD_Y_NEG), 150);

    out_delay_ms((1u<<PD_X_POS), 250);
    out_delay_ms((1u<<PD_X_NEG), 750);
    out_delay_ms((1u<<PD_X_POS), 750);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_NEG), 800);

    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_X_POS), 50); out_delay_ms((1u<<PD_Y_POS), 50); }
    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_Y_NEG), 50); out_delay_ms((1u<<PD_Y_POS), 50); }
    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_X_POS), 50); out_delay_ms((1u<<PD_X_NEG), 50); }

    out_delay_ms((1u<<PD_X_POS), 750);
    out_delay_ms((1u<<PD_X_NEG), 750);
    out_delay_ms((1u<<PD_Y_NEG), 750);

    out_delay_ms((1u<<PD_X_POS), 750);
    out_delay_ms((1u<<PD_Y_POS), 750);

    out_delay_ms((1u<<PD_Y_POS), 750);
    out_delay_ms((1u<<PD_Y_NEG), 750);

    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_Y_POS), 200);
    out_delay_ms((1u<<PD_X_POS), 450);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_Y_POS), 750);
    out_delay_ms((1u<<PD_X_NEG), 750);

    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_Y_NEG), 375);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_Y_POS), 750);

    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_Y_NEG), 375);

    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_NEG), 750);

    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_X_NEG), 1950);

    out_delay_ms((1u<<PD2), 100); // baja
    out_delay_ms((1u<<PD_X_NEG), 750); // dibuja
    out_delay_ms((1u<<PD3), 100); // sube

    out_delay_ms((1u<<PD_X_POS), 375); // posicion

    out_delay_ms((1u<<PD2), 100);
    out_delay_ms((1u<<PD_Y_POS), 600);
    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_X_POS), 400); //posicion
    out_delay_ms((1u<<PD2), 100);
    out_delay_ms((1u<<PD_X_NEG), 600);
    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_Y_NEG), 6500);
}

/*MURCIELAGO*/
static void run_murcielago(void){
    out_delay_ms((1u<<PD_X_POS), 2000);
    out_delay_ms((1u<<PD_Y_POS), 16000);

    out_delay_ms((1u<<PD2), 100);
    out_delay_ms((1u<<PD_X_NEG), 200);
    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_Y_NEG), 150); out_delay_ms((1u<<PD_X_NEG), 150); }
    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_X_NEG), 150); out_delay_ms((1u<<PD_X_POS), 150); }
    out_delay_ms((1u<<PD_X_NEG), 800);
    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_Y_NEG), 150); out_delay_ms((1u<<PD_X_NEG), 150); }
    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_X_NEG), 150); out_delay_ms((1u<<PD_X_POS), 150); }
    out_delay_ms((1u<<PD_X_NEG), 200);

    out_delay_ms((1u<<PD_X_POS), 500);
    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_X_NEG), 150); out_delay_ms((1u<<PD_Y_NEG), 150); }

    out_delay_ms((1u<<PD_X_NEG), 100);
    out_delay_ms((1u<<PD_X_POS), 3000);

    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_Y_POS), 150); out_delay_ms((1u<<PD_X_POS), 150); }
    out_delay_ms((1u<<PD_Y_POS), 100);
    out_delay_ms((1u<<PD_X_POS), 500);

    out_delay_ms((1u<<PD_Y_POS), 6000);

    out_delay_ms((1u<<PD_Y_NEG), 500);
    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_Y_POS), 150); out_delay_ms((1u<<PD_Y_NEG), 150); }
    out_delay_ms((1u<<PD_Y_POS), 100);
    out_delay_ms((1u<<PD_Y_NEG), 3000);
    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_X_NEG), 150); out_delay_ms((1u<<PD_X_POS), 150); }
    out_delay_ms((1u<<PD_X_NEG), 100);
    out_delay_ms((1u<<PD_Y_NEG), 500);
    out_delay_ms((1u<<PD_X_NEG), 150);

    out_delay_ms((1u<<PD3), 100);

    /* ojos */
    out_delay_ms((1u<<PD_X_POS), 500);
    out_delay_ms((1u<<PD_X_NEG), 1100);
    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_NEG), 900);
    out_delay_ms((1u<<PD_X_POS), 900);
    out_delay_ms((1u<<PD_Y_POS), 900);
    out_delay_ms((1u<<PD_Y_NEG), 900);
    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_X_NEG), 3000 );
    out_delay_ms((1u<<PD2), 100);

    out_delay_ms((1u<<PD_X_NEG), 900);
    out_delay_ms((1u<<PD_X_POS), 900);
    out_delay_ms((1u<<PD_Y_POS), 900);
    out_delay_ms((1u<<PD_Y_NEG), 900);
    out_delay_ms((1u<<PD_X_NEG), 900);
    out_delay_ms((1u<<PD3), 100);

    /* boca */
    out_delay_ms((1u<<PD_X_POS), 1500);
    out_delay_ms((1u<<PD2), 100);
    out_delay_ms((1u<<PD_Y_POS), 4800);
    out_delay_ms((1u<<PD_X_NEG), 4800);
    out_delay_ms((1u<<PD_Y_POS), 1000);

    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_X_POS), 70); out_delay_ms((1u<<PD_Y_POS), 70); }
    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_Y_NEG), 70); out_delay_ms((1u<<PD_Y_POS), 70); }
    out_delay_ms((1u<<PD_Y_POS), 700);
    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_X_POS), 70); out_delay_ms((1u<<PD_Y_POS), 70); }
    for (int i = 0; i < 8; i++) { out_delay_ms((1u<<PD_Y_NEG), 70); out_delay_ms((1u<<PD_Y_POS), 70); }

    out_delay_ms((1u<<PD3), 100);

    out_delay_ms((1u<<PD_Y_NEG), 12000);
    out_delay_ms((1u<<PD_X_NEG), 18000);
    PORTD=0;
}
