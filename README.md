
# What is this?

This repository contains tools and scripts for working with the tracing facilities in [LITMUS^RT](http://www.litmus-rt.org). In particular, tools are provided  for

1. recording and processing **overhead data**, obtained from [LITMUS^RT](http://www.litmus-rt.org) kernels with the help of the Feather-Trace tracing infrastructure, and for

2. recording, visualizing, and analyzing **scheduling traces** in the `sched_trace`  format, which are also typically obtained from [LITMUS^RT](http://www.litmus-rt.org) kernels.


# Prefix Convention

With one exception, tools and scripts prefixed with `ft` work on Feather-Trace overhead data (in either "raw" binary form or in derived formats). Conversely, tools and scripts prefixed with `st` work on scheduling traces in the  `sched_trace` binary format.

The one notable exception is the low-level tool `ftcat`, which is used both to record overheads and scheduling events.

# Documentation

There are two guides that provide an overview of how to to work with the provided tools.

- [HOWTO: Trace and process overhead data](doc/howto-trace-and-process-overheads.md)

- [HOWTO: Trace and analyze a schedule](doc/howto-trace-and-analyze-a-schedule.md)

Some additional information is also available on the [LITMUS^RT wiki](https://wiki.litmus-rt.org/litmus/Tracing).

Users are expected to be comfortable reading the (generally clean) source code. When in doubt, consult the source. If still confused, then contact the [LITMUS^RT mailing list](https://wiki.litmus-rt.org/litmus/Mailinglist).

