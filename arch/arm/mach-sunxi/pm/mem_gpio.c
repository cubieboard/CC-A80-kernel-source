#include "pm_types.h"
#include "pm_i.h"

/*
*********************************************************************************************************
*                                       MEM gpio INITIALISE
*
* Description: mem gpio initialise.
*
* Arguments  : none.
*
* Returns    : 0/-1;
*********************************************************************************************************
*/
__s32 mem_gpio_save(struct gpio_state *pgpio_state)
{
	int i=0;

	/*save all the gpio reg*/
	for(i=0; i<(GPIO_REG_LENGTH); i++){
		pgpio_state->gpio_reg_back[i] = *(volatile __u32 *)(IO_ADDRESS(SUNXI_PIO_PBASE) + i*0x04);
	}
	return 0;
}

/*
*********************************************************************************************************
*                                       MEM gpio INITIALISE
*
* Description: mem gpio initialise.
*
* Arguments  : none.
*
* Returns    : 0/-1;
*********************************************************************************************************
*/
__s32 mem_gpio_restore(struct gpio_state *pgpio_state)
{
	int i=0;

	/*restore all the gpio reg*/
	for(i=0; i<(GPIO_REG_LENGTH); i++){
		 *(volatile __u32 *)(IO_ADDRESS(SUNXI_PIO_PBASE) + i*0x04) = pgpio_state->gpio_reg_back[i];
	}

#ifdef CONFIG_ARCH_SUN8IW1P1
	/* restore watch-dog registers, to avoid IC's bug */
	*(volatile __u32 *)IO_ADDRESS(0x1C20CD8) = 0;
	*(volatile __u32 *)IO_ADDRESS(0x1C20CF8) = 0;
	*(volatile __u32 *)IO_ADDRESS(0x1C20D18) = 0;
#endif

	return 0;
}
