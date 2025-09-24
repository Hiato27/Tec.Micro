.include "m328pdef.inc"

.cseg
.org 0x0000
    rjmp RESET
.org OC2Aaddr
    rjmp ISR

.def a  = r16
.def t  = r17
.def t2 = r18

.equ T2_OCR = 77

RESET:
    ldi t, high(RAMEND)
    out SPH, t
    ldi t, low(RAMEND)
    out SPL, t

    ldi t, 0xFF
    out DDRD, t
    clr a
    out PORTD, a

    ldi t, (1<<WGM21)
    sts TCCR2A, t
    ldi t, (1<<CS21)
    sts TCCR2B, t
    ldi t, T2_OCR
    sts OCR2A, t
    ldi t, (1<<OCIE2A)
    sts TIMSK2, t

    ldi ZL, low(LUT_0x00_FF*2)
    ldi ZH, high(LUT_0x00_FF*2)

    sei
loop:
    rjmp loop

ISR:
    lpm  a, Z+
    out  PORTD, a

    ldi  t,  low(LUT_END_B*2)
    ldi  t2, high(LUT_END_B*2)
    cp   ZL, t
    cpc  ZH, t2
    brlo done

    ldi  ZL, low(LUT_0x00_FF*2)
    ldi  ZH, high(LUT_0x00_FF*2)
done:
    reti

