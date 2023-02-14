#ifndef Startup_H
#include "Startup.h"
#endif

/******** Extern Variables ********/
extern volatile Task_Control_Block_t *Prev_TCB;
extern volatile Task_Control_Block_t *Inactive_TCB;
extern volatile uint32_t Previous_top_stack;
extern volatile uint32_t Next_top_stack;

/******** Functions ********/
void ready_again(void);
void scheduler(void);
