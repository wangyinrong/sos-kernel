#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <common.h>
#include <sched/thread.h>
#include <sched/task.h>

void init_scheduler(task_t* initial_task);

void schedule();

#endif