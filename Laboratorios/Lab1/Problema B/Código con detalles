; ===================== Matriz 8x8 + UART + Scroll ======================
; MCU: ATmega328P @ 16 MHz (Arduino UNO)
; UART: 9600 8N1
; Estados:
;   [0] Apagar
;   [1] Mensaje desplazante: "COMO ESTAS?" (por UART)
;       En la matriz se ve "CÓMO ESTÁS?" con tildes.
;   [2] Carita feliz
;   [3] Carita triste
;   [4] Corazon
;   [5] Rombo
;   [6] Space Invader
; Controles:
;   '+' = mas rapido   '-' = mas lento
; ======================================================================

.include "m328pdef.inc"

;-----------------------------------------------------------------
; Constantes y definiciones
;-----------------------------------------------------------------
.cseg
.equ TX_BUF_SIZE = 256
.equ TX_BUF_MASK = TX_BUF_SIZE - 1

.equ _F_CPU = 16000000
.equ _BAUD  = 9600
.equ _BPS   = (_F_CPU/16/_BAUD) - 1

; Regs de trabajo
.def timer0_ovf_counter = r2
.def timer2_ovf_counter = r4
.def animation_counter  = r5
.def current_state      = r20
.def row                = r23
.def col                = r24

;-----------------------------------------------------------------
; DSEG (RAM)
;-----------------------------------------------------------------
.dseg
tx_buffer:     .byte TX_BUF_SIZE
tx_head:       .byte 1
tx_tail:       .byte 1

; Scroll runtime
scroll_target: .byte 1      ; periodo (overflows T2 por paso). 1=rapido
scroll_col:    .byte 1      ; columna global actual del mensaje
msg_len_cols:  .byte 1      ; ancho total del mensaje en columnas
tmp_byte:      .byte 1      ; auxiliar para impresion decimal

;-----------------------------------------------------------------
; Vectores
;-----------------------------------------------------------------
.cseg
.org 0x0000 rjmp RESET
.org 0x0002 rjmp INT0_ISR
.org 0x0004 rjmp INT1_ISR
.org 0x0012 rjmp T2_OVF_ISR
.org 0x0020 rjmp T0_OVF_ISR
.org 0x0024 rjmp USART_RX_ISR
.org 0x0026 rjmp USART_UDRE_ISR

;-----------------------------------------------------------------
; Reset
;-----------------------------------------------------------------
.org 0x0100
RESET:
    clr r1
    clr animation_counter

    ; --- Stack ---
    ldi r16, high(RAMEND)  out SPH, r16
    ldi r16, low(RAMEND)   out SPL, r16

    ; --- INT externas (INT0 flanco de bajada) ---
    ldi r16, 0b00000010
    sts EICRA, r16
    ldi r16, 0b00000001
    out EIMSK, r16

    ; --- Timer2: prescaler 1024 (ritmo del scroll) ---
    ldi r16, 0b00000111
    sts TCCR2B, r16
    ; TIMSK2 se habilita al entrar en estado [1]

    ; --- IO: filas/columnas como salida ---
    ; DDRB: PB0..PB5 (D8..D13) -> 1
    ldi r16, 0b00111111
    out DDRB, r16
    ; DDRC: PC0..PC5 (A0..A5)  -> 1
    ldi r16, 0b00111111
    out DDRC, r16
    ; DDRD: PD4..PD7 (D4..D7)  -> 1 (PD0..PD1 UART quedan 0)
    ldi r16, 0b11110000
    out DDRD, r16

    ; --- Punteros por defecto (para figuras 8x8) ---
    rcall SET_ANIMATION_START

    ; --- UART init ---
    ldi r16, low(_BPS)
    ldi r17, high(_BPS)
    rcall USART_INIT

    ; Velocidad por defecto (mas chico = mas rapido)
    ldi r16, 5
    sts scroll_target, r16

    ; ==== Mensaje para scroll ====
    ; Frase en la MATRIZ: "CÓMO ESTÁS?" (11 caracteres)
    ; Cada char ocupa 6 columnas (5 glyph + 1 espacio). Agregamos cola de 8.
    ldi r16, (6*11)+8         ; = 74 columnas
    sts msg_len_cols, r16
    clr r16
    sts scroll_col, r16

    ; Bienvenida + menu
    rcall SEND_WELCOME
    rcall SEND_MENU

    sei
    rjmp MAIN

;-----------------------------------------------------------------
; Loop principal
;-----------------------------------------------------------------
MAIN:
    rcall STATE_MACHINE
    rjmp MAIN

;-----------------------------------------------------------------
; Subrutinas base
;-----------------------------------------------------------------
SET_ANIMATION_START:
    clr animation_counter
    ldi XL, low(CARITA_SONRIENTE<<1)
    ldi XH, high(CARITA_SONRIENTE<<1)
    ret

RENDER_FRAME:
    push row
    push col
    push ZL
    push ZH

    mov ZL, XL
    mov ZH, XH

    ldi row, 0
RF_ROW_LOOP:
        ldi r16, 0b10000000
        lpm r17, Z+

        ldi col, 0
RF_COL_LOOP:
            rcall CLEAR_MATRIX
            push r16
            and  r16, r17
            cpi  r16, 0
            breq RF_SKIP_LED
            rcall TURN_LED
            rcall TEST_DELAY
RF_SKIP_LED:
            pop  r16
            lsr  r16
            inc  col
            cpi  col, 8
            brlo RF_COL_LOOP

        inc  row
        cpi  row, 8
        brlo RF_ROW_LOOP

    pop ZH
    pop ZL
    pop col
    pop row
    ret

; --------- Scroll runtime ---------
RENDER_SCROLL:
    ; Renderiza ventana 8x8 usando FONT5x7 (5 col/char + 1 espacio)
    push row
    push col
    push r16
    push r17
    push r18
    push r19
    push ZL
    push ZH
    push YL
    push YH

    ; Y -> tabla del mensaje (indices de caracteres)
    ldi YL, low(MSG_IDX<<1)
    ldi YH, high(MSG_IDX<<1)

    ; c0 = columna global inicial
    lds r19, scroll_col

    ldi row, 0
RS_ROW_LOOP:
        ldi col, 0
RS_COL_LOOP:
            ; g = r19 + col
            mov r16, r19
            add r16, col

            ; idxChar = g / 6 ; idxCol = g % 6
            mov r17, r16        ; r17 = g
            clr r18             ; r18 = idxChar
RS_DIV6:
            cpi r17, 6
            brlo RS_DIV6_END
            subi r17, 6
            inc  r18
            rjmp RS_DIV6
RS_DIV6_END:
            ; r18 = idxChar, r17 = idxCol (0..5)

            ; si idxCol==5 -> espacio
            cpi r17, 5
            breq RS_SPACE

            ; msg_char = MSG_IDX[idxChar]
            mov r0, r18
            ldi ZL, low(MSG_IDX<<1)
            ldi ZH, high(MSG_IDX<<1)
            add ZL, r0
            adc ZH, r1
            lpm r16, Z          ; indice en fuente

            ; Z = FONT5x7 + (r16*5) + idxCol
            ldi ZL, low(FONT5x7<<1)
            ldi ZH, high(FONT5x7<<1)
            mov r0, r16
            lsl r0              ; *2
            mov r18, r16
            add r18, r0         ; 3*char
            add r18, r0         ; 5*char
            add ZL, r18
            adc ZH, r1
            add ZL, r17
            adc ZH, r1
            lpm r16, Z          ; columna del glyph (bit0=fila 0)

            ; test bit de la fila
            mov r17, row
RS_SHIFT:
            cpi r17, 0
            breq RS_TBIT
            lsr r16
            dec r17
            rjmp RS_SHIFT
RS_TBIT:
            andi r16, 1
            rjmp RS_BIT_DONE

RS_SPACE:
            clr r16

RS_BIT_DONE:
            cpi r16, 0
            breq RS_SKIP_LED

            rcall CLEAR_MATRIX

            ; --- ESPEJAR HORIZONTAL (si el texto sale invertido) ---
            ; mov r0, col
            ; ldi r17, 7
            ; sub r17, r0
            ; mov col, r17
            ; --------------------------------------------------------

            rcall TURN_LED
            rcall TEST_DELAY

RS_SKIP_LED:
            inc  col
            cpi  col, 8
            brlo RS_COL_LOOP

        inc  row
        cpi  row, 8
        brlo RS_ROW_LOOP

    pop YH
    pop YL
    pop ZH
    pop ZL
    pop r19
    pop r18
    pop r17
    pop r16
    pop col
    pop row
    ret

; --------- Helpers de E/S ---------
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

USART_INIT:
    sts tx_head, r1
    sts tx_tail, r1
    sts UBRR0H, r17
    sts UBRR0L, r16
    ldi r16, (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0)
    sts UCSR0B, r16
    ldi r16, (1<<USBS0)|(3<<UCSZ00)
    sts UCSR0C, r16
    ret

USART_SEND:
    push r17
    push r18
    push r19
    push r20
    push ZL
    push ZH
    lds r17, tx_head
    lds r18, tx_tail
    mov r19, r17
    inc r19
    andi r19, TX_BUF_MASK
    cp  r19, r18
    breq usart_send_full
    ldi ZL, low(tx_buffer)
    ldi ZH, high(tx_buffer)
    add ZL, r17
    adc ZH, r1
    st  Z, r16
    sts tx_head, r19
    lds r20, UCSR0B
    ori r20, (1<<UDRIE0)
    sts UCSR0B, r20
    clc
    rjmp usart_send_exit
usart_send_full:
    sec
usart_send_exit:
    pop ZH
    pop ZL
    pop r20
    pop r19
    pop r18
    pop r17
    ret

CLEAR_MATRIX:
    ; Filas en 1 (apagadas), columnas en 0
    push r16
    ldi r16, 0b00001111      ; PB0..PB3=1 (R5..R8), PB4..PB5=0
    out PORTB, r16
    ldi r16, 0b00000000      ; PC0..PC5=0 (C3..C8)
    out PORTC, r16
    ldi r16, 0b11111100      ; PD7..PD2=1 (R1..R4)
    out PORTD, r16
    pop r16
    ret

TURN_LED:
    ; Activa una unica fila/columna segun (row, col)
    push row
    push col
    push r16
    push r17
    push ZL
    push ZH

    ; --- Fila (activar en 0) ---
    ldi ZH, high(ROW_PORTS<<1)
    ldi ZL, low(ROW_PORTS<<1)
    add ZL, row
    adc ZH, r1
    lpm r16, Z                ; direccion de puerto fila

    ldi ZH, high(ROW_MASKS<<1)
    ldi ZL, low(ROW_MASKS<<1)
    add ZL, row
    adc ZH, r1
    lpm r17, Z                ; mascara de pin fila

    clr ZH
    mov ZL, r16
    mov r16, r17
    rcall CLEAR_BIT           ; fila activa = 0

    ; --- Columna (activar en 1) ---
    ldi ZH, high(COL_PORTS<<1)
    ldi ZL, low(COL_PORTS<<1)
    add ZL, col
    adc ZH, r1
    lpm r16, Z                ; direccion de puerto columna

    ldi ZH, high(COL_MASKS<<1)
    ldi ZL, low(COL_MASKS<<1)
    add ZL, col
    adc ZH, r1
    lpm r17, Z                ; mascara de pin columna

    clr ZH
    mov ZL, r16
    mov r16, r17
    rcall SET_BIT             ; columna activa = 1

    pop ZH
    pop ZL
    pop r17
    pop r16
    pop col
    pop row
    ret

TEST_DELAY:
    ; pequeño tiempo de encendido por pixel
    push r18
    push r19
    push r20
    ldi  r18, 1
    ldi  r19, 10
    ldi  r20, 229
TD1: dec  r20
     brne TD1
     dec  r19
     brne TD1
     dec  r18
     brne TD1
     nop
    pop  r20
    pop  r19
    pop  r18
    ret

;-----------------------------------------------------------------
; Maquina de estados
;-----------------------------------------------------------------
STATE_MACHINE:
    cpi current_state, 0
    breq SMS0
    cpi current_state, 1
    breq SMS1
    cpi current_state, 2
    breq SMS2
    cpi current_state, 3
    breq SMS3
    cpi current_state, 4
    breq SMS4
    cpi current_state, 5
    breq SMS5
    cpi current_state, 6
    breq SMS6
    rjmp SMSD

SMS0:   rcall CLEAR_MATRIX
        rjmp SMEND
SMS1:   rcall RENDER_SCROLL
        rjmp SMEND
SMS2:   rcall RENDER_FRAME
        rjmp SMEND
SMS3:   rcall RENDER_FRAME
        rjmp SMEND
SMS4:   rcall RENDER_FRAME
        rjmp SMEND
SMS5:   rcall RENDER_FRAME
        rjmp SMEND
SMS6:   rcall RENDER_FRAME
        rjmp SMEND
SMSD:   rcall CLEAR_MATRIX
SMEND:  ret

;-----------------------------------------------------------------
; UART: textos y helpers (ASCII puro)
;-----------------------------------------------------------------
SEND_MENU:
    push r16
    push ZL
    push ZH
    ldi ZL, low(MENU_TEXT<<1)
    ldi ZH, high(MENU_TEXT<<1)
SM_LOOP:
    lpm r16, Z+
    cpi r16, 0
    breq SM_END
    rcall USART_SEND
    rjmp SM_LOOP
SM_END:
    pop ZH
    pop ZL
    pop r16
    ret

SEND_WELCOME:
    push r16
    push ZL
    push ZH
    ldi ZL, low(WELCOME_TEXT<<1)
    ldi ZH, high(WELCOME_TEXT<<1)
SW_LOOP:
    lpm r16, Z+
    cpi r16, 0
    breq SW_END
    rcall USART_SEND
    rjmp SW_LOOP
SW_END:
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
SE_LOOP:
    lpm r16, Z+
    cpi r16, 0
    breq SE_END
    rcall USART_SEND
    rjmp SE_LOOP
SE_END:
    pop ZH
    pop ZL
    pop r16
    ret

SEND_SPEED_PREFIX:
    push r16
    push ZL
    push ZH
    ldi ZL, low(SPEED_TXT<<1)
    ldi ZH, high(SPEED_TXT<<1)
SSP_LOOP:
    lpm r16, Z+
    cpi r16, 0
    breq SSP_END
    rcall USART_SEND
    rjmp SSP_LOOP
SSP_END:
    pop ZH
    pop ZL
    pop r16
    ret

; Imprime en decimal el byte en tmp_byte (1..99)
SEND_UDEC8:
    push r16
    push r17
    lds  r16, tmp_byte
    ldi  r17, 10
    clr  r0
SU8_D:
    cpi  r16, 10
    brlo SU8_P
    sub  r16, r17
    inc  r0
    rjmp SU8_D
SU8_P:
    cpi  r0, 0
    breq SU8_U
    ldi  r17, '0'
    add  r17, r0
    mov  r16, r17
    rcall USART_SEND
SU8_U:
    ldi  r17, '0'
    add  r17, r16
    mov  r16, r17
    rcall USART_SEND
    pop  r17
    pop  r16
    ret

;-----------------------------------------------------------------
; Interrupciones
;-----------------------------------------------------------------
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
    brne udre_send
    lds  r20, UCSR0B
    andi r20, ~(1<<UDRIE0)
    sts  UCSR0B, r20
    rjmp udre_exit
udre_send:
    ldi  ZL, low(tx_buffer)
    ldi  ZH, high(tx_buffer)
    add  ZL, r18
    adc  ZH, r1
    ld   r16, Z
    sts  UDR0, r16
    inc  r18
    andi r18, TX_BUF_MASK
    sts  tx_tail, r18
udre_exit:
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

    lds  r16, UDR0
    cpi  r16, '0'
    breq RX0
    cpi  r16, '1'
    breq RX1
    cpi  r16, '2'
    breq RX2
    cpi  r16, '3'
    breq RX3
    cpi  r16, '4'
    breq RX4
    cpi  r16, '5'
    breq RX5
    cpi  r16, '6'
    breq RX6
    cpi  r16, '+'
    breq RXP
    cpi  r16, '-'
    breq RXM
    rjmp RXD

RX0:
    rcall SET_ANIMATION_START
    ldi r16, 0
    sts TIMSK2, r16
    ldi r16, 0
    mov current_state, r16
    rjmp RXEND

RX1:
    clr r16
    sts scroll_col, r16
    ldi r16, 1
    mov current_state, r16
    ldi r16, 0b00000001
    sts TIMSK2, r16
    rjmp RXEND

RX2:
    ldi XL, low(CARITA_SONRIENTE<<1)
    ldi XH, high(CARITA_SONRIENTE<<1)
    ldi r16, 0
    sts TIMSK2, r16
    ldi r16, 2
    mov current_state, r16
    rjmp RXEND
RX3:
    ldi XL, low(CARITA_TRISTE<<1)
    ldi XH, high(CARITA_TRISTE<<1)
    ldi r16, 0
    sts TIMSK2, r16
    ldi r16, 3
    mov current_state, r16
    rjmp RXEND
RX4:
    ldi XL, low(CORAZON<<1)
    ldi XH, high(CORAZON<<1)
    ldi r16, 0
    sts TIMSK2, r16
    ldi r16, 4
    mov current_state, r16
    rjmp RXEND
RX5:
    ldi XL, low(ROMBO<<1)
    ldi XH, high(ROMBO<<1)
    ldi r16, 0
    sts TIMSK2, r16
    ldi r16, 5
    mov current_state, r16
    rjmp RXEND
RX6:
    ldi XL, low(ALIEN<<1)
    ldi XH, high(ALIEN<<1)
    ldi r16, 0
    sts TIMSK2, r16
    ldi r16, 6
    mov current_state, r16
    rjmp RXEND

; Velocidad +/-
RXP:
    lds r17, scroll_target
    cpi r17, 1
    breq RX_REPORT
    dec r17
    sts scroll_target, r17
    rjmp RX_REPORT
RXM:
    lds r17, scroll_target
    cpi r17, 40
    breq RX_REPORT
    inc r17
    sts scroll_target, r17
RX_REPORT:
    rcall SEND_SPEED_PREFIX
    lds  r16, scroll_target
    sts  tmp_byte, r16
    rcall SEND_UDEC8
    ldi  r16, 0x0D
    rcall USART_SEND
    ldi  r16, 0x0A
    rcall USART_SEND
    rjmp RXEND

RXD:
    ldi r16, 0
    sts TIMSK2, r16
    ldi r16, 99
    mov current_state, r16
    rcall SEND_ERROR
    rcall SEND_MENU

RXEND:
    pop r17
    pop r16
    out SREG, r16
    pop r16
    reti

INT0_ISR:
    push r16
    in r16, SREG
    push r16
    pop r16
    out SREG, r16
    pop r16
    reti

INT1_ISR:
    push r16
    in r16, SREG
    push r16
    pop r16
    out SREG, r16
    pop r16
    reti

T0_OVF_ISR:
    push r16
    in r16, SREG
    push r16
    pop r16
    out SREG, r16
    pop r16
    reti

; --- Timer2: avanza el scroll segun scroll_target ---
T2_OVF_ISR:
    push r16
    in   r16, SREG
    push r16

    inc  timer2_ovf_counter
    lds  r16, scroll_target
    cp   r16, timer2_ovf_counter
    brsh T2_END

    ; Avanza 1 columna
    lds  r16, scroll_col
    inc  r16
    lds  r17, msg_len_cols
    cp   r16, r17
    brlo 1f
    clr  r16                 ; loop
1:  sts  scroll_col, r16

    clr  timer2_ovf_counter
T2_END:
    pop  r16
    out  SREG, r16
    pop  r16
    reti

;-----------------------------------------------------------------
; Datos (PROGMEM)
;-----------------------------------------------------------------

; Puertos: direcciones IO (0x2B=PORTD, 0x25=PORTB, 0x28=PORTC)
.org 0x0310 ROW_PORTS:
    .db 0x2B, 0x2B, 0x2B, 0x2B, 0x25, 0x25, 0x25, 0x25
.org 0x0320 ROW_MASKS:
    .db 0b00010000, 0b00100000, 0b01000000, 0b10000000, 0b00000001, 0b00000010, 0b00000100, 0b00001000
.org 0x0330 COL_PORTS:
    .db 0x25, 0x25, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28
.org 0x0340 COL_MASKS:
    .db 0b00010000, 0b00100000, 0b00000001, 0b00000010, 0b00000100, 0b00001000, 0b00010000, 0b00100000

; Figuras 8x8
.org 0x0400
CARITA_SONRIENTE:
    .db 0b00111100, 0b01000010, 0b10100101, 0b10000001, 0b10100101, 0b10011001, 0b01000010, 0b00111100
CARITA_TRISTE:
    .db 0b00111100, 0b01000010, 0b10100101, 0b10000001, 0b10011001, 0b10100101, 0b01000010, 0b00111100
CORAZON:
    .db 0b00000000, 0b01100110, 0b11111111, 0b11111111, 0b11111111, 0b01111110, 0b00111100, 0b00011000
ROMBO:
    .db 0b00011000, 0b00111100, 0b01111110, 0b11111111, 0b11111111, 0b01111110, 0b00111100, 0b00011000
ALIEN:
    .db 0b00111100, 0b01111110, 0b10111101, 0b11111111, 0b11111111, 0b00100100, 0b01000010, 0b10000001

; Mensajes UART (ASCII)
.org 0x0500
MENU_TEXT:
    .db "Elija una opcion:",0x0A
    .db "[0] Apagar pantalla",0x0A
    .db "[1] Mensaje desplazante: COMO ESTAS?",0x0A
    .db "[2] Carita feliz",0x0A
    .db "[3] Carita triste",0x0A
    .db "[4] Corazon",0x0A
    .db "[5] Rombo",0x0A
    .db "[6] Space Invader",0x0A
    .db "Controles: '+' mas rapido | '-' mas lento",0x0A,0x0A,0

.org 0x0580
WELCOME_TEXT:
    .db "Bienvenido a la matriz 8x8 - ATmega328P @9600 8N1",0x0A
    .db "Use el menu para elegir.",0x0A,0

.org 0x05C0
SPEED_TXT:
    .db "Velocidad: ",0

.org 0x0600
ERROR_TEXT:
    .db "Opcion invalida!",0x0A,0x0A,0

; ===== Mensaje para la MATRIZ: "CÓMO ESTÁS?" =====
; Indices de la fuente:
; 0:' ' 1:'?' 2:'A' 3:'C' 4:'E' 5:'M' 6:'O' 7:'S' 8:'T' 9:'O_ac' 10:'A_ac'
; Frase: C  O_ac  M  O   (espacio)  E  S  T  A_ac  S  ?
.org 0x0700
MSG_IDX:
    .db 3,9,5,6,0,4,7,8,10,7,1

; Fuente 5x7 (5 bytes por char, bit0=fila superior). ASCII-only en comentarios.
.org 0x0740
FONT5x7:
; ' ' (idx 0)
    .db 0b00000000,0b00000000,0b00000000,0b00000000,0b00000000
; '?' (idx 1)
    .db 0b00011100,0b00100010,0b00000100,0b00000000,0b00000100
; 'A' (idx 2)
    .db 0b00111110,0b00001001,0b00001001,0b00001001,0b00111110
; 'C' (idx 3)
    .db 0b00011110,0b00100001,0b00100001,0b00100001,0b00010010
; 'E' (idx 4)
    .db 0b00111111,0b00101001,0b00101001,0b00100001,0b00100001
; 'M' (idx 5)
    .db 0b00111111,0b00000010,0b00000100,0b00000010,0b00111111
; 'O' (idx 6)
    .db 0b00011110,0b00100001,0b00100001,0b00100001,0b00011110
; 'S' (idx 7)
    .db 0b00010010,0b00101001,0b00101001,0b00101001,0b00000110
; 'T' (idx 8)
    .db 0b00000001,0b00000001,0b00111111,0b00000001,0b00000001
; 'O_ac' (idx 9)  -> 'O' con acento integrado
    .db 0b01011110,0b10100001,0b10100001,0b10100001,0b01011110
; 'A_ac' (idx 10) -> 'A' con acento integrado
    .db 0b01111110,0b10001001,0b10001001,0b10001001,0b01111110
