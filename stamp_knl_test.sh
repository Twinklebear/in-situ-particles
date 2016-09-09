#!/bin/bash
#SBATCH -J insitu-knl-test
#SBATCH -t 00:08:00
#SBATCH -o test_log.txt
#SBATCH -N 3
#SBATCH -n 3
#SBATCH -p development

WORK=/work/03160/will/
LONESTAR=$WORK/lonestar
WORK_DIR=$LONESTAR/ospray/build/stamp_knl/
OUT_DIR=`pwd`
# Make sure Embree environment vars are setup
source $LONESTAR/embree-2.10.0.x86_64.linux/embree-vars.sh

MODULE_ISP=$LONESTAR/ospray/modules/module_in_situ_particles/
BENCH_SCRIPT=$MODULE_ISP/bench_insituspheres.chai

NUM_OSPRAY_WORKERS=$(($SLURM_NNODES - 1))
if [ $NUM_OSPRAY_WORKERS -eq 1 ]; then
  dp_grid=(1 1 1)
elif [ $NUM_OSPRAY_WORKERS -eq 2 ]; then
  dp_grid=(2 1 1)
elif [ $NUM_OSPRAY_WORKERS -eq 4 ]; then
  dp_grid=(2 2 1)
elif [ $NUM_OSPRAY_WORKERS -eq 8 ]; then
  dp_grid=(2 2 2)
elif [ $NUM_OSPRAY_WORKERS -eq 16 ]; then
  dp_grid=(4 2 2)
elif [ $NUM_OSPRAY_WORKERS -eq 32 ]; then
  dp_grid=(4 4 2)
elif [ $NUM_OSPRAY_WORKERS -eq 64 ]; then
  dp_grid=(4 4 4)
elif [ $NUM_OSPRAY_WORKERS -eq 128 ]; then
  dp_grid=(8 4 4)
elif [ $NUM_OSPRAY_WORKERS -eq 256 ]; then
  dp_grid=(8 8 4)
fi
echo "Setting OSPRAY_DATA_PARALLEL = ${dp_grid[*]}"
export OSPRAY_DATA_PARALLEL=${dp_grid[0]}x${dp_grid[1]}x${dp_grid[2]}

TEST_SIMULATION=$MODULE_ISP/libIS/build/test_sim
UINTAH_DIR=$WORK/maverick/uintah/
UINTAH_SIMULATION=$UINTAH_DIR/uintah-modified/build_knl/StandAlone/sus
UINTAH_RESTART_FILE=$UINTAH_DIR/restart-OFC-wasatch-50Mpps
UINTAH_RANKS=$(($SLURM_NNODES * 68))

tmpfile=$(mktemp /tmp/${SLURM_JOB_NAME}-${SLURM_NNODES}-${SLURM_JOB_ID}.XXXXXXX)

`scontrol show hostname $SLURM_NODELIST | tr '\n' ',' | sed s/,$//` > $tmpfile
export SIMULATION_HEAD_NODE=`scontrol show hostname $SLURM_NODELIST | head -n 1`

cd $WORK_DIR
set -x

echo "Spawning simulation"
export OSP_IS_PARTICLE_ATTRIB=p.u
mpirun -n $UINTAH_RANKS -ppn 68 -f $tmpfile $UINTAH_SIMULATION \
  -restart $UINTAH_RESTART_FILE | tee $OUT_DIR/${SLURM_JOB_NAME}-uintah-log.txt &

sleep 60

echo "Launching ospBenchmark"

export I_MPI_PIN_DOMAIN=node

mpirun -np $SLURM_NNODES -ppn 1 -f $tmpfile ./ospBenchmark --module pkd --osp:mpi \
  --script $BENCH_SCRIPT -w 1920 -h 1080 | tee $OUT_DIR/${SLURM_JOB_NAME}-ospray-log.txt

rm $tmpfile

scancel -n $SBATCH_JOBID

