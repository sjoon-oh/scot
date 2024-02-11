#!/bin/bash
#
# github.com/sjoon-oh/scot
# Author: Sukjoon Oh, sjoon@kaist.ac.kr

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

workspace_home="$(pwd)"

printf "${normalc}$(basename "$0") started.\n"

export LIBRARY_PATH=$(pwd)/build/lib:$LIBRARY_PATH
export LIBRARY_PATH=$(pwd)/hartebeest/build/lib:$LIBRARY_PATH

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/build/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/hartebeest/build/lib


exp_home="$(pwd)/experiments"
redis_home="${exp_home}/redis-7.0.5"

#
# Option 0: clean source directory, and the binary.
if [[ "${args}" == *"clean"* ]]; then
    printf "${normalc}0. -- Redis source files clean. -- \n"

    if [ -d "${redis_home}" ]; then
        printf "${normalc}Removing existing workspace.\n"
        rm -rf ${redis_home}
    fi

    if [ -d "${workspace_home}/build/bin/redis-scot" ]; then
        printf "${normalc}Removing existing Redis binary.\n"
        rm -f ${workspace_home}/build/bin/redis-scot
    fi

   exit
fi

#
# Option 1: reset source directory
if [[ "${args}" == *"reset"* ]]; then
    printf "${normalc}1. -- Redis source files reset. -- \n"

    if [ -d "${redis_home}" ]; then
        printf "${normalc}Removing existing workspace.\n"
        rm -rf ${redis_home}
    fi

    cd ${exp_home}
    tar -xf redis-7.0.5.tar.gz
    
    cd redis-7.0.5
    git init

    printf "[user]\n\tname = local\n\temail = local@oslab.kaist.ac.kr" >> .git/config
    git add ./* --verbose

    git commit -m "redis-7.0.5 untocuhed: initial source" --verbose
fi

cd ${redis_home}

#
# Option 2: Make patch to the source directory
if [[ "${args}" == *"checkpoint"* ]]; then
    printf "${normalc}2. -- Generating modified patch. -- \n"

    if [ ! -d "${redis_home}" ]; then
        printf "${warning}No Redis source directory found.\n"
        exit
    fi
    
    git diff > ../redis-7.0.5.patch
    cd ..

    ls | grep patch
fi

#
# Option 3: Apply the patch.
if [[ "${args}" == *"patch"* ]]; then
    printf "${normalc}3. -- Applying the patch to the source. -- \n"
    printf "${normalc}Current directory: ${redis_home} \n"

    if [ ! -d "${redis_home}" ]; then
        printf "${warning}No Redis source directory found.\n"
        exit
    fi
    if [ ! -f "../redis-7.0.5.patch" ]; then
        printf "${warning}No Redis patch file found.\n"
        exit
    fi
    
    # patch -p0 -d ${redis_home} -s < ../redis-7.0.5.patch
    git apply --reject --whitespace=fix ../redis-7.0.5.patch
    printf "${normalc}Patched\n."
fi

#
# Option 4: Build Redis
if [[ "${args}" == *"make"* ]]; then
    printf "${normalc}4. -- Building REDIS -- \n"
    printf "${normalc}Current directory: ${redis_home} \n"

        if [ ! -d "${redis_home}" ]; then
        printf "${warning}No Redis source directory found.\n"
        exit
    fi

    make -j 16
    cp src/redis-server ${workspace_home}/build/bin/redis-scot
fi

#
# Option 5: Run the Redis binary
if [[ "${args}" == *"run"* ]]; then
    printf "${normalc}5. -- Executing REDIS -- \n"
    printf "${normalc}Current directory: ${redis_home} \n"

    if [ ! -f "${workspace_home}/build/bin/redis-scot" ]; then
        printf "${warning}No Redis binary file found. Did you compile?\n"
        exit
    fi

    if [ -f "${workspace_home}/dump.rdb" ]; then
        printf "${normalc}Redis persistence removed.\n"
    fi

    export HARTEBEEST_PARTICIPANTS=0,1,2
    export HARTEBEEST_EXC_IP_PORT=143.248.39.169:9999
    export HARTEBEEST_CONF_PATH=${workspace_home}/config/qp.conf
    
    export SCOT_CONF=${workspace_home}/config/scot.conf
    export SCOT_QSIZE=3

    cd ${workspace_home}


    if [ "${HARTEBEEST_NID}" == "0" ]; then
        printf "${normalc}Starting Memcached at blanc...\n"

        ssh oslab@143.248.39.169 "pkill -9 memcached"
        usleep 50

        ssh oslab@143.248.39.169 "memcached -p 9999 &" &
    else
        usleep 300
    fi

    ${workspace_home}/build/bin/redis-scot \
        --port 6379 \
        --protected-mode no \
        --io-threads-do-reads yes \
        --save "" --appendonly no
    
    if [ "${HARTEBEEST_NID}" == "0" ]; then
        printf "${normalc}Killing Memcached at blanc...\n"
        sleep 5
        ssh oslab@143.248.39.169 "pkill -9 memcached"
    fi

    cd ${workspace_home}

    mkdir -p report
    mv *.csv ./report

    cd report
    # Rename all.

    find . -type f -name 'wrkr*' | while read FILE ; do
        subst="s/wrkr/${scriptbn}-$(date +%F-%T)"-wrkr/
        new_name="$(echo ${FILE} |sed -e ${subst})" ;
        mv "${FILE}" "${new_name}" ;
    done 

fi

printf "${normalc}$(basename "$0") finished.\n"

