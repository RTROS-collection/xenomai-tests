#!/bin/bash

if [[ $# -eq 0 ]] ; then
    echo 'Please enter name of real-time compiled executable.'
    exit 1
fi

sudo LD_LIBRARY_PATH=/usr/xenomai/lib ./$1