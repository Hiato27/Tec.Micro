;Matriz 8x8

.include "m328pdef.inc"

.cseg
.org 0x0000
    rjmp RESET	; vector de reset

.org OC2Aaddr
    rjmp ISR_TIMER2_COMPA	; Timer2: multiplexado columnas

.org OC1Aaddr
    rjmp ISR_TIMER1_COMPA	; Timer1: cambio de figura


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

    ldi  r16, HIGH(RAMEND)	; SP <- RAMEND (alto)
    out  SPH, r16
    ldi  r16, LOW(RAMEND)	; SP <- RAMEND (bajo)
    out  SPL, r16
    clr  r1					; r1=0 por convención


    ldi  r16, 0b00111111
    out  DDRB, r16			; PB0..PB5 salidas (filas altas)
    ldi  r16, 0b00001111
    out  DDRC, r16			; PC0..PC3 salidas (filas bajas)
    ldi  r16, 0b11111100
    out  DDRD, r16			; PD2..PD7 salidas (columnas)


    ldi  r16, 0b00100101
    out  PORTB, r16			; estado inicial filas (reposo)
    ldi  r16, 0b00001101
    out  PORTC, r16			; estado inicial filas (reposo)
    ldi  r16, 0b00110000
    out  PORTD, r16			; columnas desactivadas


    clr  r16
    sts  col_idx, r16		; col = 0
    sts  fig_idx, r16       ; fig = 0  

 
    ldi  r16, (1<<WGM21)
    sts  TCCR2A, r16		; Timer2 CTC
    ldi  r16, (1<<CS22)|(1<<CS20)
    sts  TCCR2B, r16		; prescaler 128
    ldi  r16, 249
    sts  OCR2A, r16			; ISR a 500 Hz
    ldi  r16, (1<<OCIE2A)
    sts  TIMSK2, r16		; habilita ISR OCR2A

    ldi  r16, HIGH(46874)
    sts  OCR1AH, r16
    ldi  r16, LOW(46874)
    sts  OCR1AL, r16		; ~3 s con presc 1024
    ldi  r16, (1<<WGM12) | (1<<CS12) | (1<<CS10)
    sts  TCCR1B, r16		; Timer1 CTC
    ldi  r16, (1<<OCIE1A)
    sts  TIMSK1, r16		; habilita ISR OCR1A

    rcall UpdateFigurePointers		; punteros a tablas según fig
    sei			 ; habilita interrupciones

MAIN:
    rjmp MAIN	; loop vacío (todo en ISR)


cara_feliz_B: .db 0x27,0x25,0x25,0x35,0x25,0x2D,0x25,0x25
cara_feliz_C: .db 0x0D,0x0D,0x05,0x09,0x0B,0x05,0x0D,0x0D
cara_feliz_D: .db 0x30,0x38,0x24,0x30,0x30,0x20,0xB0,0x70

cara_triste_B: .db 0x27,0x25,0x25,0x35,0x25,0x2D,0x25,0x25
cara_triste_C: .db 0x0D,0x0D,0x01,0x0D,0x0F,0x01,0x0D,0x0D
cara_triste_D: .db 0x30,0x38,0x34,0x20,0x20,0x30,0xB0,0x70

corazon_B: .db 0x26,0x00,0x20,0x31,0x20,0x08,0x26,0x25
corazon_C: .db 0x05,0x05,0x01,0x00,0x03,0x05,0x05,0x0D
corazon_D: .db 0x30,0x18,0x04,0x00,0x00,0x10,0xB0,0x70

rombo_B: .db 0x27,0x21,0x25,0x34,0x25,0x29,0x25,0x25
rombo_C: .db 0x0D,0x0D,0x05,0x09,0x07,0x0D,0x0D,0x0D
rombo_D: .db 0x30,0x38,0x14,0x30,0x10,0x30,0xB0,0x70

alien_B: .db 0x23,0x21,0x24,0x10,0x00,0x2C,0x21,0x21
alien_C: .db 0x0C,0x05,0x00,0x05,0x07,0x00,0x05,0x0C
alien_D: .db 0x10,0x08,0x14,0x00,0x00,0x10,0x80,0x50


UpdateFigurePointers:
    push r16
    push r30
    push r31

    lds  r16, fig_idx		; lee figura actual


    cpi  r16, 0
    brne UF_L1
    rjmp set_feliz
UF_L1:
    
    cpi  r16, 1
    brne UF_L2
    rjmp set_triste
UF_L2:
  
    cpi  r16, 2
    brne UF_L3
    rjmp set_corazon
UF_L3:
   
    cpi  r16, 3
    brne UF_ELSE
    rjmp set_rombo
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

set_corazon:
    ldi  r30, LOW(corazon_B*2)
    ldi  r31, HIGH(corazon_B*2)
    sts  ptrB_lo, r30
    sts  ptrB_hi, r31
    ldi  r30, LOW(corazon_C*2)
    ldi  r31, HIGH(corazon_C*2)
    sts  ptrC_lo, r30
    sts  ptrC_hi, r31
    ldi  r30, LOW(corazon_D*2)
    ldi  r31, HIGH(corazon_D*2)
    sts  ptrD_lo, r30
    sts  ptrD_hi, r31
    rjmp UF_DONE

set_rombo:
    ldi  r30, LOW(rombo_B*2)
    ldi  r31, HIGH(rombo_B*2)
    sts  ptrB_lo, r30
    sts  ptrB_hi, r31
    ldi  r30, LOW(rombo_C*2)
    ldi  r31, HIGH(rombo_C*2)
    sts  ptrC_lo, r30
    sts  ptrC_hi, r31
    ldi  r30, LOW(rombo_D*2)
    ldi  r31, HIGH(rombo_D*2)
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

    lds  r16, col_idx		; columna actual
  
    lds  r30, ptrB_lo
    lds  r31, ptrB_hi
    add  r30, r16
    adc  r31, r1
    lpm  r17, Z
    out  PORTB, r17			; patrón filas altas

    lds  r30, ptrC_lo
    lds  r31, ptrC_hi
    add  r30, r16
    adc  r31, r1
    lpm  r17, Z
    out  PORTC, r17			; patrón filas bajas

    lds  r30, ptrD_lo
    lds  r31, ptrD_hi
    add  r30, r16
    adc  r31, r1
    lpm  r17, Z
    out  PORTD, r17			; activa columna

    inc  r16
    cpi  r16, 8
    brlo COL_OK
    clr  r16				; vuelve a 0
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
    cpi  r16, 5				; 5 figuras (0..4)
    brlo FIG_OK
    clr  r16
FIG_OK:
    sts  fig_idx, r16		; guarda nueva figura
    rcall UpdateFigurePointers		; refresca punteros
	pop  r16
    out  SREG, r16
    pop  r16
    reti

