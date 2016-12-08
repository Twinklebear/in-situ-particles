PROJECT(OSPRay)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
CMAKE_POLICY(SET CMP0003 NEW) # since 2.6
IF(POLICY CMP0015)
  CMAKE_POLICY(SET CMP0015 NEW) # since 2.8.1
ENDIF()
IF(POLICY CMP0042)
  CMAKE_POLICY(SET CMP0042 NEW) # since 3.0
ENDIF()
IF(POLICY CMP0054)
  CMAKE_POLICY(SET CMP0054 OLD) # since 3.1
ENDIF()

FIND_PACKAGE(MPI)
FIND_PACKAGE(Threads)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
FIND_PACKAGE(TBB)

SET(OSPRAY_DIR "$ENV{OSPRAY_DIR}" CACHE STRING "Install location of ospray")
INCLUDE_DIRECTORIES(${OSPRAY_DIR})
INCLUDE_DIRECTORIES(${OSPRAY_DIR}/ospray/embree-v2.7.1)
if(${STAMPEDE})
	message("Building for Stampede")
	LINK_DIRECTORIES(${OSPRAY_DIR}/bin_stamp/lib ${OSPRAY_DIR}/bin_stamp/lib64)
	INCLUDE_DIRECTORIES(${OSPRAY_DIR}/bin_stamp/include/ospray/)
  INCLUDE_DIRECTORIES(${OSPRAY_DIR}/bin_stamp/include/ospray/SDK/)
	INCLUDE_DIRECTORIES(${OSPRAY_DIR}/bin_stamp/include)
else()
	message("Building for Maverick/Generic")
	LINK_DIRECTORIES(${OSPRAY_DIR}/bin/lib ${OSPRAY_DIR}/bin/lib64)
	INCLUDE_DIRECTORIES(${OSPRAY_DIR}/bin/include/ospray/)
  INCLUDE_DIRECTORIES(${OSPRAY_DIR}/bin/include/ospray/SDK/)
	INCLUDE_DIRECTORIES(${OSPRAY_DIR}/bin/include)
endif()

INCLUDE_DIRECTORIES(${TBB_INCLUDE_DIRS} ${MPI_CXX_INCLUDE_PATH})

SET(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11")

ADD_LIBRARY(lib_is_sim
  is_sim.cpp
  )
TARGET_LINK_LIBRARIES(lib_is_sim
  ${MPI_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${TBB_LIBRARY}
  ${TBB_LIBRARY_MALLOC}
  )

ADD_EXECUTABLE(test_sim
  test_sim.cpp
  )
TARGET_LINK_LIBRARIES(test_sim
  lib_is_sim
  ospray
  )


ADD_LIBRARY(lib_is_render
  is_render.cpp
  )
TARGET_LINK_LIBRARIES(lib_is_render
  ${MPI_LIBRARIES}
  ospray
  )

ADD_EXECUTABLE(test_render
  test_render.cpp
  )
TARGET_LINK_LIBRARIES(test_render
  lib_is_render
  )
  
