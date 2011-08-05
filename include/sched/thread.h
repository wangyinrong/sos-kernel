#ifndef THREAD_H
#define THREAD_H

#include <common.h>

#define THREAD_RUNNING	1
#define THREAD_WAITING	2
#define THREAD_DYING	4

struct thread {
	uint32_t esp, ebp, ebx, esi, edi, eflags;
	// better leave above variables as they are (thread.s relies on that)
	uint32_t tid;
	uint8_t state;
};
typedef struct thread thread_t;

thread_t* init_threading();

uint32_t* allocate_stack(uint32_t size);

thread_t* create_thread(int(*fn)(void*), void* arg, uint32_t* stack);

void destroy_thread(thread_t* thread);

void switch_thread(thread_t* next);

#endif
