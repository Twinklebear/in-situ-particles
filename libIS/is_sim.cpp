#include "is_sim.h"

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
#include <thread>
#include <mutex>
#include <vector>


#include "is_common.h"
#include "ospray/common/OSPCommon.h"

typedef int *socket_t;
#define INVALID_SOCKET -1

namespace is_sim {

  using namespace ospray;

  using std::endl;
  using std::cout;

  /*! @{ communicator across all simulation processes */
  MPI_Comm simComm = MPI_COMM_NULL;
  int simSize = -1;
  int simRank = -1;
  /*! @} */
  
  std::mutex mutex;
  
  /*! vector of external mpi ports that requested data */
  std::vector<std::string> newPullRequest;
  
  /*! opens a thread on sim rank 0, and waits for incoming 'pull
    requests'.  once a external pull request comes in this function
    reads the external mpi port from the external pull request, and
    schedules this into the 'newPullRequests' queue */
  void acceptThreadFunc()
  {
    // socket at master - the one that's listening for new connections
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) throw std::runtime_error("cannot create socket");
  
    /* When the server completes, the server socket enters a time-wait
       state during which the local address and port used by the
       socket are believed to be in use by the OS. The wait state may
       last several minutes. This socket option allows bind() to reuse
       the port immediately. */
#ifdef SO_REUSEADDR
    { int flag = true; ::setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
                                    (const char*)&flag, sizeof(int)); }
#endif
  
    int port = 290374;

    /*! bind socket to port */
    struct sockaddr_in serv_addr;
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = (unsigned short) htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
  
    if (::bind(listenSocket, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
      throw std::runtime_error("binding to port failed");
  
    /*! listen to port, up to 5 pending connections */
    if (::listen(listenSocket,5) < 0)
      throw std::runtime_error("listening on socket failed");

    char hostName[10000];
    gethostname(hostName,10000);

    std::cout << "is_sim: now listening for connections on " << hostName << ":" << serv_addr.sin_port << endl;

    while (1) {
      /*! accept incoming connection */
      struct sockaddr_in addr;
      socklen_t len = sizeof(addr);
      int acceptedSocket = ::accept(listenSocket, (struct sockaddr *) &addr, &len);
      if (acceptedSocket == INVALID_SOCKET) 
        throw std::runtime_error("cannot accept connection");
      
      /*! we do not want SIGPIPE to be thrown */
#ifdef SO_NOSIGPIPE
      { 
        int flag = 1; 
        setsockopt(acceptedSocket, SOL_SOCKET, SO_NOSIGPIPE, 
                   (void*)&flag, sizeof(int)); 
      }
#endif
      
      cout << "is_sim: incoming pull request!" << endl;
      char portName[MPI_MAX_PORT_NAME];
      
      int portNameLen = -1;
      {
        ssize_t n = ::recv(acceptedSocket,&portNameLen,sizeof(portNameLen),MSG_NOSIGNAL);
        if (n != sizeof(portNameLen)) {
          cout << "#is_sim: error in reading incoming pull request" << endl;
          close(acceptedSocket);
          continue;
        }
      }
      
      {
        ssize_t n = ::recv(acceptedSocket,portName,portNameLen,MSG_NOSIGNAL);
        if (n != portNameLen) {
          cout << "#is_sim: error in reading incoming pull request" << endl;
          close(acceptedSocket);
          continue;
        }

        portName[portNameLen] = 0;
        close(acceptedSocket);
      }
      
      cout << "#is_sim: trying to establish new client connection to MPI port "
           << portName << endl;
      
      std::lock_guard<std::mutex> lock(mutex);
      newPullRequest.push_back(portName);
    } 
  }
  
  box3f computeBounds(const float *particle, size_t numParticles)
  {
    // TODO: parallelize
    box3f bounds = embree::empty;
    for (int i = 0; i < numParticles; ++i){
		size_t pid = i * OSP_IS_STRIDE_IN_FLOATS;
		bounds.extend(vec3f(particle[pid], particle[pid + 1], particle[pid + 2]));
	}
    return bounds;
  }

  bool inside(const box3f &box, const vec3f &vec)
  {
    if (vec.x < box.lower.x) return false;
    if (vec.y < box.lower.y) return false;
    if (vec.z < box.lower.z) return false;

    if (vec.x > box.upper.x) return false;
    if (vec.y > box.upper.y) return false;
    if (vec.z > box.upper.z) return false;

    return true;
  }
  void pullRequest(const std::string &portName,
                   size_t numParticles,
                   float *particle)
  {
    MPI_Comm remComm;
    if (simRank == 0)
      cout << "#is_sim: comm_connect on " << portName << endl;
    
    MPI_CALL(Comm_connect(const_cast<char*>(portName.c_str()),MPI_INFO_NULL,0,simComm,&remComm));
    MPI_CALL(Comm_set_errhandler(remComm,MPI_ERRORS_RETURN));
    int remSize;
    MPI_CALL(Comm_remote_size(remComm,&remSize));
    
    if (simRank == 0) 
      cout << "#is_sim: mpi comm from is_render established... have " 
           << remSize << " remote ranks" << endl;

    /*! this sends the (reduced) bounding box to the render
        processes, and then waits for a list of boxes from each rank
        that'll say which particles it wants */
     // compute bounds of all particles on this node ...
     box3f myBounds = computeBounds(particle,numParticles);
     box3f allBounds;
     // ... and across all sim ranks
     MPI_CALL(Allreduce(&myBounds.lower,&allBounds.lower,3,MPI_FLOAT,MPI_MIN,simComm));
     MPI_CALL(Allreduce(&myBounds.upper,&allBounds.upper,3,MPI_FLOAT,MPI_MAX,simComm));

     PRINT(myBounds);

     // now, send reduced bounds to remote group
     if (simRank == 0) {
	   // TODO WILL: Also send the stride of the data we're sending
	   // if we want more than 1 attrib
       MPI_CALL(Bcast(&allBounds,6,MPI_FLOAT,MPI_ROOT,remComm));
     }

	 std::vector<float> queried;
     for (int r=0;r<remSize;r++) {
	   queried.clear();
       int numFromR;
       MPI_CALL(Recv(&numFromR,1,MPI_INT,r,MPI_ANY_TAG,remComm,MPI_STATUS_IGNORE));
       for (int q=0;q<numFromR;q++) {
         box3f queryBox;
         MPI_CALL(Recv(&queryBox,6,MPI_FLOAT,r,
                       MPI_ANY_TAG,remComm,MPI_STATUS_IGNORE));
         
		 for (int i = 0; i < numParticles; ++i){
			 int pid = i * OSP_IS_STRIDE_IN_FLOATS;
			 if (inside(queryBox, vec3f(particle[pid], particle[pid + 1], particle[pid + 2]))){
				 for (int j = 0; j < OSP_IS_STRIDE_IN_FLOATS; ++j){
					 queried.push_back(particle[pid + j]);
				 }
			 }
		 }
         int num = queried.size() / OSP_IS_STRIDE_IN_FLOATS;
         MPI_CALL(Send(&num,1,MPI_INT,r,0,remComm));
         MPI_CALL(Send(&queried[0], queried.size(), MPI_FLOAT, r, 0, remComm));
       }
     }
    MPI_CALL(Comm_disconnect(&remComm));
  }

  extern "C" void ospIsInit(MPI_Comm comm)
  {
    if (simComm != MPI_COMM_NULL)
      throw std::runtime_error("ospIsInit called twice");
    
    MPI_CALL(Comm_dup(comm,&simComm));
    MPI_CALL(Comm_size(simComm,&simSize));
    MPI_CALL(Comm_rank(simComm,&simRank));

    MPI_CALL(Barrier(simComm));

    if (simRank == 0) {
      static std::thread acceptThread = std::thread(acceptThreadFunc);
    }
  }

  extern "C" void ospIsTimeStep(size_t numParticles, float *particle, int strideInFloats)
  {
	// TODO WILL: When sending attribs (stride > 3) where is this
	// assumption violated
    assert(strideInFloats == OSP_IS_STRIDE_IN_FLOATS);
    assert(simComm != MPI_COMM_NULL);
    
    std::lock_guard<std::mutex> lock(mutex);
    int numPullRequests = newPullRequest.size();
    MPI_CALL(Bcast(&numPullRequests,1,MPI_INT,0,simComm));

    for (int i=0;i<numPullRequests;i++)
      pullRequest(simRank == 0 ? newPullRequest[i] : "",
                  numParticles, particle);
    newPullRequest.clear();
  }  
}
