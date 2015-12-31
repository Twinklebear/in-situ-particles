#include <mpi.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

/*! helper macro that checks the return value of all MPI_xxx(...)
    calls via MPI_CALL(xxx(...)).  */
#define MPI_CALL(a) { int rc = MPI_##a; if (rc != MPI_SUCCESS) throw std::runtime_error("MPI call returned error"); }


/* debug printing macros */
#define STRING(x) #x
#define TOSTRING(x) STRING(x)
#define PING std::cout << __FILE__ << " (" << __LINE__ << "): " << __FUNCTION__ << std::endl
#define PRINT(x) std::cout << STRING(x) << " = " << (x) << std::endl
#define PRINT2(x,y) std::cout << STRING(x) << " = " << (x) << ", " << STRING(y) << " = " << (y) << std::endl
#define PRINT3(x,y,z) std::cout << STRING(x) << " = " << (x) << ", " << STRING(y) << " = " << (y) << ", " << STRING(z) << " = " << (z) << std::endl
#define PRINT4(x,y,z,w) std::cout << STRING(x) << " = " << (x) << ", " << STRING(y) << " = " << (y) << ", " << STRING(z) << " = " << (z) << ", " << STRING(w) << " = " << (w) << std::endl

#define THROW_RUNTIME_ERROR(str) \
  throw std::runtime_error(std::string(__FILE__) + " (" + std::to_string((long long)__LINE__) + "): " + std::string(str));

#if defined(__MIC__)
#define FATAL(x) { std::cerr << "Error in " << __FUNCTION__ << " : " << x << std::endl << std::flush; exit(1); }
#define WARNING(x) std::cerr << "Warning:" << std::string(x) << std::endl
#else
#define FATAL(x) THROW_RUNTIME_ERROR(x)
#define WARNING(x) std::cerr << "Warning:" << std::string(x) << std::endl
#endif

#define NOT_IMPLEMENTED FATAL(std::string(__FUNCTION__) + " not implemented")



extern "C" void ospIsInit(MPI_Comm comm);
extern "C" void ospIsTimeStep(size_t numParticles, float *particle, int strideInFloats);
