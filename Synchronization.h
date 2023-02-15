#include "Scheduler.h"

typedef struct {
    uint8_t counter;
    volatile Task_Control_Block_t * waiting_queue;
} counting_semaphore_t;

typedef struct {
    bool available;
	uint8_t lowest_priority; //Lower priority means more important, find another name?
	uint8_t original_priority; //In case we temporarily raise a task's importance for priority inheritance
	volatile Task_Control_Block_t * owner;
	volatile Task_Control_Block_t * waiting_queue;
} mutex_t;

void init_semaphore(counting_semaphore_t* input_semaphore, uint8_t initial_value);
void wait_semaphore (counting_semaphore_t* requested_semaphore);
void signal_semaphore (counting_semaphore_t* requested_semaphore);

void init_mutex(mutex_t * input_mutex);
void acquire_mutex(mutex_t * requested_mutex);
void release_mutex(mutex_t * requested_mutex);
