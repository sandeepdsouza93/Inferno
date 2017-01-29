/*
 * @scheduler_structures.h
 * @brief Contains data structures useful to the scheduler simulation
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
#ifndef __SIM_SCHED_INFERNO_H_
#define __SIM_SCHED_INFERNO_H_
#include "rbtree.h"

// Task Structure
struct task_struct_sim {
	int pid;					// Process ID of the task
	int C; 						// Computation time in ms
	int T; 						// Time Period in ms
	double utilization;			// C/T Utilization
	int arrival_time; 			// Starting offset of task in ms
	int time_executed; 			// Time executed this period
	struct rb_node task_node;	// Node for RB tree
	int power_folder;			// folder number to where the McPAT power calculations lie
	int cpuid;					// CPU id
};

// Forced-Sleep task pdata structure
struct sleeping_task {
	int sleep_period;				// period
	int sleep_phase;				// phasing 
	int sleeping_flag;				// flag indicating if the processor is in deep sleep
	int time_slept;					// time in the sleep period spent in deep sleep
	int sleeping_time;				// forced-sleep duration Csleep
};

// CPU run queue data struture (Implemented as a Red-Black Tree per CPU)
struct cpu_run_queue {
	struct rb_root head;
	struct task_struct_sim *current_task;
	int task_count;
	int utilized_cycles;
	double initialized_utilization;
};

// Wait queue data structure (Implemented as a Red-Black Tree)
struct wait_queue {
	struct rb_root head;
	int task_count;
};

#endif

