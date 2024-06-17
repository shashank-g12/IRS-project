#!/bin/bash

if [[ "$4" == "1" ]];  then
    ./bpe $1 20000
fi
./invidx_cons $1 $2 $3 $4
