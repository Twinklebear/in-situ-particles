## ======================================================================== ##
## Copyright 2009-2014 Intel Corporation                                    ##
##                                                                          ##
## Licensed under the Apache License, Version 2.0 (the "License");          ##
## you may not use this file except in compliance with the License.         ##
## You may obtain a copy of the License at                                  ##
##                                                                          ##
##     http://www.apache.org/licenses/LICENSE-2.0                           ##
##                                                                          ##
## Unless required by applicable law or agreed to in writing, software      ##
## distributed under the License is distributed on an "AS IS" BASIS,        ##
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. ##
## See the License for the specific language governing permissions and      ##
## limitations under the License.                                           ##
## ======================================================================== ##

option(OSPRAY_MODULE_PARTIKD "Build Particle KD Tree + InSituSpheres Module." ON)

if (OSPRAY_MODULE_PARTIKD)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR})

  if (NOT OSPRAY_EXP_DATA_PARALLEL)
    message(ERROR "Data parallel OSPRay is required for InSituSpheres")
  endif()

  configure_mpi()
  # Why do we need to configure MPI again when ospray has already found it?
  # Is it b/c it's done in the ospray dir and not for the entire project?
  # Should maybe be done project wide so the modules don't need to re-find it
  include_directories(${MPI_CXX_INCLUDE_PATH})
  include_directories(${PROJECT_SOURCE_DIR}/apps/common/script/)

  ospray_create_library(ospray_module_pkd
    ospray/PKDGeometry.cpp
    ospray/PKDGeometry.ispc
    ospray/MinMaxBVH2.cpp
    ospray/MinMaxBVH2.ispc
    ospray/AlphaSpheres.cpp
    ospray/AlphaSpheres.ispc
    ospray/TraversePacket.ispc
    ospray/TraverseSPMD.ispc

    ospray/InSituSpheres.cpp
    ospray/ISPRenderer.cpp
    ospray/ISPRenderer.ispc
    ospray/ISPDPRenderTask.cpp
    ospray/PKDModule.cpp
    # Need PKD builders for InSituSpheres
    apps/PartiKD.cpp
    apps/ParticleModel.cpp
    # Add libIS stuff for InSituSpheres
    libIS/is_render.cpp
    libIS/is_sim.cpp

  LINK
    ospray
    ospray_xml
  )

endif()

