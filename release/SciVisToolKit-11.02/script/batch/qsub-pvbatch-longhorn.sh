#!/bin/bash

# (C) 2010 SciberQuest Inc.
#
# pvserver-rc-batch.sh
#
# SGE batch script to start a pvserver group using it's reverse connection option.
#
# This script requires the follwing positional command line options:
#
# 1) the login node hostname
# 2) the port number used in the reverse tunnel.
# 3) the path to a ParaView install
#

#$ -V                                   # Inherit the submission environment
#$ -cwd                                 # Start job in submission dir
#$ -N pvbatch                           # Job name
#$ -j y                                 # Combine stderr and stdout into stdout
#$ -o $HOME/$JOB_NAME.out               # Name of the output file

module use -a ~/modulefiles
module load PV3-3.10.0-icc-ompi-R

USAGE="Error in usage: qsub-pvbatch.sh /path/to/paraview/install /path/to/driver.py /path/to/config.py"

if [[ -z $1 || -z $2 || -z $3 ]]
then
  echo $USAGE
  exit
fi
PV_PATH=$1
DRIVER=$2
CONFIG=$3
export SQ_DRIVER_CONFIG=$CONFIG

echo "PV_PATH="$PV_PATH
echo "DRIVER="$DRIVER
echo "CONFIG="$CONFIG

DISP_SCRIPT=/share/sge6.2/default/pe_scripts/tacc_xrun
IBRUN_PATH=/share/sge6.2/default/pe_scripts
$IBRUN_PATH/ibrun $DISP_SCRIPT $PV_PATH/bin/pvbatch $DRIVER
