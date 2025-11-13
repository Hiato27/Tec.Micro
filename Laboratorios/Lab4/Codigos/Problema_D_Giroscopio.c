#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>
#include <avr/interrupt.h>

/* Pines */
#define LED_PORT PORTD
#define LED_DDR  DDRD
#define LED_PIN  PD2
#define LED_MASK (1<<LED_PIN)

#define BTN_PINREG PIND
#define BTN_DDR DDRD
#define BTN_PORT PORTD
#define BTN_PIN PD3

/* Matriz */
#define ANCHO 8
#define ALTO  8
#define N_LEDS (ANCHO*ALTO)
#define SERPENTINE 0
#define BRILLO_SHIFT 0

/* MPU6050 */
#define MPU_ADDR 0x68
#define REG_PWR1 0x6B
#define REG_SMPL 0x19
#define REG_CFG  0x1A
#define REG_GCFG 0x1B
#define REG_ACFG 0x1C
#define REG_AX_H 0x3B

#define ACC_THR 3000
#define STEP_COOLDOWN_MS 120
#define INVERT_X 0
#define INVERT_Y 0

static uint8_t matriz_rgb[N_LEDS][3];
static uint8_t pos_x=3, pos_y=3;
static uint8_t col_r=255, col_g=0, col_b=0;
static int16_t ax_off=0, ay_off=0;

/* Random y color */
static uint16_t sem_azar=0xACE1u;
static uint16_t azar16(void){
    uint16_t bit=((sem_azar>>0)^(sem_azar>>2)^(sem_azar>>3)^(sem_azar>>5))&1u;
    sem_azar=(sem_azar>>1)|(bit<<15);
    return sem_azar;
}
static void color_aleatorio(void){
    col_r=(uint8_t)(azar16()&0xFF);
    col_g=(uint8_t)(azar16()&0xFF);
    col_b=(uint8_t)(azar16()&0xFF);
    if(col_r<20 && col_g<20 && col_b<20) col_r+=40;
}

/* Mapeo y buffer */
static uint8_t idx_xy(uint8_t x, uint8_t y){
#if SERPENTINE
    if (y & 1) return (y*ANCHO)+(ANCHO-1-x);
    else       return (y*ANCHO)+x;
#else
    return (y*ANCHO)+x;
#endif
}
static void actualizar_buf(void){
    for(uint8_t i=0;i<N_LEDS;i++){ matriz_rgb[i][0]=0; matriz_rgb[i][1]=0; matriz_rgb[i][2]=0; }
    uint8_t k=idx_xy(pos_x,pos_y);
    matriz_rgb[k][0]=col_g>>BRILLO_SHIFT;
    matriz_rgb[k][1]=col_r>>BRILLO_SHIFT;
    matriz_rgb[k][2]=col_b>>BRILLO_SHIFT;
}

/* WS2812 */
static inline void ws2812_send_byte(uint8_t dato){
    asm volatile(
        "ldi  r18, 8\n\t"
        "1:\n\t"
        "sbi  %[port], %[bit]\n\t"
        "sbrs %[dato], 7\n\t"
        "cbi  %[port], %[bit]\n\t"
        "nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t""nop\n\t"
        "cbi  %[port], %[bit]\n\t"
        "lsl  %[dato]\n\t"
        "dec  r18\n\t"
        "brne 1b\n\t"
        : [dato] "+r"(dato)
        : [port] "I" (_SFR_IO_ADDR(PORTD)), [bit] "I"(LED_PIN)
        : "r18"
    );
}
static void enviar_matriz(void){
    uint8_t s=SREG; cli();
    for(uint8_t i=0;i<N_LEDS;i++){
        ws2812_send_byte(matriz_rgb[i][0]);
        ws2812_send_byte(matriz_rgb[i][1]);
        ws2812_send_byte(matriz_rgb[i][2]);
    }
    SREG=s;
    _delay_us(70);
}

/* Botón */
static bool boton_edge(void){
    static uint8_t u=1;
    uint8_t a=(BTN_PINREG&(1<<BTN_PIN))?1:0;
    bool fl=(u==1 && a==0);
    u=a;
    if(fl) _delay_ms(25);
    return fl && ((BTN_PINREG&(1<<BTN_PIN))==0);
}

/* I2C HW */
static void i2c_init_100k(void){ TWSR=0x00; TWBR=72; }
static void i2c_start(uint8_t drw){ TWCR=(1<<TWINT)|(1<<TWSTA)|(1<<TWEN); while(!(TWCR&(1<<TWINT))); TWDR=drw; TWCR=(1<<TWINT)|(1<<TWEN); while(!(TWCR&(1<<TWINT))); }
static void i2c_write(uint8_t d){ TWDR=d; TWCR=(1<<TWINT)|(1<<TWEN); while(!(TWCR&(1<<TWINT))); }
static uint8_t i2c_read_ack(void){ TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWEA); while(!(TWCR&(1<<TWINT))); return TWDR; }
static uint8_t i2c_read_nack(void){ TWCR=(1<<TWINT)|(1<<TWEN); while(!(TWCR&(1<<TWINT))); return TWDR; }
static void i2c_stop(void){ TWCR=(1<<TWINT)|(1<<TWEN)|(1<<TWSTO); }

/* MPU */
static void mpu_write(uint8_t reg,uint8_t val){ i2c_start((MPU_ADDR<<1)|0); i2c_write(reg); i2c_write(val); i2c_stop(); }
static void mpu_read_multi(uint8_t reg,uint8_t*buf,uint8_t n){
    i2c_start((MPU_ADDR<<1)|0); i2c_write(reg);
    i2c_start((MPU_ADDR<<1)|1);
    for(uint8_t i=0;i<n;i++) buf[i]=(i<(n-1))?i2c_read_ack():i2c_read_nack();
    i2c_stop();
}
static void mpu_init(void){
    _delay_ms(100);
    mpu_write(REG_PWR1,0x00);
    _delay_ms(10);
    mpu_write(REG_CFG, 0x03);
    mpu_write(REG_SMPL,0x07);
    mpu_write(REG_GCFG,0x00);
    mpu_write(REG_ACFG,0x00);
}
static void mpu_read_accel(int16_t*ax,int16_t*ay,int16_t*az){
    uint8_t b[6]; mpu_read_multi(REG_AX_H,b,6);
    *ax=(int16_t)((b[0]<<8)|b[1]);
    *ay=(int16_t)((b[2]<<8)|b[3]);
    *az=(int16_t)((b[4]<<8)|b[5]);
}
static void mpu_calibrar(uint16_t n){
    int32_t sx=0, sy=0;
    for(uint16_t i=0;i<n;i++){ int16_t ax,ay,az; mpu_read_accel(&ax,&ay,&az); sx+=ax; sy+=ay; _delay_ms(5); }
    ax_off=(int16_t)(sx/(int32_t)n); ay_off=(int16_t)(sy/(int32_t)n);
}

int main(void){
    LED_DDR |= LED_MASK; LED_PORT &= ~LED_MASK;
    BTN_DDR &= ~(1<<BTN_PIN); BTN_PORT |= (1<<BTN_PIN);

    i2c_init_100k(); mpu_init(); mpu_calibrar(150);

    actualizar_buf(); enviar_matriz();

    uint16_t cooldown=0;
    while(1){
        int16_t ax,ay,az; mpu_read_accel(&ax,&ay,&az);
        ax-=ax_off; ay-=ay_off;

        int8_t dx=0, dy=0;
        if(ax> ACC_THR) dx = (INVERT_X? -1:+1);
        if(ax<-ACC_THR) dx = (INVERT_X? +1:-1);
        if(ay> ACC_THR) dy = (INVERT_Y? +1:-1);
        if(ay<-ACC_THR) dy = (INVERT_Y? -1:+1);

        if((dx||dy) && cooldown==0){
            int16_t nx=(int16_t)pos_x+dx, ny=(int16_t)pos_y+dy;
            if(nx<0) nx=0; if(nx>ANCHO-1) nx=ANCHO-1;
            if(ny<0) ny=0; if(ny>ALTO-1)  ny=ALTO-1;
            if(nx!=pos_x || ny!=pos_y){ pos_x=(uint8_t)nx; pos_y=(uint8_t)ny; actualizar_buf(); enviar_matriz(); cooldown=STEP_COOLDOWN_MS; }
        }

        if(boton_edge()){ color_aleatorio(); actualizar_buf(); enviar_matriz(); }

        _delay_ms(10);
        if(cooldown>=10) cooldown-=10; else cooldown=0;
    }
}
