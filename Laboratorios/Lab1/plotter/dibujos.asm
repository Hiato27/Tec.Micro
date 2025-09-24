; ===== Menú Serial =====
; UART0 9600 

.include "m328pdef.inc"

;----------------- Constantes UART -----------------
.equ UBRR_9600 = 103           ; 16 MHz, 9600 bps

;----------------- Vector de reset -----------------
.CSEG
.org 0x0000
    rjmp RESET

;===================================================
;                   INICIO
;===================================================
RESET:
    ; --- Stack ---
    ldi     r16, high(RAMEND)
    out     SPH, r16
    ldi     r16, low(RAMEND)
    out     SPL, r16

    ; --- UART init: 9600 ---
    ldi     r16, high(UBRR_9600)
    sts     UBRR0H, r16
    ldi     r16, low(UBRR_9600)
    sts     UBRR0L, r16

    ; UCSR0A: U2X0=0
    ldi     r16, 0
    sts     UCSR0A, r16

    ; UCSR0B: habilitar RX y TX
    ldi     r16, (1<<RXEN0)|(1<<TXEN0)
    sts     UCSR0B, r16

    ; UCSR0C: 8N1 => UCSZ01|UCSZ00
    ldi     r16, (1<<UCSZ01)|(1<<UCSZ00)
    sts     UCSR0C, r16

    ; --- GPIO init: PD2..PD7 como salida, iniciar en LOW ---
    ldi     r16, (1<<PD2)|(1<<PD3)|(1<<PD4)|(1<<PD5)|(1<<PD6)|(1<<PD7)
    out     DDRD, r16
    clr     r16
    out     PORTD, r16

;===================================================
;               BUCLE DEL MENÚ
;===================================================
MENU:
    rcall   PRINT_MENU        ; mostrar todas las líneas

    ; Leer una tecla válida (ignorar CR/LF) y GUARDARLA en r20
READ_KEY:
    rcall   UART_RX           ; devuelve en r24
    cpi     r24, 0x0D         ; '\r'
    breq    READ_KEY
    cpi     r24, 0x0A         ; '\n'
    breq    READ_KEY
    mov     r20, r24          ; guardar tecla

    ; Eco y salto de línea (esto pisa r24, por eso guardamos antes)
    rcall   UART_TX
    rcall   PRINT_CRLF

    ; Restaurar tecla para comparar
    mov     r24, r20

    ; Decodificar opción
    cpi     r24, '1'
    breq    DO_OP1
    cpi     r24, '2'
    breq    DO_OP2
    cpi     r24, '3'
    breq    DO_OP3
    cpi     r24, 'T'
    breq    DO_OP4

    ; Opción inválida
    rcall   PRINT_PSTR_INV
    rjmp    MENU

;===================================================
;          OPCIONES 
;===================================================
; OPCIÓN 1
DO_OP1:
    rcall   PRINT_PSTR_H1
    rcall   PRINT_CRLF
    rcall   RUN_SEQUENCE      ; Triangulo
    rjmp    MENU

; OPCIÓN 2
DO_OP2:
    rcall   PRINT_PSTR_H3
    rcall   PRINT_CRLF
	rcall   RUN_SEQUENCE3     ; Circulo
    rjmp    MENU


; OPCIÓN 3
DO_OP3:
    rcall   PRINT_PSTR_H2
    rcall   PRINT_CRLF
    rcall   RUN_SEQUENCE2     ; Cruz
    rjmp    MENU

; OPCIÓN 4
DO_OP4:
   rcall   PRINT_PSTR_H3
   rcall   PRINT_CRLF
   rcall   RUN_SEQUENCE      ; Triangulo
   rcall   RUN_SEQUENCE3     ; Circulo
   rcall   RUN_SEQUENCE2     ; Cruz
   rjmp    MENU
FIN:
    rjmp    FIN

;===================================================
;        SUBRUTINA Opcion 1, Triangulo
;===================================================

RUN_SEQUENCE:
    ; D7 25s
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_25s
    ; D7 25s
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_25s
    ; D4 25s
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_25s
    ; D4 25s
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_25s
    ; D2 1s
    ldi     r16, (1<<PD2)
    out     PORTD, r16
    rcall   DELAY_1s
    ; D4 25s
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_25s
    ; D7 25s
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_25s
    ; D6|D5 25s
    ldi     r16, (1<<PD6)|(1<<PD5)
    out     PORTD, r16
    rcall   DELAY_25s
    ; D4 25s
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_25s
    ; D3 1s
    ldi     r16, (1<<PD3)
    out     PORTD, r16
    rcall   DELAY_1s

    ; Fin: todo LOW y retornar
    clr     r16
    out     PORTD, r16
    ret

;===================================================
;   SUBRUTINA: OPCIÓN 3 Cruz
;===================================================

RUN_SEQUENCE2:
    ; D4 25s
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_25s

    ; D4 25s
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_25s

    ; D7 25s
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_25s

    ; D7 25s
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_25s

    ; D7 25s
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_25s

    ; D2 1s
    ldi     r16, (1<<PD2)
    out     PORTD, r16
    rcall   DELAY_1s

    ; D6|D5 25s
    ldi     r16, (1<<PD6)|(1<<PD5)
    out     PORTD, r16
    rcall   DELAY_25s

    ; D3 1s
    ldi     r16, (1<<PD3)
    out     PORTD, r16
    rcall   DELAY_1s

    ; D4 25s
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_25s

    ; D2 1s
    ldi     r16, (1<<PD2)
    out     PORTD, r16
    rcall   DELAY_1s

    ; (PD7|PD5) 25s  <-- lo dejé tal cual lo escribiste
    ldi     r16, (1<<PD7)|(1<<PD5)
    out     PORTD, r16
    rcall   DELAY_25s

    ; D3 1s
    ldi     r16, (1<<PD3)
    out     PORTD, r16
    rcall   DELAY_1s

    ; Fin: todo LOW y retornar
    clr     r16
    out     PORTD, r16
    ret

	;===================================================
	;   SUBRUTINA: OPCIÓN 3 Circulo
	;===================================================

	RUN_SEQUENCE3:

	; --- Paso 1: D4 = ON (25 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_25s

    ; --- Paso 1: D4 = ON (25 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_25s
	 
	; --- Paso 2: D7 = ON (25 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_25s

	; --- Paso 3: D7 = ON (25 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_25s

  	; --- Paso 4: D7 = ON (25 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_25s

	; --- Paso 5: D7 = ON (25 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_25s

	   ; --- Paso 6: D2 = ON (1 s) ---
    ldi     r16, (1<<PD2)
    out     PORTD, r16
    rcall   DELAY_1s

    ; --- Paso 7: D6= ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 8: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 9: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 10: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 11: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 12: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 13: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 14: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 15: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 16: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 17: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s
	
	; --- Paso 18: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 19: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 20: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 21: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s
	
	; --- Paso 22: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s
	
	; --- Paso 23: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s
	
	; --- Paso 24: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s
	
	; --- Paso 25: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 26: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s
	
	; --- Paso 27: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 28: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 29: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 30: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 31: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 32: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 33: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 34: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 35: D4 = ON (1 s) ---
    ldi     r16, (1<<PD4)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 36: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 37: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 38: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 39: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 40: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 41: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 42: D5 = ON (1 s) ---
    ldi     r16, (1<<PD5)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 43: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s
	
	; --- Paso 44: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 45: D5 = ON (1 s) ---
    ldi     r16, (1<<PD5)
    out     PORTD, r16
    rcall   DELAY_1s
	
	; --- Paso 46: D7 = ON (1 s) ---
    ldi     r16, (1<<PD7)
    out     PORTD, r16
    rcall   DELAY_1s
	
	; --- Paso 47: D5 = ON (1 s) ---
    ldi     r16, (1<<PD5)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 48: D5 = ON (1 s) ---
    ldi     r16, (1<<PD5)
    out     PORTD, r16
    rcall   DELAY_1s
				
	; --- Paso 49: D5 = ON (1 s) ---
    ldi     r16, (1<<PD5)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 50: D5 = ON (1 s) ---
    ldi     r16, (1<<PD5)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 51: D5 = ON (1 s) ---
    ldi     r16, (1<<PD5)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 52: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 53: D5 = ON (1 s) ---
    ldi     r16, (1<<PD5)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 54: D5 = ON (1 s) ---
    ldi     r16, (1<<PD5)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 55: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 56: D5 = ON (1 s) ---
    ldi     r16, (1<<PD5)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 57: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 58: D5 = ON (1 s) ---
    ldi     r16, (1<<PD5)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 59: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 60: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	
	; --- Paso 61: D5 = ON (1 s) ---
    ldi     r16, (1<<PD5)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 62: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 63: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 64: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s
	
	; --- Paso 65: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s
	
	; --- Paso 66: D6 = ON (1 s) ---
    ldi     r16, (1<<PD6)
    out     PORTD, r16
    rcall   DELAY_1s

	; --- Paso 67: D3 = ON (1 s) ---
	ldi     r16, (1<<PD3)
	out     PORTD, r16
    rcall   DELAY_1s

    ; Fin: todo LOW y retornar
    clr     r16
    out     PORTD, r16
    ret

;===================================================
;                  RUTINAS UART
;===================================================
; Enviar carácter en r24
UART_TX:
    lds     r16, UCSR0A
    sbrs    r16, UDRE0        ; espera hasta que el buffer esté vacío
    rjmp    UART_TX
    sts     UDR0, r24
    ret

; Recibir carácter -> r24
UART_RX:
    lds     r16, UCSR0A
    sbrs    r16, RXC0         ; espera hasta que llegue un dato
    rjmp    UART_RX
    lds     r24, UDR0
    ret

; Imprimir CRLF
PRINT_CRLF:
    ldi     r24, 0x0D         ; '\r'
    rcall   UART_TX
    ldi     r24, 0x0A         ; '\n'
    rcall   UART_TX
    ret

;===================================================
;      PRINT de strings en PROGMEM (Z/LPM)
;===================================================
; Imprime string en Flash apuntado por Z, termina en 0x00
PRINT_PSTR:
    lpm     r0, Z+
    tst     r0
    breq    PRINT_PSTR_END
    mov     r24, r0
    rcall   UART_TX
    rjmp    PRINT_PSTR
PRINT_PSTR_END:
    ret

; Wrappers
PRINT_PSTR_INV:
    ldi     r30, low(2*STR_INV)
    ldi     r31, high(2*STR_INV)
    rjmp    PRINT_PSTR

PRINT_PSTR_H1:
    ldi     r30, low(2*STR_H1)
    ldi     r31, high(2*STR_H1)
    rjmp    PRINT_PSTR

PRINT_PSTR_H2:
    ldi     r30, low(2*STR_H2)
    ldi     r31, high(2*STR_H2)
    rjmp    PRINT_PSTR

PRINT_PSTR_H3:
    ldi     r30, low(2*STR_H3)
    ldi     r31, high(2*STR_H3)
    rjmp    PRINT_PSTR

PRINT_PSTR_H4:
    ldi     r30, low(2*STR_H4)
    ldi     r31, high(2*STR_H4)
    rjmp    PRINT_PSTR

;===================================================
;            IMPRIMIR MENÚ POR LÍNEAS
;===================================================
PRINT_MENU:
    rcall   PRINT_PSTR_BANNER
    rcall   PRINT_PSTR_M1
    rcall   PRINT_PSTR_M2
    rcall   PRINT_PSTR_M3
    rcall   PRINT_PSTR_M4
    rcall   PRINT_PSTR_PROMPT
    ret

PRINT_PSTR_BANNER:
    ldi r30, low(2*STR_BANNER)
    ldi r31, high(2*STR_BANNER)
    rjmp PRINT_PSTR

PRINT_PSTR_M1:
    ldi r30, low(2*STR_M1)
    ldi r31, high(2*STR_M1)
    rjmp PRINT_PSTR

PRINT_PSTR_M2:
    ldi r30, low(2*STR_M2)
    ldi r31, high(2*STR_M2)
    rjmp PRINT_PSTR

PRINT_PSTR_M3:
    ldi r30, low(2*STR_M3)
    ldi r31, high(2*STR_M3)
    rjmp PRINT_PSTR

PRINT_PSTR_M4:
    ldi r30, low(2*STR_M4)
    ldi r31, high(2*STR_M4)
    rjmp PRINT_PSTR

PRINT_PSTR_PROMPT:
    ldi r30, low(2*STR_PROMPT)
    ldi r31, high(2*STR_PROMPT)
    rjmp PRINT_PSTR

;===================================================
;                STRINGS EN PROGMEM
;===================================================
STR_BANNER:
    .db "===== MENU PRINCIPAL =====", 0x0D,0x0A, 0

STR_M1:
    .db "1) Dibujar Triangulo", 0x0D,0x0A, 0

STR_M2:
    .db "2) Dibujar Cruz", 0x0D,0x0A, 0

STR_M3:
    .db "3) Dibujar Circulo", 0x0D,0x0A, 0

STR_M4:
    .db "4) Opcion 4", 0x0D,0x0A, 0

STR_PROMPT:
    .db "Seleccione (1-4): ", 0

STR_INV:
    .db 0x0D,0x0A,"Opcion invalida. Intente de nuevo.",0x0D,0x0A,0

STR_H1:
    .db "[Opcion 1: Iniciando Triangulo]", 0
STR_H2:
    .db "[Opcion 2: Iniciando Cruz]", 0
STR_H3:
    .db "[Opcion 3 Iniciando Circulo]", 0
STR_H4:
    .db "[Opcion 4 seleccionada]", 0

;===================================================
;                 DELAYS (tus rutinas)
;===================================================
; 1 s aprox @16 MHz (usa r16,r17,r18)
DELAY_1s:
    ldi     r18, 16          ; 16 × 62.5 ms ? 1 s
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

; 25 s = 25 × 1 s (usa r19)
DELAY_25s:
    ldi     r19, 25
D25_LOOP:
    rcall   DELAY_1s
    dec     r19
    brne    D25_LOOP
    ret
