// Inicialización y control de SPI, ADC, I/O y LCD con botones

// Librerías
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// Pines de entrada y salida
#define DHT_PIN         PD4        // Pin para sensor DHT
#define BTN_CCW         PD3        // Pin para botón sentido antihorario
#define BTN_BUZZ        PD7        // Pin para botón de buzzer
#define BTN_CW_BIT      PB0        // Pin para botón sentido horario

#define SS_PIN          PB2        // Pin de selección de esclavo SPI
#define MOSI_PIN        PB3        // Pin de Master Out Slave In
#define MISO_PIN        PB4        // Pin de Master In Slave Out
#define SCK_PIN         PB5        // Pin de reloj SPI

// Inicializa SPI en modo maestro
static inline void spi_master_init(void) {
    DDRB |= (1<<MOSI_PIN) | (1<<SCK_PIN) | (1<<SS_PIN);   // Configura pines MOSI, SCK y SS como salida
    DDRB &= ~(1<<MISO_PIN);                                // Configura MISO como entrada
    SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0);               // Habilita SPI, modo maestro, divisor de velocidad
    SPSR = 0;                                              // Configura el SPI status register
    PORTB |= (1<<SS_PIN);                                   // Habilita el pin SS (selección de esclavo)
}

// Función para transferir un byte por SPI
static inline uint8_t spi_transfer(uint8_t data) {
    SPDR = data;                                           // Carga el dato a transmitir
    while (!(SPSR & (1<<SPIF)));                            // Espera a que se complete la transferencia
    return SPDR;                                           // Retorna el dato recibido
}

// Envía un paquete de datos SPI: comando, valor y checksum
static inline void spi_send_packet(uint8_t cmd, uint8_t val) {
    const uint8_t SYNC = 0xAA;                             // Byte sincronizador
    uint8_t chk = SYNC ^ cmd ^ val;                        // Calcula el checksum
    PORTB &= ~(1<<SS_PIN);                                  // Desactiva el pin SS
    spi_transfer(SYNC);                                     // Envia sincronización
    spi_transfer(cmd);                                      // Envia el comando
    spi_transfer(val);                                      // Envia el valor
    spi_transfer(chk);                                      // Envia el checksum
    PORTB |= (1<<SS_PIN);                                   // Reactiva el pin SS
}

// Inicializa el ADC para lectura de 10 bits
static inline void adc_init(void){
    ADMUX  = (1<<REFS0);                                    // Selecciona la referencia de voltaje AVcc
    ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1);           // Habilita ADC y configura prescaler
}

// Lee un valor ADC de 10 bits del canal especificado
static inline uint16_t adc_read(uint8_t ch){
    ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);                    // Selecciona el canal ADC
    ADCSRA |= (1<<ADSC);                                    // Inicia la conversión ADC
    while (ADCSRA & (1<<ADSC));                              // Espera a que la conversión termine
    return ADC;                                             // Retorna el valor ADC de 10 bits
}

// Mapea un valor de 10 bits a un valor de 8 bits (para PWM)
static inline uint8_t map_10b_to_8b(uint16_t v){
    return (uint8_t)((v*255UL)/1023UL);                      // Convierte de 10 bits a 8 bits
}

// Configura el pin del sensor DHT como salida
static inline void dht_pin_output(void){
    DDRD |=  (1<<DHT_PIN);                                  // Configura DHT_PIN como salida
}

// Configura el pin del sensor DHT como entrada con resistencia pull-up
static inline void dht_pin_input_pullup(void){
    DDRD &= ~(1<<DHT_PIN);                                  // Configura DHT_PIN como entrada
    PORTD |= (1<<DHT_PIN);                                  // Activa la resistencia pull-up
}

// Lee la humedad y temperatura del sensor DHT
static uint8_t dht_read(uint8_t *hum, uint8_t *temp){
    uint8_t data[5] = {0};                                   // Buffer para almacenar los datos leídos
    dht_pin_output();                                         // Configura el pin como salida
    PORTD &= ~(1<<DHT_PIN);                                   // Inicia la señal de inicio
    _delay_ms(18);                                            // Espera 18 ms
    PORTD |= (1<<DHT_PIN);                                    // Termina la señal de inicio
    _delay_us(30);                                            // Espera 30 us

    dht_pin_input_pullup();                                   // Configura el pin como entrada con pull-up
    uint16_t t = 0;
    
    // Sincroniza el comienzo de la transmisión de datos
    while ((PIND & (1<<DHT_PIN))) { if (++t > 20000) return 0; }
    t = 0; while (!(PIND & (1<<DHT_PIN))) { if (++t > 20000) return 0; }
    t = 0; while ((PIND & (1<<DHT_PIN))) { if (++t > 20000) return 0; }

    // Lee los 40 bits de datos
    for (uint8_t i = 0; i < 40; i++){
        t = 0; while (!(PIND & (1<<DHT_PIN))) { if (++t > 20000) return 0; }
        _delay_us(30);
        if (PIND & (1<<DHT_PIN)){
            data[i/8] |= (1<<(7-(i%8)));                         // Asigna el bit al byte correspondiente
            t = 0; while ((PIND & (1<<DHT_PIN))) { if (++t > 20000) break; }
        }
    }
    
    // Verifica la suma de los 4 primeros bytes con el checksum
    uint8_t sum = data[0] + data[1] + data[2] + data[3];
    if (sum != data[4]) return 0;                             // Si el checksum es incorrecto, retorna error

    *hum  = data[0];                                          // Asigna la humedad
    *temp = data[2];                                          // Asigna la temperatura
    return 1;                                                 // Retorna éxito
}

// Inicializa los pines de entrada (botones)
static inline void io_init_inputs(void){
    DDRD  &= ~((1<<BTN_CCW) | (1<<BTN_BUZZ));                  // Configura los pines de los botones como entradas
    PORTD |=  ((1<<BTN_CCW) | (1<<BTN_BUZZ));                  // Activa las resistencias pull-up en los botones
    DDRB  &= ~(1<<BTN_CW_BIT);                                 // Configura el pin BTN_CW_BIT como entrada
    PORTB |=  (1<<BTN_CW_BIT);                                 // Activa la resistencia pull-up en BTN_CW_BIT
    dht_pin_input_pullup();                                    // Configura el pin DHT como entrada con pull-up
}

// Inicializa la comunicación I2C
#define LCD_ADDR 0x27
static void I2C_Init(void) {
    TWSR = 0x00;                                              // Configura el prescaler de I2C
    TWBR = 0x48;                                              // Configura el valor del registro de control de velocidad
    TWCR = (1 << TWEN);                                        // Habilita I2C
}

// Inicia una transmisión I2C
static void I2C_Start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);          // Envia una señal de inicio de I2C
    while (!(TWCR & (1 << TWINT)));                             // Espera a que se complete
}

// Detiene una transmisión I2C
static void I2C_Stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);          // Envia una señal de parada de I2C
    _delay_us(100);                                            // Espera a que se complete
}

// Escribe un byte de datos por I2C
static void I2C_Write(uint8_t data) {
    TWDR = data;                                               // Carga el dato a transmitir
    TWCR = (1 << TWINT) | (1 << TWEN);                          // Inicia la transmisión
    while (!(TWCR & (1 << TWINT)));                             // Espera a que se complete
}

// Envia un byte a LCD a través de I2C
static void LCD_SendByte(uint8_t data, uint8_t mode) {
    uint8_t high_n = data & 0xF0;
    uint8_t low_n  = (data << 4) & 0xF0;

    uint8_t d_h = high_n | mode | 0x08;
    uint8_t d_l = low_n  | mode | 0x08;

    I2C_Start();
    I2C_Write(LCD_ADDR << 1);
    
    I2C_Write(d_h | 0x04); _delay_us(1);
    I2C_Write(d_h);        _delay_us(50);

    I2C_Write(d_l | 0x04); _delay_us(1);
    I2C_Write(d_l);        _delay(50);

    I2C_Stop();
}

// Main function
int main(void){
    spi_master_init();  // Inicializa SPI
    adc_init();         // Inicializa ADC
    io_init_inputs();   // Inicializa entradas
    I2C_Init();         // Inicializa I2C
    LCD_Init();         // Inicializa LCD

    LCD_Clear();        // Limpia LCD
    LCD_Print("SPI Maestro OK");  // Muestra mensaje
    LCD_SetCursor(1,0);           // Establece cursor
    LCD_Print("Inicializando..."); // Muestra mensaje de inicialización
    _delay_ms(800);              // Espera 800 ms

    // Variables de temperatura y humedad
    uint8_t hum = 0, temp = 0;
    uint8_t dir = 0;
    uint8_t pwm_led = 0;
    uint8_t buzz_on = 0;
    
    uint16_t ui_ms = 0;
    uint8_t last_dir = 255, last_pwm = 255, last_t = 255, last_h = 255, last_b = 255;

    while (1){
        uint8_t cw   = (PINB & (1<<BTN_CW_BIT)) ? 0 : 1;    // Lee el estado del botón CW
        uint8_t ccw  = (PIND & (1<<BTN_CCW))    ? 0 : 1;    // Lee el estado del botón CCW
        buzz_on      = (PIND & (1<<BTN_BUZZ))    ? 0 : 1;    // Lee el estado del buzzer

        // Determina la dirección de rotación
        if (cw && !ccw)      dir = 1;
        else if (ccw && !cw) dir = 2;
        else                 dir = 0;

        pwm_led = map_10b_to_8b(adc_read(0));   // Mapea el valor de ADC a 8 bits para PWM

        // Lee la humedad y temperatura cada 500 ms
        static uint16_t tick=0;
        if (++tick >= 500){
            tick=0;
            (void)dht_read(&hum, &temp);  // Lee DHT
        }

        // Envía los datos al dispositivo SPI
        spi_send_packet(0x01, dir);
        _delay_us(200);
        spi_send_packet(0x02, pwm_led);
        _delay_us(200);
        spi_send_packet(0x04, buzz_on ? 1 : 0);

        // Actualiza la interfaz de usuario cada 100 ms o cuando hay cambios
        ui_ms += 2;
        uint8_t need_update = 0;
        if (dir != last_dir || pwm_led != last_pwm || temp != last_t || hum != last_h || buzz_on != last_b) {
            need_update = 1;
        }

        if (ui_ms >= 100 || need_update) {
            ui_ms = 0;
            last_dir = dir; last_pwm = pwm_led; last_t = temp; last_h = hum; last_b = buzz_on;

            char line[17];
            LCD_SetCursor(0,0);
            snprintf(line, sizeof(line), "T:%2uC H:%2u%%", temp, hum);  // Muestra la temperatura y humedad
            LCD_PrintPadded(line, 16);

            LCD_SetCursor(1,0);
            const char* dstr = (dir==1)?"H":(dir==2)?"AH":"STOP";
            snprintf(line, sizeof(line), "M:%s L:%3u B:%c", dstr, pwm_led, buzz_on?'1':'0');
            LCD_PrintPadded(line, 16);
        }

        _delay_ms(2); // Delay para no saturar el ciclo
    }
}
