#!/bin/bash
#SBATCH -J insitu-knl-test
#SBATCH -t 00:08:00
#SBATCH -N 2
#SBATCH -n 2
#SBATCH -p normal

WORK=/work/03160/will/
LONESTAR=$WORK/lonestar
WORK_DIR=$LONESTAR/ospray/build/stamp_knl/
OUT_DIR=`pwd`
# Make sure Embree environment vars are setup
source $LONESTAR/embree-2.10.0.x86_64.linux/embree-vars.sh

MODULE_ISP=$LONESTAR/ospray/modules/module_in_situ_particles/
BENCH_SCRIPT=$MODULE_ISP/bench_insituspheres.chai

NUM_UINTAH_NODES=2

UINTAH_DIR=$WORK/maverick/uintah/
UINTAH_SIMULATION=$UINTAH_DIR/uintah-modified/build_knl/StandAlone/sus
UINTAH_RESTART_FILE=$UINTAH_DIR/restart-OFC-wasatch-50Mpps
UINTAH_RANKS_PER_NODE=32
UINTAH_RANKS=$(($NUM_UINTAH_NODES * $UINTAH_RANKS_PER_NODE))

UINTAH_NODE_LIST=`scontrol show hostname $SLURM_NODELIST | tr '\n' ',' | sed s/,$//`

echo "Uintah using $UINTAH_NODE_LIST"

cd $WORK_DIR
set -x

echo "Spawning simulation"
export OSP_IS_PARTICLE_ATTRIB=p.u

mpiexec.hydra -n $UINTAH_RANKS -ppn $UINTAH_RANKS_PER_NODE -hosts $UINTAH_NODE_LIST $UINTAH_SIMULATION \
  -restart $UINTAH_RESTART_FILE

