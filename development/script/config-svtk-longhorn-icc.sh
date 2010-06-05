#!/bin/bash

COMP=/opt/apps/intel/11.1/bin/intel64

cmake \
    -DCMAKE_C_COMPILER=$COMP/icc \
    -DCMAKE_CXX_COMPILER=$COMP/icpc \
    -DCMAKE_LINKER=$COMP/icpc \
    $*

