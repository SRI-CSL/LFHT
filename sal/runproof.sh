#!/bin/csh

# Geoffrey M. Brown 
# Takes an SAL file name and runs the commented proof commands in that file.

if ($#argv == 0) then
  echo "usage $argv[0] <sal_file>"
  exit 0
endif

if (!(-e $1)) then
   echo "$1 not valid file"
   exit 0
endif

egrep "^%[ \t]*sal-inf" $1 | awk '{$1 = ""; print "echo " $0 "\n" $0}' |csh
