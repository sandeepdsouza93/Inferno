/*
 * @rm_scheduling_queues.c
 * @brief Rate-Monotonic Scheduler Task Management
 * @author Sandeep D'souza 
 * 
 * Copyright (c) Carnegie Mellon University, 2017. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, 
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef __SIM_RMQ_INFERNO_H_
#define __SIM_RMQ_INFERNO_H_

#include <stdlib.h>
#include "rbtree.h"
//#include "scheduler_structures.h"
#include "task_generator.h"

// Global Variables for the wait and run queues
extern struct cpu_run_queue *run_queue;
extern struct wait_queue wait_q;

// Add a task to the Run Queues
extern int runqueue_add(struct rb_root *head, struct task_struct_sim *task);

// Remove a task from the run queue
extern void runqueue_delete(struct rb_root *head, struct task_struct_sim *task);

// Remove all tasks from all runqueues
extern void clear_all_runqueues(struct cpu_run_queue *run_queue, int number_cores);

// Add a task to the waitqueue
extern int waitqueue_add(struct rb_root *head, struct task_struct_sim *task);               

// Remove a task from the waitqueue
extern void waitqueue_delete(struct rb_root *head, struct task_struct_sim *task);

// Clear the waitqueue of all the tasks
extern void clear_waitqueue(struct wait_queue *wait_q);

// Move all tasks from run queues to wait queues
extern void move_run_to_wait(struct wait_queue *wait_q, struct cpu_run_queue *run_queue, int no_cores);

// Move ready tasks to the runqueue
extern void move_ready_to_runqueue(struct wait_queue *wait_q, struct cpu_run_queue *run_queue, int sim_count);

#endif