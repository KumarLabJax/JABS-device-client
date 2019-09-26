# Installation

## Compilation

Before you can compile this program, you must initialize git submodules. This pulls in 
a 3rd party library that we depend on. After cloning this repository, running
`git submodule update --init` will add submodules to your git config and clone them. 

This software uses the Microsoft C++ REST SDK: https://github.com/microsoft/cpprestsdk

For Ubuntu systems, `sudo apt install libcpprest-dev` is sufficient for installation.

## Configuring as a daemon

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
