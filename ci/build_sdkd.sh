#!/bin/sh

set -e
set -x

. $WORKSPACE/sdkd-cpp-env.sh
JP=$WORKSPACE/json-cpp
wd=$PWD

cd $JP
make -f makefiles/gcc/Makefile

cd $wd
cd $WORKSPACE/sdkd-cpp

make -C src clean
make -C src \
    JSONCPP_CPPFLAGS="-I$JP/include" \
    JSONCPP_LFLAGS="-L$JP/lib -Wl,-rpath=$JP/lib -ljson-cpp" \
    LCB_CPPFLAGS="-I$CI_ROOT/include" \
    LCB_LFLAGS="-Wl,-rpath=$CI_ROOT/lib -L$CI_ROOT/lib -lcouchbase"

cd $wd
