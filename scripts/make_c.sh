#!/bin/bash
#
# github.com/sjoon-oh/scot
# Author: Sukjoon Oh, sjoon@kaist.ac.kr

project_home="scot"
workspace_home=`basename $(pwd)`

warning='\033[0;31m[WARNING]\033[0m '
normalc='\033[0;32m[MESSAGE]\033[0m '

#
# Setting proj home
if [[ ${workspace_home} != ${project_home} ]]; then
    printf "${warning}Currently in wrong directory: `pwd`\n"
    exit
fi

printf "${normalc}$(basename "$0") started.\n"

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/build/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/hartebeest/build/lib

gcc -o build/bin/scot-st-testbin-c -g \
    src/tests/scot-st-test-c.c -libverbs -lmemcached \
    -L hartebeest/build/lib/ -lhartebeest \
    -L build/lib/ -lscot

printf "${normalc}$(basename "$0") finished.\n"

