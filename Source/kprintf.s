/* KPrintf() function */


		.global _kprintf

_kprintf:       MOVE.L  4(SP),a0
                LEA     8(SP),a1
                MOVEM.L a2/a3/a6,-(SP)
                LEA     StuffChar(PC),a2
                MOVEA.L 4.w,a3
                MOVEA.L a3,a6
                JSR     -522(a6)
                MOVEM.L (SP)+,a2/a3/a6
                RTS

StuffChar:      MOVEA.L a3,a6
                JSR     -516(a6)
                RTS

                .end
