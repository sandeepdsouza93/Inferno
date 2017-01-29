/*
 * @schedule_inferno.c
 * @brief Inferno Thermal and Power Scheduling Simulator Main file
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
#include "config.h"					/* Configuration Macros */	
#include "task_generator.h"			/* Task generation/import framework */
#include "mcpat_interface.h"		/* McPAT Interface */
#include "stats_generator.h"  
#include "rm_scheduling_queues.h"	/* RM Queue Library  */      
#include "rms.h"
#include "esrms.h"
#include "esrhsp.h"
#include "sysclock.h"

// Input Files
FILE* esrms_same;
FILE* esrms_diff;
FILE* esrhsp_same;
FILE* esrhsp_diff;


/************************************** Taskset Import Functionality ************************************/
// Open Taskset from Files
void open_task_files(void)
{
	esrms_same = fopen("taskset_input/esrms_same_period_indsleep.txt", "r");
	esrms_diff = fopen("taskset_input/esrms_diff_period_indsleep.txt", "r");
	esrhsp_same = fopen("taskset_input/esrhsp_same_period_indsleep.txt", "r");
	esrhsp_diff = fopen("taskset_input/esrhsp_diff_period_indsleep.txt", "r");
}

// Close Taskset input Files
void close_task_files()
{
	fclose(esrms_same);
	fclose(esrhsp_same);
	fclose(esrms_diff);
	fclose(esrhsp_diff);
}

// Will run different schedulers for each taskset
void schedule_sim(int simulation_cycles, int no_cores, char* power_output_file, char* temperature_output_file, int global_syncsleep_flag)
{
	printf("Running ES-RHS+\n");
	schedule_sim_esrhsp(simulation_cycles, no_cores, global_syncsleep_flag, power_output_file, temperature_output_file);
	printf("Running ES-RMS\n");
	schedule_sim_esrms(simulation_cycles, no_cores, global_syncsleep_flag, power_output_file, temperature_output_file);
	printf("Running Sysclock\n");
	schedule_sim_sysclock(simulation_cycles, no_cores, power_output_file, temperature_output_file);
}

int main(int argc, char **argv)
{
	int retval = 0;											// Return value of functions
	int worst_case_flag = 1; 								// All tasks come in together in the worst case 
	int number_tasks = MAX_TASKS;							// Number of tasks
	int number_cores = MAX_CORES;							// Number of cores
	int simulation_cycles = MAX_SIMULATION_CYCLES;
	int esrhsp_flag = 0;
	struct task_struct_sim *task_list;
	int scheduling_policy = 0;
	double utilization_bound = 0;
	int random_generate_on = 0;
	char result_file[100];
	int original_sleep_time;
	int i;
	sim_timestamp = time(NULL);

	char power_output_file[1000];
	char temperature_output_file[100];

	// IndSleep task Phasing Flag
	int phasing_flag = 0;

	// SyncSleep Flag
	int global_syncsleep_flag = SYNCSLEEP_FLAG;

	// Utilization to write to util file
	double result_taskset_utilization = 0;

	// Cores
	if (argc > 1)
		number_cores = atoi(argv[1]);
	// Simulation Cycles
	if (argc > 2)
		simulation_cycles = atoi(argv[2]);
	// RHS C_sleep
	if (argc > 3)
		sleep_time = atoi(argv[3]);
	// Number of tasksets to simulate
	if (argc > 4)
		taskset_count = atoi(argv[4]);
	// ESRHSP FLag
	if (argc > 5)
		esrhsp_flag = atoi(argv[5]);
	// SyncSleep Flag
	if (argc > 6)
		global_syncsleep_flag = atoi(argv[6]);
	// IndSleep Phasing Flag
	if (argc > 7)
	{
		phasing_flag = atoi(argv[7]);
		if(phasing_flag == 1)
			printf("Sleep Task Phasing Enabled\n");
	}
	// Output File
	if (argc > 8)
	{
		strcpy(result_file, argv[8]);
		printf("Results Output File %s\n", result_file);
	}
	// Taskset generation on
	if (argc > 9)
	{
		random_generate_on = atoi(argv[9]);
	}
	// Log temperature and Power values -> Defaults to zero -> No Logging
	if (argc > 10)
	{
		log_write_flag = atoi(argv[10]);
		if(log_write_flag > 0)
			log_write_flag = 1;
		else
			log_write_flag = 0;
	}
	// Generate an initial random number based on a seed from the current time
	srand(time(0));
	// Set the value of the original sleep time
	original_sleep_time = sleep_time;
	// Initialize the McPAT LUTs -> for the MiBench Automotive Benchmark
	initialize_mcpat("mcpat_lut/mcpat_lut_file", MCPAT_FILE_READ);
	// RHS specific
	sleeper = (struct sleeping_task*)malloc((number_cores)*sizeof(struct sleeping_task));

	// Allocate Memory for Wait and Run queues and task data structures
	task_list = (struct task_struct_sim *)malloc((number_tasks+number_cores)*sizeof(struct task_struct_sim));
	if(task_list == NULL)
	{
		printf("task_list allocation failed\n");
		return -1;
	}

	wait_q.head = RB_ROOT;									// Initialize the root for the wait queue
	wait_q.task_count = 0;

	run_queue = (struct cpu_run_queue *)malloc(number_cores*sizeof(struct cpu_run_queue));
	if(run_queue == NULL)							
	{
		printf("run_queue allocation failed\n");
		return -1;
	}
	// Initialize the Run queues
	for(i=0; i<number_cores; i++)
	{
		run_queue[i].head = RB_ROOT;						// Initialize the root for the individual cpu run queues
		run_queue[i].current_task = NULL;
		run_queue[i].task_count = 0;
		run_queue[i].utilized_cycles = 0;
		run_queue[i].initialized_utilization = 0;
	}
	
	// Generate a Random task set
	if(random_generate_on == 1)
	{
		for(utilization_bound = 0.80; utilization_bound <= number_cores*MAX_UTILIZATION_BOUND; utilization_bound = utilization_bound + 0.05)
		{
			printf("Utilization %f\n", utilization_bound);
			for(taskset_counter=0; taskset_counter<taskset_count; taskset_counter++)
			{
				while(1)
				{
					number_tasks = rand() % (MAX_TASKS - MAX_TASKS/2) + 1;
					number_tasks = initialize_tasks(task_list, worst_case_flag, number_tasks, utilization_bound);
					if(number_tasks != -1)
						break;
				}
				printf("%d tasks initialized return_value = %d\n", number_tasks, retval);

				// Perform an admission test based on the scheduling policy
				number_tasks = admission_test(task_list, number_tasks, number_cores, scheduling_policy, run_queue, wait_q);
				printf("%d tasks admitted\n", number_tasks);	
				//getchar();

				// Run the scheduler
				printf("Scheduler Running.....\n");
				schedule_sim(simulation_cycles, number_cores, power_output_file, temperature_output_file, global_syncsleep_flag);

				//Display output
				printf("CPU\t\tinitialized utilization\t\tutilization\n");
				for(i=0; i<number_cores; i++)
				{
					printf("%d\t\t%f\t\t\t%f\n", i, run_queue[i].initialized_utilization, (double)run_queue[i].utilized_cycles/(double)simulation_cycles);
				}

				clear_waitqueue(&wait_q);
			}
		}
	}
	else
	{
		open_task_files();
		results = fopen(result_file, "a");
		while(1)
		{
			if(esrhsp_flag == 1)
				retval = read_task_files(esrhsp_same, task_list, sleeper, phasing_flag);
			else
				retval = read_task_files(esrms_same, task_list, sleeper, phasing_flag);
			if(retval == 0)
			{
				printf("Taskset %d Admitted\n", taskset_counter);
				if(esrhsp_flag == 1)
				{
					printf("Running ESRHSP with syncsleep flag %d\n", global_syncsleep_flag);
					schedule_sim_esrhsp(simulation_cycles, number_cores, global_syncsleep_flag, power_output_file, temperature_output_file);
				}
				else
				{
					printf("Running ESRMS with syncsleep flag %d\n", global_syncsleep_flag);
					schedule_sim_esrms(simulation_cycles, number_cores, global_syncsleep_flag, power_output_file, temperature_output_file);
				}
			}
			else if (retval == -2)
			{
				printf("Reached EOF break\n");
				break;
			}
			else
			{
				printf("Taskset %d Discarded\n", taskset_counter);
			}
			clear_waitqueue(&wait_q);
			sim_step_size = original_sim_step_size;
			sleep_time = original_sleep_time;

		}
		close_task_files();
		fclose(results);
		
	}
	// Free The McPAT LUTs
	free_mcpat();

	free(run_queue);
	free(task_list);

	free(sleeper);
	return 0;
}


	