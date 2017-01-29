/*
 * @config.h
 * @brief Header containing configuration macros
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
#ifndef __SIM_CONFIG_INFERNO_H_
#define __SIM_CONFIG_INFERNO_H_

#include <time.h>
#include <stdio.h>

#define MAX_CORES 4
#define MAX_TASKS 100
#define MAX_SIMULATION_CYCLES 10000
#define POWER_FOLDERS 11
#define SLEEP_TIME 15
#define IDLE_POWER 2
#define MAX_FREQUENCIES 6
#define MAX_TASKSETS 1
#define SIM_STEP_SIZE 0.001  // in s
#define MCPAT_FILE_READ 1  // if we need to generate the file by running McPAT then 0
#define MAX_PERIOD 400
#define MAX_UTILIZATION_BOUND 0.80
#define IDLE_POWER 2
#define MULT_FACTOR 100
#define LOG_WRITE_FLAG 0
#define SYNCSLEEP_FLAG 0
#define TASK_UPPER_BOUND 0.75

// Csleep_min specifcation
extern int sleep_time; 			// in ms

// Data Logging Flag
extern int log_write_flag;

// Scheduler Granularity
extern double sim_step_size;

extern double original_sim_step_size;

// Taskset Counters
extern int taskset_counter;
extern int taskset_count;

// Simulation timestamp to tag output files
extern time_t sim_timestamp;

// Stats Result File
extern FILE* results;

// Supported frequencies
extern double frequencies[MAX_FREQUENCIES];

#endif