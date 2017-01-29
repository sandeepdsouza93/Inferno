/*
 * @file task_generator.h
 * @brief Task Generation and Import Framework Header File
 * @author Sandeep D'souza 
 * 
 * Copyright (c) Carnegie Mellon University, 2017. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 	1. Redistributions of source code must retain the above copyright notice, 
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
#ifndef __SIM_TASKGEN_INFERNO_H_
#define __SIM_TASKGEN_INFERNO_H_

#include <stdio.h>
#include "scheduler_structures.h"

// Data structure holding a pointer to all the sleep tasks on cores
extern struct sleeping_task *sleeper; 
// Initialize random tasksets
extern int initialize_tasks(struct task_struct_sim *task_list, int worst_case_flag, int number_tasks, double utilization_bound);

// Import tasksets from a file
extern int read_task_files(FILE* task_file, struct task_struct_sim *task_list, struct sleeping_task *sleeper, int phasing_flag);

// Dummy admission test, modify as per need of algorithm -> here just allocating tasks to processors using first fit decreasing and RMS worst case bound:) ;)
extern int admission_test(struct task_struct_sim *task_list, int number_tasks, int number_cores, int scheduling_policy,struct cpu_run_queue *run_queue, struct wait_queue wait_q);

#endif