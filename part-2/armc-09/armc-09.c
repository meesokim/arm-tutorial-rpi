/*

    Part of the Raspberry-Pi Bare Metal Tutorials
    Copyright (c) 2013, Brian Sidebotham
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
        this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice,
        this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

*/

/*
    C-Library stubs introduced for newlib
*/

#include <string.h>
#include <stdlib.h>

#include "rpi-gpio.h"
#include "barrier.h"

#define RPMV_D0	(1 << 0)
#define MD00_PIN	0
#define RA08	(1 << 8)
#define RA09	(1 << 9)
#define RA10	(1 << 10)
#define RA11	(1 << 11)
#define RA12	(1 << 12)
#define RA13	(1 << 13)
#define RA14	(1 << 14)
#define RA15	(1 << 15)
#define LE_A	(1 << 16)
#define LE_B	(1 << 17)
#define LE_D	(1 << 26)
#define RCLK	(1 << 25)
#define RWAIT	(1 << 18)
#define RST0	(1 << 19)
#define RST		(1 << 23)
#define RINT	(1 << 20)
#define RBUSDIR (1 << 22)

#define SLTSL	RA11
#define MREQ	RA10
#define IORQ	RA12
#define RD		RA08
#define WR		RA14
#define RESET	RA13

#define MMUTABLEBASE 0x00004000

static unsigned char ROM[] = {
//#include "Antarctic.bin"
#include "Gradius.bin"
//#include "zemix.bin"
};	

/** GPIO Register set */
volatile unsigned int* gpio = (unsigned int*)GPIO_BASE;

void PUT32 ( unsigned int a, unsigned int b)
{
	*(unsigned int *)a = b;
}
unsigned int GET32 ( unsigned int a)
{
	return *(unsigned int *)a;
}
extern void start_mmu ( unsigned int, unsigned int );
extern void stop_mmu ( void );
extern void invalidate_tlbs ( void );
extern void invalidate_caches ( void );

//-------------------------------------------------------------------
unsigned int mmu_section ( unsigned int vadd, unsigned int padd, unsigned int flags )
{
    unsigned int ra;
    unsigned int rb;
    unsigned int rc;

    ra=vadd>>20;
    rb=MMUTABLEBASE|(ra<<2);
    rc=(padd&0xFFF00000)|0xC00|flags|2;
    PUT32(rb,rc);
    return(0);
}
//-------------------------------------------------------------------
unsigned int mmu_small ( unsigned int vadd, unsigned int padd, unsigned int flags, unsigned int mmubase )
{
    unsigned int ra;
    unsigned int rb;
    unsigned int rc;

    ra=vadd>>20;
    rb=MMUTABLEBASE|(ra<<2);
    rc=(mmubase&0xFFFFFC00)/*|(domain<<5)*/|1;
    PUT32(rb,rc); //first level descriptor
    ra=(vadd>>12)&0xFF;
    rb=(mmubase&0xFFFFFC00)|(ra<<2);
    rc=(padd&0xFFFFF000)|(0xFF0)|flags|2;
    PUT32(rb,rc); //second level descriptor
    return(0);
}

/** Main function - we'll never return from here */
void kernel_main( unsigned int r0, unsigned int r1, unsigned int atags )
{
    int loop;
    unsigned int* counters;
	unsigned char *addr;
	unsigned int addr0;
	unsigned char byte;
	unsigned int ra;
	int i = 0;
	int page[8] = {0,0,0,1,2,3,4,5};
	int mapper = 1;
	int signal = 0;
	int pg = 0;
	gpio[GPIO_GPFSEL1] = 0x49249249;
	gpio[GPIO_GPCLR0] = RWAIT;

    for(ra=0;;ra+=0x00100000)
    {
        mmu_section(ra,ra,0x0010);
        if(ra==0xFFF00000) break;
    }	
    //peripherals	
    mmu_section(0x20000000,0x20000000,0x0000); //NOT CACHED!
    mmu_section(0x20200000,0x20200000,0x0000); //NOT CACHED!	
    start_mmu(MMUTABLEBASE,0x00000001|0x1000|0x0004); //[23]=0 subpages enabled = legacy ARMv4,v5 and v6 
    /* Set the LED GPIO pin to an output to drive the LED */

    gpio[GPIO_GPFSEL0] = 0x49249249;
	gpio[GPIO_GPFSEL1] = 0x49249249;
	gpio[GPIO_GPFSEL2] = (1 << 12) | (1 << (3 * 3));
	gpio[GPIO_GPCLR0] = LE_A | 0xffff;
	gpio[GPIO_GPSET0] = LE_B | RWAIT;
	flushcache(); dmb();

	if (sizeof(ROM) > 32768)
		mapper = 1;
	if (!mapper)
	{
		while(1)
		{
			signal = ~gpio[GPIO_GPLEV0];
			if (signal & SLTSL)
			{
				if (signal & RD)
				{
					gpio[GPIO_GPCLR0] =  0xff | LE_B; 
					gpio[GPIO_GPSET0] = (LE_A);
					flushcache(); dmb();
					addr0 = gpio[GPIO_GPLEV0] & 0xffff;
					pg = (addr0 & 0xe000)>>13;
					byte = ROM[page[pg] * 0x2000 + (addr0 & 0x1fff)];
					gpio[GPIO_GPSET0] = LE_B | byte;
					flushcache(); dmb();
					gpio[GPIO_GPCLR0] = (LE_A);
					flushcache(); dmb();
				}
				else
					continue;
				while(!(gpio[GPIO_GPLEV0] & SLTSL));
			}
		}
	} else
	{
		while(1)
		{
			signal = ~gpio[GPIO_GPLEV0];
			if (signal & SLTSL)
			{
				if (signal & RD)
				{
					gpio[GPIO_GPCLR0] =  0xff | LE_B; 
					gpio[GPIO_GPSET0] = (LE_A);
					flushcache(); dmb();
					addr0 = gpio[GPIO_GPLEV0] & 0xffff;
					pg = (addr0 & 0xe000)>>13;
					byte = ROM[page[pg] * 0x2000 + (addr0 & 0x1fff)];
					gpio[GPIO_GPSET0] = LE_B | byte;
					gpio[GPIO_GPCLR0] = (LE_A);
					flushcache(); dmb();
				}
				else if (signal & WR)
				{
					byte = 0x1f & gpio[GPIO_GPLEV0];
					gpio[GPIO_GPCLR0] =  0xff | LE_B; 
					gpio[GPIO_GPSET0] = (LE_A);
					flushcache(); dmb();
					addr0 = gpio[GPIO_GPLEV0] & 0xffff;
					pg = (addr0 & 0xe000)>>13;
					if ((!(addr0 & 0x1fff) & pg > 2) || (!(addr0 & 0xfff)))
						page[pg] = byte;
					gpio[GPIO_GPSET0] = LE_B;
					gpio[GPIO_GPCLR0] = (LE_A);
					flushcache(); dmb(); 
				}
				else
					continue;
				while(!(gpio[GPIO_GPLEV0] & SLTSL));
			}			
		}
	}
}
