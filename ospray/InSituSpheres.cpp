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
#include <cstdio>
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

#define USE_RENDER_RANK_ATTRIB 0

namespace ospray {
  const std::string attribute_name = "attrib";

  InSituSpheres::InSituSpheres()
  {
    ispcEquivalent = ispc::PartiKDGeometry_create(this);
  }

  InSituSpheres::~InSituSpheres()
  {
	  std::cout << "ospray::geometry::~InSituSpheres\n";
	  // It seems like this isn't called when we commit? Yet I thought
	  // we were deleting ourselves on accident?
	  delete pkd->model;
  }

  box3f InSituSpheres::getBounds() const
  {
	  box3f b = empty;
	  if (pkd){
		  ParticleModel *model = pkd->model;
		  for (size_t i = 0; i < model->position.size(); ++i){
			  b.extend(model->position[i]);
		  }
	  }
	  return b;
  }

  void InSituSpheres::dependencyGotChanged(ManagedObject *object)
  {
	  ispc::PartiKDGeometry_updateTransferFunction(this->getIE(), transferFunction->getIE());
  }

  void InSituSpheres::finalize(Model *model) 
  {
	  radius = getParam1f("radius", 0.01f);
	  server = getParamString("server_name", NULL);
	  poll_delay = getParam1f("poll_rate", 10.0);
	  transferFunction = (TransferFunction*)getParamObject("transferFunction", NULL);
	  port = getParam1i("port", -1);
	  pkd = (PartiKD*)getParamObject("pkd", NULL);
	  ParticleModel *old_particles = (ParticleModel*)getVoidPtr("old_particles", NULL);
	  if (old_particles){
		  delete old_particles;
	  }
	  if (server.empty() || port == -1){
		  throw std::runtime_error("#ospray:geometry/InSituSpheres: No simulation server and/or port specified");
	  }
	  if (transferFunction){
		  transferFunction->registerListener(this);
	  }
	  const char *osp_data_parallel = getenv("OSPRAY_DATA_PARALLEL");
	  if (!osp_data_parallel || std::sscanf(osp_data_parallel, "%dx%dx%d", &grid.x, &grid.y, &grid.z) != 3){
		  throw std::runtime_error("#ospray:geometry/InSituSpheres: Must set OSPRAY_DATA_PARALLEL=XxYxZ"
				  " for data parallel rendering!");
	  }
	  // Do a single blocking poll to get an initial timestep to render if the thread
	  // hasn't been started
	  if (!pkd){
		  std::cout << "ospray::InSituSpheres: Making blocking initial query\n";
		  getTimeStep();
	  }

	  ParticleModel *particle_model = pkd->model;
	  const box3f centerBounds = getBounds();
	  const box3f sphereBounds(centerBounds.lower - vec3f(radius), centerBounds.upper + vec3f(radius));

	  // compute attribute mask and attrib lo/hi values
	  float attr_lo = 0.f, attr_hi = 0.f;
	  float *attribute = NULL;
	  if (particle_model->hasAttribute(attribute_name)){
		  size_t numParticles = pkd->numParticles;
		  size_t numInnerNodes = pkd->numInnerNodes;

		  attribute = particle_model->getAttribute(attribute_name)->value.data();
		  assert(numParticles == particle_model->getAttribute(attribute_name)->value.size());

#if !USE_RENDER_RANK_ATTRIB
		  attr_lo = attr_hi = attribute[0];
		  for (size_t i=0;i<numParticles;i++){
			  attr_lo = std::min(attr_lo,attribute[i]);
			  attr_hi = std::max(attr_hi,attribute[i]);
		  }
#else
		  attr_lo = 0;
		  attr_hi = static_cast<float>(ospray::mpi::worker.size);
#endif

		  binBitsArray.resize(numInnerNodes, 0);
		  size_t numBytesRangeTree = numInnerNodes * sizeof(uint32);
		  for (long long pID=numInnerNodes-1;pID>=0;--pID) {
			  size_t lID = 2*pID+1;
			  size_t rID = lID+1;
			  uint32 lBits = 0, rBits = 0;
			  if (rID < numInnerNodes)
				  rBits = binBitsArray[rID];
			  else if (rID < numParticles)
				  rBits = getAttributeBits(attribute[rID],attr_lo,attr_hi);
			  if (lID < numInnerNodes)
				  lBits = binBitsArray[lID];
			  else if (lID < numParticles)
				  lBits = getAttributeBits(attribute[lID],attr_lo,attr_hi);
			  binBitsArray[pID] = lBits|rBits;
		  }
		  std::cout << "#osp:pkd: found attribute [" << attr_lo << ".." << attr_hi
			  << "], root bits " << (int*)(int64)binBitsArray[0] << std::endl;
	  }

	  std::cout << "ospray::InSituSpheres: setting pkd geometry\n";
	  assert(particle_model);
	  ispc::PartiKDGeometry_set(getIE(), model->getIE(), false, false,
			  transferFunction ? transferFunction->getIE() : NULL,
			  radius,
			  pkd->numParticles, // TODO: Handle no particles
			  pkd->numInnerNodes,
			  (ispc::PKDParticle*)&particle_model->position[0],
			  attribute,
			  binBitsArray.data(),
			  (ispc::box3f&)centerBounds, (ispc::box3f&)sphereBounds,
			  (ispc::box3f&)sphereBounds,
			  attr_lo,attr_hi);

	  // Launch the thread to poll the sim if we haven't already
	  std::cout << "ospray::InSituSpheres: launching background polling thread\n";
	  auto sim_poller = std::thread([&]{ pollSimulation(); });
	  sim_poller.detach();
  }
  void InSituSpheres::pollSimulation(){
	  std::cout << "ospray::InSituSpheres: Polling for new timestep after " << poll_delay << "s\n";
	  const auto millis = std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(poll_delay * 1000.0));
	  std::this_thread::sleep_for(millis);
	  std::cout << "ospray::InSituSpheres: THREAD polling for new timestep\n";
	  getTimeStep();
  }
  void InSituSpheres::getTimeStep(){
	  const float ghostRegionWidth = radius * 1.5f;
	  std::cout << "World rank is " << ospray::mpi::world.rank << "\n";
	  DomainGrid *dd = ospIsPullRequest(ospray::mpi::worker.comm, server.c_str(), port,
			  grid, ghostRegionWidth);

	  // Get the model from pkd and allocate it if it's missing
	  // we also have the builder forget about the model since it asserts
	  // that no model is set when calling build
	  std::cout << "ospray::InSituSpheres: Building new timestep\n";
	  ParticleModel *model = new ParticleModel;
	  // Clear the old build data
	  PartiKD *pkd_build = new PartiKD;
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
				  std::cout << " rank: " << rank << " #" << mbID << std::endl;
				  const DomainGrid::Block &b = dd->getMine(mbID);
				  std::cout << "  lo " << b.actualDomain.lower << std::endl;
				  std::cout << "  hi " << b.actualDomain.upper << std::endl;
				  std::cout << "  ghost lo " << b.ghostDomain.lower << std::endl;
				  std::cout << "  ghost hi " << b.ghostDomain.upper << std::endl;
				  std::cout << "  #p " << b.particle.size() / OSP_IS_STRIDE_IN_FLOATS << std::endl;
				  for (size_t i = 0; i < b.particle.size() / OSP_IS_STRIDE_IN_FLOATS; ++i){
					  size_t pid = i * OSP_IS_STRIDE_IN_FLOATS;
					  model->position.push_back(vec3f(b.particle[pid], b.particle[pid + 1], b.particle[pid + 2]));
					  // TODO WILL: If we want to color by render node rank we should push back 'rank' here
					  if (OSP_IS_STRIDE_IN_FLOATS == 4){
#if !USE_RENDER_RANK_ATTRIB
						  model->addAttribute(attribute_name, b.particle[pid + 3]);
#else
						  model->addAttribute(attribute_name, rank);
#endif
					  }
				  }
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
	  std::cout << "#osp: creating 'InSituSpheres' geometry, #InSituSpheres = "
		  << model->position.size() << std::endl;

	  if (model->position.size() >= (1ULL << 30)) {
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
	  pkd_build->build(model);

	  // Set the new pkd as the one to be displayed
	  setParam("pkd", pkd_build);
	  // If we don't have a pkd to show, show this one (aka this is the first blocking call)
	  if (!pkd){
		  pkd = pkd_build;
	  } else {
		  findParam("old_particles", 1)->set(pkd->model);
	  }
	  delete dd;
	  // Wait for all workers to finish building the pkd
	  MPI_CALL(Barrier(ospray::mpi::worker.comm));

	  // Tell the render process the bounds of the geometry in the world
	  // and that we're dirty and should be updated
	  if (ospray::mpi::world.rank == 1){
		  MPI_CALL(Send(&dd->worldBounds, 6, MPI_FLOAT, 0, 1, ospray::mpi::world.comm));
	  }
  }

  OSP_REGISTER_GEOMETRY(InSituSpheres,InSituSpheres);

} // ::ospray
