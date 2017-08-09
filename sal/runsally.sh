#!/bin/bash

if [ $# -ne 1 ];then
    echo "Usage sally.sh FILE"
    exit 1
fi

INPUT=$1
sally --solver y2m5 --engine=pdkind --pdkind-add-backward $INPUT -v 3 --lsal-extensions --show-invariant --no-lets
