#include <mpi.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <vector>


#include "is_common.h"

#include "ospray/common/OSPCommon.h"
#include "ospray/dev/mpi/MPICommon.h"

namespace ospray {

  struct DomainGrid {
    struct Block {
      std::vector<vec3f> particle;
      box3f actualDomain;
      box3f ghostDomain;
      int firstOwner;
      int numOwners;
    };

    DomainGrid(const vec3i &dims,
               const box3f &domain,
               const float ghosting);
    size_t numMine() const { return myBlock.size(); }
    Block &getMine(int myBlockID) { return block[myBlock[myBlockID]]; }
    const Block &getMine(int myBlockID) const { return block[myBlock[myBlockID]]; }

    box3f worldBounds;
    Block *block;
    std::vector<int> myBlock;
    vec3i dims;
  };


  /*! data distributed particles */
  DomainGrid *ospIsPullRequest(MPI_Comm comm, char *servName, int servPort,
                               const vec3i &dims, const float ghostRegionWidth);
}
