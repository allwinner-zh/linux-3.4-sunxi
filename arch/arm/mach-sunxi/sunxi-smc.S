/*
 * sunxi platform smc functions
 */
#include <linux/linkage.h>
#include <linux/init.h>

#define TEE_SMC_NS_IRQ_DONE	(0x0FFFFFF7)
#define TEE_SMC_NS_IRQ_CALL	(0x0FFFFFF6)

ENTRY(sunxi_smc_call)
        push	{r4-r12, lr}
	mrs     r5,  cpsr
        cpsid	if
	mov	r4, r0
	ldm	r4, {r0-r3}
sunxi_issue_smc:
        .arch_extension sec
	smc	#0
	cmp     r0, #TEE_SMC_NS_IRQ_CALL
	beq	sunxi_smc_irq_handle
	stm	r4, {r0-r3}
	msr     cpsr_cxsf, r5
	pop	{r4-r12, pc}
sunxi_smc_irq_handle:
	/*
	 * RIQ raised while in secure world, unmask IRQ temporarily to
	 * receive the IRQ in as it's normally recieved.
	 */
	cpsie	i
	nop
	nop
	nop
	nop
	dsb
        isb
        cpsid	i
        ldr     r0, =TEE_SMC_NS_IRQ_DONE
	b       sunxi_issue_smc            @ Goback to secure world
ENDPROC(sunxi_smc_call)
