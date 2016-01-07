// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#undef NDEBUG

// ospray
#include "InSituSpheres.h"
#include "ospray/common/Data.h"
#include "ospray/common/Model.h"
#include "libIS/is_render.h"
// ispc-generated files
#include "InSituSpheres_ispc.h"

namespace ospray {

  InSituSpheres::InSituSpheres()
  {
    this->ispcEquivalent = ispc::InSituSpheres_create(this);
    _materialList = NULL;
  }

  InSituSpheres::~InSituSpheres()
  {
    if (_materialList) {
      free(_materialList);
      _materialList = NULL;
    }
  }

  void InSituSpheres::finalize(Model *model) 
  {
    radius            = getParam1f("radius",0.01f);
    materialID        = getParam1i("materialID",0);
    bytesPerSphere    = getParam1i("bytes_per_sphere",3*sizeof(float));
    offset_center     = getParam1i("offset_center",0);
    offset_radius     = getParam1i("offset_radius",-1);
    offset_materialID = getParam1i("offset_materialID",-1);
    offset_colorID    = getParam1i("offset_colorID",-1);
    materialList      = getParamData("materialList");
    colorData         = getParamData("color");

	// TODO: Must send these args through from command line
	// How would pulling within _finalize work vs. doing it here?
	const char *servName = "localhost";
	int servPort = 290374;

	DomainGrid *dd = ospIsPullRequest(MPI_COMM_WORLD, const_cast<char*>(servName), servPort, vec3i(1), .01f);

	int rank, size;
	float ghostRegionWidth = .1f;
	std::vector<vec3f> particles;
	MPI_CALL(Comm_rank(MPI_COMM_WORLD,&rank));
	MPI_CALL(Comm_size(MPI_COMM_WORLD,&size));
	if (rank == 0)
		PRINT(dd->worldBounds);
	for (int r = 0; r < size; ++r) {
		MPI_CALL(Barrier(MPI_COMM_WORLD));
		if (r == rank) {
			std::cout << "rank " << r << ": " << std::endl;
			for (int mbID = 0; mbID < dd->numMine(); ++mbID) {
				std::cout << " #" << mbID << std::endl;
				const DomainGrid::Block &b = dd->getMine(mbID);
				std::cout << "  lo " << b.actualDomain.lower << std::endl;
				std::cout << "  hi " << b.actualDomain.upper << std::endl;
				std::cout << "  #p " << b.particle.size() << std::endl;
				positions = b.particle;
			}
			std::cout << std::flush;
			fflush(0);
			usleep(200);
		}
		MPI_CALL(Barrier(MPI_COMM_WORLD));
	}
	// We've got our positions so now send it to the ospray geometry
    
    if (positions.empty()){
      throw std::runtime_error("#ospray:geometry/InSituSpheres: no 'InSituSpheres' data loaded from sim");
    }

    numSpheres = sizeof(vec3f) * positions.size() / bytesPerSphere;
    std::cout << "#osp: creating 'InSituSpheres' geometry, #InSituSpheres = " << numSpheres
              << std::endl;

    if (numSpheres >= (1ULL << 30)) {
      throw std::runtime_error("#ospray::InSituSpheres: too many InSituSpheres in this "
                               "sphere geometry. Consider splitting this "
                               "geometry in multiple geometries with fewer "
                               "InSituSpheres (you can still put all those "
                               "geometries into a single model, but you can't "
                               "put that many InSituSpheres into a single geometry "
                               "without causing address overflows)");
    }

    if (_materialList) {
      free(_materialList);
      _materialList = NULL;
    }

    if (materialList) {
      void **ispcMaterials = (void**) malloc(sizeof(void*) *
                                             materialList->numItems);
      for (int i=0;i<materialList->numItems;i++) {
        Material *m = ((Material**)materialList->data)[i];
        ispcMaterials[i] = m?m->getIE():NULL;
      }
      _materialList = (void*)ispcMaterials;
    }

    ispc::InSituSpheresGeometry_set(getIE(),model->getIE(),
                              &positions[0],_materialList,
                              colorData?(ispc::vec4f*)colorData->data:NULL,
                              numSpheres,bytesPerSphere,
                              radius,materialID,
                              offset_center,offset_radius,
                              offset_materialID,offset_colorID);
  }

  OSP_REGISTER_GEOMETRY(InSituSpheres,InSituSpheres);

} // ::ospray
