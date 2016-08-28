// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include <vector>
// ospray
#include "ospray/render/Renderer.h"
#include "OSPConfig.h"
// data distributed stuff
#include "ospray/volume/DataDistributedBlockedVolume.h"
#include "ospray/mpi/DistributedFrameBuffer.h"
// embree
#include "embree2/rtcore.h"
#include "embree2/rtcore_scene.h"
#include "InSituSpheres.h"

namespace ospray {
  /*! \brief Renderer which accumulates all volume samples as the
   *         frame renderers */
  struct ISPRenderer : public Renderer {
    ISPRenderer(int defaultNumSamples);
    ~ISPRenderer();

    /*! \brief create a material of given type */
    virtual ospray::Material *createMaterial(const char *type);
    std::string toString() const override;
    void commit() override;

    // TODO: This switches to return a float
    void renderFrame(FrameBuffer *fb, const uint32 fbChannelFlags) override;

  private:
    std::vector<void*> lightArray; // the 'IE's of the XXXLights
    Data *lightData;
    int defaultNumSamples;
  };
}

