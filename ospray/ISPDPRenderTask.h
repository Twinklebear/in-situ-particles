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
    InSituSpheres *isSpheres;

    struct ISPCDDSpheresBlock {
      // The actual grid domain assigned for this block
      box3f actualDomain;
      int firstOwner;
      int numOwners;
      int isMine;
      int blockID;

      // TODO: The special renderer will know how to deal with the
      // clipping of primary rays against the actual domain, the PKD
      // geometry shouldn't have to care about this at all!
      void *cpp_pkd;
      void *ispc_pkd;
    };
    // Because the layouts don't match anymore we need a seprate copy
    // of the blocks that we can share with ISPC
    std::vector<ISPCDDSpheresBlock> pkdBlocks;

    ISPDPRenderTask(Ref<Renderer> renderer, Ref<FrameBuffer> fb,
        size_t numTiles_x, size_t numTiles_y, uint32 channelFlags,
        InSituSpheres *isSpheres);

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

