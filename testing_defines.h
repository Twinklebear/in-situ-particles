// This file defines some various testing values
// and enable/disable defines for profiling and doing
// comparison images
// Sets the ghost region and AO occlusion ray length in
// ospray/InSituParticles.cpp and sg/common/Integrator.cpp
#define AO_OCCLUSION_DISTANCE 0.01f
// Toggles if we use the renderer rank to color or not in ospray/InSituParticles.cpp
#define USE_RENDER_RANK_ATTRIB 0
// Effects clipping and ao bounds used in TraversePacket.ispc
#define CORRECT_AO 1
#define CORRECT_RAY_CLIPPING 1
// Toggles whether or not we do the border bound extension in is_render.cpp
#define CORRECT_BOUND_EXTENSION 1

