
# HOWTO: Trace and Process Kernel Overheads

This guide documents how to trace and process system overheads (such as context switch costs, scheduling costs, task wake-up latencies, etc.) in a [LITMUS^RT](http://www.litmus-rt.org) system.

## Recording Overheads with Feather-Trace

To record overheads, use the high-level wrapper script `ft-trace-overheads` (➞ [source](../ft-trace-overheads)) in a system running a LITMUS^RT kernel that has been compiled with overhead tracing enabled in the kernel configuration (i.e., `CONFIG_SCHED_OVERHEAD_TRACE=y`).

Use the script as follows. First, activate the scheduler that you are interested in (e.g., `GSN-EDF`). Then simply run `ft-trace-overheads` (as root) with a given name to identify the experiment. While `ft-trace-overheads` is running, execute your benchmark to exercise the kernel. When the benchmark has completed, terminate `ft-trace-overheads` by pressing the enter key.


Example:

	$ setsched GSN-EDF
	$ ft-trace-overheads my-experiment
	[II] Recording /dev/litmus/ft_cpu_trace0 -> overheads_host=rts5_scheduler=GSN-EDF_trace=my-experiment_cpu=0.bin
	[II] Recording /dev/litmus/ft_cpu_trace1 -> overheads_host=rts5_scheduler=GSN-EDF_trace=my-experiment_cpu=1.bin
	[II] Recording /dev/litmus/ft_cpu_trace2 -> overheads_host=rts5_scheduler=GSN-EDF_trace=my-experiment_cpu=2.bin
	[II] Recording /dev/litmus/ft_cpu_trace3 -> overheads_host=rts5_scheduler=GSN-EDF_trace=my-experiment_cpu=3.bin
	[II] Recording /dev/litmus/ft_msg_trace0 -> overheads_host=rts5_scheduler=GSN-EDF_trace=my-experiment_msg=0.bin
	[II] Recording /dev/litmus/ft_msg_trace1 -> overheads_host=rts5_scheduler=GSN-EDF_trace=my-experiment_msg=1.bin
	[II] Recording /dev/litmus/ft_msg_trace2 -> overheads_host=rts5_scheduler=GSN-EDF_trace=my-experiment_msg=2.bin
	[II] Recording /dev/litmus/ft_msg_trace3 -> overheads_host=rts5_scheduler=GSN-EDF_trace=my-experiment_msg=3.bin
	Press Enter to end tracing...
	# [run your benchmark]
	# [presse Enter when done]
	Ending Trace...
	Disabling 4 events.
	Disabling 4 events.
	Disabling 4 events.
	Disabling 4 events.
	Disabling 18 events.
	Disabling 18 events.
	Disabling 18 events.
	Disabling 18 events.
	/dev/litmus/ft_msg_trace3: XXX bytes read.
	/dev/litmus/ft_msg_trace0: XXX bytes read.
	/dev/litmus/ft_msg_trace1: XXX bytes read.
	/dev/litmus/ft_cpu_trace2: XXX bytes read.
	/dev/litmus/ft_cpu_trace1: XXX bytes read.
	/dev/litmus/ft_cpu_trace3: XXX bytes read.
	/dev/litmus/ft_cpu_trace0: XXX bytes read.
	/dev/litmus/ft_msg_trace2: XXX bytes read.

For performance reasons, Feather-Trace records overhead data into separate per-processor trace buffers, and treats core-local events and inter-processor interrupts (IPIs) differently.  Correspondingly, `ft-trace-overheads`  records two trace files for each core in the system.

1. The file `overheads…cpu=$CPU.bin` contains all overhead samples related to CPU-local events such as context switches.

2. The file `overheads…msg=$CPU.bin` contains overhead samples stemming from   IPIs related such as reschedule notifications.



### Key-Value Encoding

To aid with keeping track of relevant setup information, the tool encodes the system's host name and the currently active schedule in a simple `key=value` format in the filename.

We recommend to adopt the same encoding scheme in the experiment tags. For example, when running an experiment named "foo" with (say) 40 tasks and a total utilization of 75 percent, we recommend to launch `ft-trace-overheads` as `ft-trace-overheads foo_n=40_u=75`, as the additional parameters will be added transparently to the final trace file name.

Example:

	ft-trace-overheads foo_n=40_u=75
	[II] Recording /dev/litmus/ft_cpu_trace0 -> overheads_host=rts5_scheduler=GSN-EDF_trace=foo_n=40_u=75_cpu=0.bin
	…

However, this convention is purely optional.

### Automating `ft-trace-overheads`

It can be useful to terminate `ft-trace-overheads` from another script by sending a signal. For this purpose, provide the `-s` flag to `ft-trace-overheads`, which will make it terminate cleanly when it receives the `SIGUSR1` signal.

When recording overhead on a large platform, it can take a few seconds until all tracer processes have finished initialization. To ensure that all overheads are being recorded, the benchmark workload should not be executed until initialization is complete. To this end, it is guaranteed that the string "to end tracing..." does not appear in the script's output (on STDOUT) until initialization is complete on all cores.

## What does Feather-Trace data look like?

Feather-Trace produces "raw" overhead files. Each file contains simple event records. Each event record consists of the following fields (➞ [see definition](https://github.com/LITMUS-RT/feather-trace-tools/blob/master/include/timestamp.h#L18)):

- an event ID (e.g., `SCHED_START`),
- the ID of the processor on which the event was recorded (0-255),
- a per-processor sequence number (32 bits),
- the PID of the affected or involved process (if applicable, zero otherwise),
- the type of the affected or involved process (best-effort, real-time, or unknown),
- a timestamp (48 bits),
- a flag that indicates whether any interrupts occurred since the last recorded event, and
- an approximate interrupt count (0-31, may overflow).

The timestamp is typically a raw cycle count (e.g, obtained with `rdtsc`). However, for certain events such as `RELEASE_LATENCY`, the kernel records the time value directly in nanoseconds.

**Note**: Feather-Trace records data in native endianness. When processing data files on a machine with a different endianness, endianness swapping  is required prior to further processing (see `ftsort` below).

## Event Pairs

Most Feather-Trace events come as pairs. For example, context-switch overheads are measured by first recording a `CXS_START` event prior to the context switch, and then a `CXS_END` event just after the context switch. The context-switch overhead is given by the difference of the two timestamps.

There are two event pairs related to scheduling: the pair `SCHED_START`-`SCHED_END` records the scheduling overhead prior to a context switch, and the pair `SCHED2_START`-`SCHE2D_END` records the scheduling overhead after a context switch (i.e, any clean-up).

To see which event records are available, simply record a trace with `ft-trace-overheads` and look through it with `ftdump` (see below), or have a look at the list of event IDs (➞ [see definitions](https://github.com/LITMUS-RT/feather-trace-tools/blob/master/include/timestamp.h#L41)).

## Low-Level Tools

This repository provides three main low-level tools that operate on raw overhead trace files. These tools provide the basis for the higher-level tools discussed below.

1. `ftdump` prints a human-readable version of a trace file's contents. This is useful primarily for manual inspection. Run as `ftdump <MY-TRACE-FILE>`.

2. `ftsort` sorts a Feather-Trace binary trace file by the recorded sequence numbers, which is useful to normalize traces prior to further processing in case events were stored out of order. Run as `ftsort <MY-TRACE-FILE>`. `ftsort` can also carry-out endianness swaps if needed. Run `ftsort -h` to see the available options.

3. `ft2csv` is used to extract overhead data from raw trace files. For example, to extract all context-switch overhead samples, run `ft2csv CXS <MY-TRACE-FILE>`. Run `ft2csv -h` to see the available options.

By default, `ft2csv` produces CSV data. It can also produce binary output compatible with NumPy's `float32` format, which allows for efficient processing of overhead data with NumPy's `numpy.memmap()` facility.

## High-Level Tools

This repository provides a couple of scripts around `ftsort` and `ft2csv` that automate common post-processing steps. We recommend that novice users stick to these provided high-level scripts until they have acquired some familiarity with the LITMUS^RT tracing infrastructure.

Prost-processing of (a large collection of) overhead files typically involves:

1. sorting all files with `ftsort`,

2. splitting out all recorded overhead samples from all trace files,

3. combining data from per-cpu trace files and from traces with different task counts, system utilizations, etc. into single data files for further processing,

4. counting how many events of each type were recorded,

5. shuffling and truncating all sample files, and finally

6. extracting simple statistics such as the observed median, mean, and maximum values.

Note that step 4 is required to allow a statistically meaningful comparison of the sampled maximum overheads. (That is, to avoid sampling bias, do not compare the maxima of trace files containing a different number of samples.)

Corresponding to the above steps, this repository provides a number of scripts that automate these tasks.


### Sorting Feather-Trace files

The `ft-sort-traces` script (➞ [source](../ft-sort-traces))  simply runs `ftsort` on all trace files. Invoke as  `ft-sort-traces <MY-TRACE-FILES>`. We  recommended to keep a log of all post-processing steps with `tee`.

Example:

	ft-sort-traces overheads_*.bin 2>&1 | tee -a overhead-processing.log

Sorting used to be an essential step, but in recent versions of LITMUS^RT, most traces do not contain any out-of-order samples.

### Extracting Overhead Samples

The script `ft-extract-samples` (➞ [source](../ft-extract-samples)) extracts all samples from all provided files with `ft2csv`.

Example:

	ft-extract-samples overheads_*.bin 2>&1 | tee -a overhead-processing.log

The underlying `ft2csv` tool automatically discards any samples that were disturbed by interrupts.

### Combining Samples

The script`ft-combine-samples` (➞ [source](../ft-combine-samples)) combines several data files into a single data file for further processing. This script assumes that file names follow the specific key=value naming convention already mentioned above:

    <basename>_key1=value1_key2=value2...keyN=valueN.float32

The script simply strips certain key=value pairs to concatenate files that have matching values for all parameters that were not stripped. For instance, to combine all trace data irrespective of task count, as specified by "_n=<NUMBER>_",  invoke as `ft-combine-samples -n <MY-DATA-FILES>`. The option `--std` combines files with different task counts (`_n=`), different utilizations (`_u=`), for all sequence numbers (`_seq=`), and for all CPU IDs (`_cpu=` and `_msg=`).

Example:

    ft-combine-samples --std overheads_*.float32 2>&1 | tee -a overhead-processing.log


### Counting Samples

The script `ft-count-samples` simply looks at all provided trace files and, for each overhead type, determines the minimum number of samples recorded. The output is formatted as a CSV file.

Example:

    ft-count-samples  combined-overheads_*.float32 > counts.csv


### Random Sample Selection

To allow for an unbiased comparison of the sample maxima, it is important to use the same number of samples for all compared traces. For example, to compare scheduling overhead under different schedulers, make sure you use the same number of samples for all schedulers. If the traces contain a different number of samples (which is very likely), then a subset must be selected prior to computing any statistics.

The approach chosen here is to randomly shuffle and then truncate (a copy of) the files containing the samples. This is automated by the script ` ft-select-samples` (➞ [source](../ft-select-samples)).

**Note**: the first argument to  `ft-select-samples` *must* be a CSV file produced by  `ft-count-samples`.

Example:

    ft-select-samples counts.csv combined-overheads_*.float32 2>&1 | tee -a overhead-processing.log

The script does not modify the original sample files. Instead, it produces new files of uniform size containing the randomly selected samples. These files are given the extension `sf32` (= shuffled float32).

### Compute statistics

The script `ft-compute-stats` (➞ [source](../ft-compute-stats)) processes `sf32` or `float32` files to extract the maximum, average, median, and minimum observed overheads, as well as the standard deviation and variance. The output is provided in CSV file for further processing (e.g., formatting with a spreadsheet application).

**Note**: Feather-Trace records most overheads in cycles. To convert to microseconds, one must provide the speed of the experimental platform, measured in the number of processor cycles per microsecond, with the `--cycles-per-usec` option. The speed can be inferred from the processor's spec sheet (e.g., a 2Ghz processor executes 2000 cycles per microsecond) or from `/proc/cpuinfo` (on x86 platforms)\. The LITMUS^RT user-space library [liblitmus](https://github.com/LITMUS-RT/liblitmus) also contains a tool `cycles` that can help measure this value.

Example:

    ft-compute-stats combined-overheads_*.sf32 > stats.csv


## Complete Example

Suppose all overhead files collected with `ft-trace-overheads` are located in the directory `$DIR`. Overhead statistics can be extracted as follows.

    # (1) Sort
    ft-sort-traces overheads_*.bin 2>&1 | tee -a overhead-processing.log

    # (2) Split
    ft-extract-samples overheads_*.bin 2>&1 | tee -a overhead-processing.log

    # (3) Combine
    ft-combine-samples --std overheads_*.float32 2>&1 | tee -a overhead-processing.log

    # (4) Count available samples
    ft-count-samples  combined-overheads_*.float32 > counts.csv

    # (5) Shuffle & truncate
    ft-select-samples counts.csv combined-overheads_*.float32 2>&1 | tee -a overhead-processing.log

    # (6) Compute statistics
    ft-compute-stats combined-overheads_*.sf32 > stats.csv

