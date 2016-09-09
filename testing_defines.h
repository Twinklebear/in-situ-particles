#pragma once

// This file defines some various testing values
// and enable/disable defines for profiling and doing
// comparison images
// Sets the ghost region and AO occlusion ray length in
// ospray/InSituParticles.cpp and sg/common/Integrator.cpp
// AO of 0.0003 seems to work well for Josh's Uintah sim
// Testing on the AO/ghost size for the single LAMMPS nanosphere with 0.003 radius
#define AO_OCCLUSION_DISTANCE 0.0003f
// For the LAMMPS scaling tests I used 0.028
//#define AO_OCCLUSION_DISTANCE 0.028f
// Toggles if we use the renderer rank to color or not in ospray/InSituParticles.cpp
#define USE_RENDER_RANK_ATTRIB 0
// Effects clipping and ao bounds used in TraversePacket.ispc
#define CORRECT_AO 1
#define CORRECT_RAY_CLIPPING 1
// Toggles whether or not we do the border bound extension in is_render.cpp
#define CORRECT_BOUND_EXTENSION 1
// If we want to print the total number of particles (including duplicated ghost ones)
#define PRINT_FULL_PARTICLE_COUNT 0

// Toggle to enable/disable the simulation using the insitu library
#define OSP_IS_ENABLED 1

