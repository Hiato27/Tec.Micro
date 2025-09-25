.include "m328pdef.inc"

;  Flujo: 'A' -> Perfil (1/2/3) -> Cantidad (1/2/3) -> Ejecuta
;  Cinta: AVANCE -> PAUSA -> PUNZADO -> RETROCESO (= tiempo AVANCE)
;  LEDs (PORTC):
;    A0=Espera, A1=Run, A2=Fin(2s)
;    A3=Ligera, A4=Media, A5=Pesada

.equ F_CPU              = 16000000
.equ BAUD               = 9600
.equ UBRR_VAL           = (F_CPU/16/BAUD) - 1

.equ LED_ACTIVE_LOW     = 0

; M1 (cinta)
.equ M1_A_BIT           = 5        ; PD5
.equ M1_B_BIT           = 3        ; PD3

; M2 (punzadora)
.equ M2_A_BIT           = 4        ; PD4 (cambiado para compatibilidad)
.equ M2_B_BIT           = 0        ; PB0 (cambiado para compatibilidad)

; LEDs (PORTC / A0..A5)
.equ LED_WAIT_BIT       = 0        ; A0 (PC0) - ESPERA
.equ LED_RUN_BIT        = 1        ; A1 (PC1) - EJECUCIÓN  
.equ LED_END_BIT        = 2        ; A2 (PC2) - FIN
.equ LED_LIG_BIT        = 3        ; A3 (PC3) - LIGERA
.equ LED_MED_BIT        = 4        ; A4 (PC4) - MEDIA
.equ LED_PES_BIT        = 5        ; A5 (PC5) - PESADA

.dseg
FEED_ADV_S:     .byte 1    ; Tiempo avance cinta (segundos)
FEED_PAUSE_S:   .byte 1    ; Tiempo pausa cinta (segundos)
DISC_S:         .byte 1    ; Tiempo discado (segundos)
PRESS_S:        .byte 1    ; Tiempo presión (segundos)
COUNT_N:        .byte 1    ; Cantidad de ciclos
KEYBUF:         .byte 1    ; Buffer tecla UART
KEYRDY:         .byte 1    ; Flag tecla disponible

.cseg
.org 0x0000
    rjmp RESET

initUART:
    ldi r16, low(UBRR_VAL)
    ldi r17, high(UBRR_VAL)
    sts UBRR0L, r16
    sts UBRR0H, r17
    ldi r16, (1<<RXEN0)|(1<<TXEN0)
    sts UCSR0B, r16
    ldi r16, (1<<UCSZ01)|(1<<UCSZ00)
    sts UCSR0C, r16
    ret

putc:
    lds r17, UCSR0A
    sbrs r17, UDRE0
    rjmp putc
    sts UDR0, r16
    ret

puts:
    lpm r16, Z+
    cpi r16, 0
    breq puts_end
    rcall putc
    rjmp puts
puts_end:
    ret

printCRLF:
    ldi r16, 13
    rcall putc
    ldi r16, 10
    rcall putc
    ret

getc_block:
    lds r17, KEYRDY
    tst r17
    breq gb_uart
    ldi r17, 0
    sts KEYRDY, r17
    lds r16, KEYBUF
    ret
gb_uart:
    lds r17, UCSR0A
    sbrs r17, RXC0
    rjmp gb_uart
    lds r16, UDR0
    ret

push_back:
    sts KEYBUF, r16
    ldi r17, 1
    sts KEYRDY, r17
    ret

flush_eol_or_push:
flush_loop:
    lds r17, UCSR0A
    sbrs r17, RXC0
    ret
    lds r16, UDR0
    cpi r16, 13
    breq flush_loop
    cpi r16, 10
    breq flush_loop
    rcall push_back
    ret

M1_ON:
    sbi PORTD, M1_A_BIT     ; D5=1
    cbi PORTD, M1_B_BIT     ; D3=0
    ret

M1_REV:
    cbi PORTD, M1_A_BIT     ; D5=0
    sbi PORTD, M1_B_BIT     ; D3=1
    ret

M1_OFF:
    cbi PORTD, M1_A_BIT     ; D5=0
    cbi PORTD, M1_B_BIT     ; D3=0
    ret

M2_DOWN:
    sbi PORTD, M2_A_BIT
    cbi PORTB, M2_B_BIT
    ret

M2_UP:
    cbi PORTD, M2_A_BIT
    sbi PORTB, M2_B_BIT
    ret

M2_OFF:
    cbi PORTD, M2_A_BIT
    cbi PORTB, M2_B_BIT
    ret

LED_INIT_ALL:
    sbi DDRC, LED_WAIT_BIT
    sbi DDRC, LED_RUN_BIT  
    sbi DDRC, LED_END_BIT
    sbi DDRC, LED_LIG_BIT
    sbi DDRC, LED_MED_BIT
    sbi DDRC, LED_PES_BIT
    cbi PORTC, LED_WAIT_BIT
    cbi PORTC, LED_RUN_BIT
    cbi PORTC, LED_END_BIT
    cbi PORTC, LED_LIG_BIT
    cbi PORTC, LED_MED_BIT
    cbi PORTC, LED_PES_BIT
    ret

LED_SET_WAIT:
    sbi PORTC, LED_WAIT_BIT
    cbi PORTC, LED_RUN_BIT
    cbi PORTC, LED_END_BIT
    ret

LED_SET_RUN:
    cbi PORTC, LED_WAIT_BIT
    sbi PORTC, LED_RUN_BIT
    cbi PORTC, LED_END_BIT
    ret

LED_PULSE_END_2S:
    cbi PORTC, LED_WAIT_BIT
    cbi PORTC, LED_RUN_BIT
    sbi PORTC, LED_END_BIT
    cbi PORTC, LED_LIG_BIT
    cbi PORTC, LED_MED_BIT
    cbi PORTC, LED_PES_BIT
    ldi r24, 2
    rcall DELAY_S
    cbi PORTC, LED_END_BIT
    rjmp LED_SET_WAIT

PROFILE_LEDS_OFF:
    cbi PORTC, LED_LIG_BIT
    cbi PORTC, LED_MED_BIT
    cbi PORTC, LED_PES_BIT
    ret

PROFILE_SET_LIG:
    rcall PROFILE_LEDS_OFF
    sbi PORTC, LED_LIG_BIT
    ret

PROFILE_SET_MED:
    rcall PROFILE_LEDS_OFF
    sbi PORTC, LED_MED_BIT
    ret

PROFILE_SET_PES:
    rcall PROFILE_LEDS_OFF
    sbi PORTC, LED_PES_BIT
    ret

WAIT_A:
    rcall LED_SET_WAIT
    ldi ZH, high(msgAskStart<<1)
    ldi ZL, low(msgAskStart<<1)
    rcall puts
    rcall printCRLF
    rcall flush_eol_or_push
WA_LOOP:
    rcall getc_block
    cpi r16, 13
    breq WA_LOOP
    cpi r16, 10
    breq WA_LOOP
    cpi r16, 'A'
    brne WA_LOOP
    ret

ASK_PROFILE:
    ldi ZH, high(msgAskProf<<1)
    ldi ZL, low(msgAskProf<<1)
    rcall puts
    rcall printCRLF
AP_WAIT_DIGIT:
    rcall getc_block
    cpi r16, 13
    breq AP_WAIT_DIGIT
    cpi r16, 10
    breq AP_WAIT_DIGIT
    cpi r16, '1'
    brne _ap_not1
    rcall PROFILE_SET_LIG
    rjmp PROF_LIG
_ap_not1:
    cpi r16, '2'
    brne _ap_not2
    rcall PROFILE_SET_MED
    rjmp PROF_MED
_ap_not2:
    cpi r16, '3'
    brne AP_WAIT_DIGIT
    rcall PROFILE_SET_PES
    rjmp PROF_PES
AP_CLEAN_EOL:
    rcall flush_eol_or_push
    ret

ASK_QUANTITY:
    ldi ZH, high(msgAskQty<<1)
    ldi ZL, low(msgAskQty<<1)
    rcall puts
    rcall printCRLF
    rcall flush_eol_or_push
AQ_WAIT_DIGIT:
    rcall getc_block
    cpi r16, 13
    breq AQ_WAIT_DIGIT
    cpi r16, 10
    breq AQ_WAIT_DIGIT
    cpi r16, '1'
    breq AQ_OK_1_3
    cpi r16, '2'
    breq AQ_OK_1_3
    cpi r16, '3'
    brne AQ_WAIT_DIGIT
AQ_OK_1_3:
    subi r16, '0'
    sts COUNT_N, r16
    ldi ZH, high(msgQtyOk<<1)
    ldi ZL, low(msgQtyOk<<1)
    rcall puts
    rcall printCRLF
    rcall flush_eol_or_push
    ret

PROF_LIG:
    ldi r16, 3
    sts FEED_ADV_S, r16
    ldi r16, 2
    sts FEED_PAUSE_S, r16
    ldi r16, 2
    sts PRESS_S, r16
    ldi r16, 3
    sts DISC_S, r16
    ldi ZH, high(msgProfLig<<1)
    ldi ZL, low(msgProfLig<<1)
    rcall puts
    rcall printCRLF
    rjmp AP_CLEAN_EOL

PROF_MED:
    ldi r16, 4
    sts FEED_ADV_S, r16
    ldi r16, 2
    sts FEED_PAUSE_S, r16
    ldi r16, 3
    sts PRESS_S, r16
    ldi r16, 4
    sts DISC_S, r16
    ldi ZH, high(msgProfMed<<1)
    ldi ZL, low(msgProfMed<<1)
    rcall puts
    rcall printCRLF
    rjmp AP_CLEAN_EOL

PROF_PES:
    ldi r16, 5
    sts FEED_ADV_S, r16
    ldi r16, 3
    sts FEED_PAUSE_S, r16
    ldi r16, 4
    sts PRESS_S, r16
    ldi r16, 5
    sts DISC_S, r16
    ldi ZH, high(msgProfPes<<1)
    ldi ZL, low(msgProfPes<<1)
    rcall puts
    rcall printCRLF
    rjmp AP_CLEAN_EOL

DELAY_S:
    mov r25, r24
DS_LOOP:
    rcall DELAY_1S
    dec r25
    brne DS_LOOP
    ret

DELAY_1S:
    ldi r23, 200
D1S_L:
    rcall DELAY_MS_5
    dec r23
    brne D1S_L
    ret

DELAY_MS_5:
    ldi r20, 5
D5_L:
    rcall DELAY_MS_1
    dec r20
    brne D5_L
    ret

DELAY_MS_1:
    ldi r21, 100
D1_L1:
    ldi r22, 53
D1_L2:
    dec r22
    brne D1_L2
    dec r21
    brne D1_L1
    ret

RESET:
    ldi r16, high(RAMEND)
    out SPH, r16
    ldi r16, low(RAMEND)
    out SPL, r16

    ; M1 OFF
    sbi DDRD, M1_A_BIT
    sbi DDRD, M1_B_BIT
    cbi PORTD, M1_A_BIT
    cbi PORTD, M1_B_BIT

    ; M2 OFF
    sbi DDRD, M2_A_BIT
    sbi DDRB, M2_B_BIT
    cbi PORTD, M2_A_BIT
    cbi PORTB, M2_B_BIT

    ; LEDs
    rcall LED_INIT_ALL

    ; UART
    rcall initUART

    ; Variables
    ldi r16, 0
    sts KEYRDY, r16

    ldi ZH, high(msgBanner<<1)
    ldi ZL, low(msgBanner<<1)
    rcall puts
    rcall printCRLF

MAIN:
    rcall WAIT_A
    rcall LED_SET_RUN

    rcall ASK_PROFILE
    rcall ASK_QUANTITY

    ; N ciclos
    lds r19, COUNT_N
CICLO:
    ; AVANCE
    ldi ZH, high(msgFeedOn<<1)
    ldi ZL, low(msgFeedOn<<1)
    rcall puts
    rcall printCRLF
    rcall M1_ON
    lds r24, FEED_ADV_S
    rcall DELAY_S
    rcall M1_OFF

    ; PAUSA
    ldi ZH, high(msgFeedPause<<1)
    ldi ZL, low(msgFeedPause<<1)
    rcall puts
    rcall printCRLF
    lds r24, FEED_PAUSE_S
    rcall DELAY_S

    ; PUNZADO
    ldi ZH, high(msgPunchDown<<1)
    ldi ZL, low(msgPunchDown<<1)
    rcall puts
    rcall printCRLF
    rcall M2_DOWN
    ldi r24, 1
    rcall DELAY_S
    rcall M2_OFF

    ldi ZH, high(msgPunchPress<<1)
    ldi ZL, low(msgPunchPress<<1)
    rcall puts
    rcall printCRLF
    lds r24, PRESS_S
    rcall DELAY_S

    ldi ZH, high(msgPunchUp<<1)
    ldi ZL, low(msgPunchUp<<1)
    rcall puts
    rcall printCRLF
    rcall M2_UP
    ldi r24, 1
    rcall DELAY_S
    rcall M2_OFF

    ; RETROCESO
    ldi ZH, high(msgReverseOn<<1)
    ldi ZL, low(msgReverseOn<<1)
    rcall puts
    rcall printCRLF
    rcall M1_REV
    lds r24, FEED_ADV_S
    rcall DELAY_S
    rcall M1_OFF

    dec r19
    brne CICLO

    ; Fin total
    rcall LED_PULSE_END_2S
    ldi ZH, high(msgAllDone<<1)
    ldi ZL, low(msgAllDone<<1)
    rcall puts
    rcall printCRLF
    rjmp MAIN

msgBanner:      .db "Sistema completo con cinta, punzadora y LEDs",0,0
msgAskStart:    .db "Presione 'A' para iniciar",0
msgAskProf:     .db "Perfil: 1-Ligera, 2-Mediana, 3-Pesada",0
msgAskQty:      .db "Cantidad (1,2 o 3) + ENTER",0,0
msgProfLig:     .db "Perfil LIGERA",0
msgProfMed:     .db "Perfil MEDIANA",0,0
msgProfPes:     .db "Perfil PESADA",0
msgQtyOk:       .db "Cantidad OK.",0,0
msgFeedOn:      .db "AVANCE CINTA",0,0
msgFeedPause:   .db "PAUSA CINTA",0
msgPunchDown:   .db "PUNZADOR BAJANDO",0,0
msgPunchPress:  .db "PUNZADOR PRESIONANDO",0,0
msgPunchUp:     .db "PUNZADOR SUBIENDO",0
msgReverseOn:   .db "RETROCESO CINTA",0
msgAllDone:     .db "Fin. Volviendo a ESPERA...",0,0
