
# HOWTO: Trace and Analyze a Schedule

Whereas Feather-Trace data records *how long* a scheduling decision or context switch takes, the `sched_trace` interface instead records *which* tasks are scheduled at what point and corresponding job releases and deadlines.

## Recording a Schedule with `sched_trace`

The main high-level tool for recording scheduling decisions is the script `st-trace-schedule`(➞ [source](../st-trace-schedule)).

To record the execution of a task system, follow the following rough outline:

1. start recording all scheduling decisions with `st-trace-schedule`;

2. launch and initialize all real-time tasks such that they wait for a *synchronous task system release* (see the `release_ts` utility in `liblitmus`);

3. release the task set with `release_ts`; and finally

4. stop `st-trace-schedule` when the benchmark has completed.

Example:

	st-trace-schedule my-trace
	CPU 0: 17102 > schedule_host=rts5_scheduler=GSN-EDF_trace=my-trace_cpu=0.bin [0]
	CPU 1: 17104 > schedule_host=rts5_scheduler=GSN-EDF_trace=my-trace_cpu=1.bin [0]
	CPU 2: 17106 > schedule_host=rts5_scheduler=GSN-EDF_trace=my-trace_cpu=2.bin [0]
	CPU 3: 17108 > schedule_host=rts5_scheduler=GSN-EDF_trace=my-trace_cpu=3.bin [0]
	Press Enter to end tracing...
	# [launch tasks]
	# [kick off experiment with release_ts]
	# [press Enter when done]
	Ending Trace...
	Disabling 10 events.
	Disabling 10 events.
	Disabling 10 events.
	Disabling 10 events.
	/dev/litmus/sched_trace2: XXX bytes read.
	/dev/litmus/sched_trace3: XXX bytes read.
	/dev/litmus/sched_trace1: XXX bytes read.
	/dev/litmus/sched_trace0: XXX bytes read.

As the output suggests, `st-trace-schedule` records one trace file per processor.

## What does `sched_trace` data look like?

A scheduling event is recorded whenever

- a task is dispatched (switched to),
- a task is preempted (switched away),
- a task suspends (i.e., blocks), or
- a task resumes (i.e., wakes up).

Furthermore, the release time, deadline, and completion time of each job is recorded, as are each task's parameters and the name of its executable (i.e., the `comm` field in the Linux kernel's PCB `struct task_struct`).  Finally, the time of a synchronous task system release (if any) is recorded as a reference of "time zero".

The binary format and all recorded events are documented in the header file `sched_trace.h` (➞ [source](../include/sched_trace.h)). A Python parser for trace files is available in the module `sched_trace` (➞ [low-level parser](../sched_trace/format.py) and [high-level interface](../sched_trace/__init__.py#60)).

The tool `st-dump` (➞ [source](../src/stdump.c)) may be used to print traces in a human-readable format for debugging purposes.

## Drawing a Schedule

The tool [pycairo](http://cairographics.org/pycairo/)-based `st-draw`  (➞ [source](../st-draw)) renders a trace as a PDF. By default, it will render the first one thousand milliseconds after *time zero*, which is either the first synchronous task system release (if any) or the time of the first event in the trace (otherwise).

Example:

	st-draw schedule_host=rts5_scheduler=GSN-EDF_trace=my-trace_cpu=*.bin
	# Will render the schedule as schedule_host=rts5_scheduler=GSN-EDF_trace=my-trace.pdf

Invoke `st-draw -h` for a list of possible options. If the tool takes a long time to complete, run it in verbose mode (`-v`) and try to render a shorter schedule (`-l`).

## Obtaining Job Statistics

The tool `st-job-stats` (➞ [source](../src/job_stats.c)) produces a CSV file with relevant per-job statistics for further processing with (for example) a spreadsheet application.

Example:

	st-job-stats schedule_host=rts5_scheduler=GSN-EDF_trace=my-trace_cpu=*.bin
	# Task,   Job,     Period,   Response, DL Miss?,   Lateness,  Tardiness, Forced?,       ACET
	# task NAME=rtspin PID=17406 COST=590000 PERIOD=113000000 CPU=254
	 17406,     3,  113000000,   17128309,        0,  -95871691,          0,       0,     388179
	 17406,     4,  113000000,   12138793,        0, -100861207,          0,       0,     382776
	 17406,     5,  113000000,    7137743,        0, -105862257,          0,       0,     382334
	 17406,     6,  113000000,    2236774,        0, -110763226,          0,       0,     382352
	 17406,     7,  113000000,     561701,        0, -112438299,          0,       0,     559208
	 17406,     8,  113000000,     384752,        0, -112615248,          0,       0,     382539
	 17406,     9,  113000000,     565317,        0, -112434683,          0,       0,     561602
	 17406,    10,  113000000,     379963,        0, -112620037,          0,       0,     377526
	[...]

There is one row for each job. The columns record:

1. the task's PID,

2. the job sequence number,

3. the task's period,

4. the job's response time,

5. a flag indicating whether the deadline was missed,

6. the job's lateness (i.e., response time - relative deadline),

7. the job's tardiness (i.e., max(0, lateness)),

8. a flag indicating whether the LITMUS^RT budget enforcement mechanism inserted an artificial job completion, and finally

9. the actual execution time (ACET) of the job.

Note that the *Forced?* flag is always zero for proper reservation-based schedulers (e.g., under `P-RES`). Forced job completion is an artefact of the LITMUS^RT legacy budget enforcement mechanism under process-based schedulers (such as `GSN-EDF` or `P-FP`).



