#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "Startup.h"
#include "Scheduler.h"

/******** Global Variables ********/
uint32_t msTicks = 0;
uint32_t timeslice = 100; // 0.1s

volatile Task_Control_Block_t TCBs[6];

//Scheduler
volatile Task_Control_Block_t *Current_running_TCB;
volatile Task_Control_Block_t *Prev_TCB = NULL;

volatile Task_Control_Block_t *Ready_TCBs[NUM_PRIORITIES];
volatile Task_Control_Block_t *Inactive_TCB;

uint32_t Ready_queue_bit_vector;

//Context Switch
//Initialize to an obvious invalid address so we can tell if it has been modifed or not
volatile uint32_t Previous_top_stack = (uint32_t)-1;
volatile uint32_t Next_top_stack;

/******** Interrupts ********/
void SysTick_Handler(void) {
    msTicks++;
	if (msTicks%timeslice == 0){
		scheduler();
	}
}

__asm void PendSV_Handler(void) {
	//Get Current Stack Pointer
	MRS R0, PSP

	//Push register R4-R11 onto the current task's stack
    STMDB R0!, {R4-R11}

    //Record current stack pointer
	LDR R1, =__cpp(&Previous_top_stack)
	STR R0, [R1]

	//Still need a way to get this from the global to its TCB

	//get the new stack pointer from a global
    LDR R1, =__cpp(&Next_top_stack)
	LDR R0, [R1]

	// pop R4-R11 for the new task
	LDMIA R0!, {R4-R11}

	//Set New Stack Pointer
	MSR PSP, R0

	// return from handler
	BX		LR
}

/******** Task Functions ********/
void Test_function (void *Input_args){
	uint32_t period = 400; // 0.4s
	uint32_t prev = -period;
	while(true) {
		if((uint32_t)(msTicks - prev) >= period) {
			printf("Testing... \n");
			prev += period;
			TCBs[1].Current_state = Inactive;
			TCBs[1].Number_of_Occur++;
		}
	}
}

int main(void) {
	//Systick is highest prio
	NVIC_SetPriority(SysTick_IRQn, 0);
	//PendSV is lowest prio
	NVIC_SetPriority(PendSV_IRQn, 0xff);

	Kernel_Init();

	bool Task_one_status = create_task(&Test_function, NULL, 1, 1, 400);
	Next_top_stack = (uint32_t)TCBs[1].Top_of_stack;

	Kernel_Start();
}
