/*
 * @mcpat_interface.c
 * @brief Populates Power Look-up tables using McPAT
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

#include "mcpat_interface.h" 
#include "config.h"

// Power Arrays
double ***core_power_array;
double ***l3_power_array;

double frequencies[MAX_FREQUENCIES] = {1.2, 1.5, 1.8, 2.1, 2.4, 2.66};				// in GHz
// Initialize the McPAT LookUpTable -> file_read_flag signifies read power values from a file
void initialize_mcpat(char* filename, int file_read_flag)
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
    	//lut_file = fopen("mcpat_lut_file", "w");
    	lut_file = fopen(filename, "w");
    }
    else
    {
    	lut_file = fopen(filename, "r");
    }

	printf("Populating Power Look up Tables using McPAT.....\n");
	core_power_array = (double***)malloc(POWER_FOLDERS*sizeof(double**));
	l3_power_array = (double***)malloc(POWER_FOLDERS*sizeof(double**));
	for(i=0; i<POWER_FOLDERS; i++)
	{
		core_power_array[i] = (double**)malloc(MAX_FREQUENCIES*sizeof(double*));
		l3_power_array[i] = (double**)malloc(MAX_FREQUENCIES*sizeof(double*));
		for(j=0; j<MAX_FREQUENCIES; j++)
		{
			core_power_array[i][j] = (double*)malloc(11*sizeof(double));
			l3_power_array[i][j] = (double*)malloc(11*sizeof(double));
			
			// Frequency information to get the correct performance model corresponding to the frequencies
			sprintf(frequency_string, " ");
			if(j != MAX_FREQUENCIES-1)
			{
				sprintf(frequency_string, "_%d", (int)(frequencies[j]*10));
			}

			// Assign Power Values using McPAT to the Look Up Table -> Temperature (range 300-400K)
			for(k=0; k<11; k++)
			{
				if(file_read_flag == 0)
				{
					mcpat_temperature = 300 + 10*k;
					printf("Creating LUT -> Power Folder %d, Frequency %f GHz,  Temperature %d K\n", i+1, frequencies[j], mcpat_temperature);
					// Modify this command based on your SniperSim and McPAT installation
					sprintf(command, "cd /home/sandeepdsouza/18743workspace/sniper/test/MIB_test/%d%s; python ../../../tools/mcpat_schedule.py --core %d --temperature %d", i+1, frequency_string, core, mcpat_temperature);
					system(command);
					// Get the power values from the McPAT output files
					sprintf(file_string, "core%d.txt", core);
				    core_in_file = fopen(file_string, "r");
				    fscanf(core_in_file, "%lf\t%lf",&core_power_array[i][j][k],&l3_power_array[i][j][k]);
				    fprintf(lut_file, "%f\t%f\n", core_power_array[i][j][k],l3_power_array[i][j][k]);
				    fclose(core_in_file);
				}
				else
				{
					fscanf(lut_file, "%lf\t%lf",&core_power_array[i][j][k],&l3_power_array[i][j][k]);
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

// Call McPAT every scheduler simulation interval
void run_mcpat(double temperature, struct task_struct_sim *task, double *core_power, double *l3_power, int core, int frequency_x10)
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