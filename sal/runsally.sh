#!/bin/bash

if [ $# -ne 1 ];then
    echo "Usage sally.sh FILE"
    exit 1
fi

INPUT=$1

#PRINT_OPTS="--show-invariant --no-lets"
PRINT_OPTS=""
sally --solver y2m5 --engine=pdkind --pdkind-minimize-interpolants --pdkind-minimize-frames --pdkind-induction-max=1  $INPUT -v 1 --lsal-extensions ${PRINT_OPTS}
