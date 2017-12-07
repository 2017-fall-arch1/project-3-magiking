        .arch   msp430g2553

        .data
        .global BIT0 
BIT0:   .byte   0x01 ; define BIT0

        .data
        .global BIT1 
BIT1:   .byte   0x02 ; define BIT1

        .data
        .global BIT2 
BIT2:   .byte   0x04 ; define BIT2

        .data
        .global BIT3 
BIT3:   .byte   0x08 ; define BIT3

.data
        .balign 2
        .global movePaddles
        .extern p2sw_read ;

        .extern fieldFence
        .extern ml_plU
        .extern ml_prU
        .extern ml_plD
        .extern ml_prD

        .extern layerPl
        .extern layerPr

        .word   movePaddles
movePaddles:
        sub     #8, R1      ; make space for sp and r12
        mov     R12, 0(R1) ; save r12 
        mov     R13, 2(R1) ; save r13 

        CALL    #p2sw_read ; sw = p2sw_read results
        mov     R12, 4(R1) ; sw => 4(R1)

        mov     4(R1), 6(R1) ; t = sw
        
if_0:   and.b   &BIT0, 6(R1)  ; if( ~(BIT0 & sw))  => condition satisfied when BITO & sw are 0
        jnz     fi_0
        mov     &ml_plU, R12  ; movLayerDraw(&ml_plU, &layerPl)
        mov     &layerPl, R13
        CALL    #movLayerDraw 
        mov     &ml_plU, R12  ; mlPaddleAdvance(&ml_plU, &fieldFence)
        mov     &fieldFence, R13
        CALL    #mlPaddleAdvance

fi_0:   
if_1:   and.b   &BIT1, 6(R1)  ; if (BIT1 ...
        jnz     fi_1
        mov     &ml_plD, R12  ; movLayerDraw(&ml_plD, &layerPl)
        mov     &layerPl, R13
        CALL    #movLayerDraw  
        mov     &ml_plD, R12  ; mlPaddleAdvance(&ml_plD, &fieldFence)
        mov     &fieldFence, R13
        CALL    #mlPaddleAdvance
fi_1:
if_2:   and.b   &BIT2, 6(R1)     ; if (BIT2 ...
        jnz     fi_2
        mov     &ml_prU, R12  ; movLayerDraw(&ml_prU, &layerPr)
        mov     &layerPr, R13
        CALL    #movLayerDraw  
        mov     &ml_prU, R12  ; mlPaddleAdvance(&ml_prU, &fieldFence)
        mov     &fieldFence, R13
        CALL    #mlPaddleAdvance
fi_2:
if_3:   and.b   &BIT3, 6(R1)
        jnz     fi_3
        mov     &ml_prD, R12  ; movLayerDraw(&ml_prD, &layerPr)
        mov     &layerPr, R13
        CALL    #movLayerDraw  
        mov     &ml_prD, R12  ; mlPaddleAdvance(&ml_prD, &fieldFence)
        mov     &fieldFence, R13
        CALL    #mlPaddleAdvance

fi_3:
out:    mov     0(R1), R12  ; clean up
        mov     2(R1), R13  ; clean up
        add     #8, R1      ; restore sp
        ret
