#!/bin/bash

project_home="scot"
workspace_home=`basename $(pwd)`

warning='\033[0;31m[WARNING]\033[0m '
normalc='\033[0;32m[MESSAGE]\033[0m '

# args=$@
scriptbn=$(basename "$0" .sh)
args=$(getopt -o k:p:t:h --long keysz:,paysz:,threads:,help -- "$@")

print_help() {
	echo "Usage : $0 -k <key_sz> -p <paysz> -t <threads> "
}

if [[ $? -gt 0 ]]; then
    print_help
    exit
fi

eval set -- ${args}
while true; do
    case $1 in
        -k | --keysz)
            shift; argval=$1; # Move to the argument
                # Do something here
                keysz=${argval}
            shift; continue
            ;;
        -p | --paysz)
            shift; argval=$1; # Move to the argument
                # Do something here
                paysz=${argval}
            shift; continue
            ;;
        -t | --threads)
            shift; argval=$1; # Move to the argument
                # Do something here
                threads=${argval}
            shift; continue
            ;;
        -h | --help)
                print_help
            shift; continue
            ;;
        --) 
            shift; break
            ;;
        --*)
                printf "Invalid option: $1"
                print_help;
            ;;
    esac
done

printf "${normalc} Key size: ${keysz}, Payload size: ${paysz}, Threads: ${threads}\n"

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
export SCOT_QPOOLSZ=4
qpoolsz=${SCOT_QPOOLSZ}

if [ "${HARTEBEEST_NID}" == "0" ]; then
    printf "${normalc}Starting Memcached at blanc...\n"

    ssh oslab@143.248.39.169 "pkill -9 memcached"
    usleep 50

    ssh oslab@143.248.39.169 "memcached -p 9999 &" &
else
    sleep 1
fi

# Payload size, Key size, (Total: Payload + Key), Thread number
numactl --membind 0 ./build/bin/scot-mt-testbin ${paysz} ${keysz} ${threads}

if [ "${HARTEBEEST_NID}" == "0" ]; then
    printf "${normalc}Killing Memcached at blanc...\n"
    sleep 5
    ssh oslab@143.248.39.169 "pkill -9 memcached"
fi

mkdir -p report
mv *.csv ./report

cd report
# Rename all.

fname_date=$(date +%F-%T)
aggr_name=${scriptbn}-${fname_date}-wrkr-aggregate.csv

rm ${aggr_name}

find . -type f -name 'wrkr*' | while read FILE ; do
    subst="s/wrkr/${scriptbn}-${fname_date}"-wrkr/
    new_name="$(echo ${FILE} |sed -e ${subst})" ;

    cat ${FILE} >> ${aggr_name}

    mv "${FILE}" "${new_name}" ;
done

python3 report.py

exit

# gdb env: env ~~
# set environment HARTEBEEST_NID=0
# set environment HARTEBEEST_PARTICIPANTS=0,1,2
# set environment HARTEBEEST_EXC_IP_PORT=143.248.39.169:9999
# set environment HARTEBEEST_CONF_PATH=/home/oslab/sjoon/workspace/sjoon-git/scot/config/qp.conf
# set environment SCOT_QSIZE=3


