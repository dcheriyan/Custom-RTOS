#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

uint32_t msTicks = 0;

//Store Task Control Block Information
static uint32_t TCB[6];

//Initialize the Kernel
static bool Kernel_Init(void) {
	bool status = false;

	uint32_t* MainStackBase = (uint32_t*)0x0;
	TCB[5] = *MainStackBase - 0x800;
	TCB[4] = TCB[5] - 0x400;
	TCB[3] = TCB[4] - 0x400;
	TCB[2] = TCB[3] - 0x400;
	TCB[1] = TCB[2] - 0x400;
	TCB[0] = TCB[1] - 0x400;

	status = true;
	return status;
}

void SysTick_Handler(void)
{
    msTicks++;
}


/*
__asm void PendSV_Handler(void) {

	// return from handler
	BX		LR
}
*/
int main(void) {
	SysTick_Config(SystemCoreClock/1000);
	printf("\nStarting...\n\n");

	bool Init_Status = Kernel_Init();

	printf("Kernel Init Status %d \n", Init_Status);

	uint32_t period = 1000; // 1s
	uint32_t prev = -period;
	while(true) {
		if((uint32_t)(msTicks - prev) >= period) {
			printf("tick ");
			prev += period;
		}
	}

}
