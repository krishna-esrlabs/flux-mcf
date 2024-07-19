Introduction
============

There exists quite a large amount of tooling to trace FLUX-based applications.
This document attempts to give an overview over their use cases, usage and
limitations.

In order to understand what happens inside the middleware, several points of
view can be taken: a FLUX-based (messages, components etc.), a Linux
kernel-based (CPUs and threads), a CUDA-based (calls to CUDA runtime and
computations on the relevant devices) as well as an allocation trace. The
former three views can be analyzed in the Trace Compass tool, the latter one
has a different output format.

Different views serve different purposes. For example, a timing property violation is
most simply seen in the component state view: the middleware events help identify
the violating part of the software. In order to see if the software component
was actually performing the computation or waiting for an external event (such
as unblocking of a mutex or I/O), OS-specific and CUDA events should be 
considered. 

In the following sections, we briefly describe how to obtain the events from
the aforementioned sources and ways to graphically analyze them.

Traces
======

## FLUX trace

Setting up FLUX tracing requires minor modifications to the executable. An additional `ValueStore` for the event record
needs to be created (using the same `ValueStore` object as the one for the regular application messages will result in
recursive logging of trace messages which record logging events for trace messages that... etc.) as well as
a `ComponentTraceController` which will record the trace events into this `ValueStore`. Furthermore, a `ValueRecorder`
needs to be configured if it is desired to eventually retrieve the event trace. This will produce an event record in the
specified recording directory. An example how tracing can be configured can be seen below.

```c++
mcf::ValueStore traceValueStore;
auto componentTraceController = std::make_unique<mcf::ComponentTraceController>("FLUX", traceValueStore); // "FLUX" is the process name, to differentiate between multi-process traces
mcf::ValueRecorder traceRecorder(traceValueStore);
traceRecorder.start("trace.bin"); // this is where the trace record will be stored
componentTraceController->enableTrace(true); // enables the trace recording
mcf::ComponentManager componentManager(valueStore /* message value store */,
                                       configDirs /* configuration directories */,
                                       componentTraceController.get() /* pass the controller so it will get trace messages */);
```

FLUX traces are generated in a FLUX-internal format. Converting
them to CTF (Common Trace Format), a popular format used by tools such as Trace
Compass, requires running an additional script, which can be found in
`/path/to/mcf/mcf_tools/component_tracing/trace_2_ctf.py`.
For graphical analysis of FLUX trace data, Trace Compass can be supplied with 
custom XML analyses which can be found in this repository under
`/path/to/mcf/mcf_tools/component_tracing/trace_compass_analyses`. After these files
are loaded via the _Tracing_|_XML Analyses_ view in the preferences window,
additional views appear for the traces.

* _Component execution state analysis_: Shows the execution state for each
  individual component in the system as well as the message reception and
  processing times.
* _Port trigger delay analysis_, _Port trigger execution time analysis_, 
  _Remote transfer time analysis_, …: Shows various message processing delay 
  statistics which are mostly self-explanatory.

## Linux kernel trace

There exist several ways to obtain a trace of Linux kernel events. We at ESR Labs 
use the facilities of `ftrace`, which are built-in into the kernel and require minimal additional setup. `trace-cmd` must be installed on
the target machine to gather a trace, other than that no specific preparation
and no installation of foreign kernel modules is required.

In this section, we describe how to obtain a trace with the facilities of
`trace-cmd`. As a side note, it is also possible to gather a slightly more
compact kernel event trace via LTTng; this mechanism also makes it possible 
to instrument C library functions such as `malloc`, `free`,
`pthread_mutex_lock` and some others, but this is not part of this
documentation; furthermore, kernel tracing with LTTng requires installation of
an additional kernel module (which is not necessarily easily accessible on
embedded targets such as NVIDIA Tegra). Further information on LTTng can be
found [here](https://lttng.org/docs/#doc-liblttng-ust-libc-pthread-wrapper).

In order to obtain a kernel event trace, the command line of your program must
be prefixed with `trace-cmd record -C mono -c -o kernel.dat -s 500 -b 600000
--date -e sched_switch -e sched_wakeup -e sched_wakeup_new`. This will launch
tracing processes named `trace-cmd`, one per logical CPU core, and launch the
executable. Upon program exit, the trace will finish and as a result, generate
a file named `kernel.dat`, which can then be loaded into Trace Compass. Let's
now take a look at the command line.

* `record` means that the intention is to record a kernel trace that coincides
  with the execution of a program.
* `-C mono` means that the clock source for event times is the so-called
  monotonic clock which can be accessed with `CLOCK_MONOTONIC` from the C POSIX
  API or `std::chrono::steady_clock`. On most targets, the clock 
  `std::chrono::high_resolution_clock` yields sufficiently similar measurements.
* `-c` means that also all sub-processes will be traced, which is useful if we
  combine kernel tracing with CUDA tracing.
* `-o kernel.dat` signals that the output file will be `kernel.dat`.
* `-s 500` steers the frequency of trace serializer wakeups, in this case,
  every $500$ milliseconds. The default value is $1000$, everything under $100$
  might interfere with the regular program flow.
* `-b 600000` sets the intermediate buffer size to $600000$ KB. Internally,
  `trace-cmd` reserves a ring buffer to which it stores the tracing events; if a
  program generates too much of them, the early events will be discarded, which
  is to be avoided since it is a non-deterministic information loss.
* `--date` makes the events be logged with their UNIX timestamp.
* `-e [event]` sets up the events to be traced. Usually, only a handful of
  events are of interest. The three given in the example provide complete
  information about which threads are active on which CPU at a given point in
  time. Additional events or kernel functions can be added, e.g. `do_page_fault`
  that shows if a page fault occurred.

The resulting file can be loaded into Trace Compass; however, current versions
have a [bug](https://bugs.eclipse.org/bugs/show_bug.cgi?id=573612) that might
prevent all events from being loaded if process names with square brackets or
whitespace occur, such as "[NSys Comms]". To circumvent this, the trace file
can be filtered with command-line tools such as `trace-cmd report -i
path/to/kernel.dat -R | sed -e "s/\[NSys\]/NSys/" -e "s/\[NSys
Comms\]/NSysComms/" > kernel_trace.txt`. The resulting trace, now in text
format, can also be processed in Trace Compass (and viewed in a text editor).

In a multi-executable setting, it is sufficient to run `trace-cmd` and other
kernel tracing tools only once per run, the trace will contain all relevant
information. To differentiate between threads, thread naming
(`pthread_setname_np`) is advised; FLUX already names all its component threads
so that it is easy to understand which thread belongs to which component.

The overhead of using `ftrace` manifests itself as additional threads that
require time and memory to dump kernel trace events on the disk.

## CUDA trace

To gather CUDA traces, we use the command-line version of Nsight Systems. The
target must support this; specifically for DRIVE targets, DRIVE OS 5.2.0 
supports Nsight Systems CLI. The result of Nsight Systems tracing is a SQLite3
database in which events are contained in tables by event type (kernel
execution, memory copy operations, runtime calls).

To invoke tracing, we can use the following command line.
`/usr/local/bin/nsys profile --cuda-flush-interval=2000 --duration=195
--export=sqlite --output=profile --force-overwrite=true`

Here, `profile` signals that we actually want to profile an application (which,
along with its arguments, comes after the `nsys` command line,
`--cuda-flush-interval` controls how often, in milliseconds, the CUDA tracing 
infrastructure shall flush the tracing buffer, `--duration` sets the maximal 
program duration (in seconds), `--output=profile` sets the output naming
pattern, `--export=sqlite` orders `nsys` to produce an SQLite database (in this
case, `profile.sqlite`) in addition to the report for the Nsight visual tool.

The SQLite report, prior to further processing, must be converted to CTF. 
The resulting CTF traces can be visualized in NVIDIA's own proprietary Nsight 
Systems tool, but also in ESR Labs' own visualization plugin for Trace Compass. The plugin adds several views into Trace
Compass: "NVIDIA host/device runtime view" which shows runtime calls in each individual thread, on-device computations
and their causal links, "NVIDIA device contention view" which shows the device load in terms of waiting computations,
and some statistics on kernel execution delays under "Kernels".

In a multi-process setting, each process should be traced separately, at least
as of Nsight Systems 2020.5.3. This comes with a known issue that the
chronologically second instance of `nsys` will not be able to capture the thread
names, which might be slightly inconvenient but resolvable by means of the
kernel trace which provides the relation between thread names and thread IDs.

The overhead of CUDA tracing per call is negligible. However, every now and then
(controllable with the `--cuda-flush-interval`) the runtime is blocked for up to
$10$ ms until the CUDA-internal tracing buffer (that supposedly contains the
event log) is flushed to persistent storage (this can be seen as "CUPTI buffer
flush" events). This might be inconvenient if one wants to debug spurious timing issues in CUDA-intensive code and may
require careful setting of the `--cuda-flush-interval` setting.

Advanced usage
==============

## Analyzing multiple traces

It is often useful to look at different aspects of the system simultaneously, specifically when debugging timing issues.
For this, a native tooling of Trace Compass can be used. Multiple traces can be logically grouped into an _Experiment_,
which provides synchronized views for several traces. The experiment will then be treated as a unified trace with events
coming from selected traces.

However, loading too large traces (this specifically applies for Linux kernel
and CUDA events) may slow down the indexing and pre-processing significantly;
it is thus advised to either trim the corresponding traces to a small time 
interval of interest (~10 seconds or less) or to load only the traces that are 
of real interest.

## Trace trimming

As mentioned above, traces can get too long to be sensibly processed by Trace
Compass. To trim traces to a specific interval, there is a script at
`/path/to/mcf/mcf_tools/component_tracing/trim_traces.py`. When executed, it will
(non-destructively) trim all CTF and ftrace traces in a given directory to a 
given time interval.

From experience it seems useful to load full FLUX-related traces and 
trimmed kernel and CUDA traces in one experiment to maximize the signal/noise
ratio.
