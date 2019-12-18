# ULMT-based implementation of the GNU OpenMP runtime

This project provides an implementation of OpenMP for the C, C++, and Fortran compilers in the GNU Compiler Collection that relies on the newest *User-Level-Micro-Thread* (ULMT) technology, which allows for effective management of tasks and their priorities. In more detail, this solution extends the GNU OpenMP (GOMP) runtime with newer facilities that are aimed to support ULMT-based execution of tasks still in accord with the tasking-model presented in the <a href="https://www.openmp.org/wp-content/uploads/openmp-4.5.pdf">OpenMP specification 4.5</a>.

ULMT differs from the classical *user-level-thread* (ULT) technology in that it allows to switch execution of tasks at arbitrary points in time. This is possible by making tasks capable of sliding out from the *control-flow-graph* (CFG) provided for the application at compile time, whereas the asychronous variation of the thread program flow is obtained through dedicated hardware support (e.g., <a href="https://github.com/HPDCS/IBS-Support-ULMT">IBS interrupt support for ULMT technology</a>).

The revised design of the GNU GOMP runtime, along with the support provided by dedicated hardware, allows to achieve 1) proompt switch to any higher priority task that is scheduled while a thread is processing a lower priority one and, 2) the avoidaince of thread blocking phase caused by dependencies across tasks (currently occurs in `taskwait`, entering in `critical` section and attempting acquisition of `omp_lock`s with the native GNU GOMP runtime) that have been bound to different threads.

To compile ULMT-based version of GNU OpenMP runtime download the <a href="https://ftp.gnu.org/gnu/gcc/gcc-7.2.0/gcc-7.2.0.tar.gz">GCC 7.2.0</a> archive in the preferred path, extract it and substitute the **libgomp** folder with the one provided by this GitHub repository. This includes new sources and Makefile to generate the ULMT-based version of the GOMP shared library against which you'll compile your OpenMP programs. Thus, from folder where you extracted the archive, launch the following commands.

```sh
>  mkdir gcc-7.2.0-objs
>  cd gcc-7.2.0-objs
>  ../gcc-7.2.0/configure --with-pkgversion=7.2.0-1 --disable-multilib --enable-languages=c,c++,fortran
>  make [-j <NUM_CPUs>]
```

Optionally install the shared library in the predefined folder (requires superuser privileges) with the command reported below. This will make you able to generate OpenMP applications by simply providing the **-fopenmp** option when compiling with GCC.

```sh
>  sudo make install
```
