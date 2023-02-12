#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/******** Data Structures ********/
//States a Task can be in
enum State{
	Inactive,
	Running,
	Ready,
    Blocked
};

typedef void (*rtosTaskFunc_t)(void *args);

typedef struct Task_Control_Block{
	void *Top_of_stack;
    uint8_t Priority;
	enum State Current_state;
	uint8_t Event_flags;
	uint8_t Task_id;
	volatile struct Task_Control_Block *Next_TCB;
} Task_Control_Block_t;

/******** Global Variables ********/
#define MAX_TASKS 6
//Priority Ranges from 0-5, lower is more important
#define NUM_PRIORITIES 6
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

/******** Scheduler ********/
void scheduler(void){
	//Load SP into prev task TCB after a context switch if necessary
	if((Prev_TCB != NULL) && (Prev_TCB != Current_running_TCB)) {
		Prev_TCB->Top_of_stack = (void *) Previous_top_stack;
	}

	//Update prev to the last task to run
	Prev_TCB = Current_running_TCB;

	//If current is finished store in Finished queue
	if(Current_running_TCB->Current_state == Inactive) {
		Current_running_TCB->Next_TCB = Inactive_TCB;
		Inactive_TCB = Current_running_TCB;
	}
	else {
		//If not finished set to ready, and push to back of its the ready queue
		Current_running_TCB->Current_state = Ready;

		if(Ready_TCBs[Current_running_TCB->Priority] == NULL){
			Ready_TCBs[Current_running_TCB->Priority] = Current_running_TCB;
			//Set vector since it was previously empty
			Ready_queue_bit_vector |= 1 << Current_running_TCB->Priority;
		}
		else {
			volatile Task_Control_Block_t * Find_last = Ready_TCBs[Current_running_TCB->Priority];
			while(Find_last->Next_TCB != NULL){
				Find_last = Find_last->Next_TCB;
			}
			Find_last->Next_TCB = Current_running_TCB;
		}
	}

	//Check if any finished should be moved back to ready

	//Find highest prio ready
	uint8_t Top_priority = __builtin_ctz( Ready_queue_bit_vector );

	//remove from queue and Set to running
	Current_running_TCB = Ready_TCBs[Top_priority];
	Current_running_TCB->Current_state = Running;

	if (Current_running_TCB->Next_TCB == NULL) {
		Ready_TCBs[Top_priority] = NULL;
		Ready_queue_bit_vector &= ~(1 << Current_running_TCB->Priority);
	}
	else {
		Ready_TCBs[Top_priority] = Current_running_TCB->Next_TCB;
	}

	//If the new one is different from the previous cause a context switch
	if(Current_running_TCB != Prev_TCB){
		Next_top_stack = (uint32_t) Current_running_TCB->Top_of_stack;
		//Cause an Exception to for a context switch
		SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	}
}

void ready_again(void){

}

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

/******** TCB Related Functions ********/
bool create_task(rtosTaskFunc_t Func_addr, void *Func_args, uint8_t Assigned_task_id, uint8_t Assigned_priority) {
    //Check inputs are Valid
    //Make sure valid taskid
    if (Assigned_task_id > (MAX_TASKS - 1)) {
        return false;
    }

	//Explicitly set values registers will hold so expected behavious is clear
	uint32_t default_value = 0;
	//Using the Top of Stack directly because of an error using an intermediate variable. Maybe revisit later to make whats happening more obvious

	//PSR - default value is 0x01000000
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 1) = 0x01000000;
	//PC - address of task fuction
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 2) = (uint32_t)Func_addr;
	//LR
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 3) = default_value;
	//R12
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 4) = default_value;
	//R3
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 5) = default_value;
	//R2
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 6) = default_value;
    //R1
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 7) = default_value;
    //R0 - argument to task function
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 8) = (uint32_t)Func_args;
    //R11
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 9) = 0xFFFFFF11;
    //R10
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 10) = default_value;
    //R9
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 11) = default_value;
    //R8
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 12) = default_value;
    //R7
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 13) = default_value;
    //R6
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 14) = default_value;
	//R5
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 15) = default_value;
    //R4
    *((uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 16) = 0xFFFFFFF4;

    //Finish setting up the corresponding TCB
    TCBs[Assigned_task_id].Priority = Assigned_priority;
    TCBs[Assigned_task_id].Current_state = Ready;
    TCBs[Assigned_task_id].Top_of_stack = (uint32_t *) TCBs[Assigned_task_id].Top_of_stack - 16;

	//Load TCB into the Ready Queue
	if(Ready_TCBs[Assigned_priority] == NULL){
		Ready_TCBs[Assigned_priority] = &TCBs[Assigned_task_id];
		//Set vector since it was previously empty
		Ready_queue_bit_vector |= 1 << Assigned_priority;
	}
	else {
		volatile Task_Control_Block_t * Find_last = Ready_TCBs[Assigned_priority];
		while(Find_last->Next_TCB != NULL) {
			Find_last = Find_last->Next_TCB;
		}
		Find_last->Next_TCB = &TCBs[Assigned_task_id];
	}

    return true;
}

/******** Task Functions ********/
void Idle_function (void *Input_args){
	uint32_t period = 200; // 0.2s
	uint32_t prev = -period;
	while(true) {
		//Produce some output so we know idle function is running
		if((uint32_t)(msTicks - prev) >= period) {
			printf("idle tick \n");
			prev += period;
		}
	}
}

void Test_function (void *Input_args){
	uint32_t period = 100; // 0.1s
	uint32_t prev = -period;
	while(true) {
		if((uint32_t)(msTicks - prev) >= period) {
			printf("Testing... \n");
			prev += period;
			TCBs[1].Current_state = Inactive;
		}
	}
}


/******** Kernel Functions ********/
//Initialize the Kernel based on the configuration
static void Kernel_Init(void) {
	const uint32_t *MainStackBase = (uint32_t *)0x0;

	//Revisit Assigning Task_ids based on num task created so far later
	TCBs[5].Top_of_stack = (void *)(*MainStackBase - 0x800);
	TCBs[5].Task_id = 5;
	TCBs[4].Top_of_stack = (void *)((uint32_t)(TCBs[5].Top_of_stack) - 0x400);
	TCBs[4].Task_id = 4;
	TCBs[3].Top_of_stack = (void *)((uint32_t)(TCBs[4].Top_of_stack) - 0x400);
	TCBs[3].Task_id = 3;
	TCBs[2].Top_of_stack = (void *)((uint32_t)(TCBs[3].Top_of_stack) - 0x400);
	TCBs[2].Task_id = 2;
	TCBs[1].Top_of_stack = (void *)((uint32_t)(TCBs[2].Top_of_stack) - 0x400);
	TCBs[1].Task_id = 1;
	TCBs[0].Top_of_stack = (void *)((uint32_t)(TCBs[1].Top_of_stack) - 0x400);
	TCBs[0].Task_id = 0;
}

//Start Kernel and transform into Idle Function
static void Kernel_Start(void) {
	const uint32_t *MainStackBase = (uint32_t *)0x0;
	//BITMASK
	CONTROL_Type Set_Stack_Mask;

	//Set MSP
	__set_MSP(*MainStackBase);

	//Switch to PSP
	Set_Stack_Mask = (CONTROL_Type)__get_CONTROL();
	Set_Stack_Mask.b.SPSEL = 1;
	__set_CONTROL(Set_Stack_Mask.w);

	//Set PSP  to base of Idle Task, TCB[0]
	__set_PSP((uint32_t)TCBs[0].Top_of_stack);

	//Configure Systick
	SysTick_Config(SystemCoreClock/1000);
	printf("\nStarting Systick\n\n");

	//Transform idle task
	TCBs[0].Priority = 5;
	TCBs[0].Current_state = Running;
	Current_running_TCB = &(TCBs[0]);

	Idle_function(NULL);
}

int main(void) {
	//Systick is highest prio
	NVIC_SetPriority(SysTick_IRQn, 0);
	//PendSV is lowest prio
	NVIC_SetPriority(PendSV_IRQn, 0xff);

	Kernel_Init();

	bool Task_one_status = create_task(&Test_function, NULL, 1, 1);
	Next_top_stack = (uint32_t)TCBs[1].Top_of_stack;

	Kernel_Start();
}
