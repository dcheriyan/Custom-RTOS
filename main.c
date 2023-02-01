#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

//States a Task can be in
enum State{
	Not_Running,
	Running,
	Preparing_to_Run
};

typedef struct {
    uint8_t Taskid;
	uint32_t Stack_Pointer;		//Functions that use this Address take uint32_t
    uint8_t Priority;
    void * Linked_List;			//May have to change pointer type
    uint8_t Event_Flags;
	enum State Current_State;
} Task_Control_Block;

uint32_t msTicks = 0;

//Store Task Control Block Information
static Task_Control_Block TCB[6];

//Initialize the Kernel based on the configuration
static bool Kernel_Init(void) {
	bool status = false;
	uint32_t* MainStackBase = (uint32_t*)0x0;

	TCB[5].Stack_Pointer = *MainStackBase - 0x800;
	TCB[4].Stack_Pointer = TCB[5].Stack_Pointer - 0x400;
	TCB[3].Stack_Pointer = TCB[4].Stack_Pointer - 0x400;
	TCB[2].Stack_Pointer = TCB[3].Stack_Pointer - 0x400;
	TCB[1].Stack_Pointer = TCB[2].Stack_Pointer - 0x400;
	TCB[0].Stack_Pointer = TCB[1].Stack_Pointer - 0x400;

	status = true;
	return status;
}

//Start Kernel and transform into Idle Function
static void Kernel_Start(void) {
	uint32_t* MainStackBase = (uint32_t*)0x0;
	//BITMASK
	CONTROL_Type Set_Stack_Mask;

	//Set MSP
	__set_MSP(*MainStackBase);
	 printf("MSP after being set is %x \n", __get_MSP());

	//Switch to PSP
	printf("Control before being set is %x \n", __get_CONTROL());
	Set_Stack_Mask = (CONTROL_Type)__get_CONTROL();
	Set_Stack_Mask.b.SPSEL = 1;
	printf("Control to set is %x \n", Set_Stack_Mask.w);
	__set_CONTROL(Set_Stack_Mask.w);

	//Set PSP  to base of Idle Task, TCB[0]
	__set_PSP((uint32_t)TCB[0].Stack_Pointer);
	printf("PSP after being set is %x \n", __get_PSP());

	//Configure Systick
	SysTick_Config(SystemCoreClock/1000);
	printf("\nStarting Systick\n\n");

	//Transform idle task
	TCB[0].Taskid = 0;
	//Lowest Priority
	TCB[0].Priority = 6;
	TCB[0].Current_State = Running;

	uint32_t period = 500; // 0.5s because emulator is slow
	uint32_t prev = -period;
	while(true) {
		//Produce some output so we know idle function is running
		if((uint32_t)(msTicks - prev) >= period) {
			printf("idle tick ");
			prev += period;
		}
	}
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
	bool Init_Status = Kernel_Init();
	printf("Kernel Init Status %d \n", Init_Status);

	Kernel_Start();
}
