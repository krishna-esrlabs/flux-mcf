# Advanced tracing topics

For more detailed insights, additional tooling exists for performance analysis of FLUX-based applications. Specifically,
the repository provides, in addition to the tools released with the open source distribution, means to analyze time box
events as well as memory allocations.

Both of these tools serve the explicit purpose to track down sources of latency, albeit on different levels. While the
time box view provides an insight when a timing property has been violated, the component state visualization can give
insights about the offending component, and the allocation trace can provide information about the latencied caused by
memory allocations and which code path exactly caused them.

## Converting CUDA traces

A Python conversion script is provided in this repository, which can be found in the
`nvprof2ctf` folder. The script accepts various arguments that allow one to trim
the trace to a given time interval; a typical invocation would look like `python
/path/to/nvprof2ctf/main.py -f profile.sqlite -o FOLDERNAME` where
`FOLDERNAME` is the name of the output directory that shall contain the CTF
trace.

## Time boxes

To debug time box violations, an offline time box trace must be created. It
creates time box events under the same logic under which also the online time
box monitor operates. The specialized trace is created by invoking
`python
/path/to/mcf/mcf_core/tools/time_box_monitoring/time_boxes_2_ctf.py --p
timeboxes_tracing.json --o timebox_trace.ctf trace.bin`, which processes the
middleware events, applies time box logic to them and produces a CTF time box
trace readable by Trace Compass. The command line arguments of this script mean,
in sequence,
* `--p timeboxes_tracing.json` the definition file for time boxes,
* `--o timebox_trace.ctf` the directory for the resulting CTF output,
* `trace.bin` the middleware event trace.

The time box trace can be visually analyzed with the supplied analysis XML file `TimeBoxAnalysis.xml`. The _Time
box graph_ view shows the time boxes as individual blobs in a time graph,
colored according to their completion status. Red means a violation, green means
successful completion, yellow means a lost event.

## Malloc trace

Additional tooling is provided to trace not only memory allocation events, but also their causes. The events themselves
can be traced with LTTng, but when it comes to finding their causes (and getting rid of them), a
backtrace of callers is required. For this, a drop-in `malloc()` wrapper can be
used, which can be found in this repository under `tools/tracemalloc`. This
folder contains a Project.meta that builds a local executable to test the tracing
capabilities and a shared library for the target platform. For building,
Boost.Lockfree, pthread and C++14 headers are required. Using the library is as
easy as setting the `MALLOC_TRACE` and `LD_PRELOAD` environment variables:
`MALLOC_TRACE=mtrace LD_PRELOAD=tracemalloc.so [executable] [args...]`. This
will produce two files in CSV format, `mtrace` and `mtrace.index`. The former
file is a chronologically ordered log of memory allocations, the latter one is
a list of backtraces that caused the allocations. If `MALLOC_TRACE` is not set
(or the executable is privileged), the output file name will default to
`_MALLOC_TRACE`.

More concretely, the resulting data contains the following pieces of
information:
* The time point at which a call to `malloc` was issued
* The requested amount of bytes
* The wall clock time it took to allocate memory
* The CPU time it took this specific thread to allocate memory
* The number of voluntary (I/O, mutexes) and involuntary (preemptions) context
  switches
* The backtrace in human-readable form, as pointer in the code segment, with
  some luck as a concrete function name.

The resulting files can be analyzed with supplied Python scripts,
`tracemallocParser.py` and `tracemallocAnalyzer.py`. The former script produces
two kinds of output: graphs, including `malloc` statistics for each program
module, and a textual report which contains top-10 sources by amount of
invocations and maximal thread time used of `malloc` per module as backtraces.
The latter script attempts to run some rudimentary data analysis on the calls
to identify a correspondence between long execution times and other data.
Current state of knowledge is that the most important factor is the call
backtrace (last 3~4 items).

The individual items in backtraces can be traced down to individual lines of
code with the means of GDB, if the program has been built with debugging
symbols. This makes it somewhat easy to find the actual line of code that causes
the allocations.

Here, modules are defined as either the program executable or a shared library,
whichever is the uppermost item in the backtrace. Additionally, a user-defined
module list can be supplied.

In addition to the analysis provided by the script, it is possible to compute
custom statistics using Pandas or other tools that can load CSV files.

Trying the `malloc` tracer on ATP2 smoke test runs yields a reproducible sequence
of allocations and a reproducible pattern of execution times, especially the
sources of large latencies seem to be the same. This implies that to get rid of
unnecessarily slow allocations, first these call sites need to be addressed.
