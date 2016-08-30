#include "is_sim.h"
#include <unistd.h>
#include <vector>

struct vec4f {
  float x, y, z, attrib;
};

int rank, size;
std::vector<vec4f> particle;
const float speed = .01f;
const int NUM_PARTICLES = 10000;

void doTimeStep()
{
  static int timeStep = 0;

  if (timeStep == 0) {
    for (int i=0;i<NUM_PARTICLES;i++) {
      vec4f v;
      v.x = (rank+drand48())/size;
      v.y = drand48();
      v.z = drand48();
      v.attrib = drand48();
      particle.push_back(v);
    }
  }
  MPI_CALL(Barrier(MPI_COMM_WORLD));
  sleep(1);
  for (int i=0;i<particle.size();i++) {
    particle[i].x += speed * (1.f - 2.f*drand48());
    particle[i].y += speed * (1.f - 2.f*drand48());
    particle[i].z += speed * (1.f - 2.f*drand48());
  }
  MPI_CALL(Barrier(MPI_COMM_WORLD));

  printf("time step %i\n",timeStep++);
  ospIsTimeStep(particle.size(),&particle[0].x,4);
}

int main(int ac, char **av)
{
  MPI_CALL(Init(&ac,&av));
  MPI_CALL(Comm_rank(MPI_COMM_WORLD,&rank));
  MPI_CALL(Comm_size(MPI_COMM_WORLD,&size));
  ospIsInit(MPI_COMM_WORLD);

  while (1) {
    doTimeStep();
  }
}
