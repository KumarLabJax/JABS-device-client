# JAX Mouse Behavior Analysis

## Overview

This program implements a service that is run continuously on the video 
acquisition computers. It monitors the system, periodically gathering info on 
disk space consumption, system load, memory utilization, etc and then sends 
that information to the JAX MBA webservice. It will also perform video
acquisition on request from the webservice.

## Installation

### Platform

This software **only** supports Linux.

Windows and Unix (including but not limited to BSD and Darwin (OS X)) are not 
currently supported. 

### Compilation

A C++11 compliant compiler is required. Any recent version of gcc or clang
should be sufficient.

#### Git submodules

Before you can compile this program, you must initialize git submodules. This
pulls in a 3rd party library that we depend on for reading ini config files.
After cloning this repository, running `git submodule update --init` will add
submodules to your git config and clone them. 

#### Other dependencies

This software uses the Microsoft C++ REST SDK:
https://github.com/microsoft/cpprestsdk

For Ubuntu systems, `sudo apt install libcpprest-dev` is sufficient for 
installation of the Microsoft C++ REST SDK and its dependencies (such as Boost).

The Basler pylon SDK, which we use to control our camera, is also required. It 
can be obtained from the Basler website:
https://www.baslerweb.com/en/products/software/. Our Makefile assumes Pylon is 
installed into /opt/pylon5. If this is not the case for you, you will need to 
modify the `PYLONDIR` variable in the Makefile. 

You will also need several ffmpeg libraries, and their development header files. 
We recommend installing ffmpeg from source rather than depending on the older 
versions provided by the OS package manger.

The final dependency is on libsystemd-dev, which can be installed with 
`sudo apt install libsystemd-dev` on Ubuntu. 

#### Make

Once the dependencies are installed, running  the `make` command in this
directory will compile the JAX-MBA client. Running `sudo make install` will 
install the binary in `/opt/jax-mba/bin/` and a config file template in 
`/opt/jax-mba/conf/jax-mba.ini.example`. This config file will have to be
completed with system-specific settings and renamed
`/opt/jax-mba/conf/jax-mba.ini` before the service can be started. `make install`
will also copy a Systend Unit file into `/etc/systemd/system/`

### Configuring as a daemon

This program is intended to be run as a 'new style' daemon (managed by systemd).
It uses `sd-notify` to let systemd know when it is finished initializing and is
ready, therefore it can be configured using `Type=notify` in the [Service] 
section of its systemd Unit file.

A basic systemd service file will be copied to 
`/etc/systemd/system/mba-client.service` by `make install`. It can be started
with `sudo systemctl start mba-client`. Run `sudo systemctl enable mba-client`
to have it started automatically at boot. This file assumes the binary is 
located at `/opt/jax-mba/bin/mba-client`, the config file is at 
`/opt/jax-mba/conf/jax-mba.ini`, and the name of the user the service will run
as is `nvidia`. If these assumptions are not true, then you will need to modify
this file.

The program can also be run directly outside of the control of systemd, in
which case `sd-notify` is a noop. All stderr/stdout lines are prefixed by a
loglevel, which is represented by a string of the format`<LOG_LEVEL>`.

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
