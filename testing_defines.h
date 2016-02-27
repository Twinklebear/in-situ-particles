#pragma once

// This file defines some various testing values
// and enable/disable defines for profiling and doing
// comparison images
// Sets the ghost region and AO occlusion ray length in
// ospray/InSituParticles.cpp and sg/common/Integrator.cpp
// AO of 0.0003 seems to work well for Josh's Uintah sim
//#define AO_OCCLUSION_DISTANCE 0.00025f
// Testing on the AO/ghost size for the single LAMMPS nanosphere with 0.003 radius
//#define AO_OCCLUSION_DISTANCE 0.0085f 
#define AO_OCCLUSION_DISTANCE 0.028f 
// Toggles if we use the renderer rank to color or not in ospray/InSituParticles.cpp
#define USE_RENDER_RANK_ATTRIB 0
// Effects clipping and ao bounds used in TraversePacket.ispc
#define CORRECT_AO 1
#define CORRECT_RAY_CLIPPING 1
// Toggles whether or not we do the border bound extension in is_render.cpp
#define CORRECT_BOUND_EXTENSION 1
// Whether we should poll just once or loop to get new timesteps
#define POLL_ONCE 0
// If we should reset the camera position upon getting the new world bounds
#define REPOSITION_CAMERA 1
// If we want to print the total number of particles (including duplicated ghost ones)
#define PRINT_FULL_PARTICLE_COUNT 1

// Various color map selections for picking color map for a batch render
#define PKD_COLOR_JET 1
#define PKD_COLOR_ICE_FIRE 2
#define PKD_COLOR_COOL_WARM 3
#define PKD_COLOR_BLUE_RED 4
#define PKD_COLOR_GRAYSCALE 5

//#define DEFAULT_COLOR_MAP PKD_COLOR_COOL_WARM

#define BATCH_RENDER_SAMPLES 32

