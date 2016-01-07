#include "is_render.h"
#include <assert.h>

// std
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>

using namespace ospray;
using std::endl;
using std::cout;

int rank, size;
float ghostRegionWidth = .1f;

int main(int ac, char **av)
{
  MPI_CALL(Init(&ac,&av));

  assert(ac == 3);
  char *servName = av[1];
  int servPort = atoi(av[2]);

  // TODO: We want a InSituSpheres geometry that will pull from the simulation
  // when calling commit to get the actual data. This will be easier to integrate
  // as a plugin than hacking the Qt viewer temporarily and is what we want in the long run.
  DomainGrid *dd = ospIsPullRequest(MPI_COMM_WORLD, servName, servPort, 
                                    vec3i(1), .01f);

  MPI_CALL(Comm_rank(MPI_COMM_WORLD,&rank));
  MPI_CALL(Comm_size(MPI_COMM_WORLD,&size));
  if (rank == 0)
    PRINT(dd->worldBounds);
  for (int r=0;r<size;r++) {
    MPI_CALL(Barrier(MPI_COMM_WORLD));
    if (r == rank) {
      cout << "rank " << r << ": " << endl;
      for (int mbID=0;mbID<dd->numMine();mbID++) {
        cout << " #" << mbID << endl;
        const DomainGrid::Block &b = dd->getMine(mbID);
        cout << "  lo " << b.actualDomain.lower << endl;
        cout << "  hi " << b.actualDomain.upper << endl;
        cout << "  #p " << b.particle.size() << endl;
      }
      cout << std::flush;
      fflush(0);
      usleep(200);
    }
    MPI_CALL(Barrier(MPI_COMM_WORLD));
  }
  MPI_CALL(Finalize());
}

