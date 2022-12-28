 list p=pic10f202

 #include p10f202.inc

 __config _WDTE_OFF & _MCLRE_OFF & _CP_OFF

ca equ 0x08
cb equ 0x09

 org 0x1ff
 movlw 0xde
 org 0x204
 movlw 0xfe
 org 0x000

 movlw b'11000111';option register bits
 option

;setup io
 movlw 0x00
 movwf GPIO
 TRIS GPIO

 goto main_loop

delay_1ms
ca equ 0x08
 movlw 0xfa
 movwf ca
delay_1ms_loop
 nop;padding to make it 4 inst cycles
 decfsz ca,F
 goto delay_1ms_loop 
 retlw 0x00

delay_250ms
 movlw 0xfa
 movwf cb
delay_250ms_loop
 call delay_1ms
 decfsz cb,F
 goto delay_250ms_loop
 retlw 0x00

main_loop
 comf GPIO,F
 call delay_250ms
 comf GPIO,F
 call delay_250ms
 goto main_loop

 end