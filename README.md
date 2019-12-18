# ULMT-based implementation of the GNU OpenMP runtime

This project provides an implementation of OpenMP for the C, C++, and Fortran compilers in the GNU Compiler Collection that relies on the newest *User-Level-Micro-Thread* (ULMT) technology, which allows for effective management of tasks and their priorities. In more detail, this solution extends the GNU OpenMP (GOMP) runtime with newer facilities that are aimed to support ULMT-based execution of tasks still in accord with the tasking-model presented in the <a href="https://www.openmp.org/wp-content/uploads/openmp-4.5.pdf">OpenMP specification 4.5</a>.

ULMT differs from the classical *user-level-thread* (ULT) technology in that it allows to switch execution of tasks at arbitrary points in time. This is possible by making tasks capable of sliding out from the *control-flow-graph* (CFG) provided for the application at compile time, whereas the asychronous variation of the thread program flow is obtained through dedicated hardware support, *e.g.* the <a href="https://github.com/HPDCS/IBS-Support-ULMT">IBS-interrupt support</a>), which is a kernel module for Linux OS that provides the capability to perform *control-flow-variation* (CFV) of threads managed by the operating system upon the occurrence of *instruction-based-sampling* interrupts.

The revised design of the GOMP runtime, along with the support provided by dedicated hardware, allows to achieve 1) proompt switch to any higher priority task that is scheduled while a thread is processing a lower priority one and, 2) the avoidaince of thread blocking phases caused by dependencies across tasks (currently occurs in `taskwait`, entering in `critical` sections and attempting acquisition of `omp_lock`s with the native GOMP runtime) that have been bound to different threads. This version of the runtime, instead, avoids thread blocking phases by giving control to the task scheduling routine that looks for pending tasks, waiting to be executed, always respecting the *task-scheduling-constraints* (TSC) imposed by the OpenMP specification for `tied` and `untied` tasks.

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
>  ./omp-prog
```
