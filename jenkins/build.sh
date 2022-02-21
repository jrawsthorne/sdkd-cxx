# ---- Setup Java/Maven for sdkdclient-ng ---- #
SPACE_TO_DASH=`echo $JAVA_VERSION | sed 's/ /-/g'`
cbdep install -d deps ${JAVA_VERSION}
(cd ${WORKSPACE}/deps && ls)
export JAVA_HOME=${WORKSPACE}/deps/${SPACE_TO_DASH}
export PATH=${JAVA_HOME}/bin:$PATH
(cd ${JAVA_HOME} && ls)

export M2_HOME=/usr/local/apache-maven-3.5.4
export PATH=$PATH:$M2_HOME/bin

java -version
mvn -version

cbdep install python 2.7.18 -d deps
export PATH=${WORKSPACE}/deps/python2.7.18/bin:$PATH
python --version

# latest cbdyncluster
#cbdep install -d deps golang 1.13.6
#export PATH=${WORKSPACE}/deps/go1.13.6/bin:$PATH
#git clone https://github.com/couchbaselabs/cbdyncluster.git
#cd cbdyncluster
#GOOS=linux GOARCH=amd64 go build -o cbdyncluster
#chmod +x cbdyncluster
#export PATH=${WORKSPACE}/cbdyncluster/:$PATH
#cd ..


# ----- Build lcb and sdkd ------ #
cd $WORKSPACE
rm -rf couchbase-cxx-client
if [ $IS_PR == true ]; then
	git clone --recurse-submodules -c "remote.origin.fetch=+refs/pull/*:refs/remotes/origin/pr/*" https://github.com/couchbaselabs/couchbase-cxx-client.git
else
	git clone --recurse-submodules https://github.com/couchbaselabs/couchbase-cxx-client.git
fi


cd couchbase-cxx-client
git checkout $lcb_commit

# GERRIT_COMMIT example=14/82314/5
#if [ ! -z "$GERRIT_COMMIT" ]
#then
#	git fetch http://review.couchbase.org/libcouchbase refs/changes/${GERRIT_COMMIT} && git checkout FETCH_HEAD
#fi

git log -n 2

#if [ "x${SANITIZER}" = "xasan" ]
#then
#	sudo yum install -y libasan
#    CMAKE_FLAGS_EXTRA="-DLCB_USE_ASAN=ON"
#fi

#cmake -DCMAKE_INSTALL_PREFIX=$PWD/inst -DLCB_NO_SSL=0 -DCMAKE_BUILD_TYPE=Debug ${CMAKE_FLAGS_EXTRA} ./
#make -j 8
#make install

cd $WORKSPACE
rm -rf sdkd-cxx
git clone --recurse-submodules https://github.com/couchbaselabs/sdkd-cxx.git
mkdir -p local/proc
mkdir -p local/pid

cd sdkd-cxx
git checkout $sdkd_commit

#git submodule init
#git submodule update
cd src/contrib/json-cpp
python amalgamate.py
cd ../../../

sed -i "s:/proc/self/fd:$WORKSPACE/local/proc:g" src/UsageCollector.cpp
sed -i "s:/var/run/sdkd-cpp.pid:$WORKSPACE/local/pid/sdkd-cpp.pid:g" src/sdkd_internal.h
sed -i "s:python3:python:g" src/Control.cpp
#sed -i "s:if (lcb_respview_status:if (false \&\& lcb_respview_status:g" src/ViewExecutor.cpp


mkdir build
cd build
cmake -DCXX_ROOT=$WORKSPACE/couchbase-cxx-client -DCMAKE_BUILD_TYPE=Debug ../
make -j 8
