;Matriz 8x8

.include "m328pdef.inc"

.cseg
.org 0x0000
    rjmp RESET

.org OC2Aaddr
    rjmp ISR_TIMER2_COMPA

.org OC1Aaddr
    rjmp ISR_TIMER1_COMPA


.dseg
col_idx:    .byte 1
fig_idx:    .byte 1
ptrB_lo:    .byte 1
ptrB_hi:    .byte 1
ptrC_lo:    .byte 1
ptrC_hi:    .byte 1
ptrD_lo:    .byte 1
ptrD_hi:    .byte 1

.cseg
RESET:
    ldi  r16, HIGH(RAMEND)
    out  SPH, r16
    ldi  r16, LOW(RAMEND)
    out  SPL, r16
    clr  r1


    ldi  r16, 0b00111111
    out  DDRB, r16
    ldi  r16, 0b00001111
    out  DDRC, r16
    ldi  r16, 0b11111100
    out  DDRD, r16


    ldi  r16, 0b00100101
    out  PORTB, r16
    ldi  r16, 0b00001101
    out  PORTC, r16
    ldi  r16, 0b00110000
    out  PORTD, r16

    clr  r16
    sts  col_idx, r16
    sts  fig_idx, r16         

    ldi  r16, (1<<WGM21)
    sts  TCCR2A, r16
    ldi  r16, (1<<CS22)|(1<<CS20)
    sts  TCCR2B, r16
    ldi  r16, 249
    sts  OCR2A, r16
    ldi  r16, (1<<OCIE2A)
    sts  TIMSK2, r16

    ldi  r16, HIGH(46874)
    sts  OCR1AH, r16
    ldi  r16, LOW(46874)
    sts  OCR1AL, r16
    ldi  r16, (1<<WGM12) | (1<<CS12) | (1<<CS10)
    sts  TCCR1B, r16
    ldi  r16, (1<<OCIE1A)
    sts  TIMSK1, r16

    rcall UpdateFigurePointers
    sei

MAIN:
    rjmp MAIN



; 0) Cara feliz
cara_feliz_B: .db 0x27,0x25,0x25,0x35,0x25,0x2D,0x25,0x25
cara_feliz_C: .db 0x0D,0x0D,0x05,0x09,0x0B,0x05,0x0D,0x0D
cara_feliz_D: .db 0x30,0x38,0x24,0x30,0x30,0x20,0xB0,0x70

; 1) Cara triste
cara_triste_B: .db 0x27,0x25,0x25,0x35,0x25,0x2D,0x25,0x25
cara_triste_C: .db 0x0D,0x0D,0x01,0x0D,0x0F,0x01,0x0D,0x0D
cara_triste_D: .db 0x30,0x38,0x34,0x20,0x20,0x30,0xB0,0x70


alien_B: .db 0x23,0x21,0x24,0x10,0x00,0x2C,0x21,0x21
alien_C: .db 0x0C,0x05,0x00,0x05,0x07,0x00,0x05,0x0C
alien_D: .db 0x10,0x08,0x14,0x00,0x00,0x10,0x80,0x50


UpdateFigurePointers:
    push r16
    push r30
    push r31

    lds  r16, fig_idx

    ; 0 -> feliz | 1 -> triste | 2 -> alien
    cpi  r16, 0
    brne UF_L1
    rjmp set_feliz
UF_L1:
    cpi  r16, 1
    brne UF_ELSE
    rjmp set_triste
UF_ELSE:
    rjmp set_alien

set_feliz:
    ldi  r30, LOW(cara_feliz_B*2)
    ldi  r31, HIGH(cara_feliz_B*2)
    sts  ptrB_lo, r30
    sts  ptrB_hi, r31
    ldi  r30, LOW(cara_feliz_C*2)
    ldi  r31, HIGH(cara_feliz_C*2)
    sts  ptrC_lo, r30
    sts  ptrC_hi, r31
    ldi  r30, LOW(cara_feliz_D*2)
    ldi  r31, HIGH(cara_feliz_D*2)
    sts  ptrD_lo, r30
    sts  ptrD_hi, r31
    rjmp UF_DONE

set_triste:
    ldi  r30, LOW(cara_triste_B*2)
    ldi  r31, HIGH(cara_triste_B*2)
    sts  ptrB_lo, r30
    sts  ptrB_hi, r31
    ldi  r30, LOW(cara_triste_C*2)
    ldi  r31, HIGH(cara_triste_C*2)
    sts  ptrC_lo, r30
    sts  ptrC_hi, r31
    ldi  r30, LOW(cara_triste_D*2)
    ldi  r31, HIGH(cara_triste_D*2)
    sts  ptrD_lo, r30
    sts  ptrD_hi, r31
    rjmp UF_DONE

set_alien:
    ldi  r30, LOW(alien_B*2)
    ldi  r31, HIGH(alien_B*2)
    sts  ptrB_lo, r30
    sts  ptrB_hi, r31
    ldi  r30, LOW(alien_C*2)
    ldi  r31, HIGH(alien_C*2)
    sts  ptrC_lo, r30
    sts  ptrC_hi, r31
    ldi  r30, LOW(alien_D*2)
    ldi  r31, HIGH(alien_D*2)
    sts  ptrD_lo, r30
    sts  ptrD_hi, r31

UF_DONE:
    pop  r31
    pop  r30
    pop  r16
    ret

ISR_TIMER2_COMPA:
    push r16
    in   r16, SREG
    push r16
    push r17
    push r30
    push r31

    lds  r16, col_idx

    lds  r30, ptrB_lo
    lds  r31, ptrB_hi
    add  r30, r16
    adc  r31, r1
    lpm  r17, Z
    out  PORTB, r17

    lds  r30, ptrC_lo
    lds  r31, ptrC_hi
    add  r30, r16
    adc  r31, r1
    lpm  r17, Z
    out  PORTC, r17

    lds  r30, ptrD_lo
    lds  r31, ptrD_hi
    add  r30, r16
    adc  r31, r1
    lpm  r17, Z
    out  PORTD, r17

    inc  r16
    cpi  r16, 8
    brlo COL_OK
    clr  r16
COL_OK:
    sts  col_idx, r16
    pop  r31
    pop  r30
    pop  r17
    pop  r16
    out  SREG, r16
    pop  r16
    reti


ISR_TIMER1_COMPA:
    push r16
    in   r16, SREG
    push r16
    lds  r16, fig_idx
    inc  r16
    ; Ahora solo 3 figuras: 0..2
    cpi  r16, 3
    brlo FIG_OK
    clr  r16
FIG_OK:
    sts  fig_idx, r16
    rcall UpdateFigurePointers
    pop  r16
    out  SREG, r16
    pop  r16
    reti
