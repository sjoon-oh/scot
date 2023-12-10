#!/bin/bash

project_home="scot"
workspace_home=`basename $(pwd)`

warning='\033[0;31m[WARNING]\033[0m '
normalc='\033[0;32m[MESSAGE]\033[0m '

#
# Setting proj home
if [[ ${workspace_home} != ${project_home} ]]; then
    printf "Currently in wrong directory: `pwd`\n"
    exit
fi

workspace_home=`pwd`
printf "Project dir: ${workspace_home}\n"

cd ..
workspace_par=`pwd`

printf "Sending project to all:${workspace_par}\n"
sftp -q oslab@node1:${workspace_par} <<< $"put -r ${workspace_home}"
sftp -q oslab@node2:${workspace_par} <<< $"put -r ${workspace_home}"
