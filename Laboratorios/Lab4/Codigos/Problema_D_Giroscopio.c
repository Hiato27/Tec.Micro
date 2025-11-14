#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>

#define LED_PORT     PORTD
#define LED_DDR      DDRD
#define LED_PINREG   PIND
#define LED_PIN      PD2
#define LED_MASK     (1<<LED_PIN)

#define BTN_PINREG   PIND
#define BTN_DDR      DDRD
#define BTN_PORT     PORTD
#define BTN_PIN      PD3

#define ANCHO        8
#define ALTO         8
#define N_LEDS      (ANCHO*ALTO)
#define SERPENTINE   0

#define BRILLO_SHIFT 0

#define MPU_ADDR   0x68
#define REG_PWR1   0x6B
#define REG_SMPL   0x19
#define REG_CFG    0x1A
#define REG_GCFG   0x1B
#define REG_ACFG   0x1C
#define REG_AX_H   0x3B

#define ACC_THR          3000
#define STEP_COOLDOWN_MS 120

#define INVERT_X 0
#define INVERT_Y 0

#define ROTACION_MONTAJE 0

static uint8_t matriz_rgb[N_LEDS][3];
static uint8_t pos_x = 3;
static uint8_t pos_y = 3;

static uint8_t col_rojo = 255, col_verde = 0, col_azul = 0;

static int16_t ax_sesgo = 0, ay_sesgo = 0;

static uint16_t sem_azar = 0xACE1u;
static uint16_t azar16(void){
    uint16_t bit = ((sem_azar >> 0u) ^ (sem_azar >> 2u) ^ (sem_azar >> 3u) ^ (sem_azar >> 5u)) & 1u;
    sem_azar = (sem_azar >> 1u) | (bit << 15u);
    return sem_azar;
}

static void color_aleatorio(void){
    uint16_t r_ale = azar16(), g_ale = azar16(), b_ale = azar16();
    col_rojo  = (uint8_t)(r_ale & 0xFF);
    col_verde = (uint8_t)(g_ale & 0xFF);
    col_azul  = (uint8_t)(b_ale & 0xFF);
    if(col_rojo < 20 && col_verde < 20 && col_azul < 20) col_rojo += 40;
}

static uint8_t idx_xy(uint8_t x, uint8_t y){
#if SERPENTINE
    if (y & 1) return (y * ANCHO) + (ANCHO - 1 - x);
    else       return (y * ANCHO) + x;
#else
    return (y * ANCHO) + x;
#endif
}

static void actualizar_buf(void){
    for(uint8_t i=0; i<N_LEDS; i++){
        matriz_rgb[i][0] = 0;
        matriz_rgb[i][1] = 0;
        matriz_rgb[i][2] = 0;
    }
    uint8_t indice = idx_xy(pos_x, pos_y);
    matriz_rgb[indice][0] = col_verde >> BRILLO_SHIFT;
    matriz_rgb[indice][1] = col_rojo  >> BRILLO_SHIFT;
    matriz_rgb[indice][2] = col_azul  >> BRILLO_SHIFT;
}

static void i2c_init_100k(void) {
    TWSR = 0x00;
    TWBR = 72;
}

static void i2c_start(uint8_t direccion_rw) {
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    TWDR = direccion_rw;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
}

static void i2c_write(uint8_t dato) {
    TWDR = dato;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
}

static uint8_t i2c_read_ack(void) {
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
    while (!(TWCR & (1<<TWINT)));
    return TWDR;
}

static uint8_t i2c_read_nack(void) {
    TWCR = (1<<TWINT)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    return TWDR;
}

static void i2c_stop(void) {
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
}

static void mpu_write(uint8_t reg, uint8_t valor){
    i2c_start((MPU_ADDR<<1)|0);
    i2c_write(reg);
    i2c_write(valor);
    i2c_stop();
}

static void mpu_read_multi(uint8_t reg_inicio, uint8_t *buffer, uint8_t longitud){
    i2c_start((MPU_ADDR<<1)|0);
    i2c_write(reg_inicio);
    i2c_start((MPU_ADDR<<1)|1);
    for(uint8_t i=0;i<longitud;i++){
        buffer[i] = (i < (longitud-1)) ? i2c_read_ack() : i2c_read_nack();
    }
    i2c_stop();
}

static void mpu_init(void){
    _delay_ms(100);
    mpu_write(REG_PWR1, 0x00);
    _delay_ms(10);
    mpu_write(REG_CFG,  0x03);
    mpu_write(REG_SMPL, 0x07);
    mpu_write(REG_GCFG, 0x00);
    mpu_write(REG_ACFG, 0x00);
}

static void mpu_read_accel(int16_t *ax, int16_t *ay, int16_t *az){
    uint8_t crudo[6];
    mpu_read_multi(REG_AX_H, crudo, 6);
    *ax = (int16_t)((crudo[0]<<8) | crudo[1]);
    *ay = (int16_t)((crudo[2]<<8) | crudo[3]);
    *az = (int16_t)((crudo[4]<<8) | crudo[5]);
}

static void mpu_calibrar_offsets(uint16_t muestras){
    int32_t suma_x=0, suma_y=0;
    for(uint16_t i=0;i<muestras;i++){
        int16_t ax_leido, ay_leido, az_leido;
        mpu_read_accel(&ax_leido,&ay_leido,&az_leido);
        suma_x += ax_leido;
        suma_y += ay_leido;
        _delay_ms(5);
    }
    ax_sesgo = (int16_t)(suma_x / (int32_t)muestras);
    ay_sesgo = (int16_t)(suma_y / (int32_t)muestras);
}

static inline void ws2812_send_byte(uint8_t dato) {
    asm volatile(
        "ldi  r18, 8                 \n\t"
        "1:                          \n\t"
        "sbi  %[port], %[bit]        \n\t"
        "sbrs %[dato], 7             \n\t"
        "cbi  %[port], %[bit]        \n\t"
        "nop\n\t""nop\n\t""nop\n\t""nop\n\t"
        "nop\n\t""nop\n\t""nop\n\t"
        "cbi  %[port], %[bit]        \n\t"
        "lsl  %[dato]                \n\t"
        "dec  r18                    \n\t"
        "brne 1b                     \n\t"
        : [dato] "+r" (dato)
        : [port] "I" (_SFR_IO_ADDR(PORTD)), [bit] "I" (LED_PIN)
        : "r18"
    );
}

static void enviar_matriz(void){
    cli();
    for(uint8_t i=0; i<N_LEDS; i++){
        ws2812_send_byte(matriz_rgb[i][0]);
        ws2812_send_byte(matriz_rgb[i][1]);
        ws2812_send_byte(matriz_rgb[i][2]);
    }
    sei();
    _delay_us(70);
}

static bool boton_apretado_edge(void){
    static uint8_t ultimo = 1;
    uint8_t ahora = (BTN_PINREG & (1<<BTN_PIN)) ? 1 : 0;
    bool flanco = (ultimo==1 && ahora==0);
    ultimo = ahora;
    if (flanco) { _delay_ms(25); }
    return flanco && ((BTN_PINREG & (1<<BTN_PIN))==0);
}

int main(void){
    LED_DDR  |=  LED_MASK;
    LED_PORT &= ~LED_MASK;

    BTN_DDR  &= ~(1<<BTN_PIN);
    BTN_PORT |=  (1<<BTN_PIN);

    i2c_init_100k();
    mpu_init();

    mpu_calibrar_offsets(200);

    pos_x = 3; pos_y = 3;
    col_rojo = 255; col_verde = 0; col_azul = 0;

    for(uint8_t i=0;i<N_LEDS;i++){ matriz_rgb[i][0]=matriz_rgb[i][1]=matriz_rgb[i][2]=0; }
    enviar_matriz();
    _delay_ms(10);
    actualizar_buf();
    enviar_matriz();

    uint16_t retraso_paso = 0;
    static int8_t signo_az = +1;

    while(1){
        int16_t ax_leido, ay_leido, az_leido;
        mpu_read_accel(&ax_leido,&ay_leido,&az_leido);

        ax_leido -= ax_sesgo;
        ay_leido -= ay_sesgo;

        int16_t ax_map = ax_leido;
        int16_t ay_map = ay_leido;
        int16_t az_map = az_leido;

        #if (ROTACION_MONTAJE == 90)
            { int16_t t = ax_map; ax_map = -ay_map; ay_map = t; }
        #elif (ROTACION_MONTAJE == 180)
            ax_map = -ax_map; ay_map = -ay_map;
        #elif (ROTACION_MONTAJE == 270)
            { int16_t t = ax_map; ax_map = ay_map; ay_map = -t; }
        #endif

        if (az_map >= 2000)       signo_az = +1;
        else if (az_map <= -2000) signo_az = -1;

        ax_map = ax_map * signo_az;
        ay_map = ay_map * signo_az;

        int8_t delta_x = 0, delta_y = 0;

        if (ax_map >  ACC_THR) delta_x = (INVERT_X? +1 : -1);
        if (ax_map < -ACC_THR) delta_x = (INVERT_X? -1 : +1);

        if (ay_map >  ACC_THR) delta_y = (INVERT_Y? +1 : -1);
        if (ay_map < -ACC_THR) delta_y = (INVERT_Y? -1 : +1);

        if ((delta_x!=0 || delta_y!=0) && (retraso_paso==0)){
            int16_t nuevo_x = (int16_t)pos_x + delta_x;
            int16_t nuevo_y = (int16_t)pos_y + delta_y;

            if (nuevo_x < 0) nuevo_x = 0; if (nuevo_x > (ANCHO-1)) nuevo_x = (ANCHO-1);
            if (nuevo_y < 0) nuevo_y = 0; if (nuevo_y > (ALTO-1))  nuevo_y = (ALTO-1);

            if (nuevo_x!=pos_x || nuevo_y!=pos_y){
                pos_x = (uint8_t)nuevo_x; pos_y = (uint8_t)nuevo_y;
                actualizar_buf();
                enviar_matriz();
                retraso_paso = STEP_COOLDOWN_MS;
            }
        }

        if (boton_apretado_edge()){
            color_aleatorio();
            actualizar_buf();
            enviar_matriz();
        }

        _delay_ms(10);
        if (retraso_paso >= 10) retraso_paso -= 10; else retraso_paso = 0;
    }
}
