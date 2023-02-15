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

/******** Semaphores ********/

typedef struct {
    uint8_t counter;
    volatile Task_Control_Block_t * waiting_queue;
} counting_semaphore_t;

//init - initialize counter value 's'
void init_semaphore(counting_semaphore_t* input_semaphore, uint8_t initial_value){
    input_semaphore->counter = initial_value;
    input_semaphore->waiting_queue = NULL;
}

//wait - try to decrement, block if 's' = 0
void wait_semaphore (counting_semaphore_t* requested_semaphore) {
    while (requested_semaphore->counter == 0){
        if (Current_running_TCB->Current_state != Blocked) {
            //block
            Current_running_TCB->Current_state = Blocked;
            Current_running_TCB->Next_TCB = requested_semaphore->waiting_queue;
            requested_semaphore->waiting_queue = Current_running_TCB;
        }
    }

    //decrement counter
    requested_semaphore->counter--;
}

//signal - increment 's'
void signal_semaphore (counting_semaphore_t* requested_semaphore) {
    requested_semaphore->counter++;
	volatile Task_Control_Block_t * TCB_to_remove;

    //unblock all, scheduler will ensure highest prio runs first, assuming scheduler does not interrupt here for now (later use a better data structure to ensure highest prio is first removed)
    while (requested_semaphore->waiting_queue != NULL){
        //unblock
		TCB_to_remove = requested_semaphore->waiting_queue;
		requested_semaphore->waiting_queue = TCB_to_remove->Next_TCB;
		TCB_to_remove->Current_state = Ready;
        insert_into_ready(TCB_to_remove);
		//detach from waiting queue
        TCB_to_remove->Next_TCB = NULL;
    }
}

/******** Mutex ********/
typedef struct {
    bool available;
	uint8_t lowest_priority; //Lower priority means more important, find another name?
	uint8_t original_priority; //In case we temporarily raise a task's importance for priority inheritance
	volatile Task_Control_Block_t * owner;
	volatile Task_Control_Block_t * waiting_queue;
} mutex_t;

void init_mutex(mutex_t * input_mutex) {
	input_mutex->available = true;
	input_mutex->lowest_priority = 5;
	input_mutex->original_priority = 5;
	input_mutex->owner = NULL;
	input_mutex->waiting_queue = NULL;
}

void acquire_mutex(mutex_t * requested_mutex) {
	while (!requested_mutex->available) {
		if (Current_running_TCB->Current_state != Blocked) {
            //block
            Current_running_TCB->Current_state = Blocked;
            Current_running_TCB->Next_TCB = requested_mutex->waiting_queue;
            requested_mutex->waiting_queue = Current_running_TCB;
			//Update lowest priority for priority inheritance
			if (Current_running_TCB->Priority < requested_mutex->lowest_priority) {
				requested_mutex->lowest_priority = Current_running_TCB->Priority;
//				volatile Task_Control_Block_t * TCB_to_remove;
				//remove owner from Ready TCB
				remove_from_ready(requested_mutex->owner);
				requested_mutex->owner->Priority = Current_running_TCB->Priority;
				//reinsert owner into readyTCB
				insert_into_ready(requested_mutex->owner);
			}
        }
	}
	requested_mutex->available = false;
	requested_mutex->owner = Current_running_TCB;
	requested_mutex->original_priority = Current_running_TCB->Priority;
	requested_mutex->lowest_priority = Current_running_TCB->Priority;
}

void release_mutex(mutex_t * requested_mutex) {
	//Check for owner
	if (Current_running_TCB == requested_mutex->owner) {
		//Release mutex
		requested_mutex->available = true;
		//Return to original priority
		Current_running_TCB->Priority = requested_mutex->original_priority;
	}
	//unblock all waiting

	volatile Task_Control_Block_t * TCB_to_remove;

    //unblock all, scheduler will ensure highest prio runs first, assuming scheduler does not interrupt here for now (later use a better data structure to ensure highest prio is first removed)
    while (requested_mutex->waiting_queue != NULL){
        //unblock
		TCB_to_remove = requested_mutex->waiting_queue;
		requested_mutex->waiting_queue = TCB_to_remove->Next_TCB;
		TCB_to_remove->Current_state = Ready;
        insert_into_ready(TCB_to_remove);
		//detach from waiting queue
        TCB_to_remove->Next_TCB = NULL;
    }
}

/******** Synchronization Global Variables ********/
counting_semaphore_t Ready_to_Write;
counting_semaphore_t Ready_to_Read;
mutex_t test_mutex;

uint16_t mail;

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
	uint32_t period = 200; // 0.2s
	uint32_t prev = -period;
	while(true) {
		if((uint32_t)(msTicks - prev) >= period) {
			acquire_mutex(&test_mutex);
			printf("Task 1 @ %d \n", msTicks);
			release_mutex(&test_mutex);
			prev += period;
			TCBs[1].Current_state = Inactive;
			TCBs[1].Number_of_Occur++;
		}
	}
}


//Producer
void Task2_function (void *Input_args){
	uint32_t period = 400; // 0.4s
	uint32_t prev = -period;
	while(true) {
		if((uint32_t)(msTicks - prev) >= period) {
			printf("Task 2 @ %d \n", msTicks);
			wait_semaphore(&Ready_to_Write);
			printf("Writing mail \n");
			mail = msTicks;
			signal_semaphore(&Ready_to_Read);
			prev += period;
			TCBs[2].Current_state = Inactive;
			TCBs[2].Number_of_Occur++;
		}
	}
}

//Consumer
void Task3_function (void *Input_args){
	uint32_t period = 300; // 0.3s
	uint32_t prev = -period;
	while(true) {
		if((uint32_t)(msTicks - prev) >= period) {
			printf("Task 3 @ %d \n", msTicks);
			wait_semaphore(&Ready_to_Read);
			printf("Mail is: %d \n", mail);
			mail = 0;
			signal_semaphore(&Ready_to_Write);
			prev += period;
			TCBs[3].Current_state = Inactive;
			TCBs[3].Number_of_Occur++;
		}
	}
}

void Task4_function (void *Input_args){
	uint32_t period = 700; // 0.7s
	uint32_t prev = -period;
	while(true) {
		if((uint32_t)(msTicks - prev) >= period) {
			acquire_mutex(&test_mutex);
			while(TCBs[1].Number_of_Occur*250 > msTicks){
				//busy loop to force priority inheritance
			}
			release_mutex(&test_mutex);
			printf("Task 4 @ %d \n", msTicks);
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

	init_mutex(&test_mutex);
	init_semaphore(&Ready_to_Write, 1);
	init_semaphore(&Ready_to_Read, 0);

	bool Task_one_status = create_task(&Task1_function, NULL, 1, 1, 200);
	bool Task_two_status = create_task(&Task2_function, NULL, 2, 2, 400);
	bool Task_three_status = create_task(&Task3_function, NULL, 3, 2, 300);
	bool Task_four_status = create_task(&Task4_function, NULL, 4, 4, 700);

	Kernel_Start();
}
