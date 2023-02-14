#include "Startup.h"

/******** TCB Related Functions ********/
bool create_task(rtosTaskFunc_t Func_addr, void *Func_args, uint8_t Assigned_task_id, uint8_t Assigned_priority, uint16_t Assigned_period) {
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
	TCBs[Assigned_task_id].Period = Assigned_period;
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

/******** Kernel Functions ********/
//Initialize the Kernel based on the configuration
void Kernel_Init(void) {
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
void Kernel_Start(void) {
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
