# ULMT-based implementation of the GNU OpenMP runtime

This project provides an implementation of OpenMP for the C, C++, and Fortran compilers in the GNU Compiler Collection that relies on the newest *User-Level-Micro-Thread* (ULMT) technology, which allows for effective management of tasks and their priorities. More in detail, this solution extends the GNU OpenMP (GOMP) runtime with newer facilities that are aimed to support ULMT-based execution of tasks still in accord with the specification of the OpenMP tasking-model presented in the <a href="https://www.openmp.org/wp-content/uploads/openmp-4.5.pdf">OpenMP specification 4.5</a>.

ULMT differs from the classical *user-level-thread* (ULT) technology in that it allows to switch execution of tasks at arbitrary points in time. This is possible by making tasks capable of sliding out from the *control-flow-graph* (CFG) provided for the application at compile time, whereas the asychronous variation of the thread program flow is obtained through dedicated hardware support, *e.g.* the <a href="https://github.com/HPDCS/IBS-Support-ULMT">IBS-interrupt support</a>). The latter is a kernel module for Linux OS that provides to programs the capability to perform *control-flow-variation* (CFV) of threads managed by the operating system upon the occurrence of *instruction-based-sampling* (IBS) interrupts.

The revised design of the GOMP runtime, along with the support provided by dedicated hardware, allows to achieve 1) proompt switch to any higher priority task that is scheduled while a thread is processing a lower priority one and, 2) the avoidaince of thread blocking phases caused by dependencies across tasks (*currently occurs in `taskwait`, entering in `critical` sections and attempting acquisition of `omp_lock` with the native GOMP runtime*) that have been bound to different threads. Both of the above points were not feasible in the original version of GNU OpenMP. This version of the runtime, instead, avoids thread blocking phases by giving control to the task scheduling function that looks for pending tasks always respecting the *task-scheduling-constraints* (TSC) imposed by the OpenMP specification for `tied` and `untied` tasks. Under the conditions for which TSCs do not prevent the scheduler from pick pending tasks, the execution model implements a *work-conserving* policy that the ULMT version of GOMP runtime is capable to exploit (this does not happen with the original one).


## Compilation and installation

To compile the ULMT version of GOMP runtime download first the <a href="https://ftp.gnu.org/gnu/gcc/gcc-7.2.0/gcc-7.2.0.tar.gz">GCC 7.2.0</a> archive in the preferred path, extract it and substitute the **libgomp** folder with the one provided by this GitHub repository. This includes new sources and different Makefile to generate the ULMT version of the GOMP shared library against which you'll compile your OpenMP programs. Then, from the folder where you have previously extracted the archive, launch the following commands.

```sh
>  mkdir gcc-7.2.0-objs
>  cd gcc-7.2.0-objs
>  ../gcc-7.2.0/configure --with-pkgversion=7.2.0-1 --disable-multilib --enable-languages=c,c++,fortran
>  make [-j <NUM_CPUs>]
```

In case you are intended to install the shared library in the predefined folder (commonly `/lib` and `/usr/lib`) also launch the command shown below (requires superuser privileges). This will make you able to generate OpenMP applications by simply providing the `-fopenmp` option when compiling with GCC.

```sh
>  sudo make install
```

## Basic usage

To eploit the capabilities offered by this version of the GOMP runtime, programmers must either set dedicated environmint variables or including newest execution environment routines (presented below). The native version of GOMP runtime will be used otherwise. For instance, to run your OpenMP program, named `omp-prog`, with the original GOMP runtime simply launch

```sh
>  ./omp-prog
```

Differently the following command will run your OpenMP program with the ULMT version

```sh
>  OMP_IBS_RATE=110000 OMP_ULT_THREADS=true OMP_ULT_STACK=128K ./omp-prog
```


## Synopsis


### Environment Variables

OMP_AUTO_CUTOFF=[**true**|false]
> This variable allows to emable or to disable the basic *task throttling* heuristic provided by GOMP. The default value is *true*. Nevertheless, we strongly reccomend to disable it as it is proved to be harmful for some application classes.

<br>

OMP_UNTIED_BLOCK=[**true**|false]
> This variables mitigate the aggressive behaviour through which the scheduler context-switch ULMT-based tasks. The default value is *true*. Do not change it unless you experience slow-downs that are deemed to be caused by a too conservative behaviour of the scheduler.

<br>

OMP_ULT_THREADS=[true|**false**]
> Allows to enable all the capabilities that the ULMT version of GOMP provides. Differently, the original version of GOMP will be used which is also the default configuration.

<br>

OMP_ULT_STACK=[**512K**]
> It works in combination with OMP_ULT_THREADS and it's used to set the stack size that each task. Default value is 512 KB.

<br>

OMP_IBS_RATE=[**0**]
> It works in combination with OMP_ULT_THREADS and it's used to enable IBS-interrupt support on AMD machine by passing the interrupt time interval value expressed as number of clocks. It requires the IBS-module would have been installed. Conversely the default value is 0, meanng no IBS-interrupt support will be used.

<br>

OMP_QUEUE_POLICY=[0x**FFFF0F**]
> For an exhaustive explaination of the values that can be assigned to this variable we reccomend you to read comments placed inline in the header *task.h*. Do not change this value unless you know what you are doing.


### Execution Environment Routines

```c
int omp_get_autocutoff (void)
```
> Returns current value of OMP_AUTO_CUTOFF.

<br>

```c
int omp_get_untied_block (void)
```
> Returns current value of OMP_UNTIED_BLOCK.

<br>

```c
void omp_set_untied_block (int val)
```
> Sets value of OMP_UNTIED_BLOCK to *val*.

<br>

```c
int omp_get_ult_threads (void)
```
> Returns current value of OMP_ULT_THREADS.

<br>

```c
void omp_set_ult_threads (int val)
```
> Called just prior to create a parallel region, it sets OMP_ULT_THREADS to *val* and enables ULMT.

<br>

```c
unsigned long omp_get_ult_stack (void)
```
> Returns current value of OMP_ULT_STACK.

<br>

```c
void omp_set_ult_stack (unsigned long val)
```
> Called just prior to create a parallel region, it sets OMP_ULT_STACK to *val*.

<br>

```c
int omp_get_ibs_rate (void)
```
> Returns current value of OMP_IBS_RATE.

<br>

```c
void omp_set_ibs_rate (unsigned long val)
```
> Called just prior to create a parallel region, it sets OMP_IBS_RATE to *val* and enables the IBS-interrupt support.

<br>

```c
int omp_get_queue_policy (void)
```
> Returns current value of OMP_QUEUE_POLICY.

<br>

```c
void omp_set_queue_policy (unsigned long val)
```
> Sets value OMP_QUEUE_POLICY to *val*.
