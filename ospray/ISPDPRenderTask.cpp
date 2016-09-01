#include "ospray/common/Core.h"
#include "common/tasking/parallel_for.h"

#include "ISPDPRenderTask.h"
// ispc exports
#include "ISPRenderer_ispc.h"

namespace ospray {
  ISPDPRenderTask::ISPDPRenderTask(Ref<Renderer> renderer, Ref<FrameBuffer> fb,
      size_t numTiles_x, size_t numTiles_y, uint32 channelFlags,
      InSituSpheres *isSpheres)
    : renderer(renderer), fb(fb), numTiles_x(numTiles_x), numTiles_y(numTiles_y),
    channelFlags(channelFlags), isSpheres(isSpheres)
  {
    int nextBlockID = 0;
    for (auto &b : isSpheres->ddSpheres) {
      ISPCDDSpheresBlock ispcBlock;
      ispcBlock.actualDomain = b.actualDomain;
      ispcBlock.firstOwner = b.firstOwner;
      ispcBlock.numOwners = b.numOwners;
      ispcBlock.isMine = b.isMine;
      ispcBlock.blockID = nextBlockID++;
      if (b.isMine) {
        ispcBlock.cpp_pkd = static_cast<void*>(b.pkd.ptr);
        ispcBlock.ispc_pkd = b.pkd->getIE();
      } else {
        ispcBlock.cpp_pkd = NULL;
        ispcBlock.ispc_pkd = NULL;
      }
      pkdBlocks.push_back(ispcBlock);
    }
  }

  // This is basically a straight copy from the data-distributed DVR
  void ISPDPRenderTask::operator()(int taskID) const {
    const size_t tileID = taskID;
    const size_t tile_y = taskID / numTiles_x;
    const size_t tile_x = taskID - tile_y*numTiles_x;
    const vec2i tileId(tile_x, tile_y);
    const int32 accumID = fb->accumID(tileId);
    Tile bgTile(tileId, fb->size, accumID);

    const size_t numBlocks = isSpheres->ddSpheres.size();
    ISPCacheForTiles blockTileCache(numBlocks);
    bool *blockWasVisible = STACK_BUFFER(bool, numBlocks);

    for (size_t i = 0; i < numBlocks; i++) {
      blockWasVisible[i] = false;
    }

    const bool myTile =
      (taskID % core::getWorkerCount()) == core::getWorkerRank();

    const int numJobs = (TILE_SIZE*TILE_SIZE)/RENDERTILE_PIXELS_PER_JOB;

    parallel_for(numJobs, [&](int tid){
        ispc::ISPRendererDataDistrib_renderTile(renderer->getIE(),
            (ispc::Tile&)bgTile,
            &blockTileCache,
            numBlocks,
            const_cast<ISPCDDSpheresBlock*>(pkdBlocks.data()),
            blockWasVisible,
            tileID,
            ospray::core::getWorkerRank(),
            myTile,
            tid);
        });

    if (myTile) {
      // this is a tile owned by me - i'm responsible for writing
      // generaition #0, and telling the fb how many more tiles will
      // be coming in generation #1

      size_t totalBlocksInTile = 0;
      for (size_t blockID = 0; blockID < numBlocks; blockID++) {
        if (blockWasVisible[blockID]) {
          totalBlocksInTile++;
        }
      }

      /* I expect one additional tile for background tile.
       * plus how many blocks map to this tile, IN TOTAL,
       * INCLUDING blocks on other nodes
       */
      size_t nextGenTiles = totalBlocksInTile;

      // set background tile
      bgTile.generation = 0;
      bgTile.children = nextGenTiles;
      fb->setTile(bgTile);

      // we have no foreground data that could be rendered so we do NOT
      // send a foreground tile here.
      // all other tiles for gen #1 will be set below, no matter whether
      // it's mine or not
    }

    // now, send all block cache tiles that were generated on this
    // node back to master, too. for this, it doesn't matter if this
    // is our tile or not; it's the job of the owner of this tile to
    // tell the DFB how many tiles will arrive for the final thile
    // _across_all_clients_, but we only have to send ours (assuming
    // that all clients together send exactly as many as the owner
    // told the DFB to expect)
    for (size_t blockID = 0; blockID < numBlocks; blockID++) {
      Tile *tile = blockTileCache.blockTiles[blockID];
      if (tile == nullptr)
        continue;

      tile->region = bgTile.region;
      tile->fbSize = bgTile.fbSize;
      tile->rcp_fbSize = bgTile.rcp_fbSize;
      // TODO Was not added til later
      //tile->accumID = accumID;
      tile->generation = 1;
      tile->children   = 0;

      fb->setTile(*tile);
    }
  }

  ISPCacheForTiles::ISPCacheForTiles(size_t numBlocks)
    : blockTiles(numBlocks, nullptr)
  {}
  ISPCacheForTiles::~ISPCacheForTiles() {
    for (auto &t : blockTiles) {
      if (t != nullptr) {
        delete t;
      }
    }
  }
  Tile* ISPCacheForTiles::allocTile() const {
    const float infinity = std::numeric_limits<float>::infinity();
    Tile *tile = new Tile;
    for (int i = 0; i < TILE_SIZE*TILE_SIZE; i++) {
      tile->r[i] = 0.f;
      tile->g[i] = 0.f;
      tile->b[i] = 0.f;
      tile->a[i] = 0.f;
      tile->z[i] = infinity;
    }
    return tile;
  }
  Tile* ISPCacheForTiles::getTileForBlock(size_t blockID) {
    Tile *&tile = blockTiles[blockID];
    if (tile != nullptr) {
      return tile;
    }
    std::lock_guard<std::mutex> lock(mutex);
    if (tile == nullptr) {
      tile = allocTile();
    }
    return tile;
  }
  extern "C" Tile*
  ISPCacheForTiles_getTile(ISPCacheForTiles *cache, const int32_t blockID) {
    return cache->getTileForBlock(blockID);
  }
}

