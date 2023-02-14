#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "Startup.h"
#include "Scheduler.h"

/******** Global Variables ********/
uint32_t msTicks = 0;
uint32_t timeslice = 50; // 0.05s

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
void Task1_function (void *Input_args){
	uint32_t period = 500; // 0.5s
	uint32_t prev = -period;
	while(true) {
		if((uint32_t)(msTicks - prev) >= period) {
			printf("Task 1... \n");
			prev += period;
			TCBs[1].Current_state = Inactive;
			TCBs[1].Number_of_Occur++;
		}
	}
}

void Task2_function (void *Input_args){
	uint32_t period = 1000; // 1s
	uint32_t prev = -period;
	while(true) {
		if((uint32_t)(msTicks - prev) >= period) {
			printf("Task 2... \n");
			prev += period;
			TCBs[2].Current_state = Inactive;
			TCBs[2].Number_of_Occur++;
		}
	}
}

void Task3_function (void *Input_args){
	uint32_t period = 1000; // 1s
	uint32_t prev = -period;
	while(true) {
		if((uint32_t)(msTicks - prev) >= period) {
			printf("Task 3... \n");
			prev += period;
			TCBs[3].Current_state = Inactive;
			TCBs[3].Number_of_Occur++;
		}
	}
}

void Task4_function (void *Input_args){
	uint32_t period = 1500; // 1.5s
	uint32_t prev = -period;
	while(true) {
		if((uint32_t)(msTicks - prev) >= period) {
			printf("Task 4... \n");
			prev += period;
			TCBs[4].Current_state = Inactive;
			TCBs[4].Number_of_Occur++;
		}
	}
}

int main(void) {
	//Systick is highest prio
	NVIC_SetPriority(SysTick_IRQn, 0);
	//PendSV is lowest prio
	NVIC_SetPriority(PendSV_IRQn, 0xff);

	Kernel_Init();

	bool Task_one_status = create_task(&Task1_function, NULL, 1, 1, 500);
	bool Task_two_status = create_task(&Task2_function, NULL, 2, 2, 1000);
	bool Task_three_status = create_task(&Task3_function, NULL, 3, 2, 1000);
	bool Task_four_status = create_task(&Task4_function, NULL, 4, 4, 1500);

	Kernel_Start();
}
