#!/usr/bin/env bash

ITERATIONS=8

. ./shrc

runspec --config dust-sri-glibc.cfg   --noreportable int ^gcc --iterations ${ITERATIONS}
runspec --config dust-glibc.cfg   --noreportable int ^gcc --iterations ${ITERATIONS}

