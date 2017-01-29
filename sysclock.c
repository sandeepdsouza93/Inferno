/*
 * @file sysclock.c
 * @brief SysClock RMS implementation
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
/* Standard Library Imports */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* Import Inferno Libraries */
#include "rbtree.h"            		/* Red-Black Tree Implementation */
#include "interface_hotspot.h" 		/* Hotspot Interface */
#include "rm_scheduling_queues.h"	/* RM Queue Library  */
#include "config.h"					/* Configuration Macros */	
#include "mcpat_interface.h"        /* McPAT Interface */
#include "stats_generator.h"		/* Stats Generation */
#include "trace_logging.h"			/* Data Trace Logging */


// Calculate Sysclock Multiplication factor
void scale_frequency(double *scaling_factor, int no_cores)
{
	struct task_struct_sim *task;
	struct task_struct_sim *next_task;
	struct task_struct_sim *last_task;
	struct rb_node *task_node = NULL;
	struct rb_node *temp_node = NULL;
	struct rb_node *first_node = NULL;
	double *scaling_calculations;
	int i, count = 0;
	int j,k;
	int minima = 10000000;
	int min_index;
	int temp_count;
	int temp_accumulator;
	int *dividing_factor;
	double *ideal_frequencies;
    
    int index;

	scaling_calculations = (double*)malloc((MAX_TASKS+no_cores)*sizeof(double));
	dividing_factor = (int*)malloc((MAX_TASKS+no_cores)*sizeof(int));
	
	task_node = rb_first(&wait_q.head);
	while(task_node!=NULL)
	{
		task = container_of(task_node, struct task_struct_sim, task_node);
		temp_node = rb_next(task_node);
		
		waitqueue_delete(&wait_q.head, task);
		runqueue_add(&run_queue[task->cpuid].head, task);
		
		task_node = temp_node;
	}
	
	// Calculate the Sysclock Scaling
	for(i=0; i<no_cores; i++)
	{
		scaling_factor[i] = 0;
		// Get the list of unique dividing factors
		task_node = rb_first(&run_queue[i].head);
		temp_node = rb_last(&run_queue[i].head);
		last_task = container_of(temp_node, struct task_struct_sim, task_node);
		
		count = 0;
		temp_count = 1;
		while(task_node!=temp_node)
		{
			task = container_of(task_node, struct task_struct_sim, task_node);
			if(temp_count*task->T < last_task->T)
			{
				dividing_factor[count] = temp_count*task->T;
				for(j=0; j<count; j++)
				{
					if(dividing_factor[count] == dividing_factor[j])
					{
						break;
					}
				}
				temp_count++;
				if(j==count)
				{
					count++;
				}
			}
			else
			{
				task_node = rb_next(task_node);
				temp_count = 1;
			}
		}
		dividing_factor[count] = last_task->T;
		count++;
		// Sort List of dividing factors
		for(j=0; j<count; j++)
		{
			minima = 1000000;
			min_index = j;
			for(k=j; k<count; k++)
			{
				if(dividing_factor[k] < minima)
				{
					minima = dividing_factor[k];
					min_index = k;
				}
			}
			temp_count = dividing_factor[j];
			dividing_factor[j] = dividing_factor[min_index];
			dividing_factor[min_index] = temp_count;
		}
		
		// Calculate the scaling factor for the core
		task_node = rb_first(&run_queue[i].head);
		index = 0;
		while(task_node!=NULL)
		{
			task = container_of(task_node, struct task_struct_sim, task_node);
			scaling_calculations[index] = 1000;
			
			for(j=0; j<count; j++)
			{
				if(dividing_factor[j] <= task->T)
				{
					temp_accumulator = 0;
					first_node = rb_first(&run_queue[i].head);
					while(first_node!=task_node)
					{
						next_task = container_of(first_node, struct task_struct_sim, task_node);
						temp_accumulator = temp_accumulator + ceil((double)dividing_factor[j]/next_task->T)*next_task->C;
						first_node = rb_next(first_node);
					}
					temp_accumulator = temp_accumulator + task->C;
					if((double)temp_accumulator/dividing_factor[j] < scaling_calculations[index])
					{
						scaling_calculations[index] = (double)temp_accumulator/dividing_factor[j];
					}
				}
				else
				{
					break;
				}
			}
			task_node = rb_next(task_node);
			index++;
		}
		// Assign the final scaling factor
		for(j=0; j<index; j++)
		{
			if(scaling_calculations[j] > scaling_factor[i])
			{
				scaling_factor[i] = scaling_calculations[j];
			}
		}
	}
	// Calculate scaling factor given fixed frequencies
	ideal_frequencies = (double*)malloc(no_cores*sizeof(double));
	for(i=0; i<no_cores; i++)
	{
		
		ideal_frequencies[i] = scaling_factor[i]*frequencies[MAX_FREQUENCIES-1];
		scaling_factor[i] = (double)frequencies[0]/frequencies[MAX_FREQUENCIES-1];
		for(j=0; j<MAX_FREQUENCIES-1; j++)
		{
			if(frequencies[j] < ideal_frequencies[i])
			{
				scaling_factor[i] = (double)frequencies[j+1]/frequencies[MAX_FREQUENCIES-1];
			}
			
		}

	}
	// Add tasks back to the waitqueue
	for(i=0; i<no_cores; i++)
	{
		task_node = rb_first(&run_queue[i].head);
		while(task_node!=NULL)
		{
			task = container_of(task_node, struct task_struct_sim, task_node);
			temp_node = rb_next(task_node);
	
			runqueue_delete(&run_queue[i].head, task);
			waitqueue_add(&wait_q.head, task);
			
			task_node = temp_node;
		}
	}
	free(scaling_calculations);
	free(dividing_factor);
	return;
	
}

void schedule_sim_sysclock(int simulation_cycles, int no_cores, char *power_trace_file, char *temperature_trace_file)
{
	int sim_count = 0;
	struct task_struct_sim *task;
	struct rb_node *task_node = NULL;
	struct rb_node *temp_node = NULL;

	char *command;
	double *temperature;

	int i;

	char **execution_trace;
	char temp_trace[5];
	
    double *core_power;
    double total_l3_power;
    double l3_power;

    // Trace Arrays
    double **temperature_data;
	double **power_data;

    struct stats_struct stats;

    // Sysclock Specific -> Get the Scaling Factor
    double *scaling_factor = (double*)malloc(no_cores*sizeof(double));
    scale_frequency(scaling_factor, no_cores);


    core_power = (double*)malloc(no_cores*sizeof(double));
	temperature = (double*) malloc((no_cores)*sizeof(double));
	command = (char*) malloc(500*sizeof(char));


	execution_trace = (char**) malloc(no_cores*sizeof(char*));
	temperature_data = (double**) malloc((no_cores)*sizeof(double*));
	power_data = (double**) malloc((no_cores)*sizeof(double*));
	for(i=0; i<no_cores; i++)
	{
		execution_trace[i] = (char*)malloc((simulation_cycles+5)*sizeof(char));
		sprintf(execution_trace[i], "C%d:", i);
		temperature_data[i] = (double*) malloc(simulation_cycles*sizeof(double));
		power_data[i] = (double*) malloc(simulation_cycles*sizeof(double));

	}
	alloc_stats_struct(&stats, no_cores);

	//Initialize Temperature Values for Hotspot & McPAT
	for(i=0; i<no_cores; i++)
	{
		temperature[i] = 300;
		stats.max[i] = 0;
		stats.mean[i] = 0;
		stats.min[i] = 0;
		stats.variance[i] = 0;
		sleeper[i].sleeping_flag = 0;
		sleeper[i].sleeping_time = 0;
		sleeper[i].time_slept = 0;
		run_queue[i].utilized_cycles = 0;

	}
	// Initialize Hotspot
	initialize_hotspot();

	// Simulate the scheduler
	while(sim_count<simulation_cycles)
	{
		// Move tasks to the respective run queues
		move_ready_to_runqueue(&wait_q, run_queue, sim_count);
		// Schedule Tasks on the respective cores
		for(i=0; i<no_cores; i++)
		{
			core_power[i] = 0;
			task_node = rb_first(&run_queue[i].head);
			if(task_node!=NULL)
			{
				// Scheduler part
				task = container_of(task_node, struct task_struct_sim, task_node);
				task->time_executed++;
				run_queue[i].utilized_cycles++;
				// Check Deadline Miss
				if(sim_count >= task->arrival_time + task->T)
				{
					printf("ERROR: Task %d Missed Deadline\n", task->pid);
				}
				if(task->time_executed == ceil((double)task->C/scaling_factor[i]))
				{
					task->time_executed = 0;
					task->arrival_time = task->arrival_time + task->T;
					runqueue_delete(&run_queue[i].head, task);
					run_queue[i].task_count--;
					waitqueue_add(&wait_q.head, task);
					wait_q.task_count++;
				}
				// Add to trace
				sprintf(temp_trace, "X");
				strcat(execution_trace[i], temp_trace);
				
				// Call mcpat_schedule.py to compute core power values 
				run_mcpat(temperature[i], task, &core_power[i], &l3_power, i, (int)(scaling_factor[i]*frequencies[MAX_FREQUENCIES-1]*10));
		        
			}
			else
			{
				if(sleeper[i].sleeping_flag == 0)
				{
					task_node = rb_first(&wait_q.head);
					while(task_node != NULL)
					{
						task = container_of(task_node, struct task_struct_sim, task_node);
						if(task->cpuid == i)
						{
							sleeper[i].sleeping_time = task->arrival_time - sim_count;
							sleeper[i].time_slept = 0;
							
							break;
						}
						temp_node = rb_next(task_node);
						task_node = temp_node;
					}
					sleeper[i].sleeping_flag = 1;
				}
				if(sleeper[i].sleeping_time >= sleep_time) 
				{
					core_power[i] = 0;
					sprintf(temp_trace, "S");
				}
				else 
				{
					core_power[i] = IDLE_POWER;			// Power in Idle State
					sprintf(temp_trace, "I");
				}
				sleeper[i].time_slept++;
				if(sleeper[i].time_slept >= sleeper[i].sleeping_time)
				{
					sleeper[i].sleeping_flag = 0;
				}
			
				strcat(execution_trace[i], temp_trace);
			}

		}
		
 		// Run Hotspot
	    hotspot_main(sim_step_size, 0, core_power, total_l3_power, temperature, no_cores);
	    // Store data in arrays
		for(i=0; i<no_cores; i++)
		{
			temperature_data[i][sim_count] = temperature[i];
			power_data[i][sim_count] = core_power[i];
		}
		sim_count++;
	}
	// Get the stats
	// Compute Stats
	compute_stats(power_data, simulation_cycles, no_cores, &stats);
	compute_stats(temperature_data, simulation_cycles, no_cores, &stats);

	// Dump trace data to log file
	if(log_write_flag == 1)
	{
		sprintf(temperature_trace_file, "schedule_output/sysclock_data_%d_%ld.temptrace", taskset_counter, sim_timestamp);
		write_trace_to_log_file(temperature_data, temperature_trace_file, simulation_cycles, no_cores);
		sprintf(power_trace_file, "schedule_output/sysclock_power_%d_%ld.pow", taskset_counter, sim_timestamp);
		write_trace_to_log_file(power_data, power_trace_file, simulation_cycles, no_cores);
	}
	// Put Tasks back on the wait_q
	move_run_to_wait(&wait_q, run_queue, no_cores);

	//Exit Hotspot
	hotspot_exit();

	// Free all the memory allocated
	free(core_power);
	free(temperature);
	free(command);
	for(i=0; i<no_cores; i++)
	{
		free(temperature_data[i]);
		free(execution_trace[i]);
		free(power_data[i]);
	}
	free_stats_struct(&stats);
	free(execution_trace);
	free(power_data);
	free(temperature_data);
	free(scaling_factor);
	return;
}