/*
 * @file esrms.c
 * @brief Energy-Saving RMS Implementation 
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
	

 void schedule_sim_esrms(int simulation_cycles, int no_cores, int syncsleep_flag, char *power_trace_file, char *temperature_trace_file)
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
	
	struct stats_struct stats;
 
    double *core_power;
    double total_l3_power;
    double l3_power;

    // Trace Arrays
    double **temperature_data;
	double **power_data;

    int *deep_sleep_flag;
    int *extra_sleep;
    int next_sleep_instant;

    // Memory Allocation
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
	deep_sleep_flag = (int*) malloc(no_cores*sizeof(int));
    extra_sleep = (int*) malloc(no_cores*sizeof(int));
	
	//Initialize Temperature Values for Hotspot & McPAT
	for(i=0; i<no_cores; i++)
	{
		temperature[i] = 330;
		stats.max[i] = 0;
		stats.mean[i] = 0;
		stats.min[i] = 0;
		stats.variance[i] = 0;
		sleeper[i].sleeping_flag = 0;
		sleeper[i].time_slept = 0;
		run_queue[i].utilized_cycles = 0;
		extra_sleep[i] = 0;
	}
	
	// Initialize Hotspot
	initialize_hotspot();

	// Simulate the scheduler
	while(sim_count<simulation_cycles)
	{
		// Move tasks to the respective run queues --> Convert this to a function
		move_ready_to_runqueue(&wait_q, run_queue, sim_count);
		// Schedule Tasks on the respective cores
		for(i=0; i<no_cores; i++)
		{
			core_power[i] = 0;
			task_node = rb_first(&run_queue[i].head);
			if(task_node!=NULL && (sim_count % sleeper[i].sleep_period) - sleeper[i].sleep_phase != 0 && sleeper[i].sleeping_flag != 1)
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
				if(task->time_executed == task->C)
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
				run_mcpat(temperature[i], task, &core_power[i], &l3_power, i, (int)(frequencies[MAX_FREQUENCIES-1]*10));
				
		       
			}
			else
			{
		        if((sim_count % sleeper[i].sleep_period) - sleeper[i].sleep_phase == 0)
		        {
		        	core_power[i] = 0;
		        	sleeper[i].sleeping_flag = 1;
		        	sleeper[i].time_slept = 1;
		        	if(extra_sleep[i] > 0)
		        	{
		        		extra_sleep[i]--; 
		        	}
		        	sprintf(temp_trace, "S");
		        }
		        else if(sleeper[i].sleeping_flag == 1)
		        {
		        	core_power[i] = 0;
		        	sleeper[i].time_slept++;
		        	if(sleeper[i].time_slept == sleeper[i].sleeping_time)
			        {
			        	sleeper[i].sleeping_flag = 0;
			        	sleeper[i].time_slept = 0;
			        	if(extra_sleep[i] == 0)
			        		deep_sleep_flag[i] = 0;
			        }
			        if(extra_sleep[i] > 0)
		        	{
		        		extra_sleep[i]--; 
		        	}
			        sprintf(temp_trace, "S");
		        } 
		        else if(task_node == NULL)
				{
					if(extra_sleep[i] == 0)
					{
						deep_sleep_flag[i] = 0;
						task_node = rb_first(&wait_q.head);
						while(task_node != NULL)
						{
							task = container_of(task_node, struct task_struct_sim, task_node);
							if(task->cpuid == i)
							{
								extra_sleep[i] = task->arrival_time - sim_count;
								next_sleep_instant = (((sim_count - sleeper[i].sleep_phase)/sleeper[i].sleep_period)+1)*sleeper[i].sleep_period + sleeper[i].sleep_phase;
								if (extra_sleep[i] >= sleep_time || next_sleep_instant <= task->arrival_time)
								{
									deep_sleep_flag[i] = 1;
								}
								else
								{
									deep_sleep_flag[i] = 0;
								}
								break;
							}
							temp_node = rb_next(task_node);
							task_node = temp_node;
						}
					}
					extra_sleep[i]--;
					if(deep_sleep_flag[i] > 0) 
					{
						core_power[i] = 0;
						sprintf(temp_trace, "S");
					}
					else 
					{
						// Power in Idle State
						core_power[i] = IDLE_POWER;
						sprintf(temp_trace, "I");
					}
				}
				strcat(execution_trace[i], temp_trace);
			}


		}

		if(syncsleep_flag == 1)
		{
			int all_core_sleep = 0;
			for(i=0; i<no_cores; i++)
			{
				if(core_power[i] == 0)
				{
					all_core_sleep++;
				}
			}
			if(all_core_sleep != no_cores)
			{
				for(i=0; i<no_cores; i++)
				{
					if(core_power[i] == 0)
					{
						core_power[i] = IDLE_POWER;
					}
				}
			}
		}
		for(i=0; i<no_cores; i++)
		{
			temperature_data[i][sim_count] = temperature[i];
			power_data[i][sim_count] = core_power[i];
		}
	    // Run Hotspot
	    hotspot_main(sim_step_size, 0, core_power, total_l3_power, temperature, no_cores);
		sim_count++;
	}
	// Get the stats
	// Compute Stats
	compute_stats(power_data, simulation_cycles, no_cores, &stats);
	compute_stats(temperature_data, simulation_cycles, no_cores, &stats);

	// Dump trace data to log file
	if(log_write_flag == 1)
	{
		sprintf(temperature_trace_file, "schedule_output/esrms_data_%d_%ld.temptrace", taskset_counter, sim_timestamp);
		write_trace_to_log_file(temperature_data, temperature_trace_file, simulation_cycles, no_cores);
		sprintf(power_trace_file, "schedule_output/esrms_power_%d_%ld.pow", taskset_counter, sim_timestamp);
		write_trace_to_log_file(power_data, power_trace_file, simulation_cycles, no_cores);
	}
	// Put Tasks back on the wait_q
	move_run_to_wait(&wait_q, run_queue, no_cores);
		
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
	free(power_data);
	free(execution_trace);
	free(temperature_data);
	free(extra_sleep);
	free(deep_sleep_flag);
	return;
}