# Inferno
Thermal and Power Real-Time Scheduling Simulation Framework 

# Compilation Instructions (Basics to get up and ready)
1. Clone the repository
2. Follow the instructions in the Hotspot Readme (README-6.0 and README_archive) to install all the dependencies for Hotspot
3. Navigate to the directory containing the filestop level directory
4. Build Hotspot
```
$> make all
```
4. Build Inferno
```
$> make schedule_inferno
```
5. Test if the build works 
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
