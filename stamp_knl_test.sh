#!/bin/bash
#SBATCH -J insitu-knl-test
#SBATCH -t 00:05:00
#SBATCH -o test_log.txt
# Nodes should be #osp workers + 1 (osp master) + 3 uintah
#SBATCH -N 8
#SBATCH -n 8
#SBATCH -p normal

WORK=/work/03160/will/
LONESTAR=$WORK/lonestar
WORK_DIR=$LONESTAR/ospray/build/stamp_knl/
OUT_DIR=`pwd`
# Make sure Embree environment vars are setup
source $LONESTAR/embree-2.10.0.x86_64.linux/embree-vars.sh

MODULE_ISP=$LONESTAR/ospray/modules/module_in_situ_particles/
BENCH_SCRIPT=$MODULE_ISP/bench_insituspheres.chai

NUM_UINTAH_NODES=3
NUM_OSPRAY_NODES=$(($SLURM_NNODES - $NUM_UINTAH_NODES))
NUM_OSPRAY_WORKERS=$(($NUM_OSPRAY_NODES - 1))
echo "Have $NUM_OSPRAY_WORKERS ospray workers"
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
UINTAH_RANKS_PER_NODE=34
UINTAH_RANKS=$(($SLURM_NNODES * $UINTAH_RANKS_PER_NODE))

start_osp_workers=$(($NUM_UINTAH_NODES + 1))
OSPRAY_NODE_LIST=`scontrol show hostname $SLURM_NODELIST | tail -n +${start_osp_workers} | tr '\n' ',' | sed s/,$//`
UINTAH_NODE_LIST=`scontrol show hostname $SLURM_NODELIST | head -n ${NUM_UINTAH_NODES} | tr '\n' ',' | sed s/,$//`

OSPRAY_HOST_FILE=$(mktemp /tmp/${SLURM_JOB_NAME}-${SLURM_NNODES}-${SLURM_JOB_ID}-osp.XXXXXXX)
UINTAH_HOST_FILE=$(mktemp /tmp/${SLURM_JOB_NAME}-${SLURM_NNODES}-${SLURM_JOB_ID}-uin.XXXXXXX)

echo "Uintah using $UINTAH_NODE_LIST"
echo "OSPRay using $OSPRAY_NODE_LIST"

echo "$OSPRAY_NODE_LIST" > $OSPRAY_HOST_FILE
echo "$UINTAH_NODE_LIST" > $UINTAH_HOST_FILE

export SIMULATION_HEAD_NODE=`scontrol show hostname $SLURM_NODELIST | head -n 1`

cd $WORK_DIR
set -x

echo "Spawning simulation"
export OSP_IS_PARTICLE_ATTRIB=p.u

# TODO: This won't work b/c mpirun and SLURM are all fucked up on Stampede 1.5 currently
mpirun -n $UINTAH_RANKS -ppn $UINTAH_RANKS_PER_NODE -f $UINTAH_HOST_FILE $UINTAH_SIMULATION \
  -restart $UINTAH_RESTART_FILE | tee $OUT_DIR/${SLURM_JOB_NAME}-uintah-log.txt &

sleep 60

echo "Launching ospBenchmark"

export I_MPI_PIN_DOMAIN=node

mpirun -n $NUM_OSPRAY_NODES -ppn 1 -f $OSPRAY_HOST_FILE ./ospBenchmark --module pkd --osp:mpi \
  --script $BENCH_SCRIPT -w 1920 -h 1080 | tee $OUT_DIR/${SLURM_JOB_NAME}-ospray-log.txt

scancel -n $SBATCH_JOBID

