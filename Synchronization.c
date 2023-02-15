#include "Synchronization.h"

/******** Semaphores ********/
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
