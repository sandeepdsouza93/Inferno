/*
 * @file schedule.c
 * @brief Thermal Aware Scheduling Simulator with Sniper, McPAT and Hotspot Integration
 * @author Sandeep D'souza 
 * 
 * Copyright (c) Carnegie Mellon University, 2015. All rights reserved.
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

#include "rbtree.h"
#include "schedule_hotspot.h"

#define MAX_CORES 4
#define MAX_TASKS 100
#define MAX_SIMULATION_CYCLES 10000
#define POWER_FOLDERS 11
#define SLEEP_TIME 15
#define IDLE_POWER 2
#define MAX_FREQUENCIES 6
#define MAX_TASKSETS 1
#define SIM_STEP_SIZE 1  // in ms
#define MCPAT_FILE_READ 1  // if we need to generate the file by running McPAT then 0

// Power output file
FILE *powertrace_file;
char power_output_file[1000];

// Sysclock Related
float frequencies[MAX_FREQUENCIES] = {1.2, 1.5, 1.8, 2.1, 2.4, 2.66};				// in GHz


void initialize_power_file(char *filename, int no_cores)
{
	int i;
	powertrace_file = fopen(filename, "w");
	for(i=0; i<no_cores; i++)
	{
		fprintf(powertrace_file, "Core%d\t ",i);
	}
	fprintf(powertrace_file,"L3\n");
	fclose(powertrace_file);
	return;
}

void write_to_power_file(char *filename, float *core_power, float l3_power, int no_cores)
{
	int i;
	powertrace_file = fopen(filename, "a");
	for(i=0; i<no_cores; i++)
	{
		fprintf(powertrace_file,"%f\t",core_power[i]);
	}
	fprintf(powertrace_file,"%f\n",l3_power);
	fclose(powertrace_file);
	return;
}



// Taskset counter and Output Temperatur File name
int taskset_counter = 0;
int taskset_count = MAX_TASKSETS;
time_t sim_timestamp;
char temperature_output_file[100];

void final_call_hotspot(char *powerfile)
{
	char *command = (char*)malloc(500*sizeof(char));
	sprintf(command, "cd /home/sandeepdsouza/18743workspace/hotspot/743files; cp mcpat_temp.init mcpat_temp_run.steady");
	system(command);
	sprintf(command, "cd /home/sandeepdsouza/18743workspace/hotspot/743files; cp mcpat_temp_run.steady mcpat_temp_run.init");
	system(command);
	sprintf(command, "cd /home/sandeepdsouza/18743workspace/hotspot/743files; ../hotspot -c ../hotspot.config -f mcpat_test.flp -p ../%s -init_file mcpat_temp.init -o temperature_%s.ttrace -steady_file temperature_%s.steady", powerfile, powerfile, powerfile);
	system(command);
}

void initialize_hotspot(void)
{
	char *flp_file = (char*) malloc(200*sizeof(char));
	char *init_file = (char*) malloc(200*sizeof(char));
	char *steady_file= (char*) malloc(200*sizeof(char));
	
	sprintf(init_file, "/home/sandeepdsouza/18743workspace/hotspot/743files/mcpat_temp.init");
	sprintf(flp_file, "/home/sandeepdsouza/18743workspace/hotspot/743files/mcpat_test.flp");
    sprintf(steady_file, "/home/sandeepdsouza/18743workspace/hotspot/743files/mcpat_final.steady");
	
	hotspot_init(flp_file, init_file, steady_file);
}

// Final Data
float **temperature_data;
float **power_data;

// RHS Specific
int sleep_time = SLEEP_TIME; 			// in ms

// Sleeping Task
struct sleeping_task{
	int sleep_period;				// T
	int sleep_phase;				// phi
	int sleeping_flag;
	int time_slept;					// C completed
	int sleeping_time;				// C
};

struct sleeping_task *sleeper;

// CPU run queues
struct cpu_run_queue {
	struct rb_root head;
	struct task_struct_sim *current_task;
	int task_count;
	int utilized_cycles;
	float initialized_utilization;
};

struct cpu_run_queue *run_queue;

// Wait queue node
struct wait_queue {
	struct rb_root head;
	int task_count;
};

struct wait_queue wait_q;

// Task Struct
struct task_struct_sim {
	int pid;
	int C; 						// Computation time in ms
	int T; 						// Time Period in ms
	float utilization;			// C/T Utilization
	int arrival_time; 			// Starting offset of task in ms
	int time_executed; 			// Time executed this period
	struct rb_node task_node;	// Node for RB tree
	int power_folder;
	int cpuid;					// CPU id
};


// void run_mcpat(float temperature, struct task_struct_sim *task, float *core_power, float *l3_power, int core)
// {
// 	FILE *core_in_file;
// 	char file_string[20];
// 	char* command = (char*) malloc(200*sizeof(char));
// 	// First adjusting temperature to make it McPAT compatible
// 	int mcpat_temperature = ((int)temperature/10 + 1)*10;
// 	if(mcpat_temperature < 300 )
// 	{
// 		mcpat_temperature = 300;
// 	}
// 	else if(mcpat_temperature > 400)
// 	{
// 		mcpat_temperature = 400;
// 	}
// 	sprintf(command, "cd /home/sandeepdsouza/18743workspace/sniper/test/MIB_test/%d; python ../../../tools/mcpat_schedule.py --core %d --temperature %d",task->power_folder, core, mcpat_temperature);
// 	system(command);
// 	// Get the power values from the McPAT output files
// 	sprintf(file_string, "core%d.txt", core);
//     core_in_file = fopen(file_string, "r");
//     fscanf(core_in_file,"%f\t%f",core_power,l3_power);
//     fclose(core_in_file);
//     free(command);
//     return;
// }

// Initialize the McPAT LookUpTable
float ***core_power_array;
float ***l3_power_array;
void initialize_mcpat(int file_read_flag)
{
	int i,j,k;
	int core = 0;
	int mcpat_temperature;

	FILE *core_in_file;
	FILE *lut_file;
    char file_string[20];
    char frequency_string[10];
    char* command = (char*) malloc(200*sizeof(char));

    if(file_read_flag == 0)
    {
    	lut_file = fopen("mcpat_lut_file", "w");
    }
    else
    {
    	lut_file = fopen("mcpat_lut_file", "r");
    }

	printf("Populating Power Look up Tables using McPAT.....\n");
	core_power_array = (float***)malloc(POWER_FOLDERS*sizeof(float**));
	l3_power_array = (float***)malloc(POWER_FOLDERS*sizeof(float**));
	for(i=0; i<POWER_FOLDERS; i++)
	{
		core_power_array[i] = (float**)malloc(MAX_FREQUENCIES*sizeof(float*));
		l3_power_array[i] = (float**)malloc(MAX_FREQUENCIES*sizeof(float*));
		for(j=0; j<MAX_FREQUENCIES; j++)
		{
			core_power_array[i][j] = (float*)malloc(11*sizeof(float));
			l3_power_array[i][j] = (float*)malloc(11*sizeof(float));
			
			// Frequency information to get the correct performance model corresponding to the frequencies
			sprintf(frequency_string, " ");
			if(j != MAX_FREQUENCIES-1)
			{
				sprintf(frequency_string, "_%d", (int)(frequencies[j]*10));
			}

			// Assign Power Values using McPAT to the Look Up Table
			for(k=0; k<11; k++)
			{
				if(file_read_flag == 0)
				{
					mcpat_temperature = 300 + 10*k;
					printf("Creating LUT -> Power Folder %d, Frequency %f GHz,  Temperature %d K\n", i+1, frequencies[j], mcpat_temperature);
					sprintf(command, "cd /home/sandeepdsouza/18743workspace/sniper/test/MIB_test/%d%s; python ../../../tools/mcpat_schedule.py --core %d --temperature %d", i+1, frequency_string, core, mcpat_temperature);
					system(command);
					// Get the power values from the McPAT output files
					sprintf(file_string, "core%d.txt", core);
				    core_in_file = fopen(file_string, "r");
				    fscanf(core_in_file, "%f\t%f",&core_power_array[i][j][k],&l3_power_array[i][j][k]);
				    fprintf(lut_file, "%f\t%f\n", core_power_array[i][j][k],l3_power_array[i][j][k]);
				    fclose(core_in_file);
				}
				else
				{
					fscanf(lut_file, "%f\t%f",&core_power_array[i][j][k],&l3_power_array[i][j][k]);
				}
				
			}
		}
	}
	free(command);
	return;
}

// Deallocate the space allocated for McPAT LUTs
void free_mcpat()
{
	int i,j;
	for(i=0; i<POWER_FOLDERS; i++)
	{
		for(j=0; j<MAX_FREQUENCIES; j++)
		{
			free(core_power_array[i][j]);
			free(l3_power_array[i][j]);
		}
		free(core_power_array[i]);
		free(l3_power_array[i]);
	}
	free(core_power_array);
	free(l3_power_array);
	return;
}

// Call McPAT
void run_mcpat(float temperature, struct task_struct_sim *task, float *core_power, float *l3_power, int core, int frequency_x10)
{
	int temperature_index;
	int frequency_index = 0;
	int i;

	// First adjusting temperature to make it McPAT compatible
	int mcpat_temperature = ((int)(temperature-1)/10 + 1)*10;
	if(mcpat_temperature < 300 )
	{
		mcpat_temperature = 300;
	}
	else if(mcpat_temperature > 400)
	{
		mcpat_temperature = 400;
	}
	
	// Get the LUT index corresponding to the temperature
	temperature_index = (mcpat_temperature - 300)/10;
	
	// Get the LUT index corresponding to the operating frequency (Frequency_x10 is 10 times the frequency in GHz)
	for(i=0; i<MAX_FREQUENCIES; i++)
	{
		if(frequency_x10 == (int)(frequencies[i]*10))
			frequency_index = i;
	}

	*core_power = core_power_array[task->power_folder - 1][frequency_index][temperature_index];
	*l3_power = l3_power_array[task->power_folder - 1][frequency_index][temperature_index];
    return;
}


// RB Tree functions for Run Queues
static int runqueue_add(struct rb_root *head, struct task_struct_sim *task) 
{
    struct rb_node **new = &(head->rb_node), *parent = NULL;
    int result;
    while(*new) {
        struct task_struct_sim *this = container_of(*new, struct task_struct_sim, task_node);
        // order wrt priority
        if(task->T < this->T)
        {
        	result = -1;
        }
        else
        {
        	result = 0;
        }
        parent = *new;
        if (result < 0)
            new = &((*new)->rb_left);
        else
            new = &((*new)->rb_right);
    }
    /* Add new node and rebalance tree. */
    rb_link_node(&task->task_node, parent, new);
    rb_insert_color(&task->task_node, head);
    return 0;
}

static void runqueue_delete(struct rb_root *head, struct task_struct_sim *task) 
{
    rb_erase(&task->task_node, head);
    return;
}

// RB Tree functions for wait queue
static int waitqueue_add(struct rb_root *head, struct task_struct_sim *task) {
    struct rb_node **new = &(head->rb_node), *parent = NULL;
    int result;
    while(*new) {
        struct task_struct_sim *this = container_of(*new, struct task_struct_sim, task_node);
        // order wrt priority
        if(task->arrival_time < this->arrival_time)
        {
        	result = -1;
        }
        else
        {
        	result = 0;
        }
        parent = *new;
        if (result < 0)
            new = &((*new)->rb_left);
        else
            new = &((*new)->rb_right);
    }
    /* Add new node and rebalance tree. */
    rb_link_node(&task->task_node, parent, new);
    rb_insert_color(&task->task_node, head);
    return 0;
}

static void waitqueue_delete(struct rb_root *head, struct task_struct_sim *task) 
{
    rb_erase(&task->task_node, head);
    return;
}

void clear_waitqueue(void)
{
	struct task_struct_sim *task;
	struct rb_node *task_node = NULL;
	struct rb_node *temp_node = NULL;
	task_node = rb_first(&wait_q.head);
	while(task_node != NULL)
	{
		task = container_of(task_node, struct task_struct_sim, task_node);
		temp_node = rb_next(task_node);
		waitqueue_delete(&wait_q.head, task);
		wait_q.task_count--;
		task_node = temp_node;
	}
	return;
}

// Move all tasks back to wait queue
void move_run_to_wait(int no_cores)
{
	struct task_struct_sim *task;
	struct rb_node *task_node = NULL;
	struct rb_node *temp_node = NULL;
	int i;
	int count = 0;

	task_node = rb_first(&wait_q.head);
	while(task_node!=NULL)
	{
		task = container_of(task_node, struct task_struct_sim, task_node);
		temp_node = rb_next(task_node);
		waitqueue_delete(&wait_q.head, task);
		//task->time_executed = 0;
		//task->arrival_time = task->arrival_time % task->T;
		runqueue_add(&run_queue[task->cpuid].head, task);
		task_node = temp_node;
	}
	
	// Reassign arrival times
	for(i=0; i<no_cores; i++)
	{
		task_node = rb_first(&run_queue[i].head);
		while(task_node!=NULL)
		{
			count++;
			task = container_of(task_node, struct task_struct_sim, task_node);
			temp_node = rb_next(task_node);
			runqueue_delete(&run_queue[i].head, task);
			printf("%d Removed from CPU %d: %d T = %d phi = %d\n", count, task->cpuid, task->pid, task->T, task->arrival_time);
			run_queue[i].task_count--;
			task->time_executed = 0;
			task->arrival_time = task->arrival_time % task->T;
			waitqueue_add(&wait_q.head, task);
			wait_q.task_count++;
			task_node = temp_node;
		}
	}
	count = 0;
	// Print Re-assigned Phasings
	task_node = rb_first(&wait_q.head);
	while(task_node!=NULL)
	{
		count++;
		task = container_of(task_node, struct task_struct_sim, task_node);
		printf("%d CPU %d: %d T = %d phi = %d\n", count, task->cpuid, task->pid, task->T, task->arrival_time);
		temp_node = rb_next(task_node);
		task_node = temp_node;
	}
	// getchar();
	return;
}

// Calculate Sysclock Multiplication factor
void scale_frequency(float *scaling_factor, int no_cores)
{
	struct task_struct_sim *task;
	struct task_struct_sim *next_task;
	struct task_struct_sim *last_task;
	struct rb_node *task_node = NULL;
	struct rb_node *temp_node = NULL;
	struct rb_node *first_node = NULL;
	float *scaling_calculations;
	int i, count = 0;
	int j,k;
	int minima = 10000000;
	int min_index;
	int temp_count;
	int temp_accumulator;
	int *dividing_factor;
	float *ideal_frequencies;
    
    int index;

	scaling_calculations = (float*)malloc(MAX_TASKS*sizeof(float));
	dividing_factor = (int*)malloc(MAX_TASKS*sizeof(int));
	
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
		printf("Core %d\n", i);
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
				printf("Task %d period %d\n", task->pid, task->T);
				task_node = rb_next(task_node);
				temp_count = 1;
			}
		}
		printf("Task %d period %d\n", last_task->pid, last_task->T);
		dividing_factor[count] = last_task->T;
		count++;
		//getchar();
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
		for(j=0; j<count; j++)
		{
			printf("%d\n", dividing_factor[j]);
		}
		//getchar();
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
						temp_accumulator = temp_accumulator + ceil((float)dividing_factor[j]/next_task->T)*next_task->C;
						first_node = rb_next(first_node);
					}
					temp_accumulator = temp_accumulator + task->C;
					if((float)temp_accumulator/dividing_factor[j] < scaling_calculations[index])
					{
						scaling_calculations[index] = (float)temp_accumulator/dividing_factor[j];
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
			printf("Scaling Calc for Core %d = %f\n", i, scaling_calculations[j]);
			if(scaling_calculations[j] > scaling_factor[i])
			{
				scaling_factor[i] = scaling_calculations[j];
			}
		}
	}
	// Calculate scaling factor given fixed frequencies
	ideal_frequencies = (float*)malloc(no_cores*sizeof(float));
	for(i=0; i<no_cores; i++)
	{
		
		ideal_frequencies[i] = scaling_factor[i]*frequencies[MAX_FREQUENCIES-1];
		printf("Core %d Ideal Scaling %f and frequency %f GHz\n", i, scaling_factor[i], ideal_frequencies[i]);
		scaling_factor[i] = (float)frequencies[0]/frequencies[MAX_FREQUENCIES-1];
		for(j=0; j<MAX_FREQUENCIES-1; j++)
		{
			if(frequencies[j] < ideal_frequencies[i])
			{
				scaling_factor[i] = (float)frequencies[j+1]/frequencies[MAX_FREQUENCIES-1];
			}
			
		}
		printf("Real Scaling %f and frequency %f GHz\n", scaling_factor[i], scaling_factor[i]*frequencies[MAX_FREQUENCIES-1]);

	}
	//getchar();
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

/*Admission tests for different policies*/

// Worst Fit Decreasing -> List Scheduling
int admission_test_wfd(struct task_struct_sim *task_list, int number_tasks, int number_cores, int rhs_flag)
{
	int i,j;
	int temp;
	int temp_storage;
	float max_utilization = 0;
	float target_utilization = 0.4;
	float *cpu_utilization;
	int *worst_fit_order;
	struct task_struct_sim temp_task;

	// RHS specific
	int *rhs_period_flag;
	int *rhs_highest_priority;
	int *rhs_highest_priority_task_index;

	// Set the initial CPU utilization to 0
	cpu_utilization = (float*)malloc(number_cores*sizeof(float));
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
	printf("\nAdmitted Task List\n");
	for(i=0; i<number_tasks; i++)
	{
		j = 0;
		if(task_list[i].utilization + cpu_utilization[worst_fit_order[j]] < target_utilization)
		{
			task_list[i].cpuid = worst_fit_order[j];
			cpu_utilization[worst_fit_order[j]] = cpu_utilization[worst_fit_order[j]] + task_list[i].utilization;
			run_queue[worst_fit_order[j]].initialized_utilization = run_queue[worst_fit_order[j]].initialized_utilization + task_list[i].utilization;
			waitqueue_add(&wait_q.head ,&task_list[i]);
			printf("Task %d with C = %d T = %d utilization = %f, on cpu %d\n", task_list[i].pid, task_list[i].C, task_list[i].T, task_list[i].utilization, worst_fit_order[j]);
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
			printf("For Core %d Sleep Period  = %d Sleep Phase = %d\n", j, sleeper[j].sleep_period, sleeper[j].sleep_phase);
		}
		else
		{
			printf("Case where all task periods are strange, hence halving the sleep time for core %d\n", j);
			//getchar();
			sleeper[j].sleep_period = rhs_highest_priority[j]/2;
			sleeper[j].sleeping_time = sleep_time/2;
			printf("For Core %d Sleep Period = %d Sleep Phase = %d\n", j, sleeper[j].sleep_period, sleeper[j].sleep_phase);
			rhs_period_flag[0] = 1;
		}

	}
	if(rhs_period_flag[0] == 1)
	{
		sleep_time = sleep_time/2;
	}
	//getchar();
	free(rhs_period_flag);
	free(rhs_highest_priority);
	free(rhs_highest_priority_task_index);
	free(cpu_utilization);
	free(worst_fit_order);
	return temp;
}

// Dummy admission test, modify as per need of algorithm -> here just allocating tasks to processors using first fit decreasing and RMS worst case bound:) ;)
int admission_test(struct task_struct_sim *task_list, int number_tasks, int number_cores, int scheduling_policy)
{
	int admitted_task_count = 0;
	int rhs_flag;
	// RHS
	if(scheduling_policy == 0)
	{
		rhs_flag = 1;
		admitted_task_count = admission_test_wfd(task_list, number_tasks, number_cores, rhs_flag);
	}
	// RMS
	if(scheduling_policy == 1)
	{
		rhs_flag = 0;
		admitted_task_count = admission_test_wfd(task_list, number_tasks, number_cores, rhs_flag);
	}
	return admitted_task_count;
}

// Initialize a Random Task Set
int initialize_tasks(struct task_struct_sim *task_list, int worst_case_flag, int number_tasks, int seed)
{
	int min_period = sleep_time*2;					// in ms
	int max_period = 70;							// in ms
	int max_utilization_integer = 10; 				// analogue for 0.1
	int i;
	int random;
	float utilization;

	// printf("Initialized Task List \n");
	for(i=0; i<number_tasks; i++)
	{
		task_list[i].pid = i;
		// Randomly Initialize Time Periods
		random = rand();
		task_list[i].T = (random % (max_period-min_period)) + min_period;
		
		// Randomly set computation_time values
		random = rand();
		utilization = (float)((random % max_utilization_integer) + 1)/(float)100;
		task_list[i].C = floor(utilization*task_list[i].T)+1;
		task_list[i].utilization = utilization;

		//Randomly set the power folder to run mcpat
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

		task_list[i].cpuid = -1;
		task_list[i].time_executed = 0;

		printf("Task %d with C = %d T = %d utilization = %f\n", task_list[i].pid, task_list[i].C, task_list[i].T, task_list[i].utilization);

	}
	return 0;
}

void write_to_file(float **temperature_data, FILE *temperature_data_output, int simulation_cycles, int no_cores, float *mean, float *variance)
{
	int i,j;
	fprintf(temperature_data_output, "%d %d\n", no_cores, simulation_cycles);
	for(i=0; i<no_cores; i++)
	{
		for(j=0; j<simulation_cycles; j++)
		{
			fprintf(temperature_data_output, "%f ", temperature_data[i][j]);
		}
		fprintf(temperature_data_output, "\n");
	}
	for(i=0; i<no_cores; i++)
	{
		fprintf(temperature_data_output, "%f %f\n", mean[i], variance[i]);
	}
	fclose(temperature_data_output);
	return;
}

void write_to_temperature_file(float **temperature_data, FILE *temperature_data_output, int simulation_cycles, int no_cores)
{
	int i,j;
	fprintf(temperature_data_output, "Core0\tCore1\tCore2\tCore3\tL3\n");
	
	for(j=0; j<simulation_cycles; j++)
	{
		for(i=0; i<(no_cores+1); i++)
		{
			fprintf(temperature_data_output, "%f\t", temperature_data[i][j]);
			
		}
		fprintf(temperature_data_output, "\n");
	}
	fclose(temperature_data_output);
	return;
}

void compute_mean_variance(float **temperature_data, int simulation_cycles, int no_cores, float *mean, float *variance)
{
	int i,j;
	// Compute Mean
	for(i=0; i<no_cores; i++)
	{
		mean[i] = 0;
		for(j=0; j<simulation_cycles; j++)
		{
			mean[i] = mean[i] + temperature_data[i][j];
		}
		mean[i] = mean[i]/simulation_cycles;
	}
	// Compute Variance
	for(i=0; i<no_cores; i++)
	{
		variance[i] = 0;
		for(j=0; j<simulation_cycles; j++)
		{
			variance[i] = variance[i] + powf((temperature_data[i][j] - mean[i]),2);
		}
		variance[i] = sqrt(variance[i]/simulation_cycles);
	}
	return;
}



void schedule_sim_sysclock(int simulation_cycles, int no_cores)
{
	int sim_count = 0;
	struct task_struct_sim *task;
	struct rb_node *task_node = NULL;
	struct rb_node *temp_node = NULL;

	char *command;
	float *temperature;

	int i;

	char **execution_trace;
	char temp_trace[5];
	float *max_temperature;
	float *average_temperature;
	FILE *temperature_data_output;
	
    float *core_power;
    float total_l3_power;
    float l3_power;

    // Sysclock Specific -> Get the Scaling Factor
    float *scaling_factor = (float*)malloc(no_cores*sizeof(float));
    scale_frequency(scaling_factor, no_cores);


    core_power = (float*)malloc(no_cores*sizeof(float));
	temperature = (float*) malloc((no_cores+1)*sizeof(float));
	command = (char*) malloc(500*sizeof(char));


	execution_trace = (char**) malloc(no_cores*sizeof(char*));
	temperature_data = (float**) malloc((no_cores+1)*sizeof(float*));
	for(i=0; i<no_cores; i++)
	{
		execution_trace[i] = (char*)malloc((simulation_cycles+5)*sizeof(char));
		sprintf(execution_trace[i], "C%d:", i);
		temperature_data[i] = (float*) malloc(simulation_cycles*sizeof(float));
	}
	temperature_data[no_cores] = (float*) malloc(simulation_cycles*sizeof(float));
	max_temperature = (float*) malloc(no_cores*sizeof(float));
	average_temperature = (float*) malloc(no_cores*sizeof(float));

	// Initialize the power file
	sprintf(power_output_file, "schedule_output/sysclock_power_%d_%ld.pow", taskset_counter, sim_timestamp);
	initialize_power_file(power_output_file, no_cores);

	//Initialize Temperature Values for Hotspot & McPAT
	for(i=0; i<no_cores; i++)
	{
		temperature[i] = 300;
		max_temperature[i] = 0;
		average_temperature[i] = 0;
		sleeper[i].sleeping_flag = 0;
		sleeper[i].sleeping_time = 0;
		sleeper[i].time_slept = 0;
		run_queue[i].utilized_cycles = 0;

	}
	temperature[no_cores] = 300;
	// Initialize Hotspot
	initialize_hotspot();

	// Simulate the scheduler
	while(sim_count<simulation_cycles)
	{
		// Move tasks to the respective run queues
		if(wait_q.task_count > 0)
		{
			task_node = rb_first(&wait_q.head);
			while(task_node != NULL)
			{
				task = container_of(task_node, struct task_struct_sim, task_node);
				if(task->arrival_time <= sim_count)
				{
					printf("Task %d with C = %d T = %d removed from waitqueue\n", task->pid, task->C, task->T);
					temp_node = rb_next(task_node);
					waitqueue_delete(&wait_q.head, task);
					wait_q.task_count--;
					runqueue_add(&run_queue[task->cpuid].head, task);
					run_queue[task->cpuid].task_count++;
					task_node = temp_node;
				}
				else
				{
					break;
				}
			}
			
		}
		// Schedule Tasks on the respective cores
		total_l3_power = 0;

		// Scheduling Instance
		printf("Sysclock Scheduler State at %d ms\n", sim_count);
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
				printf("CPU %d: Task %d with C = %d T = %d power_type %d executing, time executed = %d\n", task->cpuid, task->pid, task->C, task->T, task->power_folder, task->time_executed);
				// Check Deadline Miss
				if(sim_count >= task->arrival_time + task->T)
				{
					printf("ERROR: Task %d Missed Deadline\n", task->pid);
					//getchar();
				}
				if(task->time_executed == ceil((float)task->C/scaling_factor[i]))
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
		        total_l3_power += l3_power;
		        
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
					printf("CPU %d: Sleep\n", i);
				}
				else 
				{
					core_power[i] = 1.5;			// Power in Idle State
					sprintf(temp_trace, "I");
					printf("CPU %d: Idle\n", i);
				}
				sleeper[i].time_slept++;
				if(sleeper[i].time_slept >= sleeper[i].sleeping_time)
				{
					sleeper[i].sleeping_flag = 0;
				}
			
				strcat(execution_trace[i], temp_trace);
			}

		}
		// Print the execution Trace
		// for(i=0; i<no_cores; i++)
		// {
		// 	printf("%s\n", execution_trace[i]);
		// }

		// Print Stats

		printf("\tUtilization\tTemp\t\tAvg_Temp\tMax_Temp\n");
		for(i=0; i<no_cores; i++)
		{
			temperature_data[i][sim_count] = temperature[i];
			average_temperature[i] = ((sim_count)*average_temperature[i]+temperature[i])/(sim_count+1);
			if(temperature[i] > max_temperature[i])
			{
				max_temperature[i] = temperature[i];
			}
			printf("Core %d:\t%f\t%f\t%f\t%f\n", i,(((float)run_queue[i].utilized_cycles)/(sim_count+1)), temperature[i], average_temperature[i], max_temperature[i]);
		}
		//getchar();
		// Try to average out the effect of high L3 Power
		total_l3_power = total_l3_power/no_cores;
		temperature_data[no_cores][sim_count] = temperature[no_cores];
	    write_to_power_file(power_output_file, core_power, total_l3_power, no_cores);
 		// Run Hotspot

	    hotspot_main(SIM_STEP_SIZE*0.01, 0, core_power, total_l3_power, temperature, no_cores);

		// Display the Temperature
		// printf("Core0\\tCore1\tCore2\tCore3\tL3\n");
		// for(i=0; i<no_cores+1; i++)
		// {
		// 	printf("%f\t", temperature[i]);
		// }
		// printf("\n");
		sim_count++;
	}
	// Dump data to file

	sprintf(temperature_output_file, "schedule_output/sysclock_data_%d_%ld.temptrace", taskset_counter, sim_timestamp);
	temperature_data_output = fopen(temperature_output_file, "w");
	
	write_to_temperature_file(temperature_data, temperature_data_output, simulation_cycles, no_cores);
	
	// Put Tasks back on the wait_q
	move_run_to_wait(no_cores);

	//Exit Hotspot
	hotspot_exit();

	// Free all the memory allocated
	
	free(core_power);
	free(temperature);
	free(command);
	free(temperature_data);
	free(scaling_factor);
	return;
}

void schedule_sim_rms(int simulation_cycles, int no_cores)
{
	int sim_count = 0;
	struct task_struct_sim *task;
	struct rb_node *task_node = NULL;
	struct rb_node *temp_node = NULL;

	char *command;
	float *temperature;
	int i;

	char **execution_trace;
	char temp_trace[5];
	float *max_temperature;
	float *average_temperature;
	FILE *temperature_data_output;
 
    float *core_power;
    float total_l3_power;
    float l3_power;

    core_power = (float*)malloc(no_cores*sizeof(float));
	
	temperature = (float*) malloc((no_cores+1)*sizeof(float));
	command = (char*) malloc(500*sizeof(char));


	execution_trace = (char**) malloc(no_cores*sizeof(char*));
	temperature_data = (float**) malloc((no_cores+1)*sizeof(float*));
	for(i=0; i<no_cores; i++)
	{
		execution_trace[i] = (char*)malloc((simulation_cycles+5)*sizeof(char));
		sprintf(execution_trace[i], "C%d:", i);
		temperature_data[i] = (float*) malloc(simulation_cycles*sizeof(float));
	}
	temperature_data[no_cores] = (float*) malloc(simulation_cycles*sizeof(float));
	max_temperature = (float*) malloc(no_cores*sizeof(float));
	average_temperature = (float*) malloc(no_cores*sizeof(float));

	// Initialize the power file
	sprintf(power_output_file, "schedule_output/rms_power_%d_%ld.pow", taskset_counter, sim_timestamp);
	initialize_power_file(power_output_file, no_cores);

	//Initialize Temperature Values for Hotspot & McPAT
	for(i=0; i<no_cores; i++)
	{
		temperature[i] = 300;
		max_temperature[i] = 0;
		average_temperature[i] = 0;
		sleeper[i].sleeping_flag = 0;
		sleeper[i].sleeping_time = 0;
		sleeper[i].time_slept = 0;
		run_queue[i].utilized_cycles = 0;

	}
	temperature[no_cores] = 300;
	
	// Initialize Hotspot
	initialize_hotspot();

	// Simulate the scheduler
	while(sim_count<simulation_cycles)
	{
		// Move tasks to the respective run queues
		if(wait_q.task_count > 0)
		{
			task_node = rb_first(&wait_q.head);
			while(task_node != NULL)
			{
				task = container_of(task_node, struct task_struct_sim, task_node);
				if(task->arrival_time <= sim_count)
				{
					printf("Task %d with C = %d T = %d removed from waitqueue\n", task->pid, task->C, task->T);
					temp_node = rb_next(task_node);
					waitqueue_delete(&wait_q.head, task);
					wait_q.task_count--;
					runqueue_add(&run_queue[task->cpuid].head, task);
					run_queue[task->cpuid].task_count++;
					task_node = temp_node;
				}
				else
				{
					break;
				}
			}
			
		}
		// Schedule Tasks on the respective cores
		total_l3_power = 0;

		printf("RMS Scheduler State at %d ms\n", sim_count);
		
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
				printf("CPU %d: Task %d with C = %d T = %d power_type %d executing, time executed = %d\n", task->cpuid, task->pid, task->C, task->T, task->power_folder, task->time_executed);
				// Check Deadline Miss
				if(sim_count >= task->arrival_time + task->T)
				{
					printf("ERROR: Task %d Missed Deadline\n", task->pid);
					//getchar();
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
		        total_l3_power += l3_power;
		       
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
					printf("CPU %d: Sleep\n", i);
				}
				else 
				{
					// Power in Idle State
					core_power[i] = 1.5;
					sprintf(temp_trace, "I");
					printf("CPU %d: Idle\n", i);
				}
				sleeper[i].time_slept++;
				if(sleeper[i].time_slept >= sleeper[i].sleeping_time)
				{
					sleeper[i].sleeping_flag = 0;
				}
				strcat(execution_trace[i], temp_trace);
			}

		}
		// Print the execution Trace
		// for(i=0; i<no_cores; i++)
		// {
		// 	printf("%s\n", execution_trace[i]);
		// }

		// Print Stats

		printf("\tUtilization\tTemp\t\tAvg_Temp\tMax_Temp\n");
		for(i=0; i<no_cores; i++)
		{
			temperature_data[i][sim_count] = temperature[i];
			average_temperature[i] = ((sim_count)*average_temperature[i]+temperature[i])/(sim_count+1);
			if(temperature[i] > max_temperature[i])
			{
				max_temperature[i] = temperature[i];
			}
			printf("Core %d:\t%f\t%f\t%f\t%f\n", i,(((float)run_queue[i].utilized_cycles)/(sim_count+1)), temperature[i], average_temperature[i], max_temperature[i]);
		}
		//getchar();
		// Try to average out the effect of high L3 Power
		total_l3_power = total_l3_power/no_cores;
		temperature_data[no_cores][sim_count] = temperature[no_cores];

	    write_to_power_file(power_output_file, core_power, total_l3_power, no_cores);
	    
	    // Run Hotspot

	    hotspot_main(SIM_STEP_SIZE*0.01, 0, core_power, total_l3_power, temperature, no_cores);

	    // Display the Temperature
		// printf("Core0\\tCore1\tCore2\tCore3\tL3\n");
		// for(i=0; i<no_cores+1; i++)
		// {
		// 	printf("%f\t", temperature[i]);
		// }
		// printf("\n");
		sim_count++;
	}

	// Dump data to file
	sprintf(temperature_output_file, "schedule_output/rms_data_%d_%ld.temptrace", taskset_counter, sim_timestamp);
	temperature_data_output = fopen(temperature_output_file, "w");
	write_to_temperature_file(temperature_data, temperature_data_output, simulation_cycles, no_cores);
	
	// Put Tasks back on the wait_q
	move_run_to_wait(no_cores);
	
	// Free all the memory allocated
	
	free(core_power);
	free(temperature);
	free(command);
	free(temperature_data);
	return;
}

void schedule_sim_rhs(int simulation_cycles, int no_cores)
{
	int sim_count = 0;
	struct task_struct_sim *task;
	struct rb_node *task_node = NULL;
	struct rb_node *temp_node = NULL;
	
	char *command;
	float *temperature;
	
	int i;

	char **execution_trace;
	char temp_trace[5];
	float *max_temperature;
	float *average_temperature;
	FILE *temperature_data_output;
    
    float *core_power;
    float total_l3_power;
    float l3_power;
   
    core_power = (float*)malloc(no_cores*sizeof(float));
	temperature = (float*) malloc((no_cores+1)*sizeof(float));
	command = (char*) malloc(500*sizeof(char));
	execution_trace = (char**) malloc(no_cores*sizeof(char*));
	temperature_data = (float**) malloc((no_cores+1)*sizeof(float*));
	
	for(i=0; i<no_cores; i++)
	{
		execution_trace[i] = (char*)malloc((simulation_cycles+5)*sizeof(char));
		sprintf(execution_trace[i], "C%d:", i);
		temperature_data[i] = (float*) malloc(simulation_cycles*sizeof(float));
		
	}

	temperature_data[no_cores] = (float*) malloc(simulation_cycles*sizeof(float));
	max_temperature = (float*) malloc(no_cores*sizeof(float));
	average_temperature = (float*) malloc(no_cores*sizeof(float));

	// Initialize the power file
	sprintf(power_output_file, "schedule_output/rhs_power_%d_%ld.pow", taskset_counter, sim_timestamp);
	initialize_power_file(power_output_file, no_cores);

	//Initialize Temperature Values for Hotspot & McPAT
	for(i=0; i<no_cores; i++)
	{
		temperature[i] = 300;
		max_temperature[i] = 0;
		average_temperature[i] = 0;
		run_queue[i].utilized_cycles = 0;
	}
	temperature[no_cores] = 300;
	// Initialize Hotspot
	initialize_hotspot();
	
	// Simulate the scheduler
	while(sim_count<simulation_cycles)
	{
		// Move tasks to the respective run queues
		
		if(wait_q.task_count > 0)
		{
			task_node = rb_first(&wait_q.head);
			
			while(task_node != NULL)
			{
				task = container_of(task_node, struct task_struct_sim, task_node);
				if(task->arrival_time <= sim_count && ((sim_count % sleeper[task->cpuid].sleep_period) - sleeper[task->cpuid].sleep_phase == 0))
				{
					printf("Task %d with C = %d T = %d removed from waitqueue\n", task->pid, task->C, task->T);
					temp_node = rb_next(task_node);
					waitqueue_delete(&wait_q.head, task);
					wait_q.task_count--;
					runqueue_add(&run_queue[task->cpuid].head, task);
					run_queue[task->cpuid].task_count++;
					task_node = temp_node;
				}
				else 
				{
					if(task->arrival_time > sim_count)
					{
						break;
					}
					else
					{
						task_node = rb_next(task_node);
					}
				}
			}
			
		}
		
		// Schedule Tasks on the respective cores
		total_l3_power = 0;

	    // Scheduling Instance
		printf("RHS Scheduler State at %d ms\n", sim_count);
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
				printf("CPU %d: Task %d with C = %d T = %d power_type %d executing, time executed = %d\n", task->cpuid, task->pid, task->C, task->T, task->power_folder, task->time_executed);
				
				// Check Deadline Miss
				if(sim_count >= task->arrival_time + task->T)
				{
					printf("ERROR: Task %d Missed Deadline\n", task->pid);
					//getchar();
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
				total_l3_power += l3_power;
		        
			}
			else
			{
				printf("CPU %d: Sleep\n", i);
				core_power[i] = 0;
		        sleeper[i].sleeping_flag = 1;
		        if((sim_count % sleeper[i].sleep_period) - sleeper[i].sleep_phase == 0)
		        {
		        	sleeper[i].time_slept = 1;
		        }
		        else
		        {
		        	sleeper[i].time_slept++;
		        }
		        if(sleeper[i].time_slept == sleeper[i].sleeping_time)
		        {
		        	sleeper[i].sleeping_flag = 0;
		        	sleeper[i].time_slept = 0;
		        }
		        sprintf(temp_trace, "S");
				strcat(execution_trace[i], temp_trace);
			}

		}
		// Print the execution Trace
		// for(i=0; i<no_cores; i++)
		// {
		// 	printf("%s\n", execution_trace[i]);
		// }

		// Print Stats

		printf("\tUtilization\tTemp\t\tAvg_Temp\tMax_Temp\n");
		for(i=0; i<no_cores; i++)
		{
			temperature_data[i][sim_count] = temperature[i];
			average_temperature[i] = ((sim_count)*average_temperature[i]+temperature[i])/(sim_count+1);
			if(temperature[i] > max_temperature[i])
			{
				max_temperature[i] = temperature[i];
			}
			printf("Core %d:\t%f\t%f\t%f\t%f\n", i,(((float)run_queue[i].utilized_cycles)/(sim_count+1)), temperature[i], average_temperature[i], max_temperature[i]);
		}
		//getchar();
		// Try to average out the effect of high L3 Power
		total_l3_power = total_l3_power/no_cores;
		temperature_data[no_cores][sim_count] = temperature[no_cores];
	    write_to_power_file(power_output_file, core_power, total_l3_power, no_cores);
	    
	    // Run Hotspot

	    hotspot_main(SIM_STEP_SIZE*0.01, 0, core_power, total_l3_power, temperature, no_cores);

		// Display the Temperature
		// printf("Core0\\tCore1\tCore2\tCore3\tL3\n");
		// for(i=0; i<no_cores+1; i++)
		// {
		// 	printf("%f\t", temperature[i]);
		// }
		// printf("\n");
		sim_count++;
	}
	// Dump data to file

	sprintf(temperature_output_file, "schedule_output/rhs_data_%d_%ld.temptrace", taskset_counter, sim_timestamp);
	temperature_data_output = fopen(temperature_output_file, "w");
	
	write_to_temperature_file(temperature_data, temperature_data_output, simulation_cycles, no_cores);
	
	// Put Tasks back on the wait_q
	move_run_to_wait(no_cores);

	//Exit Hotspot
	hotspot_exit();

	// Free all the memory allocated
	free(core_power);
	free(temperature);
	free(command);
	free(temperature_data);
	return;
}

void schedule_sim(int simulation_cycles, int no_cores)
{
	printf("Running RHS\n");
	schedule_sim_rhs(simulation_cycles, no_cores);
	printf("Running RMS\n");
	schedule_sim_rms(simulation_cycles, no_cores);
	printf("Running Sysclock\n");
	schedule_sim_sysclock(simulation_cycles, no_cores);
}



int main(int argc, char **argv)
{
	int retval = 0;
	int worst_case_flag = 1; 								// All tasks come in together in the worst case 
	int number_tasks = MAX_TASKS;
	int number_cores = MAX_CORES;
	int simulation_cycles = MAX_SIMULATION_CYCLES;
	int seed = 0;
	int i;
	struct task_struct_sim *task_list;
	int scheduling_policy = 0;
	sim_timestamp = time(NULL);

	// Cores
	if (argc > 1)
		number_cores = atoi(argv[1]);
	// Tasks
	if (argc > 2)
		number_tasks = atoi(argv[2]);
	// Simulation Cycles
	if (argc > 3)
		simulation_cycles = atoi(argv[3]);
	// RHS C_sleep
	if (argc > 4)
		sleep_time = atoi(argv[4]);
	// Number of tasksets to simulate
	if (argc > 5)
		taskset_count = atoi(argv[5]);

	// Generate an initial random number based on a seed from the current time
	srand(time(0));
	
	// Initialize the McPAT LUTs
	initialize_mcpat(MCPAT_FILE_READ);

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

	run_queue = (struct cpu_run_queue *)malloc(MAX_CORES*sizeof(struct cpu_run_queue));
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
	for(taskset_counter=0; taskset_counter<taskset_count; taskset_counter++)
	{
		retval = initialize_tasks(task_list, worst_case_flag, number_tasks, seed);
		printf("%d tasks initialized return_value = %d\n", number_tasks, retval);

		// Perform an admission test based on the scheduling policy
		number_tasks = admission_test(task_list, number_tasks, number_cores, scheduling_policy);
		printf("%d tasks admitted\n", number_tasks);	
		//getchar();

		// Run the scheduler
		printf("Scheduler Running.....\n");
		schedule_sim(simulation_cycles, number_cores);

		//Display output
		printf("CPU\t\tinitialized utilization\t\tutilization\n");
		for(i=0; i<number_cores; i++)
		{
			printf("%d\t\t%f\t\t\t%f\n", i, run_queue[i].initialized_utilization, (float)run_queue[i].utilized_cycles/(float)simulation_cycles);
		}

		clear_waitqueue();
	}
	
	// Free The McPAT LUTs
	free_mcpat();

	free(run_queue);
	free(task_list);

	free(sleeper);
	return 0;
}


	