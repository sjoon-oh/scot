#!/bin/bash

project_home="scot"
workspace_home=`basename $(pwd)`

warning='\033[0;31m[WARNING]\033[0m '
normalc='\033[0;32m[MESSAGE]\033[0m '

args=$@
printf "${normalc}Arguments: ${args}\n"

#
# Setting proj home
if [[ ${workspace_home} != ${project_home} ]]; then
    printf "${warning}Currently in wrong directory: `pwd`\n"
    exit
fi

workspace_home=`pwd`

export HARTEBEEST_PARTICIPANTS=0,1,2
# export HARTEBEEST_EXC_IP_PORT=143.248.39.61:9999

export HARTEBEEST_EXC_IP_PORT=143.248.231.40:9999
export HARTEBEEST_CONF_PATH=${workspace_home}/config/qp.conf

export SCOT_QSIZE=3

if [ "${HARTEBEEST_NID}" == "0" ]; then
    printf "${normalc}Starting Memcached at blanc...\n"
    ssh oslab@143.248.231.40 "memcached -p 9999 &" &
else
    usleep 300
fi

numactl --membind 0 ./build/bin/scot-testbin

if [ "${HARTEBEEST_NID}" == "0" ]; then
    printf "${normalc}Killing Memcached at blanc...\n"
    sleep 5
    ssh oslab@143.248.231.40 "pkill -9 memcached"
fi

