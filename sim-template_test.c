/* 
 * A dummy simulator template file to illustrate the use of
 * HotSpot in a cycle-accurate simulator like Simplescalar. 
 * This file contains the following sample routines:
 * 	a) Model initialization	(hotspot_init)
 *	b) Model use in a cycle-by-cycle power model (hotspot_main)
 *	c) Model uninitialization (hotspot_exit)
 *  Please note that all of these routines are just instructional
 *  templates and not full-fledged code. Hence they are not used
 *  anywhere else in the distribution.
 */
#include <string.h>



#include "temperature_grid.h"	/* for dump_steady_temp_grid	*/
#include "flp.h"
#include "util.h"
#include "temperature.h"
//#include "sim743.h"

/* floorplan	*/
static flp_t *flp;
/* hotspot temperature model	*/
static RC_model_t *model;
/* instantaneous temperature and power values	*/
static double *temp, *power;
/* steady state temperature and power values	*/
static double *overall_power, *steady_temp;
/* Simulation Time */
static double total_elapsed_cycles = 0;

/* sample model initialization	*/
void hotspot_init(char *flp_file, char *init_file, char *steady_file)
{
	/* input and output files	*/
	/* flp_file		/* has the floorplan configuration	*/
	/* init_file;		/* initial temperatures	from file	*/
	/* steady_file;	/* steady state temperatures to file	*/
	total_elapsed_cycles = 0;
	/* initialize flp, get adjacency matrix */
	flp = read_flp(flp_file, FALSE);

	/* 
	 * configure thermal model parameters. default_thermal_config 
	 * returns a set of default parameters. only those configuration
	 * parameters (config.*) that need to be changed are set explicitly. 
	 */
	thermal_config_t config = default_thermal_config();
	strcpy(config.init_file, init_file);
	strcpy(config.steady_file, steady_file);

	/* default_thermal_config selects block model as the default.
	 * in case grid model is needed, select it explicitly and
	 * set the grid model parameters (grid_rows, grid_cols, 
	 * grid_steady_file etc.) appropriately. for e.g., in the
	 * following commented line, we just choose the grid model 
	 * and let everything else to be the default. 
	 * NOTE: for modeling 3-D chips, it is essential to set
	 * the layer configuration file (grid_layer_file) parameter.
	 */
	/* strcpy(config->model_type, GRID_MODEL_STR); */

	/* allocate and initialize the RC model	*/
	model = alloc_RC_model(&config, flp, 0);
	populate_R_model(model, flp);
	populate_C_model(model, flp);

	/* allocate the temp and power arrays	*/
	/* using hotspot_vector to internally allocate any extra nodes needed	*/
	temp = hotspot_vector(model);
	power = hotspot_vector(model);
	steady_temp = hotspot_vector(model);
	overall_power = hotspot_vector(model);
	
	/* set up initial instantaneous temperatures */
	if (strcmp(model->config->init_file, NULLFILE)) {
		if (!model->config->dtm_used)	/* initial T = steady T for no DTM	*/
			read_temp(model, temp, model->config->init_file, FALSE);
		else	/* initial T = clipped steady T with DTM	*/
			read_temp(model, temp, model->config->init_file, TRUE);
	}
	else	/* no input file - use init_temp as the common temperature	*/
		set_temp(model, temp, model->config->init_temp);
}

/* 
 * sample routine to illustrate the possible use of hotspot in a 
 * cycle-by-cycle power model. note that this is just a stub 
 * function and is not called anywhere in this file	
 */
int hotspot_main(double elapsed_time, int first_call, float *power_array, float l3_power, float *output_temperature, int no_cores)
{
	
	int i, j, base, idx;


	/* set the per cycle power values as returned by Wattch/power simulator	*/
	if (model->type == BLOCK_MODEL) {
		power[get_blk_index(flp, "Core0")] =  (double) power_array[0];	/* set the power numbers instead of '0'	*/
		power[get_blk_index(flp, "Core1")] =  (double) power_array[1];	
		power[get_blk_index(flp, "Core2")] =  (double) power_array[2];	
		power[get_blk_index(flp, "Core3")] =  (double) power_array[3];	
		power[get_blk_index(flp, "L3")] =  (double) l3_power;	
		/* ... more functional units ...	*/
	
	/* for the grid model, set the power numbers for all power dissipating layers	*/
	} 
	else
	{
		for(i=0, base=0; i < model->grid->n_layers; i++) {
			if(model->grid->layers[i].has_power) {
				idx = get_blk_index(model->grid->layers[i].flp, "Icache");
				power[base+idx] += 0;	/* set the power numbers instead of '0'	*/
				idx = get_blk_index(model->grid->layers[i].flp, "Dcache");
				power[base+idx] += 0;
				idx = get_blk_index(model->grid->layers[i].flp, "Bpred");
				power[base+idx] += 0;
				/* ... more functional units ...	*/

			}	
			base += model->grid->layers[i].flp->n_units;	
		}
	}

	/* call compute_temp at regular intervals */
	if (model->type == BLOCK_MODEL)
	{
		for (i = 0; i < flp->n_units; i++) 
		{
			/* for steady state temperature calculation	*/
			overall_power[i] += power[i];
		}
			
	}
	/* for the grid model, account for all the power dissipating layers	*/	
	else
	{
		for(i=0, base=0; i < model->grid->n_layers; i++) 
		{
			if(model->grid->layers[i].has_power)
				for(j=0; j < model->grid->layers[i].flp->n_units; j++) {
					/* for steady state temperature calculation	*/
					overall_power[base+j] += power[base+j];
				}
			base += model->grid->layers[i].flp->n_units;
		}
	}


	/* calculate the current temp given the previous temp, time
	 * elapsed since then, and the average power dissipated during 
	 * that interval. for the grid model, only the first call to 
	 * compute_temp passes a non-null 'temp' array. if 'temp' is  NULL, 
	 * compute_temp remembers it from the last non-null call. 
	 * this is used to maintain the internal grid temperatures 
	 * across multiple calls of compute_temp
	 */
	if (model->type == BLOCK_MODEL || first_call)
		compute_temp(model, power, temp, elapsed_time);
	else
		compute_temp(model, power, NULL, elapsed_time);
	

	for (i = 0; i < no_cores+1; i++) 
	{
		/* Return the Temperature value in Kelvin to the simulator */
		output_temperature[i] = (float)temp[i] + 273;
	}
	/* make sure to record the first call	*/
	first_call = FALSE;

	/* reset the power array */
	if (model->type == BLOCK_MODEL)
	{
		for (i = 0; i < flp->n_units; i++)
			power[i] = 0;
	}
	else
	{
		for(i=0, base=0; i < model->grid->n_layers; i++) 
		{
			if(model->grid->layers[i].has_power)
				for(j=0; j < model->grid->layers[i].flp->n_units; j++)
					power[base+j] = 0;
			base += model->grid->layers[i].flp->n_units;
		}
	}	
	/* Update Total Time */
	total_elapsed_cycles++; 
	return;
}

/* 
 * sample uninitialization routine to illustrate the possible use of hotspot in a 
 * cycle-by-cycle power model. note that this is just a stub 
 * function and is not called anywhere in this file	
 */
void hotspot_exit()
{
	/* set this to be the correct time elapsed  (in cycles) */
	int i, j, base;

	/* find the average power dissipated in the elapsed time */
	if (model->type == BLOCK_MODEL)
		for (i = 0; i < flp->n_units; i++)
			overall_power[i] /= total_elapsed_cycles;
	else		
		for(i=0, base=0; i < model->grid->n_layers; i++) {
			if(model->grid->layers[i].has_power)
				for(j=0; j < model->grid->layers[i].flp->n_units; j++)
					overall_power[base+j] /= total_elapsed_cycles;
			base += model->grid->layers[i].flp->n_units;
		}

	/* get steady state temperatures */
	steady_state_temp(model, overall_power, steady_temp);

	/* dump temperatures if needed	*/
	if (strcmp(model->config->steady_file, NULLFILE))
		dump_temp(model, steady_temp, model->config->steady_file);

	/* for the grid model, optionally dump the internal 
	 * temperatures of the grid cells	
	 */
	if (model->type == GRID_MODEL &&
		strcmp(model->config->grid_steady_file, NULLFILE))
		dump_steady_temp_grid(model->grid, model->config->grid_steady_file);

	/* cleanup */
	delete_RC_model(model);
	free_flp(flp, FALSE);
	free_dvector(temp);
	free_dvector(power);
	free_dvector(steady_temp);
	free_dvector(overall_power);
}
