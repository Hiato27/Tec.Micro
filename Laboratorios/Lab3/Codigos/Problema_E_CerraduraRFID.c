#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <avr/eeprom.h>
#include <stdint.h>

//  Pines ADAPTADOS 

// LEDs

#define LED_VERDE_PIN     PD6 
#define LED_ROJO_PIN      PD7 
#define LED_VERDE_PORT    PORTD
#define LED_ROJO_PORT     PORTD
#define LED_VERDE_DDR     DDRD
#define LED_ROJO_DDR      DDRD

// Botones 
// BTN BORRAR: D3 (PD3) y BTN ACTUALIZAR: D2 (PD2)
#define BTN_BORRAR_PIN       PD3 
#define BTN_ACTUALIZAR_PIN   PD2 
#define BTN_PORT             PORTD
#define BTN_DDR              DDRD
#define BTN_PINREG           PIND
#define BTN_BORRAR_MASK      (1 << BTN_BORRAR_PIN)
#define BTN_ACTUALIZAR_MASK  (1 << BTN_ACTUALIZAR_PIN)

// SPI RC522
#define SS_PIN      PB2  // pin D10
#define MOSI_PIN    PB3  // pin D11
#define MISO_PIN    PB4  // pin D12
#define SCK_PIN     PB5  // pin D13
#define RST_PIN     PB1  // pin D9

// I2C: PC4=A4 (SDA), PC5=A5 (SCL) (Sin cambios)

// EEPROM
#define EEPROM_SIG0_ADDR    0x00    
#define EEPROM_SIG1_ADDR    0x01    
#define EEPROM_UID_ADDR     0x02    
#define EEPROM_BCC_ADDR     0x06    

// Estados 
typedef enum {
    STATE_IDLE,
    STATE_UPDATE_MODE
} SystemState;

// Globales 
volatile SystemState currentState = STATE_IDLE;
uint8_t storedCardID[4] = {0};
uint8_t currentCardID[4] = {0};
static uint8_t tarjeta_presente = 0;    // 0=espera llegada; 1=ya procesada 

// Prototipos 

static void GPIO_Init(void);
static void SPI_Init(void);
static void UART_Init(void);
static void I2C_Init(void);
static uint8_t SPI_Transfer(uint8_t data);
static void PCD_Init(void);
static uint8_t PICC_IsNewCardPresent(void);
static uint8_t PICC_ReadCardSerial(uint8_t* uid4);
static void PICC_HaltA(void);
static void RC522_AntennaOn(void);
static void RC522_Reset(void);
static void RC522_WriteRegister(uint8_t addr, uint8_t val);
static uint8_t RC522_ReadRegister(uint8_t addr);
static uint8_t RC522_Request(uint8_t reqMode, uint8_t* atqa);
static uint8_t RC522_Anticoll(uint8_t* uid4);
static uint8_t RC522_Transceive(uint8_t *sendData, uint8_t sendLen,
                                 uint8_t *backData, uint8_t *backLen,
                                 uint8_t rxAlign);
static uint8_t PCD_CalcCRC(uint8_t *data, uint8_t len, uint8_t *crcL, uint8_t *crcH);
static void LCD_Init(void);
static void LCD_Clear(void);
static void LCD_SetCursor(uint8_t row, uint8_t col);
static void LCD_Print(const char* str);
static void I2C_Start(void);
static void I2C_Stop(void);
static void I2C_Write(uint8_t data);
static void LCD_SendByte(uint8_t data, uint8_t mode);
static void UART_Transmit(uint8_t data);
static void UART_Print(const char* str);
static void UART_PrintUID(uint8_t* uid);
static void EEPROM_SaveCard(uint8_t* cardID);
static uint8_t EEPROM_LoadCard(uint8_t* cardID);
static void EEPROM_ClearCard(void);
static void LED_Verde_On(void);
static void LED_Verde_Off(void);
static void LED_Rojo_On(void);
static void LED_Rojo_Off(void);
static void Sistema_AccesoConcedido(void);
static void Sistema_AccesoDenegado(void);
static void Sistema_RegistrarTarjeta(uint8_t* cardID);
static void Sistema_BorrarTarjeta(void);
static uint8_t CompararTarjetas(uint8_t* id1, uint8_t* id2);
static uint8_t btn_pressed_once(uint8_t mask);

// RC522 Registros/Constantes 

#define RC522_REG_COMMAND        0x01
#define RC522_REG_COMIRQ         0x04
#define RC522_REG_DIVIRQ         0x05
#define RC522_REG_ERROR          0x06
#define RC522_REG_STATUS2        0x08
#define RC522_REG_FIFODATA       0x09
#define RC522_REG_FIFOLEVEL      0x0A
#define RC522_REG_BITFRAMING     0x0D
#define RC522_REG_MODE           0x11
#define RC522_REG_TXCONTROL      0x14
#define RC522_REG_TXASK          0x15  
#define RC522_REG_RFCFG          0x26  
#define RC522_REG_TMODE          0x2A
#define RC522_REG_TPRESCALER     0x2B
#define RC522_REG_TRELOAD_H      0x2C
#define RC522_REG_TRELOAD_L      0x2D
#define RC522_REG_CRCRESULTH     0x21
#define RC522_REG_CRCRESULTL     0x22

#define RC522_CMD_IDLE           0x00
#define RC522_CMD_CALCCRC        0x03
#define RC522_CMD_TRANSCEIVE     0x0C
#define RC522_CMD_SOFTRESET      0x0F

// PICC
#define PICC_REQIDL              0x26  
#define PICC_SEL_CL1             0x93
#define PICC_ANTICOLL_CL1        0x20
#define PICC_HALT                0x50

// LCD I2C
#define LCD_ADDR                 0x27  // ajusta donde el módulo es 0x3F


// Debounce 
static uint8_t btn_pressed_once(uint8_t mask)
{
    
    if (!(PIND & mask)) {
        _delay_ms(15);
        if (!(PIND & mask)) {
            while (!(PIND & mask)) { _delay_ms(1); }
            _delay_ms(15);
            return 1;
        }
    }
    return 0;
}

// MAIN 
int main(void)
{
    
    GPIO_Init();
    SPI_Init();
    UART_Init();
    I2C_Init();

    sei();

    _delay_ms(100);
    LCD_Init();
    PCD_Init();

    // Limpieza de EEPROM al encender 
    if (!(PIND & BTN_BORRAR_MASK)) {
        uint16_t t = 0;
        while (!(PIND & BTN_BORRAR_MASK) && t < 150) { _delay_ms(10); t++; }
        if (t >= 150) {
            for (uint8_t i = 0; i < 16; i++) eeprom_write_byte((uint8_t*)i, 0xFF);
            LCD_Clear(); LCD_Print("EEPROM limpia");
            UART_Print("EEPROM limpiada (boot long-press PD3)\r\n"); // Mensaje actualizado
            _delay_ms(1200);
        }
    }

    LCD_Clear();
    LCD_Print("Sistema RFID");
    LCD_SetCursor(1,0);
    LCD_Print("Inicializando...");
    UART_Print("Sistema RFID Iniciado\r\n");
    _delay_ms(1200);

    uint8_t cardExists = EEPROM_LoadCard(storedCardID);
    if (cardExists) {
        UART_Print("Tarjeta cargada: ");
        UART_PrintUID(storedCardID);
        UART_Print("\r\n");
    } else {
        UART_Print("Sin tarjeta registrada\r\n");
    }

    LCD_Clear();
    LCD_Print("Acerque tarjeta");

    for (;;)
    {
        // Botón de BORRAR (PD3) 
        if (btn_pressed_once(BTN_BORRAR_MASK)) { 
            Sistema_BorrarTarjeta();
            cardExists = 0;
        }

        // Botón de ACTUALIZAR (PD2)
        if (btn_pressed_once(BTN_ACTUALIZAR_MASK)) {
            currentState = STATE_UPDATE_MODE;
            LCD_Clear();
            LCD_Print("Modo Registro");
            LCD_SetCursor(1,0);
            LCD_Print("Acerque tarjeta");
            UART_Print("Modo registro activado\r\n");
        }

        
        if (!tarjeta_presente) {
            if (PICC_IsNewCardPresent()) {
                if (PICC_ReadCardSerial(currentCardID)) {
                    tarjeta_presente = 1;    

                    UART_Print("Tarjeta detectada: ");
                    UART_PrintUID(currentCardID);
                    UART_Print("\r\n");

                    if (currentState == STATE_UPDATE_MODE) {
                        Sistema_RegistrarTarjeta(currentCardID);
                        cardExists = 1;
                        currentState = STATE_IDLE;
                        _delay_ms(1000);
                        LCD_Clear();
                        LCD_Print("Acerque tarjeta");
                    } else {
                        if (cardExists && CompararTarjetas(currentCardID, storedCardID)) {
                            Sistema_AccesoConcedido();
                        } else {
                            Sistema_AccesoDenegado();
                        }
                        _delay_ms(1000);
                        LCD_Clear();
                        LCD_Print("Acerque tarjeta");
                        LED_Verde_Off();
                        LED_Rojo_Off();
                    }

                    // Finaliza la lectura de la tarjeta actual
                    PICC_HaltA();
                }
            }
        } else {
            // Re-armar cuando se retire (IsNewCardPresent falla)
            if (!PICC_IsNewCardPresent()) {
                tarjeta_presente = 0;
                _delay_ms(120);
            }
        }

        _delay_ms(30);
    }
}

// GPIO ADAPTADO 
static void GPIO_Init(void)
{
    // LEDs
    LED_VERDE_DDR |= (1 << LED_VERDE_PIN); // PD6
    LED_ROJO_DDR  |= (1 << LED_ROJO_PIN);  // PD7
    LED_Verde_Off();
    LED_Rojo_Off();

    // Botones de entradas con pull-up
    BTN_DDR  &= ~(1 << BTN_BORRAR_PIN);      // PD3 como entrada
    BTN_DDR  &= ~(1 << BTN_ACTUALIZAR_PIN);  // PD2 como entrada
    BTN_PORT |=  (1 << BTN_BORRAR_PIN);      // Pull-up en PD3
    BTN_PORT |=  (1 << BTN_ACTUALIZAR_PIN);  // Pull-up en PD2

    // SPI (Sin cambios)
    DDRB |= (1 << SS_PIN) | (1 << MOSI_PIN) | (1 << SCK_PIN) | (1 << RST_PIN);
    DDRB &= ~(1 << MISO_PIN);
    PORTB |= (1 << SS_PIN); 
}



// Incluyo las funciones de bajo nivel que no fueron modificadas solo para la compilación completa:

static void SPI_Init(void) {
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1); 
    SPSR &= ~(1 << SPI2X);
}

static uint8_t SPI_Transfer(uint8_t data) {
    SPDR = data;
    while (!(SPSR & (1 << SPIF)));
    return SPDR;
}

static void UART_Init(void) {
    uint16_t ubrr = F_CPU/16/9600 - 1;
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

static void UART_Transmit(uint8_t data) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

static void UART_Print(const char* str) {
    while (*str) UART_Transmit(*str++);
}

static void UART_PrintUID(uint8_t* uid) {
    const char hex[] = "0123456789ABCDEF";
    for (uint8_t i = 0; i < 4; i++) {
        UART_Transmit(hex[uid[i] >> 4]);
        UART_Transmit(hex[uid[i] & 0x0F]);
        if (i < 3) UART_Transmit(' ');
    }
}

static void I2C_Init(void) {
    TWSR = 0x00;
    TWBR = 0x48;
    TWCR = (1 << TWEN);
}

static void I2C_Start(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

static void I2C_Stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    _delay_us(100);
}

static void I2C_Write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
}

static void LCD_SendByte(uint8_t data, uint8_t mode) {
    uint8_t high_n = data & 0xF0;
    uint8_t low_n  = (data << 4) & 0xF0;

    uint8_t d_h = high_n | mode | 0x08; // BL=1
    uint8_t d_l = low_n  | mode | 0x08;

    I2C_Start();
    I2C_Write(LCD_ADDR << 1);

    I2C_Write(d_h | 0x04); _delay_us(1); // E=1
    I2C_Write(d_h);         _delay_us(50);// E=0

    I2C_Write(d_l | 0x04); _delay_us(1); // E=1
    I2C_Write(d_l);         _delay_us(50);// E=0

    I2C_Stop();
}

static void LCD_Init(void) {
    _delay_ms(50);
    LCD_SendByte(0x30, 0);
    _delay_ms(5);
    LCD_SendByte(0x30, 0);
    _delay_us(150);
    LCD_SendByte(0x30, 0);
    LCD_SendByte(0x20, 0); 
    LCD_SendByte(0x28, 0); 
    LCD_SendByte(0x0C, 0); 
    LCD_SendByte(0x06, 0); 
    LCD_Clear();
}

static void LCD_Clear(void) {
    LCD_SendByte(0x01, 0);
    _delay_ms(2);
}

static void LCD_SetCursor(uint8_t row, uint8_t col) {
    uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
    LCD_SendByte(addr, 0);
}

static void LCD_Print(const char* str) {
    while (*str) LCD_SendByte(*str++, 1);
}

static void PCD_Init(void) {
    RC522_Reset();
    RC522_WriteRegister(RC522_REG_TMODE, 0x80);
    RC522_WriteRegister(RC522_REG_TPRESCALER, 0xA9);
    RC522_WriteRegister(RC522_REG_TRELOAD_H, 0x03);
    RC522_WriteRegister(RC522_REG_TRELOAD_L, 0xE8);
    RC522_WriteRegister(RC522_REG_TXASK, 0x40);
    RC522_WriteRegister(RC522_REG_MODE, 0x3D);
    RC522_WriteRegister(RC522_REG_RFCFG, 0x70);
    RC522_AntennaOn();
    _delay_ms(5);
}

static uint8_t PICC_IsNewCardPresent(void) {
    uint8_t atqa[2];
    return RC522_Request(PICC_REQIDL, atqa);
}

static uint8_t PICC_ReadCardSerial(uint8_t* uid4) {
    return RC522_Anticoll(uid4);
}

static void PICC_HaltA(void) {
    uint8_t frame[4];
    frame[0] = PICC_HALT;
    frame[1] = 0x00;
    uint8_t crcL, crcH;
    if (!PCD_CalcCRC(frame, 2, &crcL, &crcH)) return;
    frame[2] = crcL; frame[3] = crcH;
    uint8_t backLen = 0;
    (void)RC522_Transceive(frame, 4, NULL, &backLen, 0);
}

static void RC522_WriteRegister(uint8_t addr, uint8_t val) {
    PORTB &= ~(1 << SS_PIN);
    SPI_Transfer((addr << 1) & 0x7E);
    SPI_Transfer(val);
    PORTB |= (1 << SS_PIN);
}

static uint8_t RC522_ReadRegister(uint8_t addr) {
    PORTB &= ~(1 << SS_PIN);
    SPI_Transfer(((addr << 1) & 0x7E) | 0x80);
    uint8_t val = SPI_Transfer(0x00);
    PORTB |= (1 << SS_PIN);
    return val;
}

static void RC522_Reset(void) {
    PORTB &= ~(1 << RST_PIN);
    _delay_us(10);
    PORTB |= (1 << RST_PIN);
    _delay_ms(50);
    RC522_WriteRegister(RC522_REG_COMMAND, RC522_CMD_SOFTRESET);
    _delay_ms(50);
}

static void RC522_AntennaOn(void) {
    uint8_t t = RC522_ReadRegister(RC522_REG_TXCONTROL);
    if (!(t & 0x03)) RC522_WriteRegister(RC522_REG_TXCONTROL, t | 0x03);
}

static uint8_t RC522_Request(uint8_t reqMode, uint8_t* atqa) {
    RC522_WriteRegister(RC522_REG_BITFRAMING, 0x07);

    RC522_WriteRegister(RC522_REG_COMMAND, RC522_CMD_IDLE);
    RC522_WriteRegister(RC522_REG_FIFOLEVEL, 0x80);
    RC522_WriteRegister(RC522_REG_FIFODATA, reqMode);
    RC522_WriteRegister(RC522_REG_COMIRQ, 0x7F);

    RC522_WriteRegister(RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);
    RC522_WriteRegister(RC522_REG_BITFRAMING, 0x87);

    uint16_t to = 3000;
    while (!(RC522_ReadRegister(RC522_REG_COMIRQ) & 0x30)) {
        if (--to == 0) return 0;
    }
    if (RC522_ReadRegister(RC522_REG_ERROR) & 0x1B) return 0;

    uint8_t level = RC522_ReadRegister(RC522_REG_FIFOLEVEL);
    if (level != 2) return 0;

    atqa[0] = RC522_ReadRegister(RC522_REG_FIFODATA);
    atqa[1] = RC522_ReadRegister(RC522_REG_FIFODATA);

    RC522_WriteRegister(RC522_REG_BITFRAMING, 0x00);
    return 1;
}

static uint8_t RC522_Anticoll(uint8_t* uid4) {
    RC522_WriteRegister(RC522_REG_BITFRAMING, 0x00);
    RC522_WriteRegister(RC522_REG_COMMAND, RC522_CMD_IDLE);
    RC522_WriteRegister(RC522_REG_FIFOLEVEL, 0x80);
    RC522_WriteRegister(RC522_REG_FIFODATA, PICC_SEL_CL1);
    RC522_WriteRegister(RC522_REG_FIFODATA, PICC_ANTICOLL_CL1);
    RC522_WriteRegister(RC522_REG_COMIRQ, 0x7F);
    RC522_WriteRegister(RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);
    RC522_WriteRegister(RC522_REG_BITFRAMING, 0x80);

    uint16_t to = 3000;
    while (!(RC522_ReadRegister(RC522_REG_COMIRQ) & 0x30)) {
        if (--to == 0) return 0;
    }
    if (RC522_ReadRegister(RC522_REG_ERROR) & 0x1B) return 0;

    uint8_t level = RC522_ReadRegister(RC522_REG_FIFOLEVEL);
    if (level < 5) return 0;

    uint8_t s[5];
    for (uint8_t i = 0; i < 5; i++) s[i] = RC522_ReadRegister(RC522_REG_FIFODATA);

    if ((uint8_t)(s[0] ^ s[1] ^ s[2] ^ s[3]) != s[4]) return 0;
    for (uint8_t i = 0; i < 4; i++) uid4[i] = s[i];
    return 1;
}

static uint8_t RC522_Transceive(uint8_t *sendData, uint8_t sendLen,
                                 uint8_t *backData, uint8_t *backLen,
                                 uint8_t rxAlign) {
    (void)rxAlign;
    RC522_WriteRegister(RC522_REG_COMMAND, RC522_CMD_IDLE);
    RC522_WriteRegister(RC522_REG_FIFOLEVEL, 0x80);
    for (uint8_t i = 0; i < sendLen; i++)
        RC522_WriteRegister(RC522_REG_FIFODATA, sendData[i]);
    RC522_WriteRegister(RC522_REG_COMIRQ, 0x7F);
    RC522_WriteRegister(RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);
    RC522_WriteRegister(RC522_REG_BITFRAMING, 0x80);

    uint16_t to = 3000;
    while (!(RC522_ReadRegister(RC522_REG_COMIRQ) & 0x30)) {
        if (--to == 0) return 0;
    }
    if (RC522_ReadRegister(RC522_REG_ERROR) & 0x1B) return 0;

    if (backData && backLen) {
        uint8_t level = RC522_ReadRegister(RC522_REG_FIFOLEVEL);
        if (level > *backLen) level = *backLen;
        for (uint8_t i = 0; i < level; i++)
            backData[i] = RC522_ReadRegister(RC522_REG_FIFODATA);
        *backLen = level;
    }
    return 1;
}

static uint8_t PCD_CalcCRC(uint8_t *data, uint8_t len, uint8_t *crcL, uint8_t *crcH) {
    RC522_WriteRegister(RC522_REG_COMMAND, RC522_CMD_IDLE);
    RC522_WriteRegister(RC522_REG_FIFOLEVEL, 0x80);
    for (uint8_t i = 0; i < len; i++)
        RC522_WriteRegister(RC522_REG_FIFODATA, data[i]);

    RC522_WriteRegister(RC522_REG_COMMAND, RC522_CMD_CALCCRC);

    uint16_t to = 5000;
    while (!(RC522_ReadRegister(RC522_REG_DIVIRQ) & 0x04)) {
        if (--to == 0) return 0;
    }

    *crcL = RC522_ReadRegister(RC522_REG_CRCRESULTL);
    *crcH = RC522_ReadRegister(RC522_REG_CRCRESULTH);
    return 1;
}

static void EEPROM_SaveCard(uint8_t* cardID) {
    uint8_t bcc = cardID[0] ^ cardID[1] ^ cardID[2] ^ cardID[3];
    eeprom_write_byte((uint8_t*)EEPROM_SIG0_ADDR, 0xA5);
    eeprom_write_byte((uint8_t*)EEPROM_SIG1_ADDR, 0x5A);
    for (uint8_t i = 0; i < 4; i++)
        eeprom_write_byte((uint8_t*)(EEPROM_UID_ADDR + i), cardID[i]);
    eeprom_write_byte((uint8_t*)EEPROM_BCC_ADDR, bcc);
}

static uint8_t EEPROM_LoadCard(uint8_t* cardID) {
    uint8_t s0 = eeprom_read_byte((uint8_t*)EEPROM_SIG0_ADDR);
    uint8_t s1 = eeprom_read_byte((uint8_t*)EEPROM_SIG1_ADDR);
    if (s0 != 0xA5 || s1 != 0x5A) return 0;

    for (uint8_t i = 0; i < 4; i++)
        cardID[i] = eeprom_read_byte((uint8_t*)(EEPROM_UID_ADDR + i));

    uint8_t bcc = eeprom_read_byte((uint8_t*)EEPROM_BCC_ADDR);
    if ((uint8_t)(cardID[0]^cardID[1]^cardID[2]^cardID[3]) != bcc) return 0;

    if (cardID[0]==0 && cardID[1]==0 && cardID[2]==0 && cardID[3]==0) return 0;
    return 1;
}

static void EEPROM_ClearCard(void) {
    eeprom_write_byte((uint8_t*)EEPROM_SIG0_ADDR, 0xFF);
    eeprom_write_byte((uint8_t*)EEPROM_SIG1_ADDR, 0xFF);
    for (uint8_t i = 0; i < 4; i++)
        eeprom_write_byte((uint8_t*)(EEPROM_UID_ADDR + i), 0x00);
    eeprom_write_byte((uint8_t*)EEPROM_BCC_ADDR, 0x00);
}

static void LED_Verde_On(void)  { LED_VERDE_PORT |=  (1 << LED_VERDE_PIN); }
static void LED_Verde_Off(void) { LED_VERDE_PORT &= ~(1 << LED_VERDE_PIN); }
static void LED_Rojo_On(void)   { LED_ROJO_PORT  |=  (1 << LED_ROJO_PIN); }
static void LED_Rojo_Off(void)  { LED_ROJO_PORT  &= ~(1 << LED_ROJO_PIN); }

static uint8_t CompararTarjetas(uint8_t* id1, uint8_t* id2) {
    for (uint8_t i = 0; i < 4; i++) if (id1[i] != id2[i]) return 0;
    return 1;
}

static void Sistema_AccesoConcedido(void) {
    LCD_Clear();
    LCD_Print("Acceso");
    LCD_SetCursor(1,0);
    LCD_Print("PERMITIDO");
    LED_Verde_On();
    LED_Rojo_Off();
    UART_Print("Acceso permitido\r\n");
}

static void Sistema_AccesoDenegado(void) {
    LCD_Clear();
    LCD_Print("Acceso");
    LCD_SetCursor(1,0);
    LCD_Print("DENEGADO");
    LED_Rojo_On();
    LED_Verde_Off();
    UART_Print("Acceso denegado\r\n");
}

static void Sistema_RegistrarTarjeta(uint8_t* cardID) {
    EEPROM_SaveCard(cardID);
    memcpy(storedCardID, cardID, 4);

    LCD_Clear();
    LCD_Print("Nueva tarjeta");
    LCD_SetCursor(1,0);
    LCD_Print("Registrada OK");

    UART_Print("Nueva tarjeta registrada: ");
    UART_PrintUID(cardID);
    UART_Print("\r\n");

    LED_Verde_On();
    _delay_ms(400);
    LED_Verde_Off();
}

static void Sistema_BorrarTarjeta(void) {
    EEPROM_ClearCard();
    memset(storedCardID, 0, 4);

    LCD_Clear();
    LCD_Print("Tarjeta");
    LCD_SetCursor(1,0);
    LCD_Print("BORRADA");

    UART_Print("Tarjeta borrada de la memoria\r\n");

    LED_Rojo_On(); _delay_ms(300);
    LED_Rojo_Off(); _delay_ms(300);
    LED_Rojo_On(); _delay_ms(300);
    LED_Rojo_Off();

    _delay_ms(800);
    LCD_Clear();
    LCD_Print("Sin tarjeta");
    LCD_SetCursor(1,0);
    LCD_Print("registrada");
}
