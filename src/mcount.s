 .syntax unified
 .arch armv7-a
 
.globl __gnu_mcount_nc
.type __gnu_mcount_nc, %function

.globl mcount
.type mcount, %function
 
__gnu_mcount_nc:
 push {r0, r1, r2, r3, lr} /* save registers */
 bic r1, lr, #1 /* R1 contains callee address, with thumb bit cleared */
 ldr r0, [sp, #20] /* R0 contains caller address */
 bic r0, r0, #1 /* clear thumb bit */
 bl _mcount_internal(PLT) /* jump to internal mcount implementation */
 pop {r0, r1, r2, r3, ip, lr} /* restore saved registers */
 bx ip /* return to callee */

mcount:
 tst lr, #1
 movne r1, r7
 moveq r1, r11
 ldr r0, [r1, #4]
 bic r0, r0, #1
 bic r1, lr, #1
 b _mcount_internal(PLT)

/* TODO remove (PLT) ? */
