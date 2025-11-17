# Flux + mpibind in local development containers

The Flux Framework is a suite of distributed services that provide
resource management and scheduling for any Linux system, from a
laptop to the world's largest supercomputers. Flux's ability to run
locally in containers provides a layer of portability for application
developers and scientific programmers.

In this demo, we'll explore Flux's ability to run locally and both
Flux and mpibind's support for affinity and binding. We'll explain
how to get Flux containers through docker hub and work with binding
on just a laptop — no supercomputer required.

## What is Flux?
Flux is a suite of projects that you can compose together or use separately
for services, job launch, scheduling and resource management. At LLNL, the
Flux Framework has been under active development since 2012. Several of Flux's
innovations include:
- hierarchical job launch
- scheduling and messaging at each job level independently
- a small security footprint (very little of Flux requires elevated privilege)

These innovations, like having a small security footprint, mean that you can 
run jobs with Flux on your laptop just as you would on a supercomputer that 
uses Flux to schedule an entire cluster.

## What is mpibind?
mpibind is an application developed by Edgar Leon at LLNL (a "friend of flux", 
if you will) that applies an algorithm to applications to map parallel applications
to hardware resources with a focus on memory and cache hierarchy. In other resource
managers, this mapping is handled by an elevated privilege prolog, but this demo
will show it running in a container.

## Wait, you mean I can get Flux running on my laptop?
Yes, and that's exactly what we're here to do. Locally, let's assume
you have Docker installed and the Docker daemon running.

For this demo, clone the repository I created first

```
git clone https://github.com/wihobbs/doe-booth.git
cd doe-booth/demo1
docker build -t demo .
docker run -it demo
```
The flux-core and flux-sched containers are published on Dockerhub 
with [many different tags for different distros and architectures](https://hub.docker.com/u/fluxrm).

With just this container, you can start a flux _instance_, and have a local
resource manager running on your laptop. Try it out with:

```
flux start
```

Then, try it out with:
```
flux start -s 4
```

The `-s` flag to create a "test instance" sets Flux up to start multiple brokers
per node and fool itself into thinking it has more resources than it actually does.
It's a great way to test things out for development or other purposes. Notice what
comes back when you try:

```
flux resource list
```
and / or 
```
flux queue list
```

### Back up, why did I have to pull so many different containers?
As we said before, the Flux Framework is actually a _suite_ of different 
services, so we actually have several different projects (and containers to
serve purposes for each one).

**flux-core**
This is the core set of services for the Flux framework, including a key-value
store, brokers that transmit messages over an overlay network, and job submission
tools.

**flux-sched** 
The Fluxion scheduler enables scheduling beyond cores and nodes, including gpus,
multiple different scheduling policies, and is being expanded to include locality
and subsystem scheduling.

## How do I submit jobs through Flux?
Here's a basic table that shows the four submission commands we use in Flux. 

|                        | creates subinstance           | runs distributed application          |
|------------------------|-------------------------------|---------------------------------------|
| interactive            | `flux alloc`                  | `flux run`                            |
| backgrounded           | `flux batch`                  | `flux submit`👀                       |

* `flux alloc` will allocate resources and start an interactive
  Flux sub-instance underneath those resources. Within that subinstance,
  you can submit as many jobs as you like, with no worry about backing
  up the parent (usually system) instance.  
* `flux batch` will also allocate resources and start a Flux sub-instance, 
  but the job is not interactive, and thus `batch` requires a script outlining 
  the work to do.  
* `flux run` runs a program under a Flux instance. It does not create a new 
  sub-instance, and will watch until the program completes.  
* `flux submit` does not exist in other resource managers,
  notably Slurm. It does the same thing as `flux run`, but does not
  watch for job output, instead writing this to a file.

So, in our test container, let's get started with some simple applications 
that leverage these features.

Start by running `make` in the container. Then, try simple job submission with:

```
flux run -N1 -n10 ./hello
flux alloc -N1 -n10 ./hello

flux batch -N1 -n10 ./hello
flux batch -N1 -n10 --wrap --output=./hello.out --error=./hello.err ./hello
flux watch $(flux job last)
```
Notice the last one doesn't run 16 MPI tasks, but rather creates an 
_initial program_ of `./hello` and then allocates 10 cores to it.

Some submission flags of note
* `-N` specifies a number of nodes
* `-n` specifies a number of tasks for distributed applications, and cores for interactive allocations
* `--exclusive` with `-N` forces 
* `-c` specifies a number of cores per task
* `--requires` constrains a job to run on a specific rank or hostname
* `--dependency` makes a job depend on another job
* `-cc` submits carbon copies of the same job many times
* `--output` and `--error` redirect output and error to files

There are a lot of flags to adjust what your job actually does: 
```
flux submit -N1 -n4 -c2 --requires=hosts:$(hostname) --output /tmp/file.txt --error /dev/null ./hello
flux run -N1 --dependency=afterok:$(flux job last) cat /tmp/file.txt
```

Now, let's look into how binding works in Flux.

## What is the `-o cpu-affinity` option?
Affinity is the process of mapping specific tasks of parallel applications to
specific cores or gpus in resources. We have support for this in Flux natively, 
through the cpu-affinity shell plugin. 

The cpu-affinity shell plugin is controlled by the short option `-o`, which
controls the shell, and passing a `KEY[=VAL]` pair where the `KEY` is `cpu-affinity`
and the `VAL` are the options you wish to employ with `cpu-affinity`. 

`-o cpu-affinity=on` is the default, and will assign all your tasks to all 
available cores you requested. For example,
```
flux run -N1 -n4 -o mpibind=off -o cpu-affinity=verbose,on ./hello
flux run -N1 -n4 -o mpibind=off -o cpu-affinity=verbose,on ./vcpu
```

`-o cpu-affinity=per-task` will assign cpus based on the number of tasks you have
told Flux you are going to start. For example, try
```
flux run -N1 -n4 -c2 -o mpibind=off -o cpu-affinity=verbose,per-task ./hello
```
And see how the MPI hello application gets mapped to tasks. You can also provide
a custom mapping of tasks to cores, i.e.
```
flux run -N1 -n4 -c2 -o mpibind=off -o cpu-affinity=map:1-4 ./hello
flux run -N1 -n4 -c2 -o mpibind=off -o cpu-affinity=verbose,map:1\;7\;8\;9 ./hello
```
Note the `\` character is just to escape the `;` in the bash shell.

`-o cpu-affinity=off` is implied when another affinity-based shell plugin is being
used, like mpibind.

## You mentioned shell plugins, what are those?
Affinity is the process of mapping specific tasks of parallel applications to
specific cores or gpus in resources. To start applications, Flux spawns a shell
whenever a job starts, and a shell can have one or more plugins providing 
additional information, extra functionality, or options.

Shell plugins are shared object libraries loaded at runtime that implement a set
of defined callbacks.

cpu-affinity and mpibind are both examples of shell plugins, and both take data
about the job they're going to start and use that to make decisions about which
cores should be used to run processes.

## How can I get mpibind running in the container?

It's as simple as:
```
git clone https://github.com/LLNL/mpibind.git
mkdir build
cd mpibind
./bootstrap
./configure --prefix=$(pwd)/../build
make -j 8 install
export FLUX_SHELL_RC_PATH=/home/fluxuser/build/share/mpibind:$FLUX_SHELL_RC_PATH
```

All of these commands in the container are wrapped into the `build-mpibind.sh` 
script, although you will have to set the last environment variable manually.

## What are the binding options I can use with `-o mpibind`?
Because of the setup of the container, by default mpibind is going to be 
doing our affinity mappings for us. You can see this if you run
```
flux run -N1 -o verbose=1 hostname
```
which will show that mpibind was a loaded plugin, although it didn't do much
because this was only a one node/one-core job. 

mpibind has more options for how to space programs out across the whole node,
like `-o mpibind=greedy:1` and `-o mpibind=smt:1`. Here's some commands to
illustrate what those will do:

With mpibind we can also restrict the number of cores  that can be used 
for the job:
```
MPIBIND_RESTRICT=1,2,3 flux run -n7 -o mpibind=verbose hostname
```
which will make sure that our 7 tasks are only run on cpu 1, 2, or 3. You can 
check this with the vcpu program in the container. 

mpibind also provides an option, `MPIBIND_RESTRICT_TYPE`, which can be
set to either `cpu` or `mem` to optimize its mappings for either, respectively.

I also put Ben Cumming's [affinity package](https://github.com/bcumming/affinity) 
in this container for an MPI and OpenMP demo of affinity within the container.

## For more information, see:

* The fabulous [Flux Tutorial Series](https://github.com/converged-computing/flux-tutorials/)
  by dinosaur extraordinaire Vanessa Sochat
* A [more in-depth mpibind tutorial](https://github.com/LLNL/mpibind/tree/master/tutorials/flux)
  by Jane Herriman and Edgar Leon
* The Flux manual pages, particularly the `cpu-affinity` section.