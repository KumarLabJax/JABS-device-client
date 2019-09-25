# Installation

Before you can compile this program, you must initialize git submodules. This pulls in 
a 3rd party library that we depend on. After cloning this repository, running
`git submodule update --init` will add submodules to your git config and clone them. 

This software uses the Microsoft C++ REST SDK: https://github.com/microsoft/cpprestsdk

For Ubuntu systems, `sudo apt install libcpprest-dev` is sufficient for installation.