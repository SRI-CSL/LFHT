#!/bin/bash

if [ $# -ne 2 ];then
    echo "Usage gen_mcmt_from_sal.sh FILE PROPERTY"
    exit 1
fi
    
INPUT=$1
PROPERTY=$2

filename=$(basename "$INPUT")
extension="${filename##*.}"
filename="${filename%.*}"

if [ $filename != $extension ]; then
    echo "The input file should have no extension: found extension .$extension"
    exit
fi

salenv-safe <<EOF | tail -n +12 | head -n -1 | sed s/'AND'/'and'/g | sed s/'if'/'ite'/g | sed s/'OR'/'or'/g > $INPUT.flat 
 (sal/set-pp-max-width! 10000)
 (sal/set-pp-max-depth! 10000)
 (sal/set-pp-max-num-lines! 100000)
 (sal/set-sal-pp-proc! sal-ast->lsal-doc) 
 (sal/set-trace-info-enabled! #f)
 (make-flat-assertion "$INPUT!$PROPERTY")
EOF
