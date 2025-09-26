; Laboratorio 1: Parte B- Matriz de LEDs

.include "m328pdef.inc"


.cseg
.equ TX_BUF_SIZE = 256                ; tamaño del buffer (potencia de 2)
.equ TX_BUF_MASK = TX_BUF_SIZE - 1

.equ _TIMER0_OVF_COUNT = 60           ; enfriamiento del botón (overflow T0)
.equ _TIMER2_OVF_COUNT = 8            ; velocidad del scroll (más grande = más lento)

.equ _F_CPU = 16000000
.equ _BAUD  = 9600
.equ _BPS   = (_F_CPU/16/_BAUD) - 1

; Variables simbólicas (renombradas)
.def cont_b           = r2
.def cont_t2          = r4
.def cont_fotograma   = r5
.def estado           = r20
.def fila             = r23
.def columna          = r24


.dseg
tx_buffer: .byte TX_BUF_SIZE          ; almacenamiento del buffer circular
tx_head:   .byte 1                    ; índice de escritura
tx_tail:   .byte 1                    ; índice de lectura


.cseg
.org 0x0000 rjmp RESET               ; inicio del programa
.org 0x0002 rjmp INT0_ISR            ; interrupción externa 0
.org 0x0004 rjmp INT1_ISR            ; interrupción externa 1
.org 0x0012 rjmp T2_OVF_ISR          ; overflow del Timer 2
.org 0x0020 rjmp T0_OVF_ISR          ; overflow del Timer 0
.org 0x0024 rjmp USART_RX_ISR        ; dato recibido por USART
.org 0x0026 rjmp USART_UDRE_ISR      ; USART Data Register Empty


.org 0x0100
RESET:
    clr r1
    clr cont_fotograma

    ; Configurar la pila
    ldi r16, high(RAMEND)
    out SPH, r16
    ldi r16, low(RAMEND)
    out SPL, r16

    ; INT0 por flanco descendente
    ldi r16, 0b00000010
    sts EICRA, r16
    ldi r16, 0b00000001
    out EIMSK, r16

    ; Timer 2: prescaler 1024
    ldi r16, 0b00000111
    sts TCCR2B, r16

    ; IO: D8..D13 (PORTB), A0..A5 (PORTC), D4..D7 (PORTD)
    ldi r16, 0b00111111
    out DDRB,  r16
    ldi r16, 0b00111111
    out DDRC,  r16
    ldi r16, 0b11110000
    out DDRD,  r16

    ; Arrancar animación al principio
    rcall SET_ANIMATION_START

    ; USART @ 9600 (8N2)
    ldi r16, low(_BPS)
    ldi r17, high(_BPS)
    rcall USART_INIT

    rcall SEND_MENU

    ; Habilitar interrupciones globales
    sei

    rjmp MAIN

MAIN:
    rcall STATE_MACHINE
    rjmp MAIN

; Pone X = inicio del bloque de patrones
SET_ANIMATION_START:
    clr cont_fotograma
    ldi XL, low(MATRIX_PATTERNS<<1)
    ldi XH, high(MATRIX_PATTERNS<<1)
    ret

; Avanza el puntero del frame y vuelve al inicio al llegar al límite seguro
MOVE_ANIMATION_FRAME:
    push r30
    push r31

    adiw XL, 1
    adc  XH, r1

    ; Si X >= MATRIX_LOOP_END ? reiniciar
    ldi  r30, low(MATRIX_LOOP_END<<1)
    ldi  r31, high(MATRIX_LOOP_END<<1)
    cp   XL, r30
    cpc  XH, r31
    brlo MOVE_OK

    rcall SET_ANIMATION_START

MOVE_OK:
    pop  r31
    pop  r30
    ret

; Renderiza el frame apuntado por X (8 filas x 8 columnas)
RENDER_FRAME:
    push fila
    push columna
    push ZL
    push ZH
    push r16
    push r17

    mov ZL, XL
    mov ZH, XH

    ldi fila, 0
RENDER_FRAME_ROW_LOOP:
    ldi r16, 0b10000000        ; máscara de columna (bit 7)
    lpm r17, Z+                ; byte de la fila actual

    ldi columna, 0
RENDER_FRAME_COL_LOOP:
    rcall CLEAR_MATRIX

    push r16
    and  r16, r17
    cpi  r16, 0
    breq RENDER_FRAME_SKIP_LED

    rcall TURN_LED
    rcall TEST_DELAY

RENDER_FRAME_SKIP_LED:
    pop  r16
    lsr  r16                   ; siguiente columna

    inc  columna
    cpi  columna, 8
    brlo RENDER_FRAME_COL_LOOP

    inc  fila
    cpi  fila, 8
    brlo RENDER_FRAME_ROW_LOOP

    pop r17
    pop r16
    pop ZH
    pop ZL
    pop columna
    pop fila

    rcall CLEAR_MATRIX         ; limpiar antes del próximo frame
    ret

; Operaciones de bit sobre la dirección en Z (PORTx)
SET_BIT:
    push r16
    push r17
    push ZL
    push ZH
    ld  r17, Z
    or  r17, r16
    st  Z, r17
    pop ZH
    pop ZL
    pop r17
    pop r16
    ret

CLEAR_BIT:
    push r16
    push r17
    push ZL
    push ZH
    ld  r17, Z
    com r16
    and r17, r16
    st  Z, r17
    pop ZH
    pop ZL
    pop r17
    pop r16
    ret

; Inicialización de USART
USART_INIT:
    sts  tx_head, r1
    sts  tx_tail, r1
    sts  UBRR0H, r17
    sts  UBRR0L, r16
    ldi  r16, (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0)
    sts  UCSR0B, r16
    ldi  r16, (1<<USBS0)|(3<<UCSZ00)
    sts  UCSR0C, r16
    ret

; Envío no bloqueante por buffer circular
USART_SEND:
    push r17
    push r18
    push r19
    push r20
    push ZL
    push ZH

    lds  r17, tx_head
    lds  r18, tx_tail
    mov  r19, r17
    inc  r19
    andi r19, TX_BUF_MASK
    cp   r19, r18
    breq usart_send_full

    ldi  ZL, low(tx_buffer)
    ldi  ZH, high(tx_buffer)
    add  ZL, r17
    adc  ZH, r1
    st   Z, r16
    sts  tx_head, r19
    lds  r20, UCSR0B
    ori  r20, (1<<UDRIE0)
    sts  UCSR0B, r20
    clc

    pop ZH
    pop ZL
    pop r20
    pop r19
    pop r18
    pop r17
    ret

usart_send_full:
    sec
    pop ZH
    pop ZL
    pop r20
    pop r19
    pop r18
    pop r17
    ret

; Apaga toda la matriz (filas en HIGH, columnas en LOW)
CLEAR_MATRIX:
    push r16
    ldi r16, 0b00001111        ; PORTB: PB0..PB3=1 (filas off), PB4..PB5=0 (columnas off)
    out PORTB, r16
    ldi r16, 0b00000000        ; PORTC: columnas en 0
    out PORTC, r16
    ldi r16, 0b11111100        ; PORTD: PD4..PD7=1 (filas off), PD2..PD3=0 (columnas off)
    out PORTD, r16
    pop r16
    ret

; Enciende un LED (fila en LOW + columna en HIGH) usando tablas
TURN_LED:
    push fila
    push columna
    push r16
    push r17
    push ZL
    push ZH

    ; Fila a LOW
    ldi ZH, high(ROW_PORTS<<1)
    ldi ZL, low(ROW_PORTS<<1)
    add ZL, fila
    adc ZH, r1
    lpm r16, Z                ; dirección del PORT de la fila

    ldi ZH, high(ROW_MASKS<<1)
    ldi ZL, low(ROW_MASKS<<1)
    add ZL, fila
    adc ZH, r1
    lpm r17, Z                ; máscara de la fila

    clr ZH
    mov ZL, r16               ; Z = &PORTx
    mov r16, r17
    rcall CLEAR_BIT           ; poner fila en LOW

    ; Columna a HIGH
    ldi ZH, high(COL_PORTS<<1)
    ldi ZL, low(COL_PORTS<<1)
    add ZL, columna
    adc ZH, r1
    lpm r16, Z                ; dirección del PORT de la columna

    ldi ZH, high(COL_MASKS<<1)
    ldi ZL, low(COL_MASKS<<1)
    add ZL, columna
    adc ZH, r1
    lpm r17, Z                ; máscara de la columna

    clr ZH
    mov ZL, r16
    mov r16, r17
    rcall SET_BIT             ; poner columna en HIGH

    pop ZH
    pop ZL
    pop r17
    pop r16
    pop columna
    pop fila
    ret

; Retención mínima por punto (el ritmo general lo marca T2)
TEST_DELAY:
    push r18
    push r19
    push r20

    ldi  r18, 1
    ldi  r19, 10
    ldi  r20, 229
L1:
    dec  r20
    brne L1
    dec  r19
    brne L1
    dec  r18
    brne L1
    nop

    pop  r20
    pop  r19
    pop  r18
    ret

STATE_MACHINE:
    cpi estado, 0
    breq STATE_MACHINE_STATE_0    ; sí → ir a estado 0
    cpi estado, 1
    breq STATE_MACHINE_STATE_1
    cpi estado, 2
    breq STATE_MACHINE_STATE_2
    cpi estado, 3
    breq STATE_MACHINE_STATE_3
    cpi estado, 4
    breq STATE_MACHINE_STATE_4
    cpi estado, 5
    breq STATE_MACHINE_STATE_5
    cpi estado, 6
    breq STATE_MACHINE_STATE_6
    rjmp STATE_MACHINE_DEFAULT     ; si no coincide con ninguno → estado inválido

STATE_MACHINE_STATE_0:
    rcall CLEAR_MATRIX             ; Estado 0 → apagar pantalla
    rjmp STATE_MACHINE_END

STATE_MACHINE_STATE_1:             ; Estado 1 → animación tipo scroll
    rcall RENDER_FRAME             ; dibuja el frame actual apuntado por X
    rjmp STATE_MACHINE_END

STATE_MACHINE_STATE_2:
    rcall RENDER_FRAME             ; Estado 2 → figura fija (ej. carita feliz)
    rjmp STATE_MACHINE_END

STATE_MACHINE_STATE_3:
    rcall RENDER_FRAME             ; Estado 3 → figura (carita triste)
    rjmp STATE_MACHINE_END

STATE_MACHINE_STATE_4:
    rcall RENDER_FRAME             ; Estado 4 → figura (corazón)
    rjmp STATE_MACHINE_END

STATE_MACHINE_STATE_5:
    rcall RENDER_FRAME              ; Estado 5 → figura (rombo)
    rjmp STATE_MACHINE_END

STATE_MACHINE_STATE_6:
    rcall RENDER_FRAME              ; Estado 6 → figura (alien)
    rjmp STATE_MACHINE_END

STATE_MACHINE_DEFAULT:      ; estado inválido
    rcall CLEAR_MATRIX
    rjmp STATE_MACHINE_END

STATE_MACHINE_END:
    ret

SEND_MENU:
    push r16
    push ZL
    push ZH
    ldi ZL, low(MENU_TEXT<<1)
    ldi ZH, high(MENU_TEXT<<1)
SEND_MENU_LOOP:
    lpm r16, Z+
    cpi r16, 0
    breq SEND_MENU_END
    rcall USART_SEND
    rjmp SEND_MENU_LOOP
SEND_MENU_END:
    pop ZH
    pop ZL
    pop r16
    ret

SEND_ERROR:
    push r16
    push ZL
    push ZH
    ldi ZL, low(ERROR_TEXT<<1)
    ldi ZH, high(ERROR_TEXT<<1)
SEND_ERROR_LOOP:
    lpm r16, Z+
    cpi r16, 0
    breq SEND_ERROR_END
    rcall USART_SEND
    rjmp SEND_ERROR_LOOP
SEND_ERROR_END:
    pop ZH
    pop ZL
    pop r16
    ret

USART_UDRE_ISR:
    push r16
    push r17
    push r18
    push r19
    push r20
    push ZH
    push ZL

    lds  r17, tx_head
    lds  r18, tx_tail
    cp   r17, r18
    brne usart_udre_send
    lds  r20, UCSR0B
    andi r20, ~(1<<UDRIE0)
    sts  UCSR0B, r20
    rjmp usart_udre_exit

usart_udre_send:
    ldi  ZL, low(tx_buffer)
    ldi  ZH, high(tx_buffer)
    add  ZL, r18
    adc  ZH, r1
    ld   r16, Z
    sts  UDR0, r16
    inc  r18
    andi r18, TX_BUF_MASK
    sts  tx_tail, r18

usart_udre_exit:
    pop  ZL
    pop  ZH
    pop  r20
    pop  r19
    pop  r18
    pop  r17
    pop  r16
    reti

USART_RX_ISR:
    push r16 
    in   r16, SREG 
    push r16 
    push r17
    
    lds r16, UDR0
    cpi r16, '0' 
    breq USART_RX_ISR_CASE_0
    cpi r16, '1' 
    breq USART_RX_ISR_CASE_1
    cpi r16, '2' 
    breq USART_RX_ISR_CASE_2
    cpi r16, '3' 
    breq USART_RX_ISR_CASE_3
    cpi r16, '4' 
    breq USART_RX_ISR_CASE_4
    cpi r16, '5' 
    breq USART_RX_ISR_CASE_5
    cpi r16, '6' 
    breq USART_RX_ISR_CASE_6
    rjmp USART_RX_ISR_CASE_DEFAULT

USART_RX_ISR_CASE_0:
    rcall SET_ANIMATION_START
    ldi r16, 0b00000000
    sts TIMSK2, r16
    ldi r16, 0
    mov estado, r16
    rjmp USART_RX_ISR_END

USART_RX_ISR_CASE_1:
    rcall SET_ANIMATION_START
    ldi r16, 1
    mov estado, r16
    ldi r16, 0b00000001         ; habilita Timer2 OVF para desplazar
    sts TIMSK2, r16
    rjmp USART_RX_ISR_END

USART_RX_ISR_CASE_2:
    ldi XL, low(CARITA_SONRIENTE<<1)
    ldi XH, high(CARITA_SONRIENTE<<1)
    ldi r16, 0b00000000
    sts TIMSK2, r16
    ldi r16, 2
    mov estado, r16
    rjmp USART_RX_ISR_END

USART_RX_ISR_CASE_3:
    ldi XL, low(CARITA_TRISTE<<1)
    ldi XH, high(CARITA_TRISTE<<1)
    ldi r16, 0b00000000
    sts TIMSK2, r16
    ldi r16, 3
    mov estado, r16
    rjmp USART_RX_ISR_END

USART_RX_ISR_CASE_4:
    ldi XL, low(CORAZON<<1)
    ldi XH, high(CORAZON<<1)
    ldi r16, 0b00000000
    sts TIMSK2, r16
    ldi r16, 4
    mov estado, r16
    rjmp USART_RX_ISR_END

USART_RX_ISR_CASE_5:
    ldi XL, low(ROMBO<<1)
    ldi XH, high(ROMBO<<1)
    ldi r16, 0b00000000
    sts TIMSK2, r16
    ldi r16, 5
    mov estado, r16
    rjmp USART_RX_ISR_END

USART_RX_ISR_CASE_6:
    ldi XL, low(ALIEN<<1)
    ldi XH, high(ALIEN<<1)
    ldi r16, 0b00000000
    sts TIMSK2, r16
    ldi r16, 6
    mov estado, r16
    rjmp USART_RX_ISR_END

USART_RX_ISR_CASE_DEFAULT:
    ldi r16, 0b00000000
    sts TIMSK2, r16
    ldi r16, 99
    mov estado, r16
    rcall SEND_ERROR
    rcall SEND_MENU
    rjmp USART_RX_ISR_END

USART_RX_ISR_END:
    pop  r17 
    pop  r16
    out  SREG, r16
    pop  r16
    reti

INT0_ISR:
    push r16 
    in   r16, SREG 
    push r16 
    pop  r16
    out  SREG, r16
    pop  r16
    reti

INT1_ISR:
    push r16 
    in   r16, SREG 
    push r16 
    pop  r16
    out  SREG, r16
    pop  r16
    reti

T0_OVF_ISR:
    push r16 
    in   r16, SREG 
    push r16 
    pop  r16
    out  SREG, r16
    pop  r16
    reti 

T2_OVF_ISR:
    push r16 
    in   r16, SREG 
    push r16 
    inc  cont_t2
    ldi  r16, _TIMER2_OVF_COUNT
    cp   r16, cont_t2 
    brsh T2_OVF_ISR_END
    ; avanzar al siguiente frame del texto
    rcall MOVE_ANIMATION_FRAME
    clr  cont_t2
T2_OVF_ISR_END:
    pop  r16
    out  SREG, r16
    pop  r16
    reti


; Direcciones en data space:
; PORTD = 0x2B, PORTC = 0x28, PORTB = 0x25

.org 0x0310
ROW_PORTS:
    .db 0x2B, 0x2B, 0x2B, 0x2B, 0x25, 0x25, 0x25, 0x25

.org 0x0320
ROW_MASKS:
    .db 0b00010000, 0b00100000, 0b01000000, 0b10000000, 0b00000001, 0b00000010, 0b00000100, 0b00001000

.org 0x0330
COL_PORTS:
    .db 0x25, 0x25, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28

.org 0x0340
COL_MASKS:
    .db 0b00010000, 0b00100000, 0b00000001, 0b00000010, 0b00000100, 0b00001000, 0b00010000, 0b00100000 


; MATRIZ: "AMO LA PESCA"

.org 0x0360
MATRIX_PATTERNS:
    ; padding inicial
    .db 0,0,0,0,0,0,0,0

    ; A
    .db 0b00111100, 0b01000010, 0b10000001, 0b10000001, 0b11111111, 0b10000001, 0b10000001, 0b10000001
    .db 0,0

    ; M
    .db 0b10000001, 0b11000011, 0b10100101, 0b10011001, 0b10000001, 0b10000001, 0b10000001, 0b10000001
    .db 0,0

    ; O
    .db 0b00111100, 0b01000010, 0b10000001, 0b10000001, 0b10000001, 0b10000001, 0b01000010, 0b00111100

    ; espacio
    .db 0,0,0,0

    ; L
    .db 0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b11111111
    .db 0,0

    ; A
    .db 0b00111100, 0b01000010, 0b10000001, 0b10000001, 0b11111111, 0b10000001, 0b10000001, 0b10000001

    ; espacio
    .db 0,0,0,0

    ; P
    .db 0b11111110, 0b10000001, 0b10000001, 0b11111110, 0b10000000, 0b10000000, 0b10000000, 0b10000000
    .db 0,0

    ; E
    .db 0b11111111, 0b10000000, 0b10000000, 0b11111110, 0b10000000, 0b10000000, 0b10000000, 0b11111111
    .db 0,0

    ; S
    .db 0b01111110, 0b10000001, 0b10000000, 0b01111110, 0b00000001, 0b10000001, 0b10000001, 0b01111110
    .db 0,0

    ; C
    .db 0b00111110, 0b01000001, 0b10000000, 0b10000000, 0b10000000, 0b10000000, 0b01000001, 0b00111110
    .db 0,0

    ; A
    .db 0b00111100, 0b01000010, 0b10000001, 0b10000001, 0b11111111, 0b10000001, 0b10000001, 0b10000001

    
MATRIX_LOOP_END:
    .db 0,0,0,0,0,0,0,0

    ; guarda 
MATRIX_PATTERNS_END:
    .db 0,0,0,0,0,0,0,0


; Figuras 
.org 0x0400
CARITA_SONRIENTE:
    .db 0b00000000, 0b00000000, 0b00100100, 0b00000000, 0b00100100, 0b00011000, 0b00000000, 0b00000000
CARITA_TRISTE:
    .db 0b00000000, 0b00000000, 0b00100100, 0b00000000, 0b00000000, 0b00011000, 0b00100100, 0b00000000
CORAZON:
    .db 0b00000000, 0b01100110, 0b11111111, 0b11111111, 0b11111111, 0b01111110, 0b00111100, 0b00011000
ROMBO:
    .db 0b00011000, 0b00111100, 0b01111110, 0b11111111, 0b11111111, 0b01111110, 0b00111100, 0b00011000
ALIEN:
    .db 0b00111100, 0b01111110, 0b11011011, 0b11111111, 0b11111111, 0b00100100, 0b01000010, 0b10100101


; Textos


.org 0x0500
MENU_TEXT:
    .db "Elija una opcion:", 0x0A
    .db "[0] Apagar pantalla", 0x0A
    .db "[1] Mensaje animado", 0x0A
    .db "[2] Carita feliz ", 0x0A
    .db "[3] Carita triste", 0x0A
    .db "[4] Corazon", 0x0A
    .db "[5] Rombo", 0x0A
    .db "[6] Space Invader",0x0A,0x0A, 0

.org 0x0600
ERROR_TEXT:
    .db "Opcion invalida!", 0x0A, 0x0A, 0, 0
