#pragma once

#include "OSPConfig.h"

#include "ospray/render/Renderer.h"
#include "ospray/fb/FrameBuffer.h"
#include "ospray/InSituSpheres.h"

namespace ospray {
  struct ISPDPRenderTask {
    mutable Ref<Renderer> renderer;
    mutable Ref<FrameBuffer> fb;
    size_t numTiles_x;
    size_t numTiles_y;
    uint32 channelFlags;
    const InSituSpheres *isSpheres;

    void operator()(int taskID) const;
  };
  // This is the data-distributed renderer's cache for block's tiles
  struct ISPCacheForTiles {
    std::vector<Tile*> blockTiles;
    std::mutex mutex;

    ISPCacheForTiles(size_t numBlocks);
    ~ISPCacheForTiles();
    Tile* allocTile() const;
    Tile* getTileForBlock(size_t blockID);
  };
}

