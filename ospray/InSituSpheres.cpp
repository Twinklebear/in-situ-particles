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

#include <iostream>
#include <chrono>
// ospray
#include "InSituSpheres.h"
#include "PKDGeometry.h"
#include "ospray/common/Data.h"
#include "ospray/common/Model.h"
#include "libIS/is_render.h"
#include "ospray/mpi/MPICommon.h"
// ispc-generated files
#include "InSituSpheres_ispc.h"
#include "PKDGeometry_ispc.h"

namespace ospray {

  InSituSpheres::InSituSpheres()
  {
    ispcEquivalent = ispc::PartiKDGeometry_create(this);
    _materialList = NULL;
	rendered_pkd = 0;
	poller_exit = false;
  }

  InSituSpheres::~InSituSpheres()
  {
    if (_materialList) {
      free(_materialList);
      _materialList = NULL;
    }
	// Tell the sim polling thread to exist and join it back
	poller_exit = true;
	sim_poller.join();
  }

  box3f InSituSpheres::getBounds() const
  {
	  int to_render = rendered_pkd.load() % 2;
	  ParticleModel *model = pkds[to_render].model;
	  box3f b = empty;
	  if (model != NULL){
		  for (size_t i = 0; i < model->position.size(); ++i){
			  b.extend(model->position[i]);
		  }
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
	  server = getParamString("server_name", NULL);
	  port = getParam1i("port", -1);
	  if (server.empty() || port == -1){
		  throw std::runtime_error("#ospray:geometry/InSituSpheres: No simulation server and/or port specified");
	  }
	  // Do a single blocking poll to get an initial timestep to render if the thread
	  // hasn't been started
	  if (sim_poller.get_id() == std::thread::id()){
		  std::cout << "ospray::InSituSpheres: Making blocking initial query\n";
		  getTimeStep();
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

	  int to_render = rendered_pkd.load() % 2;
	  PartiKD &pkd_active = pkds[to_render];
	  ParticleModel *particle_model = pkd_active.model;
	  const box3f centerBounds = getBounds();
	  const box3f sphereBounds(centerBounds.lower - vec3f(radius), centerBounds.upper + vec3f(radius));
	  std::cout << "ospray::InSituSpheres: setting pkd geometry\n";

	  assert(particle_model);
	  ispc::PartiKDGeometry_set(getIE(), model->getIE(), false, false,
			  // TODO: Transfer function
			  /*transferFunction ? transferFunction->getIE() : NULL,*/ NULL,
			  radius,
			  pkd_active.numParticles,
			  pkd_active.numInnerNodes,
			  (ispc::PKDParticle*)&particle_model->position[0],
			  /*attribute,*/ NULL, // TODO: Attribs
			  /*binBitsArray,*/ NULL, // TODO: attribs
			  (ispc::box3f&)centerBounds, (ispc::box3f&)sphereBounds,
			  /*attr_lo,attr_hi);*/ 0, 0); // TODO: Attribs

	  // Launch the thread to poll the sim if we haven't already
	  if (sim_poller.get_id() == std::thread::id()){
		  std::cout << "ospray::InSituSpheres: launching background polling thread\n";
		  sim_poller = std::thread([&]{ pollSimulation(); });
	  }
  }
  void InSituSpheres::pollSimulation(){
	  while (!poller_exit){
		  std::this_thread::sleep_for(std::chrono::seconds(10));
		  std::cout << "ospray::InSituSpheres: THREAD polling for new timestep\n";
		  getTimeStep();
	  }
  }
  void InSituSpheres::getTimeStep(){
	  std::cout << "ospray::InSituSpheres: Getting a Timestep\n";
	  const float ghostRegionWidth = radius * 1.5f;
	  DomainGrid *dd = ospIsPullRequest(ospray::mpi::worker.comm, const_cast<char*>(server.c_str()), port, vec3i(2),
			  ghostRegionWidth);

	  // Get the model from pkd and allocate it if it's missing
	  // we also have the builder forget about the model since it asserts
	  // that no model is set when calling build
	  int to_update = rendered_pkd.load() + 1;
	  std::cout << "ospray::InSituSpheres: Building new timestep on pkd " << to_update % 2 << "\n";
	  ParticleModel *model = pkds[to_update % 2].model;
	  // Clear the old build data
	  pkds[to_update % 2] = PartiKD{};
	  PartiKD &pkd_build = pkds[to_update % 2];
	  if (!model){
		  model = new ParticleModel;
	  }
	  // TODO WILL: If we store more stuff here we'll need a proper
	  // way to clean & reset the ParticleModel
	  model->position.clear();
	  model->radius = radius;
	  int rank = ospray::mpi::worker.rank;
	  int size = ospray::mpi::worker.size;
	  if (rank == 0)
		  PRINT(dd->worldBounds);
	  for (int r = 0; r < size; ++r) {
		  MPI_CALL(Barrier(ospray::mpi::worker.comm));
		  if (r == rank) {
			  std::cout << "worker rank " << r << " (global rank "
				  << ospray::mpi::world.rank << ") : " << std::endl;
			  for (int mbID = 0; mbID < dd->numMine(); ++mbID) {
				  std::cout << " #" << mbID << std::endl;
				  const DomainGrid::Block &b = dd->getMine(mbID);
				  std::cout << "  lo " << b.actualDomain.lower << std::endl;
				  std::cout << "  hi " << b.actualDomain.upper << std::endl;
				  std::cout << "  #p " << b.particle.size() << std::endl;
				  model->position = b.particle;
			  }
			  std::cout << std::flush;
			  fflush(0);
			  usleep(200);
		  }
		  MPI_CALL(Barrier(ospray::mpi::worker.comm));
	  }

	  if (model->position.empty()){
		  throw std::runtime_error("#ospray:geometry/InSituSpheres: no 'InSituSpheres' data loaded from sim");
	  }

	  // We've got our positions so now send it to the ospray geometry
	  numSpheres = sizeof(vec3f) * model->position.size() / bytesPerSphere;
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
	  // Build the pkd tree on the particles
	  std::cout << "InSituSpheres: building pkd\n";
	  pkd_build.build(model);

	  rendered_pkd.store(to_update); 
	  // Wait for all workers to finish building the pkd
	  MPI_CALL(Barrier(ospray::mpi::worker.comm));

	  // Tell the render process the bounds of the geometry in the world
	  // and that we're dirty and should be updated
	  if (ospray::mpi::world.rank == 1){
		  std::cout << "Sending world bounds\n";
		  MPI_CALL(Send(&dd->worldBounds, 6, MPI_FLOAT, 0, 1, ospray::mpi::world.comm));
	  }
  }

  OSP_REGISTER_GEOMETRY(InSituSpheres,InSituSpheres);

} // ::ospray
