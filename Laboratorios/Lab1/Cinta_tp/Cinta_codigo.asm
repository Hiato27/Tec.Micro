.include "m328pdef.inc"

.equ TIEMPO_DESC_MS     = 350      ; ms de descenso (≈ 1/2 vuelta)
.equ TIEMPO_ASC_MS      = 350      ; ms de ascenso  (≈ 1/2 vuelta)
.equ TIEMPO_PRESION_MS  = 2000     ; ms motor apagado (presionando)
.equ TIEMPO_PAUSA_MS    = 10000    ; ms motor apagado (pausa tras ascenso)
.equ DEAD_MS            = 50       ; ms de “muerto” al cambiar de sentido

.equ M2_INVERTIR        = 0        ; 0=normal, 1=invierte sentido
.equ CICLOS_N           = 5        ; cantidad de ciclos por tanda

.equ M2_A_BIT           = 6        ; PD6
.equ M2_B_BIT           = 1        ; PB1

.equ F_CPU              = 16000000
.equ BAUD               = 9600
.equ UBRR_VAL           = (F_CPU/16/BAUD) - 1

.cseg
.org 0x0000
    rjmp RESET

initUART:
    ldi  r16, low(UBRR_VAL)
    ldi  r17, high(UBRR_VAL)
    sts  UBRR0L, r16
    sts  UBRR0H, r17
    ldi  r16, (1<<RXEN0)|(1<<TXEN0)     ; RX/TX enable
    sts  UCSR0B, r16
    ldi  r16, (1<<UCSZ01)|(1<<UCSZ00)   ; 8N1
    sts  UCSR0C, r16
    ret

putc:                                   ; r16 = byte a TX
    lds  r17, UCSR0A
    sbrs r17, UDRE0
    rjmp putc
    sts  UDR0, r16
    ret

puts:                                   ; Z -> cadena FLASH 0-terminada
    lpm  r16, Z+
    cpi  r16, 0
    breq puts_end
    rcall putc
    rjmp puts
puts_end:
    ret

printCRLF:
    ldi  r16, 13
    rcall putc
    ldi  r16, 10
    rcall putc
    ret

T2_INICIAR_1MS:
    ldi  r16, (1<<WGM21)                ; CTC
    sts  TCCR2A, r16
    ldi  r16, (1<<CS22)                 ; prescaler = 64
    sts  TCCR2B, r16
    ldi  r16, 249                       ; 1 ms @16MHz/64
    sts  OCR2A, r16
    ldi  r16, 0
    sts  TCNT2, r16
    ldi  r16, (1<<OCF2A)
    out  TIFR2, r16
    ret

T2_REINICIAR_1MS:
    ldi  r16, 0
    sts  TCNT2, r16
    ldi  r16, (1<<OCF2A)
    out  TIFR2, r16
    ret

; r24:r25 = milisegundos (0..65535)
RETARDO_MS:
    cp   r24, r1
    cpc  r25, r1
    breq ret_ready
loop_ms:
    ldi  r18, (1<<OCF2A)
    out  TIFR2, r18
wait_1ms:
    in   r19, TIFR2
    sbrs r19, OCF2A
    rjmp wait_1ms
    sbiw r24, 1
    brne loop_ms
ret_ready:
    ret

M2_DETENER:
    cbi  PORTD, M2_A_BIT
    cbi  PORTB, M2_B_BIT
    ret

.if M2_INVERTIR == 0
M2_DESCENDER:                           ; bajar
    sbi  PORTD, M2_A_BIT                ; A=1
    cbi  PORTB, M2_B_BIT                ; B=0
    ret
M2_ASCENDER:                            ; subir
    cbi  PORTD, M2_A_BIT                ; A=0
    sbi  PORTB, M2_B_BIT                ; B=1
    ret
.else
M2_DESCENDER:                           ; bajar (invertido)
    cbi  PORTD, M2_A_BIT
    sbi  PORTB, M2_B_BIT
    ret
M2_ASCENDER:                            ; subir (invertido)
    sbi  PORTD, M2_A_BIT
    cbi  PORTB, M2_B_BIT
    ret
.endif

RESET:
    ; Stack y ABI
    ldi  r16, high(RAMEND)
    out  SPH, r16
    ldi  r16, low(RAMEND)
    out  SPL, r16
    clr  r1

    ; GPIO M2
    sbi  DDRD, M2_A_BIT                 ; PD6 salida
    sbi  DDRB, M2_B_BIT                 ; PB1 salida
    rcall M2_DETENER

    ; Timer2 base 1 ms
    rcall T2_INICIAR_1MS

    ; UART y banner
    rcall initUART
    ldi  ZH, high(msgBanner<<1)
    ldi  ZL, low(msgBanner<<1)
    rcall puts
    rcall printCRLF

MAIN:
    ldi  r20, CICLOS_N                   ; r20 = contador de ciclos
CICLO:
    ldi  ZH, high(msgBajando<<1)
    ldi  ZL, low(msgBajando<<1)
    rcall puts
    rcall printCRLF

    rcall M2_DESCENDER
    ldi  r24, low(TIEMPO_DESC_MS)
    ldi  r25, high(TIEMPO_DESC_MS)
    rcall T2_REINICIAR_1MS
    rcall RETARDO_MS

    rcall M2_DETENER
    ldi  r24, low(DEAD_MS)
    ldi  r25, high(DEAD_MS)
    rcall T2_REINICIAR_1MS
    rcall RETARDO_MS

    ldi  ZH, high(msgPresion<<1)
    ldi  ZL, low(msgPresion<<1)
    rcall puts
    rcall printCRLF

    ldi  r24, low(TIEMPO_PRESION_MS)
    ldi  r25, high(TIEMPO_PRESION_MS)
    rcall T2_REINICIAR_1MS
    rcall RETARDO_MS

    ldi  ZH, high(msgSubiendo<<1)
    ldi  ZL, low(msgSubiendo<<1)
    rcall puts
    rcall printCRLF

    rcall M2_ASCENDER
    ldi  r24, low(TIEMPO_ASC_MS)
    ldi  r25, high(TIEMPO_ASC_MS)
    rcall T2_REINICIAR_1MS
    rcall RETARDO_MS

    rcall M2_DETENER
    ldi  r24, low(DEAD_MS)
    ldi  r25, high(DEAD_MS)
    rcall T2_REINICIAR_1MS
    rcall RETARDO_MS

    ldi  ZH, high(msgPausa<<1)
    ldi  ZL, low(msgPausa<<1)
    rcall puts
    rcall printCRLF

    ldi  r24, low(TIEMPO_PAUSA_MS)
    ldi  r25, high(TIEMPO_PAUSA_MS)
    rcall T2_REINICIAR_1MS
    rcall RETARDO_MS

    ldi  ZH, high(msgFinCiclo<<1)
    ldi  ZL, low(msgFinCiclo<<1)
    rcall puts
    rcall printCRLF

    dec  r20
    brne CICLO

    ldi  ZH, high(msgFinTanda<<1)
    ldi  ZL, low(msgFinTanda<<1)
    rcall puts
    rcall printCRLF

    rjmp MAIN                             ; repetir tandas

msgBanner:    .db "ETAPA 2: Punzadora M2 con UART, ciclos y dead-time.",0
msgBajando:   .db "M2: Bajando...",0
msgPresion:   .db "M2: Presionando (motor OFF)...",0
msgSubiendo:  .db "M2: Subiendo...",0
msgPausa:     .db "M2: Pausa en alto...",0
msgFinCiclo:  .db "Fin de ciclo.",0
msgFinTanda:  .db "Fin de tanda. Reiniciando...",0
