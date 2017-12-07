        .arch msp430g2553

        .data
        global BIT0 
BIT0:   .byte 0x01 ; define BIT0

        .data
        global BIT1 
BIT1:   .byte 0x02 ; define BIT1

        .data
        global BIT2 
BIT2:   .byte 0x04 ; define BIT2

        .data
        global BIT3 
BIT3:   .byte 0x08 ; define BIT3

        .data
        global BIT4 
BIT0:   .byte 0x01 ; define BIT0


        .data
movaPaddles:
        sub #6, R1 ; make space for sp and r12

        mov R12, 0(R1) ; save r12 
        call #p2sw_read ;
        mov R12, 2(R1) ; sw = p2sw_read results

        mov 2(R1), 4(R1) ; t = sw
if_0:   and BIT0, 4(R1)
        jnz fi_0
        
        call #movLayerDraw 
        call #mlPaddleAdvance
        jmp out

fi_0:   
        mov 2(R1), 4(R1) ; t = sw
if_1:   and BIT1, 4(R1)
        jnz fi_2
        call #movLayerDraw 
        call #mlPaddleAdvance
        jmp out
fi_1:
        
        mov 2(R1), 4(R1) ; t = sw
if_2:   and BIT2, 4(R1)
        jnz fi_0
        call #movLayerDraw 
        call #mlPaddleAdvance
        jmp out
fi_2:

        mov 2(R1), 4(R1) ; t = sw
if_3:   and BIT3, 4(R1)
        jnz fi_0
        call #movLayerDraw 
        call #mlPaddleAdvance
        jmp out

fi_3:
out:    mov 0(R1), R13
        ret
