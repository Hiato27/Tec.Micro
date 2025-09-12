.include "m328pdef.inc"

.equ DISPLAY_ANODO = 0

.org 0x0000
    rjmp inicio

; Direcciones base para las tablas de segmentos (LUTs)
.equ LUTB_DIR  = 0x0100
.equ LUTD_DIR  = 0x0110

; Pines de entrada
.equ PIN_INICIO = 5
.equ PIN_ALTO   = 4

; Máscaras para PORTB y PORTD
.equ MASCARA_B     = 0b00000011
.equ MASCARA_D     = 0b11111100
.equ INV_MASCARA_B = 0b11111100
.equ INV_MASCARA_D = 0b00000011

configurar_sistema:
    ; Configura PB0 y PB1 como salida
    in      r20, DDRB
    ori     r20, MASCARA_B
    out     DDRB, r20

    ; Configura PD2 a PD7 como salida
    in      r20, DDRD
    ori     r20, MASCARA_D
    out     DDRD, r20

    ; Establece PC como entrada y activa pull-ups en PC4 y PC5
    clr     r20
    out     DDRC, r20
    ldi     r20, (1<<PIN_INICIO) | (1<<PIN_ALTO)
    out     PORTC, r20

    rcall   cargar_luts_7seg    ; Carga tabla de segmentos
    ret

esperar_pulsacion:
    ; Espera a que el botón seleccionado en r18 sea presionado
esperar_pulsacion_loop:
    in      r19, PINC
    and     r19, r18
    brne    esperar_pulsacion_loop
    rcall   retardo_5ms    ; Antirrebote
    in      r19, PINC
    and     r19, r18
    brne    esperar_pulsacion_loop
    ret

esperar_liberacion:
    ; Espera a que el botón sea liberado
esperar_liberacion_loop:
    in      r19, PINC
    and     r19, r18
    breq    esperar_liberacion_loop
    rcall   retardo_5ms    ; Antirrebote
    in      r19, PINC
    and     r19, r18
    breq    esperar_liberacion_loop
    ret

retardo_5ms:
    ldi     r22, 65
ret5ms_externo:
    ldi     r23, 255
ret5ms_interno:
    dec     r23
    brne    ret5ms_interno
    dec     r22
    brne    ret5ms_externo
    ret

retardo_200ms:
    ; Repite el retardo de 5 ms 255 veces (~200 ms)
    ldi     r24, 255
ret200ms_loop:
    rcall   retardo_5ms
    dec     r24
    brne    ret200ms_loop
    ret

obtener_digito:
    ; Extrae los 4 bits bajos de r16 y los guarda en r21
    mov     r20, r16
    andi    r20, 0x0F
    mov     r21, r20
    ret

mostrar_digito_7seg:
    ; Carga el byte de segmentos desde LUTB usando índice r21
    ldi     r28, LOW(LUTB_DIR)
    ldi     r29, HIGH(LUTB_DIR)
    add     r28, r21
    adc     r29, r1
    ld      r22, Y    ; Resultado para PORTB en r22

    ; Carga el byte de segmentos desde LUTD usando el mismo índice
    ldi     r28, LOW(LUTD_DIR)
    ldi     r29, HIGH(LUTD_DIR)
    add     r28, r21
    adc     r29, r1
    ld      r23, Y    ; Resultado para PORTD en r23

.if DISPLAY_ANODO
    ldi     r25, MASCARA_B
    eor     r22, r25
    ldi     r25, MASCARA_D
    eor     r23, r25
.endif

    in      r25, PORTB
    andi    r25, INV_MASCARA_B
    or      r25, r22
    out     PORTB, r25

    in      r25, PORTD
    andi    r25, INV_MASCARA_D
    or      r25, r23
    out     PORTD, r25
    ret

cargar_luts_7seg:
    ldi     r28, LOW(LUTB_DIR)
    ldi     r29, HIGH(LUTB_DIR)
    ldi     r20, 0b00000001
    st      Y+, r20
    ldi     r20, 0b00000000
    st      Y+, r20
    ldi     r20, 0b00000010
    st      Y+, r20
    ldi     r20, 0b00000010
    st      Y+, r20
    ldi     r20, 0b00000011
    st      Y+, r20
    ldi     r20, 0b00000011
    st      Y+, r20
    ldi     r20, 0b00000011
    st      Y+, r20
    ldi     r20, 0b00000000
    st      Y+, r20
    ldi     r20, 0b00000011
    st      Y+, r20
    ldi     r20, 0b00000011
    st      Y+, r20

    ldi     r28, LOW(LUTD_DIR)
    ldi     r29, HIGH(LUTD_DIR)
    ldi     r20, 0b11111000
    st      Y+, r20
    ldi     r20, 0b01001000
    st      Y+, r20
    ldi     r20, 0b11110000
    st      Y+, r20
    ldi     r20, 0b11011000
    st      Y+, r20
    ldi     r20, 0b01001000
    st      Y+, r20
    ldi     r20, 0b10011000
    st      Y+, r20
    ldi     r20, 0b10111000
    st      Y+, r20
    ldi     r20, 0b11001000
    st      Y+, r20
    ldi     r20, 0b11111000
    st      Y+, r20
    ldi     r20, 0b11011000
    st      Y+, r20
    ret

inicio:
    ldi     r16, HIGH(RAMEND)
    out     SPH, r16
    ldi     r16, LOW(RAMEND)
    out     SPL, r16

    clr     r1
    rcall   configurar_sistema

bucle_principal:
    ldi     r18, (1<<PIN_INICIO)
    rcall   esperar_pulsacion
    rcall   esperar_liberacion

    clr     r16

bucle_conteo:
    rcall   obtener_digito
    rcall   mostrar_digito_7seg

    in      r20, PINC
    sbrc    r20, PIN_ALTO
    rjmp    sin_alto
    ldi     r18, (1<<PIN_ALTO)
    rcall   esperar_pulsacion
    rcall   esperar_liberacion
    rjmp    bucle_principal

sin_alto:
    rcall   retardo_200ms
    inc     r16
    cpi     r16, 10
    brlo    bucle_conteo
    clr     r16
    rjmp    bucle_conteo
