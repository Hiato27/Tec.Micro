// CONFIGURACIÓN BÁSICA 
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>

// DEFINICIÓN DE PINES 
#define LED_PORT     PORTD
#define LED_DDR      DDRD
#define LED_PINREG   PIND
#define LED_PIN      PD2
#define LED_MASK     (1<<LED_PIN)

// Botón para cambiar el color del pixel
#define BTN_PINREG   PIND
#define BTN_DDR      DDRD
#define BTN_PORT     PORTD
#define BTN_PIN      PD3

// PARÁMETROS DE MATRIZ 
#define ANCHO        8              // Ancho de la matriz
#define ALTO         8              // Alto de la matriz
#define N_LEDS      (ANCHO*ALTO)    // Número total de LEDs
#define SERPENTINE   0              // 0 = mapeo lineal, 1 = serpentino

#define BRILLO_SHIFT 0              // Shift para bajar brillo (0 = máximo)

// REGISTROS MPU6050 (I2C) 
#define MPU_ADDR   0x68
#define REG_PWR1   0x6B
#define REG_SMPL   0x19
#define REG_CFG    0x1A
#define REG_GCFG   0x1B
#define REG_ACFG   0x1C
#define REG_AX_H   0x3B      // Primer registro de acelerómetro (AX high)

// UMBRALES Y AJUSTES
#define ACC_THR          3000   // Umbral de aceleración para mover el pixel
#define STEP_COOLDOWN_MS 120    // Tiempo de espera entre pasos (para no "volar")

// Inversión de ejes
#define INVERT_X 0
#define INVERT_Y 0

// Rotación física del módulo MPU respecto a la matriz
#define ROTACION_MONTAJE 0

// ESTADO DE LA MATRIZ Y COLOR
// Buffer de colores para cada LED [G,R,B]
static uint8_t matriz_rgb[N_LEDS][3];

// Posición actual del "pixel" encendido
static uint8_t pos_x = 3;
static uint8_t pos_y = 3;

// Color actual del pixel (RGB)
static uint8_t col_rojo = 255, col_verde = 0, col_azul = 0;

// Offsets (sesgos) para calibrar el acelerómetro en X e Y
static int16_t ax_sesgo = 0, ay_sesgo = 0;

// GENERADOR DE NÚMEROS ALEATORIOS 
// LFSR de 16 bits para generar pseudo-aleatorios
static uint16_t sem_azar = 0xACE1u;
static uint16_t azar16(void){
    uint16_t bit = ((sem_azar >> 0u) ^ (sem_azar >> 2u) ^ (sem_azar >> 3u) ^ (sem_azar >> 5u)) & 1u;
    sem_azar = (sem_azar >> 1u) | (bit << 15u);
    return sem_azar;
}

// Elige un color aleatorio no demasiado oscuro
static void color_aleatorio(void){
    uint16_t r_ale = azar16(), g_ale = azar16(), b_ale = azar16();
    col_rojo  = (uint8_t)(r_ale & 0xFF);
    col_verde = (uint8_t)(g_ale & 0xFF);
    col_azul  = (uint8_t)(b_ale & 0xFF);

    // Evita colores demasiado cercanos a negro
    if(col_rojo < 20 && col_verde < 20 && col_azul < 20) col_rojo += 40;
}

// MAPEO (x,y)
static uint8_t idx_xy(uint8_t x, uint8_t y){
#if SERPENTINE
    // Mapeo serpentino: filas pares en un sentido, impares en el otro
    if (y & 1) return (y * ANCHO) + (ANCHO - 1 - x);
    else       return (y * ANCHO) + x;
#else
    // Mapeo lineal: fila por fila de izquierda a derecha
    return (y * ANCHO) + x;
#endif
}

// Rellena el buffer con todo apagado excepto el pixel actual (pos_x,pos_y)
static void actualizar_buf(void){
    // Apaga todos los LEDs
    for(uint8_t i=0; i<N_LEDS; i++){
        matriz_rgb[i][0] = 0;
        matriz_rgb[i][1] = 0;
        matriz_rgb[i][2] = 0;
    }
    // Enciende solo el LED en la posición del "pixel"
    uint8_t indice = idx_xy(pos_x, pos_y);
    // WS2812 usa formato GRB
    matriz_rgb[indice][0] = col_verde >> BRILLO_SHIFT;
    matriz_rgb[indice][1] = col_rojo  >> BRILLO_SHIFT;
    matriz_rgb[indice][2] = col_azul  >> BRILLO_SHIFT;
}

// I2C 
static void i2c_init_100k(void) {
    TWSR = 0x00;   // Prescaler = 1
    TWBR = 72;     // ~100 kHz para F_CPU=16MHz
}

// Envia condición START + dirección (R/W incluido)
static void i2c_start(uint8_t direccion_rw) {
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);        // START
    while (!(TWCR & (1<<TWINT)));
    TWDR = direccion_rw;                           // Dirección + bit R/W
    TWCR = (1<<TWINT)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
}

// Envia un byte por I2C
static void i2c_write(uint8_t dato) {
    TWDR = dato;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
}

// Lee un byte y responde ACK (queda listo para seguir leyendo)
static uint8_t i2c_read_ack(void) {
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
    while (!(TWCR & (1<<TWINT)));
    return TWDR;
}

// Lee un byte y responde NACK (último byte)
static uint8_t i2c_read_nack(void) {
    TWCR = (1<<TWINT)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT)));
    return TWDR;
}

// Envia condición STOP
static void i2c_stop(void) {
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
}

// FUNCIONES MPU6050

// Escribe un registro del MPU6050
static void mpu_write(uint8_t reg, uint8_t valor){
    i2c_start((MPU_ADDR<<1)|0);  // Write
    i2c_write(reg);
    i2c_write(valor);
    i2c_stop();
}

// Lee varios registros consecutivos del MPU6050
static void mpu_read_multi(uint8_t reg_inicio, uint8_t *buffer, uint8_t longitud){
    i2c_start((MPU_ADDR<<1)|0);  // Write: selecciona registro inicial
    i2c_write(reg_inicio);
    i2c_start((MPU_ADDR<<1)|1);  // Read
    for(uint8_t i=0;i<longitud;i++){
        buffer[i] = (i < (longitud-1)) ? i2c_read_ack() : i2c_read_nack();
    }
    i2c_stop();
}

// Inicializa el MPU6050: saca del sleep, configura filtros y rangos básicos
static void mpu_init(void){
    _delay_ms(100);
    mpu_write(REG_PWR1, 0x00);  // Quita sleep
    _delay_ms(10);
    mpu_write(REG_CFG,  0x03);  // Filtro DLPF
    mpu_write(REG_SMPL, 0x07);  // Sample rate
    mpu_write(REG_GCFG, 0x00);  // ±250°/s
    mpu_write(REG_ACFG, 0x00);  // ±2g
}

// Lee aceleraciones crudas en X, Y, Z
static void mpu_read_accel(int16_t *ax, int16_t *ay, int16_t *az){
    uint8_t crudo[6];
    mpu_read_multi(REG_AX_H, crudo, 6);
    *ax = (int16_t)((crudo[0]<<8) | crudo[1]);
    *ay = (int16_t)((crudo[2]<<8) | crudo[3]);
    *az = (int16_t)((crudo[4]<<8) | crudo[5]);
}

// Calcula offsets promediando varias lecturas con el módulo quieto
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

// WS2812: ENVÍO DE BITS POR ASM
// Envia un byte a una tira WS2812 usando temporización por ensamblador
static inline void ws2812_send_byte(uint8_t dato) {
    asm volatile(
        "ldi  r18, 8                 \n\t" // Contador de bits = 8
        "1:                          \n\t"
        "sbi  %[port], %[bit]        \n\t" // Sube el pin (inicio de bit)
        "sbrs %[dato], 7             \n\t" // ¿bit7 = 0?
        "cbi  %[port], %[bit]        \n\t" // si es 0, baja antes (T0)
        "nop\n\t""nop\n\t""nop\n\t""nop\n\t"  // relleno para ajustar tiempos
        "nop\n\t""nop\n\t""nop\n\t"
        "cbi  %[port], %[bit]        \n\t" // baja el pin (final del bit)
        "lsl  %[dato]                \n\t" // desplaza dato (siguiente bit al MSB)
        "dec  r18                    \n\t" // decrementa contador
        "brne 1b                     \n\t" // repite hasta 0
        : [dato] "+r" (dato)
        : [port] "I" (_SFR_IO_ADDR(PORTD)), [bit] "I" (LED_PIN)
        : "r18"
    );
}

// Recorre el buffer matriz_rgb y lo envía completo a la cadena WS2812
static void enviar_matriz(void){
    cli();  // Deshabilita interrupciones para no arruinar timings WS2812
    for(uint8_t i=0; i<N_LEDS; i++){
        ws2812_send_byte(matriz_rgb[i][0]);  // G
        ws2812_send_byte(matriz_rgb[i][1]);  // R
        ws2812_send_byte(matriz_rgb[i][2]);  // B
    }
    sei();  // Rehabilita interrupciones
    _delay_us(70);  // Tiempo de reset (>50 µs)
}

// BOTÓN CON DETECCIÓN DE FLANCO
// Devuelve true sólo cuando se detecta flanco de bajada (1->0) con simple debounce
static bool boton_apretado_edge(void){
    static uint8_t ultimo = 1;  // Estado anterior (1 = no apretado, pull-up)
    uint8_t ahora = (BTN_PINREG & (1<<BTN_PIN)) ? 1 : 0;
    bool flanco = (ultimo==1 && ahora==0);  // transicion de HIGH a LOW
    ultimo = ahora;
    if (flanco) { _delay_ms(25); }          // Pequeño debounce
    // Vuelve a comprobar que sigue bajo después del debounce
    return flanco && ((BTN_PINREG & (1<<BTN_PIN))==0);
}

// FUNCIÓN PRINCIPAL
int main(void){
    // LED debug como salida, apagado al inicio
    LED_DDR  |=  LED_MASK;
    LED_PORT &= ~LED_MASK;

    // Botón como entrada con pull-up
    BTN_DDR  &= ~(1<<BTN_PIN);
    BTN_PORT |=  (1<<BTN_PIN);

    // Inicializa I2C y MPU6050
    i2c_init_100k();
    mpu_init();

    // Calibración inicial (módulo quieto)
    mpu_calibrar_offsets(200);

    // Posición inicial del pixel (centro aproximado)
    pos_x = 3; pos_y = 3;
    col_rojo = 255; col_verde = 0; col_azul = 0;

    // Limpia la matriz (todo apagado) y muestra primer frame
    for(uint8_t i=0;i<N_LEDS;i++){ matriz_rgb[i][0]=matriz_rgb[i][1]=matriz_rgb[i][2]=0; }
    enviar_matriz();
    _delay_ms(10);
    actualizar_buf();
    enviar_matriz();

    uint16_t retraso_paso = 0;     // contador para cooldown entre pasos
    static int8_t signo_az = +1;   // signo según orientación (cara arriba/abajo)

    while(1){
        int16_t ax_leido, ay_leido, az_leido;
        mpu_read_accel(&ax_leido,&ay_leido,&az_leido);

        // Compensa sesgos calculados en la calibración
        ax_leido -= ax_sesgo;
        ay_leido -= ay_sesgo;

        int16_t ax_map = ax_leido;
        int16_t ay_map = ay_leido;
        int16_t az_map = az_leido;

        // Ajusta ejes según rotación física del MPU en la placa
        #if (ROTACION_MONTAJE == 90)
            { int16_t t = ax_map; ax_map = -ay_map; ay_map = t; }
        #elif (ROTACION_MONTAJE == 180)
            ax_map = -ax_map; ay_map = -ay_map;
        #elif (ROTACION_MONTAJE == 270)
            { int16_t t = ax_map; ax_map = ay_map; ay_map = -t; }
        #endif

        // Determina "arriba/abajo" según Z para que el movimiento sea coherente
        if (az_map >= 2000)       signo_az = +1;
        else if (az_map <= -2000) signo_az = -1;

        // Ajusta X,Y según orientación vertical
        ax_map = ax_map * signo_az;
        ay_map = ay_map * signo_az;

        int8_t delta_x = 0, delta_y = 0;

        // Decide movimiento en X según inclinación y umbral
        if (ax_map >  ACC_THR) delta_x = (INVERT_X? +1 : -1);
        if (ax_map < -ACC_THR) delta_x = (INVERT_X? -1 : +1);

        // Decide movimiento en Y según inclinación y umbral
        if (ay_map >  ACC_THR) delta_y = (INVERT_Y? +1 : -1);
        if (ay_map < -ACC_THR) delta_y = (INVERT_Y? -1 : +1);

        // Si se detecta movimiento y no estamos en cooldown de paso
        if ((delta_x!=0 || delta_y!=0) && (retraso_paso==0)){
            int16_t nuevo_x = (int16_t)pos_x + delta_x;
            int16_t nuevo_y = (int16_t)pos_y + delta_y;

            // Limita posición a los bordes de la matriz
            if (nuevo_x < 0) nuevo_x = 0;
            if (nuevo_x > (ANCHO-1)) nuevo_x = (ANCHO-1);
            if (nuevo_y < 0) nuevo_y = 0;
            if (nuevo_y > (ALTO-1))  nuevo_y = (ALTO-1);

            // Sólo actualiza si realmente cambió de casilla
            if (nuevo_x!=pos_x || nuevo_y!=pos_y){
                pos_x = (uint8_t)nuevo_x;
                pos_y = (uint8_t)nuevo_y;
                actualizar_buf();
                enviar_matriz();
                retraso_paso = STEP_COOLDOWN_MS;  // Arranca cooldown
            }
        }

        // Cambio de color cuando se detecta flanco en el botón
        if (boton_apretado_edge()){
            color_aleatorio();
            actualizar_buf();
            enviar_matriz();
        }

        // Temporización base del loop
        _delay_ms(10);

        // Decrementa cooldown de paso en bloques de 10 ms
        if (retraso_paso >= 10) retraso_paso -= 10;
        else                    retraso_paso = 0;
    }
}
