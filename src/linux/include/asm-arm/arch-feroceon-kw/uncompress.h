/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/autoconf.h>
#include "../arch/arm/mach-feroceon-kw/config/mvSysHwConfig.h"

#define MV_UART0_LSR 	(*(volatile unsigned char *)(INTER_REGS_BASE + 0x12000 + 0x14))
#define MV_UART0_THR	(*(volatile unsigned char *)(INTER_REGS_BASE + 0x12000 + 0x0 ))	 

#define MV_UART1_LSR    (*(volatile unsigned char *)(INTER_REGS_BASE + 0x12100 + 0x14))
#define MV_UART1_THR    (*(volatile unsigned char *)(INTER_REGS_BASE + 0x12100 + 0x0 ))


#define LSR_THRE	0x20

#define DEV_REG		(*(volatile unsigned int *)(INTER_REGS_BASE + 0x40000))
#define CLK_REG         (*(volatile unsigned int *)(INTER_REGS_BASE + 0x2011c))
/*
 * This does not append a newline
 */
static void putstr(const char *s)
{
	unsigned int model;
	
	/* Get dev ID, make sure pex clk is on */
	if((CLK_REG & 0x4) == 0)
	{
		CLK_REG = CLK_REG | 0x4;
		model = (DEV_REG >> 16) & 0xffff;
		CLK_REG = CLK_REG & ~0x4;
	}
	else
		model = (DEV_REG >> 16) & 0xffff;
	
	
	/* DB 6280 is using UART 1 by default */	
	if(model == 0x6280) {
        	while (*s) {
                	while ((MV_UART1_LSR & LSR_THRE) == 0);
                	MV_UART1_THR = *s;

                	if (*s == '\n') {
                        	while ((MV_UART1_LSR & LSR_THRE) == 0);
                        	MV_UART1_THR = '\r';
                	}
                	s++;
        	}
		return;
	}
		
        while (*s) {
		while ((MV_UART0_LSR & LSR_THRE) == 0);
		MV_UART0_THR = *s;
		
                if (*s == '\n') {
                        while ((MV_UART0_LSR & LSR_THRE) == 0); 
                        MV_UART0_THR = '\r';
                }
                s++;
        }
}

/*
 * nothing to do
 */
#define arch_decomp_setup()
#define arch_decomp_wdog()
