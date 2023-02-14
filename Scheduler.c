#include "Scheduler.h"

void ready_again(void){
	//Go through queue and check if a task needs to be reset
	if (Inactive_TCB != NULL) {
		volatile Task_Control_Block_t * TCB_to_check = Inactive_TCB;
		volatile Task_Control_Block_t * next_TCB_to_check;
		volatile Task_Control_Block_t * prev_TCB_checked = NULL;
		bool moved = false;

		while(TCB_to_check != NULL){
			next_TCB_to_check = TCB_to_check->Next_TCB;
			if(msTicks/TCB_to_check->Period >= TCB_to_check->Number_of_Occur) {
				//If its time to run them again move them to the ready queue
				moved = true;
				TCB_to_check->Current_state = Ready;
				if(Ready_TCBs[TCB_to_check->Priority] == NULL){
					Ready_TCBs[TCB_to_check->Priority] = TCB_to_check;
					//Set vector since it was previously empty
					Ready_queue_bit_vector |= 1 << TCB_to_check->Priority;
				}
				else {
					volatile Task_Control_Block_t * Find_last = Ready_TCBs[TCB_to_check->Priority];
					while(Find_last->Next_TCB != NULL) {
						Find_last = Find_last->Next_TCB;
					}
					Find_last->Next_TCB = TCB_to_check;
				}
				//Remove the next TCB so we don't accidently copy it incorrectly
				TCB_to_check->Next_TCB = NULL;
				if (Inactive_TCB == TCB_to_check) {
					Inactive_TCB = next_TCB_to_check;
				}
				else {
					prev_TCB_checked->Next_TCB = next_TCB_to_check;
				}
			}
			if (!moved){
				prev_TCB_checked = TCB_to_check;
			}
			moved = false;
			TCB_to_check = next_TCB_to_check;
		}
	}
}

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
	ready_again();

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
