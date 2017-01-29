/*
 * @file stats_generator.c
 * @brief Statistics Generation Functions
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

#include "stats_generator.h"
#include "config.h"

// Stats Result file
FILE* results;

void compute_stats(double **data, int simulation_cycles, int no_cores, struct stats_struct *stats)
{
	int i,j;
	double max_temperature = 0, min_temperature = 10000000000;
	// Compute Mean
	for(i=0; i<no_cores; i++)
	{
		stats->mean[i] = 0;
		max_temperature = 0;
		for(j=0; j<simulation_cycles; j++)
		{
			stats->mean[i] = stats->mean[i] + data[i][j];
			if(data[i][j] > max_temperature)
				max_temperature = data[i][j];
			if(data[i][j] < min_temperature)
				min_temperature = data[i][j];
		}
		stats->mean[i] = stats->mean[i]/simulation_cycles;
		stats->max[i]  = max_temperature;
		stats->min[i]  = min_temperature;
	}
	// Compute Variance
	for(i=0; i<no_cores; i++)
	{
		stats->variance[i] = 0;
		for(j=0; j<simulation_cycles; j++)
		{
			stats->variance[i] = stats->variance[i] + powf((data[i][j] - stats->mean[i]),2);
		}
		stats->variance[i] = sqrt(stats->variance[i]/simulation_cycles);
		printf("Core %d Stats: Mean = %f, Max = %f, Min = %f, Var = %f\n", i, stats->mean[i], stats->max[i], stats->min[i], stats->variance[i]);
		fprintf(results, "%d\n", taskset_counter);//%f\n", taskset_counter, taskset_utilization);
		fprintf(results, "%d %f %f %f %f\n", i, stats->mean[i], stats->max[i], stats->min[i], stats->variance[i]);

	}
	return;
}

int alloc_stats_struct(struct stats_struct *stats, int no_cores)
{
	stats->max = (double*) malloc(no_cores*sizeof(double));
	stats->mean = (double*) malloc(no_cores*sizeof(double));
	stats->min = (double*) malloc(no_cores*sizeof(double));
	stats->variance = (double*) malloc(no_cores*sizeof(double));
	return 0;
}

void free_stats_struct(struct stats_struct *stats)
{
	free(stats->mean);
	free(stats->min);
	free(stats->max);
	free(stats->variance);
}