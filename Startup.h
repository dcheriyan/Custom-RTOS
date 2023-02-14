#ifndef Startup_H
#define Startup_H

#include <LPC17xx.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/******** Predefined Values ********/
#define MAX_TASKS 6				//We divide the stack into 6, so only 6 tasks can exist at a time
#define NUM_PRIORITIES 6		//Priority Ranges from 0-5, lower is more important

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
	uint16_t Period;
	uint32_t Number_of_Occur;
} Task_Control_Block_t;

/******** Extern Variables ********/
extern volatile Task_Control_Block_t TCBs[6];
extern uint32_t msTicks;
extern volatile Task_Control_Block_t *Current_running_TCB;
extern volatile Task_Control_Block_t *Ready_TCBs[NUM_PRIORITIES];
extern uint32_t Ready_queue_bit_vector;


/******** Functions ********/
bool create_task(rtosTaskFunc_t Func_addr, void *Func_args, uint8_t Assigned_task_id, uint8_t Assigned_priority, uint16_t Assigned_period);

void Kernel_Init(void);

void Kernel_Start(void);

void Idle_function (void *Input_args);

#endif
