#!/bin/bash
#SBATCH -J insitu-knl-test
#SBATCH -t 00:05:00
#SBATCH -o test_log.txt
#SBATCH -N 3
#SBATCH -n 3
#SBATCH -p development

WORK=/work/03160/will/lonestar/
WORK_DIR=$WORK/ospray/build/stamp_knl/
OUT_DIR=`pwd`

MODULE_ISP=$WORK/ospray/modules/module_in_situ_particles/
TEST_SIMULATION=$MODULE_ISP/libIS/build/test_sim
BENCH_SCRIPT=$MODULE_ISP/bench_insituspheres.chai

tmpfile=$(mktemp /tmp/${SLURM_JOB_NAME}-${SLURM_NNODES}-${SLURM_JOB_ID}.XXXXXXX)

`scontrol show hostname $SLURM_NODELIST | tr '\n' ',' | sed s/,$//` > $tmpfile
export SIMULATION_HEAD_NODE=`scontrol show hostname $SLURM_NODELIST | head -n 1`

export I_MPI_PIN_DOMAIN=omp
export OMP_NUM_THREADS=272

set -x

echo "Spawning simulation"

# TODO: We can't use ibrun for both b/c we'll be doing one with one rank/core or so
# and ospray one rank/node
ibrun $TEST_SIMULATION &

sleep 30

echo "Launching ospBenchmark"

export OSPRAY_DATA_PARALLEL=2x1x1

cd $WORK_DIR
mpirun -np 3 -ppn 1 -f $tmpfile ./ospBenchmark --module pkd --osp:mpi \
  --script $BENCH_SCRIPT | tee $OUT_DIR/osplog.txt

pkill -u will

rm $tmpfile

