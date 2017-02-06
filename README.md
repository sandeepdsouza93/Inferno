# Inferno
Thermal and Power Real-Time Scheduling Simulation Framework 
```
/*
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
 ```

## Compilation Instructions (Basic setup to get started)
1. Clone the repository
2. Follow the instructions in the Hotspot Readme (README-6.0 and README_archive) to install all the dependencies for Hotspot
3. Navigate to the directory containing the filestop level directory
4. Build Hotspot
```
$> make all
```
5. Build Inferno
```
$> make schedule_inferno
```
6. Test if the build works 
```
$> ./schedule_inferno <number_cores> <simulation_cycles> <sleep_time> <taskset_count> <esrhsp_flag> <global_syncsleep_flag> \
                      <phasing_flag> <result_file> <taskset_generation_flag> <logging_flag>
```
```
where, <number_cores> is the number of cores (should be the same as the cores in the floorplan)
       <simulation_cycles> is the number of scheduling cycles or steps to simulate
       <sleep_time> corresponds to the minimum round-trip time to transition the processor into deep sleep
       <taskset_count> number of tasksets to simulate
       <esrhsp_flag> Simulate ESRHS+ (setting to 1 uses ES-RHS+ as the scheduling policy, 0 defaults to ES-RMS)
       <global_syncsleep_flag> Synchronous sleep across all cores
       <phasing_flag> Independent sleep phasing flag for UniformSleep
       <result_file> Filename to dump output simulation statistics into
       <taskset_generation_flag> Set to 1 to use default taskset generator, 0 to read tasksets from files
       <logging_flag> Generate power and thermal trace of the simulation (set to 1 to enable, 0 to disable)
```

## Important Files
* Main File
 	* schedule_inferno.c: This is the main file. Should be modified to change the type of simulation. Provides a template for writing the scheduler simulation
* Scheduling Data Structures Header
	* scheduler_structures.h 
* Fixed Priority Scheduling Queues
	* rm_scheduling_queues.c - Implements the scheduler waitqueue and per-CPU runqueues as Red-Black trees
	* rm_scheduling_queues.h - header file, contains functions to interact with the scheduling queues
	* rbtree.c - Implements the Red-Black (RB) tree data structures 
	* rbtree.h - rb-tree header file
* Hotspot Interface for Thermal Simulation
	* interface_hotspot.c - Provides an interface to initialize Hotspot, and call Hotspot every scheduling simulation cycle 
	* interface_hotspot.h - Hotspot interface header file
* McPAT Interface for Power Calculation
	* mcpat_interface.c - Can be used to read the default power lookup tables. Can also be configured to generate McPAT power based on a Sniper/McPAT installation. By default uses pre-computed power values for the MiBench Embedded Benchmark.
	* mcpat_interface.h - Header file for McPAT interface
* Stats Generation Framework
 	* stats_generator.c
	* stats_generator.h
* Task Generation Framework
	* task_generator.c - Uses UUniFast-Discard to generate tasksets
	* task_generator.h - Header file 
* Simulation Logging Framework
	* trace_logging.c - Generates a simulation trace
	* trace_logging.h - Header file 
* Supported Scheduling Policies
	* rms.c      - Rate Monotonic Scheduling (RMS)
	* rms.h      - RMS header
	* esrms.c    - Energy-Saving Rate-Monotonic Scheduling (ES-RMS)
	* esrms.h    - ES-RMS header
	* esrhsp.c   - Energy-Saving Rate-Harmonized Scheduling+ (ES-RHS+)
	* esrhsp.h   - ES-RHS+ header
	* sysclock.c - SysClock RMS 
	* sysclock.h - SysClock RMS Header


