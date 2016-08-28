#pragma once

#include "OSPConfig.h"

#if EXP_DATA_PARALLEL
#include "ospray/render/Renderer.h"
#include "ospray/fb/FrameBuffer.h"
#include "ospray/volume/DataDistributedBlockedVolume.h"

namespace ospray {
  struct DPRenderTask {
    mutable Ref<Renderer> renderer;
    mutable Ref<FrameBuffer> fb;
    size_t numTiles_x;
    size_t numTiles_y;
    uint32 channelFlags;
    const DomainGrid *domainGrid;

    void operator()(int taskID) const;
  };
}

#endif


