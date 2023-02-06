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
//On this hardware system addresses are uint32_t. We use a type def to make our OS more portable
typedef uint32_t address_type;

typedef struct Task_Control_Block{
	void *Top_of_stack;
    uint8_t Priority;
	enum State Current_state;
	uint8_t Event_flags;
	uint8_t Task_id;
	struct Task_Control_Block *Next_TCB;
} Task_Control_Block_t;

/******** Global Variables ********/
const uint8_t MAX_TASKS = 6;
uint32_t msTicks = 0;
static volatile Task_Control_Block_t TCBs[6];
static volatile Task_Control_Block_t *Current_running_TCB;

/******** TCB Related Functions ********/
bool create_task(rtosTaskFunc_t Func_addr, void *Func_args, uint8_t Assigned_task_id, uint8_t Assigned_priority) {
    //Check inputs are Valid
    //Make sure valid taskid
    if (Assigned_task_id > (MAX_TASKS - 1)) {
        return false;
    }

	//Assign sp a non void type so we can do pointer arithmetic with it
    address_type *Current_sp = TCBs[Assigned_task_id].Top_of_stack;
	uint32_t default_value = 0;

	//Explicitly set values registers will hold so expected behavious is clear
    //R4
    *Current_sp = default_value;
    //R5
    *(Current_sp + 1) = default_value;
    //R6
    *(Current_sp + 2) = default_value;
    //R7
    *(Current_sp + 3) = default_value;
    //R8
    *(Current_sp + 4) = default_value;
    //R9
    *(Current_sp + 5) = default_value;
    //R10
    *(Current_sp + 6) = default_value;
    //R11
    *(Current_sp + 7) = default_value;
    //R0 - argument to task function
    *(Current_sp + 8) = (uint32_t)Func_args;
    //R1
    *(Current_sp + 9) = default_value;
    //R2
    *(Current_sp + 10) = default_value;
    //R3
    *(Current_sp + 11) = default_value;
    //R12
    *(Current_sp + 12) = default_value;
    //LR
    *(Current_sp + 13) = default_value;
    //PC - address of task fuction
    *(Current_sp + 14) = (uint32_t)Func_addr;
    //PSR - default value is 0x01000000
    *(Current_sp + 15) = 0x01000000;

    //Finish setting up the corresponding TCB
    TCBs[Assigned_task_id].Priority = Assigned_priority;
    TCBs[Assigned_task_id].Current_state = Ready;

    return true;
}

/******** Task Functions ********/
void Idle_function (void *Input_args){
	uint32_t period = 500; // 0.5s because emulator is slow
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
	uint32_t period = 200; // 0.5s because emulator is slow
	uint32_t prev = -period;
	while(true) {
		//Produce some output so we know idle function is running
		if((uint32_t)(msTicks - prev) >= period) {
			printf("Testing... \n");
			prev += period;
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
	__set_PSP((address_type)TCBs[0].Top_of_stack);

	//Configure Systick
	SysTick_Config(SystemCoreClock/1000);
	printf("\nStarting Systick\n\n");

	//Transform idle task
	TCBs[0].Priority = 0;
	TCBs[0].Current_state = Running;
	Idle_function(NULL);
}

/******** Interrupts ********/
void SysTick_Handler(void) {
    msTicks++;
}


/*
__asm void PendSV_Handler(void) {

	// return from handler
	BX		LR
}
*/

int main(void) {
	Kernel_Init();

	bool Task_one_status = create_task(&Test_function, NULL, 1, 1);

	Kernel_Start();
}
