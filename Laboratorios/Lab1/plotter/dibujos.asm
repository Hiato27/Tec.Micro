; Secuencia: D2 -> 1 s, luego D4 -> 25 s, luego D7 -> 25 s, luego D6+D5 -> 25 s
; ATmega328P @ 16 MHz (Arduino UNO/Nano)
; PD2 = D2, PD4 = D4, PD7 = D7, PD6 = D6, PD5 = D5

.include "m328pdef.inc"

.org 0x0000
    rjmp RESET

RESET:
    ;----- Stack -----
    ldi     r16, high(RAMEND)
    out     SPH, r16
    ldi     r16, low(RAMEND)
    out     SPL, r16

    ;----- Configurar salidas (PD2, PD4, PD5, PD6, PD7) -----
    ldi     r16, (1<<PD2)|(1<<PD4)|(1<<PD5)|(1<<PD6)|(1<<PD7)
    out     DDRD, r16

MAIN_LOOP:
    ; --- Paso 1: D2 = ON (1 s) ---
    ldi     r16, (1<<PD2)
    out     PORTD, r16
    rcall   DELAY_1s

    ; --- Paso 2: D4 = ON (25 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_25s

    ; --- Paso 3: D7 = ON (25 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_25s

    ; --- Paso 4: D6 y D5 = ON (25 s) ---
    ldi     r16, (1<<PD6)|(1<<PD5)
    out     PORTD, r16
    rcall   DELAY_25s

    ; --- loop ---
    clr     r16
    out     PORTD, r16

    rjmp    MAIN_LOOP

;---------------- DELAY 1 s (aprox, 16 MHz) ----------------
DELAY_1s:
    ldi     r18, 16          ; 16 * 62.5 ms ? 1 s
D1_LOOP:
    ldi     r17, 200
D1_INNER:
    ldi     r16, 250
D1_INNER2:
    dec     r16
    brne    D1_INNER2
    dec     r17
    brne    D1_INNER
    dec     r18
    brne    D1_LOOP
    ret

;---------------- DELAY 25 s (25 × 1 s) ----------------
DELAY_25s:
    ldi     r19, 25
D25_LOOP:
    rcall   DELAY_1s
    dec     r19
    brne    D25_LOOP
    ret
