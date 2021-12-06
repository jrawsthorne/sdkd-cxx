# C++ SDKD Implementation

This is the couchbase-cxx-client-based sdkd implementation in C++, it utilizes [couchbase-cxx-client](https://github.com/couchbaselabs/couchbase-cxx-client/) to perform SDK operations.

## Prerequisites

- CMake 3.17+
- couchbase-cxx-client
- C and C++ compilers (CXX 17 standard)
- Python (2.x)
- Linux, OS X, or Windows with VS9 or higher

## Build Steps

This documents how to build the SDKD for couchbase-cxx-client.

The SDKD has several submodules you need to initialize.
This assumes the couchbase-cxx-client source is already cloned somewhere.

1. First, clone the submodules for this repository
```
$ git submodule init
$ git submodule update
```

2. Afterwards, you will need to generate the 'amalgamated' I<json-cpp> files
```
$ cd src/contrib/json-cpp
$ python amalgamate.py
```

3. You're all set. You might want to define some I<CMake> variables.
You'll also probably want a separate build directory. From the
source root, do:
```
$ mkdir build
$ cd build
$ cmake -DCXX_ROOT=/Code/couchbase-cxx-client -DCMAKE_BUILD_TYPE=Debug ../
$ make
```

Where the `CXX_ROOT` variable defines the directory couchbase-cxx-client is cloned to

Once installed, it should be present as `build/sdkd_lcb`

## Using

This is a normal SDKD implementation; as such it listens on a port for commands
from the `cbsdkd` harness (i.e. `brun`, `stester`, and `cbsdk_client`).

The following options are influential

`-l` `--listen` PORT

Port to listen on

`-P` `--persist`

By default, the SDKD exits after each harness session. This option makes the
SDKD persist between sessions and function somewhat like a standalone daemon.

`-ttl` SECONDS

Set the 'TTL' timer for the SDKD. This will set the absolute runtime for which
the SDKD can run before it C<abort(3)>s. This may also be adjusted by the
harness itself. This is helpful for automated runs and ensures the SDKD does
not hang indefinitely.

`-V` `--version`

Prints information about this SDKD (including couchbase-cxx-client version information)
