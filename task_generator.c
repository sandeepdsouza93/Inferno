/*
 * @file task_generator.c
 * @brief Task Generation and Import Framework
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "task_generator.h"
#include "config.h"
#include "rm_scheduling_queues.h"

/********************************** Sleeper initialization ****************************************/

struct sleeping_task *sleeper; 

// Csleep_min specifcation
int sleep_time = SLEEP_TIME; 			// in ms

// Scheduler Granularity
double sim_step_size = SIM_STEP_SIZE/MULT_FACTOR;
double original_sim_step_size = SIM_STEP_SIZE/MULT_FACTOR;

// Simulation timestamp
time_t sim_timestamp;

// Initialize Sleeper Data Structure
int initialize_sleeper(struct sleeping_task *sleeper, double csleep, double tsleep, int core_id)
{
	
	sleeper[core_id].sleep_period = ceil(tsleep*MULT_FACTOR);				// T
	sleeper[core_id].sleep_phase = 0;							// phi
	sleeper[core_id].sleeping_flag = 0;							// Sleeping Flag
	sleeper[core_id].time_slept = 0;							// C completed
	sleeper[core_id].sleeping_time = floor(csleep*MULT_FACTOR);				// C
	return 0;
}

// Phase the Sleep Tasks
void phase_indlseep_uni(struct sleeping_task *sleeper, int num_cores)
{
	int i;
	for(i=0; i<num_cores; i++)
	{
		if(i%2 == 0)
			sleeper[i].sleep_phase = 0;
		else
			sleeper[i].sleep_phase = sleeper[i].sleep_period - sleeper[i].sleeping_time;
	}
	return;
}

/******************************* Taskset Generation ***********************************/
// Taskset Counters
int taskset_counter = 0;
int taskset_count = MAX_TASKSETS;
// Implements UUniFast-Discard to perform taskset generation
int UUniFast(int number_tasks, double utilization_bound, double *utilization_array)
{
	double sum;
	double next_sum;
	double task_upper_bound = TASK_UPPER_BOUND;
	int i;
	double random;
	int found_flag;
	int unifast_iterations = 0;
	int terminate_iterations = 1000;	// Iterations to terminate UUniFast-Discard if any task has util > 1
	for(unifast_iterations = 0; unifast_iterations< terminate_iterations; unifast_iterations++)
	{ 
		sum = utilization_bound;
		found_flag = 1;
		for(i=1; i<number_tasks; i++)
		{
			random = (double)(rand() % 10000000)/(double)10000000;
			next_sum = sum*((double)pow(random, ((double)1/((double)(number_tasks - i)))));
			utilization_array[i-1] = sum - next_sum;
			if(utilization_array[i-1] > task_upper_bound)
			{
				found_flag = 0;
				break;
			}
			sum = next_sum;
		}
		if(found_flag == 1)
		{
			utilization_array[i-1] = sum;
			if(utilization_array[i-1] <= task_upper_bound)
			{
				break;
			}
		}
	}
	return 1-found_flag;		// Returns 0 if found, 1 if not
}

// Taskset Initialization Function (Uses UUniFast)
int initialize_tasks(struct task_struct_sim *task_list, int worst_case_flag, int number_tasks, double utilization_bound)
{
	int min_period = sleep_time*3;					// in ms
	int max_period = MAX_PERIOD;					// in ms
	int i = 0;
	int random;
	int harmonic_flag = 0;

	double *utilization_array;
	utilization_array = (double*)malloc(number_tasks*sizeof(double));
	if(utilization_array == NULL)
	{
		printf("ENOMEM\n");
	}
	// Generate utilization array using UUniFast-Discard
	if(UUniFast(number_tasks, utilization_bound, utilization_array))
	{
		printf("Couldnt find a taskset\n");
		return -1;
	}

	while(i < number_tasks)
	{
		task_list[i].pid = i;
		// Randomly Initialize Time Periods
		random = rand();
		if(harmonic_flag == 1 && i == 0)
		{
			task_list[i].T = (random % (2*min_period-min_period)) + min_period;
		}
		else if(harmonic_flag == 1 && i > 0)
		{
			task_list[i].T = ((random % (3)) + 1)*task_list[i-1].T;
		}
		else
		{
			task_list[i].T = (random % (max_period-min_period)) + min_period;
		}
		// Set computation time based on UUniFast
		task_list[i].C = floor(utilization_array[i]*task_list[i].T)+1;
		task_list[i].utilization = utilization_array[i];

		// Randomly set the power folder to run mcpat
		random = rand();
		task_list[i].power_folder = (random % POWER_FOLDERS) + 1;

		// Set arrival times
		if(worst_case_flag == 0)
		{
			random = rand();
			task_list[i].arrival_time = (random % (min_period));
		}
		else
		{
			task_list[i].arrival_time = 0;
		}

		// Do not assign a CPUID -> -1 means not assigned
		task_list[i].cpuid = -1;
		task_list[i].time_executed = 0;
		i++;
	}
	free(utilization_array);
	return i;
}

/*********************************** Import Tasksets from Files ********************************************/
// Read Taskset from Files
int read_task_files(FILE* task_file, struct task_struct_sim *task_list, struct sleeping_task *sleeper, int phasing_flag)
{
	int i, j, csleep_min, num_cores, taskset, core_id, num_tasks, C, T;
	int task_count = 0, ret = 0;
	double util, csleep, tsleep;
	int return_flag = 0;
	int smallest_T = 1000000000;
	
	ret = fscanf(task_file, "%d %lf %d %d", &taskset, &util, &csleep_min, &num_cores);

	// Write util value for result file
	//result_taskset_utilization = util;

	if(ret < 1)
	{
		return -2; // EOF or some scanning Error
	}

	taskset_counter = taskset;

	for(i=0; i<num_cores; i++)
	{
		ret = fscanf(task_file, "%d %d", &core_id, &num_tasks);
		if(ret < 1)
			return -2; // EOF or some scanning Error
		if(num_tasks > 0)
		{
			ret = fscanf(task_file, "%lf %lf", &csleep, &tsleep);
			if(ret < 1)
				return -2; // EOF or some scanning Error
			initialize_sleeper(sleeper, csleep, tsleep, i);
			for(j=0; j<num_tasks; j++)
			{
				ret = fscanf(task_file, "%d %d", &C, &T);
				if(ret < 1)
					return -2; // EOF or some scanning Error
				if(smallest_T > T*MULT_FACTOR)
					smallest_T = T*MULT_FACTOR;
				task_list[task_count].pid = task_count;
				task_list[task_count].C = C*MULT_FACTOR; 						
				task_list[task_count].T = T*MULT_FACTOR; 						
				task_list[task_count].utilization = (double)C/T;	
				task_list[task_count].arrival_time = 0; 			
				task_list[task_count].time_executed = 0; 			
				task_list[task_count].power_folder = 7;
				task_list[task_count].cpuid = i;
				waitqueue_add(&wait_q.head, &task_list[task_count]);
				wait_q.task_count++;
				task_count++;	
			}
		}
		else
		{
			// Discard Taskset if a single core is unused
			return_flag = -1;
		}

	}
	if (return_flag == -1)
	{
		clear_waitqueue(&wait_q);
		return -1;
	}
	// for(i=0; i< num_cores; i++)
	// {
	// 	sleeper[i].sleep_period = smallest_T;
	// 	sleeper[i].sleeping_time = csleep_min*MULT_FACTOR;
	// }
	if(phasing_flag == 1)
	{
		phase_indlseep_uni(sleeper, num_cores);
	}
	sleep_time = csleep_min*MULT_FACTOR;
	return 0;
}

/******************************** Admission tests for different policies *************************/
// Worst Fit Decreasing -> List Scheduling
int admission_test_wfd(struct task_struct_sim *task_list, int number_tasks, int number_cores, int rhs_flag, struct cpu_run_queue *run_queue, struct wait_queue wait_q)
{
	int i,j;
	int temp;
	int temp_storage;
	double max_utilization = 0;
	double target_utilization = 0.4;
	double *cpu_utilization;
	int *worst_fit_order;
	struct task_struct_sim temp_task;

	// RHS specific
	int *rhs_period_flag;
	int *rhs_highest_priority;
	int *rhs_highest_priority_task_index;

	// Set the initial CPU utilization to 0
	cpu_utilization = (double*)malloc(number_cores*sizeof(double));
	worst_fit_order = (int*)malloc(number_cores*sizeof(int));
	for(i=0; i<number_cores; i++)
	{
		cpu_utilization[i] = 0;
		worst_fit_order[i] = i;
	}
	
	// Sort tasks in descending order of utilization
	for(i=0; i<number_tasks; i++)
	{
		max_utilization = 0;
		temp = i;
		for(j=i; j<number_tasks; j++)
		{
			if(max_utilization < task_list[j].utilization)
			{
				max_utilization = task_list[j].utilization;
				temp = j;
			}
		}
		memcpy(&temp_task, &task_list[i], sizeof(struct task_struct_sim));
		memcpy(&task_list[i], &task_list[temp], sizeof(struct task_struct_sim));
		memcpy(&task_list[temp], &temp_task, sizeof(struct task_struct_sim));
	}
	
	// Add to bins here using worst_fit
	temp = 0;						// Keeps track of number of tasks added
	for(i=0; i<number_tasks; i++)
	{
		j = 0;
		if(task_list[i].utilization + cpu_utilization[worst_fit_order[j]] < target_utilization)
		{
			task_list[i].cpuid = worst_fit_order[j];
			cpu_utilization[worst_fit_order[j]] = cpu_utilization[worst_fit_order[j]] + task_list[i].utilization;
			run_queue[worst_fit_order[j]].initialized_utilization = run_queue[worst_fit_order[j]].initialized_utilization + task_list[i].utilization;
			waitqueue_add(&wait_q.head ,&task_list[i]);
			temp++;
			wait_q.task_count++;
		}
		else
		{
			break;
		}
		for(j=0; j<number_cores-1; j++)
		{
			if(cpu_utilization[worst_fit_order[j]] > cpu_utilization[worst_fit_order[j+1]])
			{
				temp_storage = worst_fit_order[j];
				worst_fit_order[j] = worst_fit_order[j+1];
				worst_fit_order[j+1] = temp_storage;
			}
			else
			{
				break;
			}
		}
	}

	if(rhs_flag == 0)
	{
		free(cpu_utilization);
		free(worst_fit_order);
		return temp;
	}

	// RHS specific code
	rhs_highest_priority = (int*)malloc(number_cores*sizeof(int));
	rhs_period_flag = (int*)malloc(number_cores*sizeof(int));
	rhs_highest_priority_task_index = (int*)malloc(number_cores*sizeof(int));

	// Initialize RHS specific data structures
	for(j=0; j<number_cores; j++)
	{
		rhs_period_flag[j] = 0;								// 1 signifies Tsleep = Thighest_priority/2
		rhs_highest_priority[j] = 10000000; 				// RMS notion of priority, here the task period
		rhs_highest_priority_task_index[j] = 0;				// Indexes the highest priority task on a core, allows for setting its phase to zero
	}

	// Find the highest priority task
	for(i=0; i<number_tasks; i++)
	{
		for(j=0; j<number_cores; j++)
		{
			if(task_list[i].cpuid == j)
			{
				if(rhs_highest_priority[j] > task_list[i].T )
				{
					rhs_highest_priority[j] = task_list[i].T;
					rhs_highest_priority_task_index[j] = i;
				}
				break;
			}
		}
	}

	// Find what task period the sleep task needs to use
	for(i=0; i<number_tasks; i++)
	{
		for(j=0; j<number_cores; j++)
		{
			if(task_list[i].cpuid == j)
			{
				if(task_list[i].T <= 2*rhs_highest_priority[j])
					rhs_period_flag[j] = 1;
				
				break;
			}
		}
	}

	// Set the RHS Time Period
	for(j=0; j<number_cores; j++)
	{
		task_list[rhs_highest_priority_task_index[j]].arrival_time = 0;

		// Setup the sleep task data struture
		sleeper[j].time_slept = 0;
		sleeper[j].sleep_phase = task_list[rhs_highest_priority_task_index[j]].arrival_time;
		sleeper[j].sleeping_flag = 0;
		if(rhs_period_flag[j] == 0)
		{
			sleeper[j].sleep_period = rhs_highest_priority[j];
			sleeper[j].sleeping_time = sleep_time;
		}
		else
		{
			sleeper[j].sleep_period = rhs_highest_priority[j]/2;
			sleeper[j].sleeping_time = sleep_time/2;
			rhs_period_flag[0] = 1;
		}

	}
	if(rhs_period_flag[0] == 1)
	{
		sleep_time = sleep_time/2;
	}
	free(rhs_period_flag);
	free(rhs_highest_priority);
	free(rhs_highest_priority_task_index);
	free(cpu_utilization);
	free(worst_fit_order);
	return temp;
}

// Dummy admission test, modify as per need of algorithm -> here just allocating tasks to processors using first fit decreasing and RMS worst case bound:) ;)
int admission_test(struct task_struct_sim *task_list, int number_tasks, int number_cores, int scheduling_policy, struct cpu_run_queue *run_queue, struct wait_queue wait_q)
{
	int admitted_task_count = 0;
	int rhs_flag;
	// RHS
	if(scheduling_policy == 0)
	{
		rhs_flag = 1;
		admitted_task_count = admission_test_wfd(task_list, number_tasks, number_cores, rhs_flag, run_queue, wait_q);
	}
	// RMS
	if(scheduling_policy == 1)
	{
		rhs_flag = 0;
		admitted_task_count = admission_test_wfd(task_list, number_tasks, number_cores, rhs_flag, run_queue, wait_q);
	}
	return admitted_task_count;
}