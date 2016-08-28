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

// ospray
#include "ISPRenderer.h"
#include "ISPDPRenderTask.h"
#include "ospray/render/simpleAO/SimpleAOMaterial.h"
#include "ospray/camera/Camera.h"
#include "ospray/texture/Texture2D.h"
// ispc exports
#include "ISPRenderer_ispc.h"

#include "ospray/mpi/DistributedFrameBuffer.h"
#include "ospray/render/LoadBalancer.h"
#include "ospray/common/Core.h"

namespace ospray {
  //! \brief Constructor
  ISPRenderer::ISPRenderer(int defaultNumSamples)
    : defaultNumSamples(defaultNumSamples)
  {
    ispcEquivalent = ispc::ISPRenderer_create(this,NULL,NULL);
  }
  ISPRenderer::~ISPRenderer() {
  }

  /*! \brief create a material of given type */
  ospray::Material *ISPRenderer::createMaterial(const char * /*type*/)
  {
    return new simpleao::Material;
  }

  /*! \brief common function to help printf-debugging */
  std::string ISPRenderer::toString() const
  {
    return "ospray::render::ISPRenderer";
  }

  /*! \brief commit the object's outstanding changes
   *         (such as changed parameters etc) */
  void ISPRenderer::commit()
  {
    Renderer::commit();

    int   numSamples = getParam1i("aoSamples", defaultNumSamples);
    float rayLength  = getParam1f("aoOcclusionDistance", 1e20f);
    ispc::ISPRenderer_set(getIE(), numSamples, rayLength);
  }

  void ISPRenderer::renderFrame(FrameBuffer *fb, const uint32 fbChannelFlags) {
    // check if we're even in mpi parallel mode (can't do
    // data-parallel otherwise)
    if (!ospray::core::isMpiParallel()){
      throw std::runtime_error("ISPRenderer only makes sense in MPI parallel mode!");
    }
    const int workerRank = core::getWorkerRank();
    assert(workerRank >= 0);

    const InSituSpheres *isSpheres = nullptr;
    for (size_t i = 0; i < model->geometry.size(); ++i) {
      const InSituSpheres *is = dynamic_cast<const InSituSpheres*>(model->geometry[i].ptr);
      if (is) {
        if (isSpheres) {
          throw std::runtime_error("Only one InSituSpheres distributed geometry is supported!");
        }
        isSpheres = is;
      }
    }
    if (!isSpheres) {
      throw std::runtime_error("No InSituSpheres geometry but using InSituSphere rendering!?");
    }

    // We want to mirror what the data parallel volume renderer is doing here
    // but since I've hacked the MPILoadBalancer::Slave to do data-parallel we
    // have a bit less work to be done here.
    // switch (distributed) frame buffer into compositing mode
    DistributedFrameBuffer *dfb = dynamic_cast<DistributedFrameBuffer *>(fb);
    if (!dfb){
      throw std::runtime_error("OSPRay data parallel rendering error. "
          "this is a data-parallel scene, but we're "
          "not using the distributed frame buffer!?");
    }
    dfb->setFrameMode(DistributedFrameBuffer::ALPHA_BLENDING);

    Renderer::beginFrame(fb);

    dfb->startNewFrame();

    // create the render task
    ISPDPRenderTask renderTask;
    renderTask.fb = fb;
    renderTask.renderer = this;
    renderTask.numTiles_x = divRoundUp(dfb->size.x,TILE_SIZE);
    renderTask.numTiles_y = divRoundUp(dfb->size.y,TILE_SIZE);
    renderTask.channelFlags = fbChannelFlags;
    renderTask.isSpheres = isSpheres;

    const size_t NTASKS = renderTask.numTiles_x * renderTask.numTiles_y;
    parallel_for(NTASKS, renderTask);

    dfb->waitUntilFinished();
    Renderer::endFrame(NULL, fbChannelFlags);

    // TODO: This starts returning a float in ospray 1.0
    //return 0.f;
  }

  // OSP_REGISTER_RENDERER(ISPRenderer, ao);

  /*! \note Reintroduce aoX renderers for compatibility, they should be
    depricated!*/

#define OSP_REGISTER_AO_RENDERER(external_name, nSamples)               \
  extern "C" OSPRAY_INTERFACE                                           \
  Renderer *ospray_create_renderer__##external_name()                   \
  {                                                                     \
    ISPRenderer *renderer = new ISPRenderer(nSamples);                        \
    return renderer;                                                    \
  }

  OSP_REGISTER_AO_RENDERER(isp,   4 );
  OSP_REGISTER_AO_RENDERER(isp1,  1 );
  OSP_REGISTER_AO_RENDERER(isp2,  2 );
  OSP_REGISTER_AO_RENDERER(isp4,  4 );
  OSP_REGISTER_AO_RENDERER(isp8,  8 );
  OSP_REGISTER_AO_RENDERER(isp16, 16);

} // ::ospray


