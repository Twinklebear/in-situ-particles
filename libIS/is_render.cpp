#include "is_render.h"

// socket stuff
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h> 
#include <string.h>
#include <assert.h>

// std
#include <iostream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <vector>

typedef int *socket_t;
#define INVALID_SOCKET -1

namespace ospray {
  using std::endl;
  using std::cout;
  int rank, size;
  
  MPI_Comm simComm = MPI_COMM_NULL;
  MPI_Comm ownComm = MPI_COMM_NULL;
  // Our name that identifies us to the simulation
  std::string my_port_name;

  int numSimRanks=-1;

  MPI_Comm establishConnection(const char *servName, int servPort)
  {
	assert(simComm == MPI_COMM_NULL);
    if (rank == 0)
      cout << "is_render: connecting to is_sim on " << servName << endl;
    MPI_CALL(Barrier(ownComm));
    
    int sockfd = -1;
    if (rank == 0) {
      sockfd = socket(AF_INET, SOCK_STREAM, 0);
      if (sockfd == INVALID_SOCKET) 
        throw std::runtime_error("cannot create socket");
      
      /*! perform DNS lookup */
      struct hostent* server = ::gethostbyname(servName);
      if (server == nullptr) THROW_RUNTIME_ERROR("server "+std::string(servName)+" not found");

      /*! perform connection */
      struct sockaddr_in serv_addr;
      memset((char*)&serv_addr, 0, sizeof(serv_addr));
      serv_addr.sin_family = AF_INET;
      serv_addr.sin_port = (unsigned short) htons(servPort);
      memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);
      
      if (::connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0)
        THROW_RUNTIME_ERROR("connection to is_sim socket failed");

      cout << "is_render: connection to is_sim socket established, now creating MPI intercomm" << endl;
    }      
    
    char mpiPortName[MPI_MAX_PORT_NAME];
	// TODO: Will this work? will we send back the same MPI port name each time or
	// should that be cached as well?
    MPI_CALL(Open_port(MPI_INFO_NULL,mpiPortName));
	my_port_name = mpiPortName;
    
    if (rank == 0) {
      cout << "is_render: mpi port opened at " << mpiPortName << endl;
      int portLen = strlen(mpiPortName);
      {
        ssize_t n = ::send(sockfd,&portLen,sizeof(portLen),0);
        assert(n == sizeof(portLen));
      }
      {
        ssize_t n = ::send(sockfd,mpiPortName,portLen,0);
        assert(n == portLen);
      }
    }

	cout << "is_render: mpi_accept on port " << mpiPortName << endl;
	MPI_CALL(Comm_accept(mpiPortName,MPI_INFO_NULL,0,ownComm,&simComm));

	if (rank == 0) 
		cout << "is_render: mpi comm established" << endl;

	MPI_CALL(Close_port(mpiPortName));
    return simComm;
  }
  // The simulation knows us by the port name we opened initially, so send that back
  // to indicate we want a new timestep
  MPI_Comm reuseConnection(const char *servName, int servPort){
	  assert(simComm != MPI_COMM_NULL && !my_port_name.empty());
	  if (rank == 0)
		  cout << "is_render: re-connecting to is_sim on " << servName << endl;

	  int sockfd = -1;
	  if (rank == 0) {
		  sockfd = socket(AF_INET, SOCK_STREAM, 0);
		  if (sockfd == INVALID_SOCKET) 
			  throw std::runtime_error("cannot create socket");

		  /*! perform DNS lookup */
		  struct hostent* server = ::gethostbyname(servName);
		  if (server == nullptr) THROW_RUNTIME_ERROR("server "+std::string(servName)+" not found");

		  /*! perform connection */
		  struct sockaddr_in serv_addr;
		  memset((char*)&serv_addr, 0, sizeof(serv_addr));
		  serv_addr.sin_family = AF_INET;
		  serv_addr.sin_port = (unsigned short) htons(servPort);
		  memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr, server->h_length);

		  if (::connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0)
			  THROW_RUNTIME_ERROR("connection to is_sim socket failed");

		  cout << "is_render: connection to is_sim socket established, now creating MPI intercomm" << endl;
	  }      

	  if (rank == 0) {
		  cout << "is_render: sending back our old port name " << my_port_name << std::endl;
		  int portLen = my_port_name.size();
		  {
			  ssize_t n = ::send(sockfd, &portLen, sizeof(portLen), 0);
			  assert(n == sizeof(portLen));
		  }
		  {
			  ssize_t n = ::send(sockfd, my_port_name.c_str(), portLen, 0);
			  assert(n == portLen);
		  }
	  }
	  MPI_CALL(Barrier(ownComm));
	  return simComm;
  }
  MPI_Comm connectSim(const char *servName, int servPort){
	  if (simComm == MPI_COMM_NULL){
		  return establishConnection(servName, servPort);
	  } else {
		  return reuseConnection(servName, servPort);
	  }
  }


  DomainGrid::DomainGrid(const vec3i &dims,
                         const box3f &domain,
                         const float ghosting)
    : dims(dims), worldBounds(domain)
  {
    size_t numBlocks = dims.x*dims.y*dims.z;
    block = new Block[numBlocks];
    int bID = 0;
    for (int iz=0;iz<dims.z;iz++)
      for (int iy=0;iy<dims.y;iy++)
        for (int ix=0;ix<dims.x;ix++, bID++) {
          Block &b = block[bID];
		  vec3f block_id = vec3f(ix, iy, iz);
          vec3f f_lo = vec3f(ix,iy,iz) / vec3f(dims);
          vec3f f_up = vec3f(ix+1,iy+1,iz+1) / vec3f(dims);
          b.actualDomain.lower
            = (vec3f(1.f)-f_lo)*domain.lower+f_lo*domain.upper;
          b.actualDomain.upper
            = (vec3f(1.f)-f_up)*domain.lower+f_up*domain.upper;
          b.ghostDomain.lower
            = b.actualDomain.lower - vec3f(ghosting);
          b.ghostDomain.upper
            = b.actualDomain.upper + vec3f(ghosting);

		  // Find out which faces of this block are not shared by
		  // any other block, and extend the region out so particles
		  // aren't clipped when rendering
		  if (ix == 0){
			  b.actualDomain.lower.x = b.ghostDomain.lower.x;
		  }
		  if (ix == dims.x - 1){
			  b.actualDomain.upper.x = b.ghostDomain.upper.x;
		  }
		  if (iy == 0){
			  b.actualDomain.lower.y = b.ghostDomain.lower.y;
		  }
		  if (iy == dims.y - 1){
			  b.actualDomain.upper.y = b.ghostDomain.upper.y;
		  }
		  if (iz == 0){
			  b.actualDomain.lower.z = b.ghostDomain.lower.z;
		  }
		  if (iz == dims.z - 1){
			  b.actualDomain.upper.z = b.ghostDomain.upper.z;
		  }

          if (numBlocks >= size) {
            b.firstOwner = bID % size;
            b.numOwners = 1;
          } else {
            b.firstOwner = (bID * size) / numBlocks;
            b.numOwners = ((bID+1) * size) / numBlocks - b.firstOwner;
          }

          if (rank >= b.firstOwner && rank < b.firstOwner+b.numOwners)
            myBlock.push_back(bID);
        }
  }
  DomainGrid::~DomainGrid(){
	  delete[] block;
  }

  DomainGrid *ospIsPullRequest(MPI_Comm comm, const char *servName, int servPort,
                               const vec3i &dims, const float ghostRegionWidth)
  {
	if (ownComm == MPI_COMM_NULL){
		std::cout << "libis: dup'ing communicator" << std::endl;
		MPI_CALL(Comm_dup(comm,&ownComm));
		MPI_CALL(Comm_rank(comm,&rank));
		MPI_CALL(Comm_size(comm,&size));
	}
    
    simComm = connectSim(servName, servPort);

    MPI_CALL(Comm_remote_size(simComm,&numSimRanks));
    if (rank == 0){
      cout << "num sim ranks " << numSimRanks
		  << ", using comm: " << std::hex << ownComm
		  << ", sim comm: " << simComm << std::dec
		  << endl;
	}
    MPI_CALL(Barrier(ownComm));
    
    box3f worldBounds;
	// Receive the world bounds from the simulation, this is also our indicator
	// that it is ready to send us a timestep
    MPI_CALL(Bcast(&worldBounds,6,MPI_FLOAT,0,simComm));
	// TODO WILL: We can send the stride after the world bounds if we want
	// to have dynamically sized stride based on what the simulation has
	if (rank == 0){
		std::cout << "worldBounds = " << worldBounds.lower << " to " << worldBounds.upper << "\n";
	}
    DomainGrid *grid = new DomainGrid(dims,worldBounds,ghostRegionWidth);
    size_t numParticles;
    for (int s=0;s<numSimRanks;s++) {
      int numMine = grid->myBlock.size();
      MPI_CALL(Send(&numMine,1,MPI_INT,s,0,simComm));
      for (int b=0;b<numMine;b++) {
        DomainGrid::Block &block = grid->getMine(b);
        MPI_CALL(Send(&block.ghostDomain,6,MPI_FLOAT,s,0,simComm));
      }
    }
    for (int b=0;b<grid->myBlock.size();b++) {
      size_t numParticles = 0;
      DomainGrid::Block &block = grid->getMine(b);
      int numFrom[numSimRanks];
      for (int s=0;s<numSimRanks;s++) {
        MPI_CALL(Recv(&numFrom[s],1,MPI_INT,s,0,simComm,MPI_STATUS_IGNORE));
        numParticles += numFrom[s];
      }
      block.particle.resize(numParticles * OSP_IS_STRIDE_IN_FLOATS);
      size_t sum = 0;
      for (int s=0;s<numSimRanks;s++) {
        MPI_CALL(Recv(&block.particle[sum * OSP_IS_STRIDE_IN_FLOATS],
					OSP_IS_STRIDE_IN_FLOATS * numFrom[s],
					MPI_FLOAT, s, 0, simComm, MPI_STATUS_IGNORE));
        sum += numFrom[s];
      }
    }

    MPI_CALL(Barrier(ownComm));
    return grid;
  }

} // ::ospray


std::ostream& operator<<(std::ostream &os, const ospray::DomainGrid::Block &b){
	os << "Block on actual domain " << b.actualDomain
		<< " (ghost " << b.ghostDomain << ")"
		<< " has " << b.particle.size() << " particles";
	return os;
}

