// Control de motor, PWM, DHT, buzzer y comunicación I2C

// Librerías
#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// Definición de pines
#define DHT_PIN         PD4        // Pin para el sensor DHT
#define BTN_CCW         PD3        // Pin para el botón de sentido antihorario
#define BTN_BUZZ        PD7        // Pin para el botón de buzzer
#define BTN_CW_BIT      PB0        // Pin para el botón de sentido horario

#define I2C_FREQ        100000UL   // Frecuencia de comunicación I2C
#define SLAVE_ADDR      0x12       // Dirección del dispositivo esclavo
#define LCD_ADDR        0x27       // Dirección del LCD I2C

// Inicializa el ADC para lectura de 10 bits
static inline void adc_init(void){
    ADMUX  = (1<<REFS0);              // Selecciona la referencia de voltaje AVcc
    ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1);  // Habilita el ADC y establece el prescaler
}

// Lee un valor ADC de 10 bits del canal especificado
static inline uint16_t adc_read(uint8_t ch){
    ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);   // Selecciona el canal ADC
    ADCSRA |= (1<<ADSC);                   // Inicia la conversión
    while (ADCSRA & (1<<ADSC));             // Espera a que la conversión termine
    return ADC;                            // Retorna el valor ADC de 10 bits
}

// Mapea el valor ADC de 10 bits a un valor de 8 bits para PWM
static inline uint8_t map_10b_to_8b(uint16_t v){ return (uint8_t)((v*255UL)/1023UL); }

// Configura el pin del sensor DHT como salida
static inline void dht_pin_output(void){ DDRD |=  (1<<DHT_PIN); }

// Configura el pin del sensor DHT como entrada con resistencia pull-up
static inline void dht_pin_input_pullup(void){ DDRD &= ~(1<<DHT_PIN); PORTD |= (1<<DHT_PIN); }

// Lee la humedad y temperatura del sensor DHT
static uint8_t dht_read(uint8_t *hum, uint8_t *temp){
    uint8_t data[5] = {0};  // Buffer para almacenar los datos leídos
    dht_pin_output();       // Configura el pin como salida
    PORTD &= ~(1<<DHT_PIN); // Inicia la señal de inicio
    _delay_ms(18);          // Espera 18 ms
    PORTD |= (1<<DHT_PIN);  // Termina la señal de inicio
    _delay_us(30);          // Espera 30 us
    dht_pin_input_pullup(); // Configura el pin como entrada con pull-up

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
            data[i / 8] |= (1 << (7 - (i % 8)));  // Asigna el bit al byte correspondiente
            t = 0; while ((PIND & (1<<DHT_PIN))) { if (++t > 20000) break; }
        }
    }
    
    // Verifica la suma de los 4 primeros bytes con el checksum
    uint8_t sum = data[0] + data[1] + data[2] + data[3];
    if (sum != data[4]) return 0;  // Si el checksum no es correcto, retorna error

    *hum  = data[0];  // Asigna la humedad
    *temp = data[2];  // Asigna la temperatura
    return 1;         // Retorna éxito
}

// Configura los pines de entrada (botones)
static inline void io_init_inputs(void){
    DDRD  &= ~((1<<BTN_CCW) | (1<<BTN_BUZZ));  // Configura los pines de los botones como entradas
    PORTD |=  ((1<<BTN_CCW) | (1<<BTN_BUZZ));  // Activa las resistencias pull-up
    DDRB  &= ~(1<<BTN_CW_BIT);                  // Configura el pin BTN_CW_BIT como entrada
    PORTB |=  (1<<BTN_CW_BIT);                  // Activa la resistencia pull-up en BTN_CW_BIT
    dht_pin_input_pullup();                     // Configura el pin DHT como entrada con pull-up
}

// Inicializa la comunicación I2C
static inline void I2C_Init(void){
    TWSR = 0x00;                              // Configura el prescaler de I2C
    TWBR = (uint8_t)((F_CPU / I2C_FREQ - 16) / 2); // Calcula el valor para el registro de velocidad
    TWCR = (1<<TWEN);                         // Habilita la comunicación I2C
}

// Inicia una transmisión I2C
static inline void I2C_Start(void){
    TWCR = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN); // Envia la señal de inicio
    while(!(TWCR & (1<<TWINT)));                 // Espera a que la transmisión termine
}

// Detiene una transmisión I2C
static inline void I2C_Stop(void){
    TWCR = (1<<TWINT) | (1<<TWSTO) | (1<<TWEN);  // Envia la señal de parada
}

// Escribe un byte de datos por I2C
static inline void I2C_Write(uint8_t b){
    TWDR = b;                                  // Carga el dato a transmitir
    TWCR = (1<<TWINT) | (1<<TWEN);              // Inicia la transmisión
    while(!(TWCR & (1<<TWINT)));                 // Espera a que la transmisión termine
}

// Escribe un valor en el registro de un dispositivo I2C
static inline void i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t val){
    I2C_Start();
    I2C_Write((addr7 << 1) | 0);  // Envía la dirección del dispositivo
    I2C_Write(reg);               // Envía el registro
    I2C_Write(val);               // Envía el valor
    I2C_Stop();
}

// Definición de los pines del LCD
#define LCD_RS 0x01
#define LCD_RW 0x02
#define LCD_E  0x04
#define LCD_BL 0x08

// Función para enviar un byte al LCD
static inline void LCD_I2C_Send(uint8_t x){ I2C_Write(x); }

// Genera un pulso de control para el LCD
static void LCD_Pulse(uint8_t data){
    LCD_I2C_Send(data | LCD_E);   _delay_us(1);  // Pulso de habilitación
    LCD_I2C_Send(data & ~LCD_E);  _delay_us(50); // Finaliza el pulso
}

// Escribe un nibble de 4 bits en el LCD
static void LCD_Write4(uint8_t nibble, uint8_t rs){
    uint8_t base = (nibble & 0xF0) | LCD_BL | (rs ? LCD_RS : 0);
    I2C_Start(); I2C_Write(LCD_ADDR << 1);
    LCD_Pulse(base);  // Pulso de escritura
    I2C_Stop();
}

// Envía un byte de datos o comando al LCD
static void LCD_SendByte(uint8_t data, uint8_t rs){
    uint8_t high_n = data & 0xF0;
    uint8_t low_n  = (data << 4) & 0xF0;
    I2C_Start(); I2C_Write(LCD_ADDR << 1);
    LCD_Pulse(high_n | LCD_BL | (rs ? LCD_RS : 0));
    LCD_Pulse(low_n  | LCD_BL | (rs ? LCD_RS : 0));
    I2C_Stop();
}

// Envia un comando al LCD
static inline void LCD_Cmd(uint8_t c){ LCD_SendByte(c, 0); }

// Limpia la pantalla del LCD
static inline void LCD_Clear(void){ LCD_Cmd(0x01); _delay_ms(2); }

// Inicializa el LCD
static void LCD_Init(void){
    _delay_ms(50);               // Espera para estabilidad
    LCD_Write4(0x30, 0); _delay_ms(5);    // Inicializa el LCD en modo de 8 bits
    LCD_Write4(0x30, 0); _delay_us(150);
    LCD_Write4(0x30, 0); _delay_us(150);
    LCD_Write4(0x20, 0); _delay_us(80);  // Configura el LCD en modo de 4 bits

    LCD_Cmd(0x28);               // Configura el LCD (modo de 2 líneas, 5x8)
    LCD_Cmd(0x0C);               // Activa la visualización
    LCD_Cmd(0x06);               // Establece el cursor para que se mueva a la derecha
    LCD_Clear();                 // Limpia la pantalla
}

// Establece el cursor en la posición especificada
static inline void LCD_SetCursor(uint8_t row, uint8_t col){
    uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col); // Calcula la dirección de memoria
    LCD_Cmd(addr);                // Envia el comando para mover el cursor
}

// Imprime una cadena de caracteres en el LCD
static void LCD_Print(const char* s){ while(*s) LCD_SendByte(*s++, 1); }

// Imprime una cadena de caracteres con espacios a la derecha en el LCD
static void LCD_PrintPadded(const char* s, uint8_t width){
    uint8_t i = 0;
    while (*s && i < width) { LCD_SendByte(*s++, 1); i++; }
    while (i < width) { LCD_SendByte(' ', 1); i++; }
}

// Función principal
int main(void){
    I2C_Init();        // Inicializa I2C
    LCD_Init();        // Inicializa LCD
    adc_init();        // Inicializa ADC
    io_init_inputs();  // Inicializa entradas

    LCD_Clear();       // Limpia la pantalla
    LCD_Print("I2C Maestro OK");  // Muestra mensaje de inicio
    LCD_SetCursor(1, 0);
    LCD_Print("Inicializando..."); // Muestra mensaje de inicialización
    _delay_ms(800);

    uint8_t hum = 0, temp = 0;
    uint8_t dir = 0;
    uint8_t pwm_led = 0;
    uint8_t buzz_on = 0;

    uint16_t ui_ms = 0;
    uint8_t last_dir = 255, last_pwm = 255, last_t = 255, last_h = 255, last_b = 255;

    while (1){
        uint8_t cw  = (PINB & (1<<BTN_CW_BIT)) ? 0 : 1;
        uint8_t ccw = (PIND & (1<<BTN_CCW))    ? 0 : 1;
        buzz_on      = (PIND & (1<<BTN_BUZZ))   ? 0 : 1;

        if (cw && !ccw)      dir = 1;
        else if (ccw && !cw) dir = 2;
        else                 dir = 0;

        pwm_led = map_10b_to_8b(adc_read(0));

        static uint16_t tick = 0;
        if (++tick >= 500){
            tick = 0;
            (void)dht_read(&hum, &temp);  // Lee la humedad y temperatura del DHT
        }

        if (dir != last_dir){
            i2c_write_reg(SLAVE_ADDR, 0x01, dir);  // Envia la dirección del motor
            last_dir = dir;
        }

        if (last_pwm == 255 || (pwm_led > last_pwm + 3) || (pwm_led + 3 < last_pwm)){
            i2c_write_reg(SLAVE_ADDR, 0x02, pwm_led);  // Ajusta el PWM del motor
            last_pwm = pwm_led;
        }

        if (buzz_on != last_b){
            i2c_write_reg(SLAVE_ADDR, 0x04, buzz_on ? 1 : 0);  // Enciende o apaga el buzzer
            last_b = buzz_on;
        }

        ui_ms += 2;
        uint8_t need_update = 0;
        if (dir != last_dir || pwm_led != last_pwm || temp != last_t || hum != last_h || buzz_on != last_b){
            need_update = 1;
        }
        if (ui_ms >= 100 || need_update){
            ui_ms = 0;
            last_t = temp; last_h = hum;
            char line[17];

            LCD_SetCursor(0,0);
            snprintf(line, sizeof(line), "T:%2uC H:%2u%%", temp, hum);  // Muestra la temperatura y humedad
            LCD_PrintPadded(line, 16);

            LCD_SetCursor(1,0);
            const char* dstr = (dir == 1) ? "H" : (dir == 2) ? "AH" : "STOP";
            snprintf(line, sizeof(line), "M:%s L:%3u B:%c", dstr, pwm_led, buzz_on ? '1' : '0');
            LCD_PrintPadded(line, 16);
        }

        _delay_ms(2);  // Delay de 2 ms
    }
}
