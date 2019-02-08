# cse-proj2

CSE 124 Winter 2019 Project 2 starter code

## Dependencies

This project relies on four dependencies:

* inih
  * A header-only config file handle (https://github.com/jtilly/inih)
* spdlog
  * A fast thread-safe logging library (https://github.com/gabime/spdlog)
* rpclib
  * A simple c++ RPC library (http://rpclib.net/)
* picosha2
  * The PicoSHA2 SHA-256 hash generator (https://github.com/okdshin/PicoSHA2)

## Preparing the starter code for your environment

You must use the same compiler to build the rpc library as you use
for the rest of your project, otherwise you will get a series of errors.
Thus to start working on the project, you need to re-build the rpc
library.

To rebuild the library, do the following:

1. cd dependencies/src
1. tar -xzf cmake-3.13.2-Linux-x86_64.tar.gz
1. tar -xzf rpclib.tar.gz
1. cd rpclib
1. mkdir build
1. cd build
1. ../../cmake-3.13.2-Linux-x86_64/bin/cmake ..
1. ../../cmake-3.13.2-Linux-x86_64/bin/cmake --build .
  * (note the dot after --build)
1. cp librpc.a ../../../lib/

OK you should be set.  Proceed to testing your starter code.

## Testing the starter code

To ensure that your starter code is ready to go, cd to the "src" directory off
the main directory, and type "make".

In one window, run the server:

* ./ssd myconfig.ini

In another window, run the client:

* ./ss myconfig.ini

You should see a few of the basic RPC calls get executed without any error
messages.  If you see any kind of error message or error output, contact the
teaching staff.
