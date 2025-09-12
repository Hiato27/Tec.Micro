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

 ; Actualiza PORTB respetando bits ajenos
.if DISPLAY_ANODO             ; Si el display es ánodo común, invertir lógicas solo en bits útiles
    ldi     r25, MASCARA_B
    eor     r22, r25           ; Invierte bits de MASCARA_B en r22
    ldi     r25, MASCARA_D
    eor     r23, r25           ; Invierte bits de MASCARA_D en r23
.endif

    in      r25, PORTB           ; Lee estado actual de PORTB
    andi    r25, INV_MASCARA_B   ; Limpia solo PB0
    or      r25, r22             ; Inserta nuevo patrón
    out     PORTB, r25           ; Escribe a PORTB

; Actualiza PORTD respetando bits ajenos
    in      r25, PORTD                    ; Lee estado actual de PORTD
    andi    r25, INV_MASCARA_D
    or      r25, r23                      ; Inserta un nuevo patrón
    out     PORTD, r25                    ; Escribe a PORTD
    ret

 ; LUT de PORTB (PB1 a PB0) 
cargar_luts_7seg:
    ldi     r28, LOW(LUTB_DIR)
    ldi     r29, HIGH(LUTB_DIR)
    ldi     r20, 0b00000001       ;0
    st      Y+, r20
    ldi     r20, 0b00000000       ;1
    st      Y+, r20
    ldi     r20, 0b00000010       ;2
    st      Y+, r20
    ldi     r20, 0b00000010       ;3
    st      Y+, r20
    ldi     r20, 0b00000011       ;4
    st      Y+, r20
    ldi     r20, 0b00000011       ;5
    st      Y+, r20
    ldi     r20, 0b00000011       ;6
    st      Y+, r20
    ldi     r20, 0b00000000       ;7
    st      Y+, r20
    ldi     r20, 0b00000011       ;8
    st      Y+, r20
    ldi     r20, 0b00000011       ;9
    st      Y+, r20

 ;  LUT de PORTD (PD7 a PD2)
    ldi     r28, LOW(LUTD_DIR)
    ldi     r29, HIGH(LUTD_DIR)
    ldi     r20, 0b11111000      ;0
    st      Y+, r20
    ldi     r20, 0b01001000      ;1
    st      Y+, r20
    ldi     r20, 0b11110000      ;2
    st      Y+, r20
    ldi     r20, 0b11011000      ;3
    st      Y+, r20
    ldi     r20, 0b01001000      ;4
    st      Y+, r20
    ldi     r20, 0b10011000      ;5
    st      Y+, r20
    ldi     r20, 0b10111000      ;6
    st      Y+, r20
    ldi     r20, 0b11001000      ;7
    st      Y+, r20
    ldi     r20, 0b11111000      ;8
    st      Y+, r20
    ldi     r20, 0b11011000      ;9
    st      Y+, r20
    ret

 ; Inicializa el puntero de pila (Stack Pointer)
inicio:
    ldi     r16, HIGH(RAMEND)
    out     SPH, r16
    ldi     r16, LOW(RAMEND)
    out     SPL, r16

    clr     r1
    rcall   configurar_sistema

; Espera la señal de INICIO 
bucle_principal:
    ldi     r18, (1<<PIN_INICIO)
    rcall   esperar_pulsacion
    rcall   esperar_liberacion

    clr     r16

; Bucle de conteo y visualización
bucle_conteo:
    rcall   obtener_digito
    rcall   mostrar_digito_7seg

 ; Chequeo del botón ALTO para "resetear" al estado de espera
    in      r20, PINC
    sbrc    r20, PIN_ALTO
    rjmp    sin_alto
    ldi     r18, (1<<PIN_ALTO)      ;  Si ALTO está presionado (bit=0), se confirma con anti-rebote y se vuelve al inicio
    rcall   esperar_pulsacion
    rcall   esperar_liberacion
    rjmp    bucle_principal

; Continuar conteo si ALTO no está presionado
sin_alto:
    rcall   retardo_200ms
    inc     r16
    cpi     r16, 10
    brlo    bucle_conteo
    clr     r16
    rjmp    bucle_conteo
