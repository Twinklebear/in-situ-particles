#pragma once

#include <mpi.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <ostream>

#include "is_common.h"

#include "ospray/common/OSPCommon.h"
#include "ospray/mpi/MPICommon.h"

namespace ospray {

  struct DomainGrid {
    struct Block {
      std::vector<float> particle;
      box3f actualDomain;
      box3f ghostDomain;
      int firstOwner;
      int numOwners;
      bool isMine;
    };

    DomainGrid(const vec3i &dims,
               const box3f &domain,
               const float ghosting);
    ~DomainGrid();
    size_t numMine() const { return myBlock.size(); }
    Block &getMine(int myBlockID) { return block[myBlock[myBlockID]]; }
    const Block &getMine(int myBlockID) const { return block[myBlock[myBlockID]]; }

    box3f worldBounds;
    Block *block;
    size_t numBlocks;
    std::vector<int> myBlock;
    vec3i dims;
  };


  /*! data distributed particles */
  DomainGrid *ospIsPullRequest(MPI_Comm comm, const char *servName, int servPort,
                               const vec3i &dims, const float ghostRegionWidth);
}

std::ostream& operator<<(std::ostream &os, const ospray::DomainGrid::Block &b);

