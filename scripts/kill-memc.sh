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
else
    usleep 300
fi
