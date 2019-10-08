# Long Term Monitoring System Readme

## Overview

This program implements a service that is run continuously on the video 
acquisition computers. It monitors the system, periodically gathering info on 
disk space consumption, system load, memory utilization, etc and then sends 
that information to the LTMS webservice. It will also perform video acquisition 
on request from the webservice.

## Installation

### Platform

This software *only* supports Linux.

Windows and Unix (including but not limited to BSD and Darwin (OS X)) are not 
currently supported. 

### Compilation

A C++11 compliant compiler is required. Any recent version of gcc or clang
should be sufficient.

Before you can compile this program, you must initialize git submodules. This pulls in 
a 3rd party library that we depend on. After cloning this repository, running
`git submodule update --init` will add submodules to your git config and clone them. 

This software uses the Microsoft C++ REST SDK: https://github.com/microsoft/cpprestsdk

For Ubuntu systems, `sudo apt install libcpprest-dev` is sufficient for 
installation of the Microsoft C++ REST SDK and its dependencies (such as Boost).

The Basler pylon SDK, which we use to control our camera, is also required. It 
can be obtained from the Basler website: https://www.baslerweb.com/en/products/software/

You will also need libavcodec, libavformat, libavfilter, and their development header files. 
On Ubuntu, `sudo apt install libavcodec-dev libavcodec-dev libavfilter-dev` will
install these and their dependencies.

### Configuring as a daemon

This program is intended to be run as a 'new style' daemon (managed by systemd). It uses 
`sd-notify` to let systemd know when it is finished initializing and is ready, therefore 
it can be configured using `Type=notify` in the [Service] section of its systemd Unit file

The program can also be run directly outside of the control of systemd, in which case 
`sd-notify` is a noop. All stderr/stdout lines are prefixed by a loglevel, which is 
represented by a string of the format`<LOG_LEVEL>`.

```C
#define SD_EMERG   "<0>"  /* system is unusable */
#define SD_ALERT   "<1>"  /* action must be taken immediately */
#define SD_CRIT    "<2>"  /* critical conditions */
#define SD_ERR     "<3>"  /* error conditions */
#define SD_WARNING "<4>"  /* warning conditions */
#define SD_NOTICE  "<5>"  /* normal but significant condition */
#define SD_INFO    "<6>"  /* informational */
#define SD_DEBUG   "<7>"  /* debug-level messages */
```
