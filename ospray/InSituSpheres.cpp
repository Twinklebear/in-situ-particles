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
#include "PKDGeometry.h"
#include "ospray/common/Data.h"
#include "ospray/common/Model.h"
#include "libIS/is_render.h"
// ispc-generated files
#include "InSituSpheres_ispc.h"
#include "PKDGeometry_ispc.h"
// pkd builder library
#include "apps/PartiKD.h"

namespace ospray {

  InSituSpheres::InSituSpheres()
  {
    ispcEquivalent = ispc::PartiKDGeometry_create(this);
    _materialList = NULL;
	particle_model.radius = 0.01;
  }

  InSituSpheres::~InSituSpheres()
  {
    if (_materialList) {
      free(_materialList);
      _materialList = NULL;
    }
  }

  box3f InSituSpheres::getBounds() const
  {
    box3f b = empty;
    for (size_t i = 0; i < particle_model.position.size(); ++i){
      b.extend(particle_model.position[i]);
    }
    return b;
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
	const char *server = getParamString("server_name", NULL);
	int port = getParam1i("port", -1);
	if (!server || port == -1){
		throw std::runtime_error("#ospray:geometry/InSituSpheres: No simulation server and/or port specified");
	}

	DomainGrid *dd = ospIsPullRequest(MPI_COMM_WORLD, const_cast<char*>(server), port, vec3i(1), .01f);

	int rank, size;
	float ghostRegionWidth = .1f;
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
				particle_model.position = b.particle;
			}
			std::cout << std::flush;
			fflush(0);
			usleep(200);
		}
		MPI_CALL(Barrier(MPI_COMM_WORLD));
	}
	// We've got our positions so now send it to the ospray geometry
    
    if (particle_model.position.empty()){
      throw std::runtime_error("#ospray:geometry/InSituSpheres: no 'InSituSpheres' data loaded from sim");
    }

	// TODO: Here we want to actually build a pkd tree on the particles and use the pkd
	// geometry to intersect against
    numSpheres = sizeof(vec3f) * particle_model.position.size() / bytesPerSphere;
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

	// Build the pkd tree on the particles
	PartiKD pkd;
	std::cout << "InSituSpheres: building pkd\n";
	pkd.build(&particle_model);
	const box3f centerBounds = getBounds();
    const box3f sphereBounds(centerBounds.lower - vec3f(particle_model.radius),
                             centerBounds.upper + vec3f(particle_model.radius));
	std::cout << "InSituSpheres: setting pkd geometry\n";
    ispc::PartiKDGeometry_set(getIE(), model->getIE(), false, false,
							  // TODO: Transfer function
                              /*transferFunction ? transferFunction->getIE() : NULL,*/ NULL,
                              particle_model.radius,
                              pkd.numParticles,
                              pkd.numInnerNodes,
                              (ispc::PKDParticle*)&particle_model.position[0],
                              /*attribute,*/ NULL, // TODO: Attribs
							  /*binBitsArray,*/ NULL, // TODO: attribs
                              (ispc::box3f&)centerBounds, (ispc::box3f&)sphereBounds,
                              /*attr_lo,attr_hi);*/ 0, 0); // TODO: Attribs
  }

  OSP_REGISTER_GEOMETRY(InSituSpheres,InSituSpheres);

} // ::ospray
