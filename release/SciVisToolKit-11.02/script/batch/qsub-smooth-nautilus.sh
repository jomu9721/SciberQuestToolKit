#!/bin/bash

# (C) 2010 SciberQuest Inc.

#$ -V                                   # Inherit the submission environment
#$ -cwd                                 # Start job in submission dir
#$ -j y                                 # Combine stderr and stdout into stdout
#!/bin/bash

if [ $# != 10 ]
then
  echo "Usage: start_pvserver.sh NCPUS WALLTIME ACCOUNT PORT"
  echo
  echo "  NCPUS        - number of processes in mutiple of 8."
  echo "  MEM          - ram per process. each 8 cpus comes with 32GB ram."
  echo "  WALLTIME     - wall time in HH:MM:SS format."
  echo "  ACCOUNT      - your tg account for billing purposes."
  echo "  QUEUE        - the queue to use."
  echo "  CONFIG_FILE  - xml run configuration"
  echo "  BOV_FILE     - bov file of dataset to process"
  echo "  OUTPUT_PATH  - path where results are stored"
  echo "  START_TIME   - start time"
  echo "  END_TIME=    - end time"
  echo
  exit
fi

NCPUS=$1
MEM=$2
WALLTIME=$3
ACCOUNT=$4
QUEUE=$5
export CONFIG_FILE=$6
export BOV_FILE=$7
export OUTPUT_PATH=$8
export START_TIME=$9
export END_TIME=${10}
MEM=`echo "$NCPUS*$MEM*1000" | bc`

export MPI_TYPE_MAX=1000000

export PV_PATH=/sw/analysis/paraview/3.12.0/sles11.1_intel11.1.038

export LD_LIBRARY_PATH=$PV_PATH/lib/paraview-3.12/:/sw/analysis/python/2.7.1/sles11.1_intel11.1/lib:/opt/sgi/mpt/mpt-2.04/lib:/opt/intel/Compiler/11.1/038/lib/intel64:/opt/intel/Compiler/11.1/038/mkl/lib/em64t:/opt/intel/Compiler/11.1/038/ipp/em64t/lib:/opt/intel/Compiler/11.1/038/tbb/em64t/cc4.1.0_libc2.4_kernel2.6.16.21/lib:/opt/torque/3.0.3/lib

export PATH=$PV_PATH/bin:/sw/analysis/python/2.7.1/sles11.1_intel11.1/bin:/opt/sgi/mpt/mpt-2.04/bin:/sw/analysis/git/1.7.6/sles11.1_intel11.1/bin:/nics/e/sw/tools/bin:/usr/local/hsi/bin:/usr/local/gold/bin:/opt/intel/Compiler/11.1/038/bin/intel64:/opt/moab/6.0.4/bin:/opt/torque/3.0.3/bin:/usr/local/bin:/usr/bin:/bin:/usr/lib/mit/bin:/usr/lib/mit/sbin

export SMB_EXE=`which SmoothBatch`

JID=`qsub -v MPI_TYPE_MAX,PV_PATH,PATH,LD_LIBRARY_PATH,PV_NCPUS,CONFIG_FILE,BOV_FILE,OUTPUT_PATH,START_TIME,END_TIME,SMB_EXE -N sm-batch -A $ACCOUNT -q $QUEUE -l ncpus=$NCPUS,mem=$MEM\MB,walltime=$WALLTIME /sw/analysis/paraview/3.12.0/sles11.1_intel11.1.038/bin/qsub-smooth-nautilus.qsub`
ERRNO=$?
if [ $ERRNO == 0 ] 
then
echo "Job submitted succesfully."
else
echo "ERROR $ERRNO: in job submission."
fi





























