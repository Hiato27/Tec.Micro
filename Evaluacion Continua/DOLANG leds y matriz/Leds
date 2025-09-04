
; ATmega328P @ 16 MHz (Arduino Uno)
; BUCLE AUTOMÁTICO de 3 secuencias :
;   1) Desplazamiento 1 LED Izquierda a Derecha (pin 13 al pin 6)
;   2) Secuencial Izquierda a Derecha acumulando (pin 13 añ pin 6)
;   3) Extremos al Centro (en pares) y vuelta
; Pines LED (izquierda a derecha):
;   13 PB5, 12 PB4, 11 PB3, 10 PB2, 9 PB1, 8 PB0, 7 PD7, 6 PD6


.include "m328pdef.inc"

.cseg
.org 0x0000
    rjmp RESET


RESET:
    ; Stack
    ldi  r16, high(RAMEND)
    out  SPH, r16
    ldi  r16, low(RAMEND)
    out  SPL, r16
    clr  r1

    ; Salidas: PB0..PB5 y PD6..PD7
    ldi  r16, 0b00111111
    out  DDRB, r16
    in   r17, DDRD
    ori  r17, (1<<DDD6)|(1<<DDD7)
    out  DDRD, r17

    ; Apagar LEDS
    ldi  r16, 0
    out  PORTB, r16
    cbi  PORTD, PD6
    cbi  PORTD, PD7

; Bucle principal (cambia las secuencias en el bucle)
MainLoop:
    rcall SecuenciaSoloUno        ; 1) del pin 13 al pin 6, uno por vez
    rcall Pausa
    rcall SecuenciaAcumulada      ; 2) del pin 13 al pin 6, acumulando
    rcall Pausa
    rcall ExtremosCentro          ; 3) pares extremos al centro y vuelta
    rcall Pausa
    rjmp  MainLoop

; 1) Desplazamiento de Izquierda a Derecha (LED13 al LED6)

SecuenciaSoloUno:
    ; Apagar
    ldi  r20, 0
    out  PORTB, r20
    cbi  PORTD, PD6
    cbi  PORTD, PD7

    ; 13
    sbi  PORTB, PB5
    rcall Mseg
    cbi  PORTB, PB5
    ; 12
    sbi  PORTB, PB4
    rcall Mseg
    cbi  PORTB, PB4
    ; 11
    sbi  PORTB, PB3
    rcall Mseg
    cbi  PORTB, PB3
    ; 10
    sbi  PORTB, PB2
    rcall Mseg
    cbi  PORTB, PB2
    ; 9
    sbi  PORTB, PB1
    rcall Mseg
    cbi  PORTB, PB1
    ; 8
    sbi  PORTB, PB0
    rcall Mseg
    cbi  PORTB, PB0
    ; 7
    sbi  PORTD, PD7
    rcall Mseg
    cbi  PORTD, PD7
    ; 6
    sbi  PORTD, PD6
    rcall Mseg
    cbi  PORTD, PD6
    ret

; 2) Secuencial de Izquierda a Derecha acumulando (LED13 al LED6)
SecuenciaAcumulada:
    ; Apagar
    ldi  r22, 0
    out  PORTB, r22
    cbi  PORTD, PD6
    cbi  PORTD, PD7

    sbi  PORTB, PB5   ; 13
    rcall Mseg
    sbi  PORTB, PB4   ; 13,12
    rcall Mseg
    sbi  PORTB, PB3   ; 13..11
    rcall Mseg
    sbi  PORTB, PB2   ; 13..10
    rcall Mseg
    sbi  PORTB, PB1   ; 13..9
    rcall Mseg
    sbi  PORTB, PB0   ; 13..8
    rcall Mseg
    sbi  PORTD, PD7   ; +7
    rcall Mseg
    sbi  PORTD, PD6   ; +6
    rcall Mseg

    ; Apagar al final
    ldi  r22, 0
    out  PORTB, r22
    cbi  PORTD, PD6
    cbi  PORTD, PD7
    ret

; 3) Extremos al Centro (pares) y vuelta
;    (13+6) al (12+7) al (11+8) al (10+9)  y apagar en sentido inverso
ExtremosCentro:
    ; Encendido
    sbi  PORTB, PB5   ; 13
    sbi  PORTD, PD6   ; 6
    rcall Mseg

    sbi  PORTB, PB4   ; 12
    sbi  PORTD, PD7   ; 7
    rcall Mseg

    sbi  PORTB, PB3   ; 11
    sbi  PORTB, PB0   ; 8
    rcall Mseg

    sbi  PORTB, PB2   ; 10
    sbi  PORTB, PB1   ; 9
    rcall Mseg

    ; Apagado (centro a extremos)
    cbi  PORTB, PB2   ; 10
    cbi  PORTB, PB1   ; 9
    rcall Mseg

    cbi  PORTB, PB3   ; 11
    cbi  PORTB, PB0   ; 8
    rcall Mseg

    cbi  PORTB, PB4   ; 12
    cbi  PORTD, PD7   ; 7
    rcall Mseg

    cbi  PORTB, PB5   ; 13
    cbi  PORTD, PD6   ; 6
    rcall Mseg
    ret

; Pausas entre las secuencias 
Pausa:
    rcall Mseg
    rcall Mseg
    rcall Mseg
    ret

; Delay por software 
Mseg:
    ldi  r25, 21
    ldi  r26, 75
    ldi  r27, 189
L1:
    dec  r27
    brne L1
    dec  r26
    brne L1
    dec  r25
    brne L1
    nop
    ret
