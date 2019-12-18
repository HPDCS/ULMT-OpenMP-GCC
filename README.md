# ULMT-based implementation of the GNU OpenMP runtime

This project provides an implementation of OpenMP for the C, C++, and Fortran compilers in the GNU Compiler Collection that relies on the newest *User-Level-Micro-Thread* (ULMT) technology, which allows for effective management of tasks and their priorities. In more detail, this solution extends the GNU OpenMP (GOMP) runtime with newer facilities that are aimed to support ULMT-based execution of tasks still in accord with the tasking-model presented in the <a href="https://www.openmp.org/wp-content/uploads/openmp-4.5.pdf">OpenMP specification 4.5</a>.

ULMT differs from the classical *user-level-thread* (ULT) technology in that it allows to switch execution of tasks at arbitrary points in time. This is possible by making tasks capable of sliding out from the *control-flow-graph* (CFG) provided for the application at compile time, whereas the asychronous variation of the thread program flow is obtained through dedicated hardware support, *e.g.* the <a href="https://github.com/HPDCS/IBS-Support-ULMT">IBS-interrupt support</a>), which is a kernel module for Linux OS that provides the capability to perform *control-flow-variation* (CFV) of threads managed by the operating system upon the occurrence of *instruction-based-sampling* interrupts.

The revised design of the GOMP runtime, along with the support provided by dedicated hardware, allows to achieve 1) proompt switch to any higher priority task that is scheduled while a thread is processing a lower priority one and, 2) the avoidaince of thread blocking phases caused by dependencies across tasks (*currently occurs in `taskwait`, entering in `critical` sections and attempting acquisition of `omp_lock` with the native GOMP runtime*) that have been bound to different threads. This version of the runtime, instead, avoids thread blocking phases by giving control to the task scheduling function that looks for pending tasks always respecting the *task-scheduling-constraints* (TSC) imposed by the OpenMP specification for `tied` and `untied` tasks. Under the conditions for which TSCs do not prevent the scheduler from pick pending tasks, the execution model implements a *work-conserving* policy [1,2] that the ULMT version of GOMP runtime is capable to exploit (this does not happen with the original runtime).


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

* OMP_AUTO_CUTOFF=[**true**|false]
> This variable allows to emable or to disable the basic *task throttling* heuristic provided by GOMP. The default value is *true*. Nevertheless, we strongly reccomend to disable it as it is proved to be harmful [3] for some application classes.

* OMP_UNTIED_BLOCK=[**true**|false]
> This variables mitigate the aggressive behaviour through which the scheduler context-switch ULMT-based tasks. The default value is *true*. Do not change it unless you experience slow-downs that are deemed to be caused by a too conservative behaviour of the scheduler.

* OMP_ULT_THREADS=[true|**false**]
> Allows to enable all the capabilities that the ULMT version of GOMP provides. Differently, the original version of GOMP will be used which is also the default configuration.

* OMP_ULT_STACK=[**512K**]
> It works in combination with OMP_ULT_THREADS and it's used to set the stack size that each task. Default value is 512 KB.

* OMP_IBS_RATE=[**0**]
> It works in combination with OMP_ULT_THREADS and it's used to enable IBS-interrupt support on AMD machine by passing the interrupt time interval value expressed as number of clocks. It requires the IBS-module would have been installed. Conversely the default value is 0, meanng no IBS-interrupt support will be used.

* OMP_QUEUE_POLICY=[0x**FFFF0F**]
> For an exhaustive explaination of the values that can be assigned to this variable we reccomend you to read comments placed inline in the header *task.h*. Do not change this value unless you know what you are doing.

### Execution Environment Routines

* int omp_get_autocutoff (void)
> Returns current value of OMP_AUTO_CUTOFF.

* int omp_get_untied_block (void)
> Returns current value of OMP_UNTIED_BLOCK.

* void omp_set_untied_block (int val)
> Sets value of OMP_UNTIED_BLOCK to *val*.

* int omp_get_ult_threads (void)
> Returns current value of OMP_ULT_THREADS.

* void omp_set_ult_threads (int val)
> Called just prior to create a parallel region, it sets OMP_ULT_THREADS to *val* and enables ULMT.

* unsigned long omp_get_ult_stack (void)
> Returns current value of OMP_ULT_STACK.

* void omp_set_ult_stack (unsigned long val)
> Called just prior to create a parallel region, it sets OMP_ULT_STACK to *val*.

* int omp_get_ibs_rate (void)
> Returns current value of OMP_IBS_RATE.

* void omp_set_ibs_rate (unsigned long val)
> Called just prior to create a parallel region, it sets OMP_IBS_RATE to *val* and enables the IBS-interrupt support.

* int omp_get_queue_policy (void)
> Returns current value of OMP_QUEUE_POLICY.

* void omp_set_queue_policy (unsigned long val)
> Sets value OMP_QUEUE_POLICY to *val*.


## References

<p><sub>
[1] M. A. Serrano, A. Melani, R. Vargas, A. Marongiu, M. Bertogna, and E. Quinones, “Timing characterization of openmp4 tasking model”, in 2015 International Conference on Compilers, Architecture and Synthesis for Embedded Systems, CASES 2015, Amsterdam, The Netherlands, October 4-9, 2015, 2015, pp. 157–166.
</sub></p>

<p><sub>
[2] J. Sun, N. Guan, Y. Wang, Q. He, and W. Yi, “Real-time scheduling and analysis of openmp task systems with tied tasks”, in 2017 IEEE Real-Time Systems Symposium, RTSS 2017, Paris, France, December 5-8, 2017, 2017, pp. 92–103.
</sub></p>

<p><sub>
[3] T. Gautier, C. Perez, and J. Richard, “On the impact of openmp task granularity”, in Evolving OpenMP for Evolving Architectures - 14th International Workshop on OpenMP, IWOMP 2018, Barcelona, Spain, September 26-28, 2018, Proceedings, 2018, pp. 205–221.
</sub></p>
