#!/bin/bash

project_home="scot"
workspace_home=`basename $(pwd)`

warning='\033[0;31m[WARNING]\033[0m '
normalc='\033[0;32m[MESSAGE]\033[0m '

args=$@
scriptbn=$(basename "$0" .sh)
printf "${normalc}${scriptbn} arguments: ${args}\n"

#
# Setting proj home
if [[ ${workspace_home} != ${project_home} ]]; then
    printf "${warning}Currently in wrong directory: `pwd`\n"
    exit
fi

workspace_home=`pwd`

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/build/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/hartebeest/build/lib

printf "${normalc} LD_LIBRARY_PATH=$LD_LIBRARY_PATH\n"

export HARTEBEEST_PARTICIPANTS=0,1,2
# export HARTEBEEST_EXC_IP_PORT=143.248.39.61:9999

export HARTEBEEST_EXC_IP_PORT=143.248.39.169:9999
export HARTEBEEST_CONF_PATH=${workspace_home}/config/qp.conf
export SCOT_CONF=${workspace_home}/config/scot.conf

printf "${normalc}Configuration file path(HB): `echo $HARTEBEEST_CONF_PATH`\n"
printf "${normalc}Configuration file path(SCOT): `echo $SCOT_CONF`\n"

export SCOT_QSIZE=3

if [ "${HARTEBEEST_NID}" == "0" ]; then
    printf "${normalc}Starting Memcached at blanc...\n"

    ssh oslab@143.248.39.169 "pkill -9 memcached"
    usleep 50

    ssh oslab@143.248.39.169 "memcached -p 9999 &" &
else
    usleep 300
fi

# Payload size, Key size, (Total: Payload + Key)
numactl --membind 0 ./build/bin/scot-st-testbin-c 24 8

if [ "${HARTEBEEST_NID}" == "0" ]; then
    printf "${normalc}Killing Memcached at blanc...\n"
    sleep 5
    ssh oslab@143.248.39.169 "pkill -9 memcached"
fi

mkdir -p report
mv *.csv ./report

cd report
# Rename all.

find . -type f -name 'wrkr*' | while read FILE ; do
    subst="s/wrkr/${scriptbn}-$(date +%F-%T)"-wrkr/
    new_name="$(echo ${FILE} |sed -e ${subst})" ;
    mv "${FILE}" "${new_name}" ;
done 

exit

# gdb env: env ~~
# set environment HARTEBEEST_NID=0
# set environment HARTEBEEST_PARTICIPANTS=0,1,2
# set environment HARTEBEEST_EXC_IP_PORT=143.248.39.169:9999
# set environment HARTEBEEST_CONF_PATH=/home/oslab/sjoon/workspace/sjoon-git/scot/config/qp.conf
# set environment SCOT_QSIZE=3


