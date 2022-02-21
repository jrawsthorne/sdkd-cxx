SPACE_TO_DASH=`echo $JAVA_VERSION | sed 's/ /-/g'`
export JAVA_HOME=${WORKSPACE}/deps/${SPACE_TO_DASH}
export PATH=${JAVA_HOME}/bin:$PATH

cbdep install python 3.8.6 -d deps
export PATH=${WORKSPACE}/deps/python3.8.6/bin:$PATH
python --version
pip install boto3


 
 cd $WORKSPACE
 rm -rf sdkdclient-ng
 git clone https://sdkqe:$token@github.com/${SDKDCLIENT_NG_GITHUB_REPO}.git
 cd  sdkdclient-ng
 git checkout $ng_commit


mvn clean package -q -DskipTests=true 

export BRUN_PERCENTILE=99

if [[ $cluster_version =~ ^([0-9]+)\.([0-9]+) ]]
then
    major=${BASH_REMATCH[1]}
    minor=${BASH_REMATCH[2]}
fi

#Use a bucket password with more recent CB versions
if (($major>=5)); then
	pwstring=--bucket-password=password
else
	pwstring=--bucket-password=""
fi

#Enable collections
if (($major>=7)); then
	collections=--cluster-collectionRunType="many"
fi

if [ $use_ssl == true ]; then
	sslString=--cluster-useSSL=true
    echo "Running with SSL"
else
	sslString=--cluster-useSSL=false
fi

log_root=${WORKSPACE}/sdkd-cxx/logs-${BUILD_NUMBER}
mkdir -p ${log_root}
log_file=${log_root}/log.txt
stderr_file=${log_root}/stderr.txt

# remove any core dumps from previous runs
rm -f /tmp/core.sdkd_cxx.*

function show_backtraces () {
    for i in /tmp/core.sdkd_cxx.*
    do
       if [ -f $i ]
       then
         echo $i
         file $i
         gdb ${WORKSPACE}/sdkd-cxx/build/sdkd_cxx $i --batch -ex "thread apply all bt"
         #rm $i
       fi
    done
    if [ -f "${log_file}" ]
    then
    	tail -n 100 ${log_file}
    fi
    if [ -f "${stderr_file}" ]
    then
    	tail -n 100 ${stderr_file}
    fi
}

sdkd_executable="${WORKSPACE}/sdkd-cxx/build/sdkd_cxx"

function install_wrapper () {
sdkd_wrapper="${sdkd_executable}_wrapper.sh"
#if [ "x${SANITIZER}" = "xasan" ]
#then
#    EXTRA_ENV="export LD_PRELOAD=/usr/lib64/libasan.so.5"
#fi
cat <<EOF > "${sdkd_wrapper}"
echo -e "\n\n# Date=\"\$(date)\"" >> ${stderr_file}
echo -e "# PID=\$\$" >> ${stderr_file}
echo "# ${sdkd_executable} \$@" >> ${stderr_file}
${EXTRA_ENV}
cd ${WORKSPACE}/sdkd-cxx
exec ${sdkd_executable} \$@ 2>&1 | tee -a ${stderr_file}
EOF
chmod a+x "${sdkd_wrapper}"
cat "${sdkd_wrapper}"
sdkd_executable="${sdkd_wrapper}"
}

# install wrapper, that logs all output from SDKD (uncomment when debugging things)
#install_wrapper

ulimit -c unlimited
cat /proc/sys/kernel/core_pattern

echo -C share/rexec --rexec_path="${sdkd_executable}" --rexec_port=8050 --rexec_arg=-l 8050 --rexec_arg=-L ${log_file} --cluster-ssh-key=$SSH_KEY_PATH --cluster-use-hostname > sdkd.args

# -------------- Create cbdyncluster for use in runs -------------- #
cbdyncluster info
cbdyncluster ps -a
cluster_id=$(cbdyncluster allocate --num-nodes=4 --server-version=$cluster_version --os amzn2 --arch x86_64 --platform ec2)
ips=$(cbdyncluster ips $cluster_id)
ip_array=(${ips//,/ })

NODE1=${ip_array[0]}
NODE2=${ip_array[1]}
NODE3=${ip_array[2]}
NODE4=${ip_array[3]}


# ------------ Modify ini files with the node ips for use in runs ---------------- #
function subst() { eval echo -E "$2"; }

mapfile -c 1 -C subst <   $WORKSPACE/sdkdclient-ng/src/main/resources/INI/linux/watson-basic.ini  > watson-basic.ini 

mapfile -c 1 -C subst <   $WORKSPACE/sdkdclient-ng/src/main/resources/INI/linux/watson-ssl.ini  > watson-ssl.ini 

mapfile -c 1 -C subst <   $WORKSPACE/sdkdclient-ng/src/main/resources/INI/linux/watson-n1ql.ini   > watson-n1ql.ini 

mapfile -c 1 -C subst <   $WORKSPACE/sdkdclient-ng/src/main/resources/INI/linux/analytics-basic.ini   > analytics-basic.ini

mapfile -c 1 -C subst <   $WORKSPACE/sdkdclient-ng/src/main/resources/INI/linux/homog-basic.ini   > homog-basic.ini


#------------ Change ssh creds to ec2-user ----------------------#
sed -i 's/root/ec2-user/g' watson-basic.ini
sed -i 's/root/ec2-user/g' watson-ssl.ini
sed -i 's/root/ec2-user/g' watson-n1ql.ini
sed -i 's/root/ec2-user/g' analytics-basic.ini
sed -i 's/root/ec2-user/g' homog-basic.ini

#------------ Remove ssh password, use ssh key ----------------------#
sed -i 's/ssh-password=couchbase/;ssh-password=couchbase/g' watson-basic.ini
sed -i 's/ssh-password=couchbase/;ssh-password=couchbase/g' watson-ssl.ini
sed -i 's/ssh-password=couchbase/;ssh-password=couchbase/g' watson-n1ql.ini
sed -i 's/ssh-password=couchbase/;ssh-password=couchbase/g' analytics-basic.ini
sed -i 's/ssh-password=couchbase/;ssh-password=couchbase/g' homog-basic.ini

#------- TESTS ------- #
#Cbdyncluster warmup...

while true
do
  sleep 30
  curl -I http://${NODE1}:8091/ && break || continue
done

if [ x${run_regular} = "xtrue" ] ; then
	cbdyncluster refresh $cluster_id 2h

#HYBRID#
	./brun --install-skip=true --install-version=$cluster_version $sslString --rebound $rebound --ramp $ramp -I watson-basic.ini  --testsuite-variants="HYBRID" $pwstring -I sdkd.args --bucket-storageBackend $storage_backend -d all:debug

fi


if [ x${run_n1ql} = "xtrue" ] ; then
	cbdyncluster refresh $cluster_id 2h

	./brun  --install-skip=true --install-version=$cluster_version $sslString --rebound $rebound -I watson-n1ql.ini $pwstring --testsuite-variants="N1QL"  --testsuite-suite suites/N1QL.json  --n1ql-index-engine gsi --n1ql-index-type secondary --n1ql-scan-consistency=not_bounded --n1ql-preload true --n1ql-prepared=true  -d all:debug  -I sdkd.args 
	./brun  --install-skip=true --install-version=$cluster_version $sslString --rebound $rebound -I watson-n1ql.ini $pwstring --testsuite-variants="N1QLHYBRID"  --testsuite-suite suites/N1QL.json  --n1qlhybrid-n1ql-index-engine gsi  --n1qlhybrid-n1ql-index-type secondary  --n1qlhybrid-n1ql-scan-consistency=not_bounded  --n1qlhybrid-n1ql-preload true  --n1qlhybrid-n1ql-prepared=true  -d all:debug -I sdkd.args 
fi


if [ x${run_subdoc} = "xtrue" ] ; then
	cbdyncluster refresh $cluster_id 2h

	./brun  --install-skip=true --install-version=$cluster_version $sslString --rebound $rebound  -I watson-basic.ini $pwstring --testsuite-variants="SUBDOC" -d all:debug -I sdkd.args
fi 


if [ x${run_analytics} = "xtrue" ]&&(($major>=6)) ; then
cbdyncluster refresh $cluster_id 1h

 ./brun --install-skip=true $pwstring --install-version=$cluster_version $sslString --rebound $rebound -I analytics-basic.ini  --testsuite-variants="CBAS" --testsuite-suite suites/Analytics.json -d all:debug  -I sdkd.args
fi


if [ x${run_fts} = "xtrue" ] ; then
cbdyncluster refresh $cluster_id 1h

./brun --install-skip=true --install-version=$cluster_version $sslString --rebound $rebound -I watson-fts.ini  --testsuite-variants="FTS" $pwstring -d all:debug -I sdkd.args
fi 

#Put reruns with individual tests here
if [ x$run_reruns == "xtrue" ] ; then
cbdyncluster refresh $cluster_id 1h

./brun --install-skip=true $pwstring --install-version=$cluster_version $sslString --rebound $rebound -I watson-basic.ini  --testsuite-variants="SUBDOC" --testsuite-test="Rb2Out" -d all:debug  -I sdkd.args

fi

#Clean up
cbdyncluster rm $cluster_id

./report

show_backtraces