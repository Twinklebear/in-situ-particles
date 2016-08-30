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
#include "ospray/mpi/MPICommon.h"

#include "../testing_defines.h"

namespace ospray {
  const std::string attribute_name = "attrib";

  InSituSpheres::InSituSpheres(){}

  InSituSpheres::~InSituSpheres()
  {
    PING;
    ddSpheres.clear();
  }

  box3f InSituSpheres::getBounds() const
  {
    box3f b = empty;
    if (!ddSpheres.empty()){
      for (const auto &s : ddSpheres) {
        b.extend(s.pkd->getBounds());
      }
    }
    PING;
    return b;
  }

  void InSituSpheres::finalize(Model *model)
  {
    radius = getParam1f("radius", 0.01f);
    server = getParamString("server_name", NULL);
    poll_delay = getParam1f("poll_rate", 10.0);
    transferFunction = (TransferFunction*)getParamObject("transferFunction", NULL);
    port = getParam1i("port", -1);
    if (server.empty() || port == -1){
      throw std::runtime_error("#ospray:geometry/InSituSpheres: No simulation server and/or port specified");
    }
    const char *osp_data_parallel = getenv("OSPRAY_DATA_PARALLEL");
    if (!osp_data_parallel || std::sscanf(osp_data_parallel, "%dx%dx%d", &grid.x, &grid.y, &grid.z) != 3){
      throw std::runtime_error("#ospray:geometry/InSituSpheres: Must set OSPRAY_DATA_PARALLEL=XxYxZ"
          " for data parallel rendering!");
    }

    // Do a single blocking poll to get an initial timestep to render if the thread
    // hasn't been started
    if (nextDDSpheres.empty()){
      std::cout << "ospray::InSituSpheres: Making blocking initial query\n";
      getTimeStep();
      // We're going to be re-committed immediately so just bail out for now
      return;
    }
    ddSpheres = std::move(nextDDSpheres);
    for (auto &spheres : ddSpheres) {
      std::cout << "committing.." << std::endl;
      spheres.pkd->finalize(model);
      spheres.pkd->commit();
      spheres.ispc_pkd = spheres.pkd->getIE();
    }

    PING;

    // Wait for all workers to finish committing their pkds
    MPI_CALL(Barrier(ospray::mpi::worker.comm));

#if !POLL_ONCE
    // Launch the thread to poll the sim if we haven't already
    std::cout << "ospray::InSituSpheres: launching background polling thread\n";
    auto sim_poller = std::thread([&]{ pollSimulation(); });
    sim_poller.detach();
#endif
  }
  void InSituSpheres::pollSimulation(){
    std::cout << "ospray::InSituSpheres: Polling for new timestep after " << poll_delay << "s\n";
    const auto millis = std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(poll_delay * 1000.0));
    std::this_thread::sleep_for(millis);
    getTimeStep();
  }
  void InSituSpheres::getTimeStep(){
#ifdef AO_OCCLUSION_DISTANCE
    const float ghostRegionWidth = std::max(radius, AO_OCCLUSION_DISTANCE);
#else
    const float ghostRegionWidth = radius * 1.5f;
#endif
    DomainGrid *dd = ospIsPullRequest(ospray::mpi::worker.comm, server.c_str(), port,
        grid, ghostRegionWidth);

    int rank = ospray::mpi::worker.rank;
    int size = ospray::mpi::worker.size;
    if (rank == 0){
      PRINT(dd->worldBounds);
    }
    nextDDSpheres.clear();
    for (size_t i = 0; i < dd->numBlocks; ++i) {
      const DomainGrid::Block &b = dd->block[i];
      DDSpheres spheres;
      spheres.actualDomain = b.actualDomain;
      spheres.firstOwner = b.firstOwner;
      spheres.numOwners = b.numOwners;
      spheres.isMine = b.isMine;
      if (b.isMine) {
        buildPKDBlock(b, spheres);
      }
      nextDDSpheres.push_back(std::move(spheres));
    }
    MPI_CALL(Barrier(ospray::mpi::worker.comm));

#if PRINT_FULL_PARTICLE_COUNT
    // TODO: Should count across all ddSpheres
    uint64_t num_particles = 0;
    for (const auto &b : nextDDSpheres) {
      num_particles += b.positions.size();
    }
    uint64_t total_particles = 0;
    MPI_CALL(Allreduce(&num_particles, &total_particles, 1, MPI_UINT64_T, MPI_SUM,
          ospray::mpi::worker.comm));
    if (rank == 0){
      std::cout << "#ospray:geometry/InSituSpheres: Total Particles = " << total_particles
        << std::endl;
    }
#endif

    // TODO: Find the local attrib range over all our blocks, then figure out global range
    float local_attr_lo = std::numeric_limits<float>::max();
    float local_attr_hi = std::numeric_limits<float>::lowest();
#if !USE_RENDER_RANK_ATTRIB
    for (const auto &b : nextDDSpheres) {
      for (size_t i = 0; i < b.attributes.size(); ++i) {
        local_attr_lo = std::min(local_attr_lo, b.attributes[i]);
        local_attr_hi = std::max(local_attr_hi, b.attributes[i]);
      }
    }
    // We need to figure out the min/max attribute range over ALL the workers
    // to properly color by attribute with the same transfer function. Otherwise
    // workers will map different values to the same color
    float attr_lo = local_attr_lo, attr_hi = local_attr_hi;
    MPI_CALL(Allreduce(&local_attr_lo, &attr_lo, 1, MPI_FLOAT, MPI_MIN, ospray::mpi::worker.comm));
    MPI_CALL(Allreduce(&local_attr_hi, &attr_hi, 1, MPI_FLOAT, MPI_MAX, ospray::mpi::worker.comm));
#else
    float attr_lo = 0;
    float attr_hi = static_cast<float>(ospray::mpi::worker.size);
#endif

    // Give each PKD the global attribute range and commit them
    for (auto &ddspheres : nextDDSpheres) {
      ddspheres.pkd->findParam("attribute_low", 1)->set(attr_lo);
      ddspheres.pkd->findParam("attribute_high", 1)->set(attr_hi);
    }

    // Wait for all workers to finish building their pkds
    MPI_CALL(Barrier(ospray::mpi::worker.comm));

    // Tell the render process the bounds of the geometry in the world
    // and that we're dirty and should be updated
    if (ospray::mpi::world.rank == 1){
      MPI_CALL(Send(&dd->worldBounds, 6, MPI_FLOAT, 0, 1, ospray::mpi::world.comm));
    }
    delete dd;
  }

  void InSituSpheres::buildPKDBlock(const DomainGrid::Block &b, DDSpheres &ddspheres) const {
    ParticleModel model;
    PartiKD partikd;
    model.radius = radius;

    ddspheres.actualDomain = b.actualDomain;
    ddspheres.firstOwner = b.firstOwner;
    ddspheres.numOwners = b.numOwners;
    PRINT(ddspheres.actualDomain);
    PRINT(ddspheres.firstOwner);
    PRINT(ddspheres.numOwners);
    std::cout << "  lo " << b.actualDomain.lower << std::endl;
    std::cout << "  hi " << b.actualDomain.upper << std::endl;
    std::cout << "  ghost lo " << b.ghostDomain.lower << std::endl;
    std::cout << "  ghost hi " << b.ghostDomain.upper << std::endl;
    std::cout << "  #p " << b.particle.size() / OSP_IS_STRIDE_IN_FLOATS << std::endl;
    for (size_t i = 0; i < b.particle.size() / OSP_IS_STRIDE_IN_FLOATS; ++i){
      size_t pid = i * OSP_IS_STRIDE_IN_FLOATS;
      model.position.push_back(vec3f(b.particle[pid], b.particle[pid + 1],
            b.particle[pid + 2]));
      if (OSP_IS_STRIDE_IN_FLOATS == 4){
#if !USE_RENDER_RANK_ATTRIB
        model.addAttribute(attribute_name, b.particle[pid + 3]);
#else
        model.addAttribute(attribute_name, rank);
#endif
      }
    }

    if (model.position.empty()){
      throw std::runtime_error("#ospray:geometry/InSituSpheres: no 'InSituSpheres' data loaded from sim");
    }
    // We've got our positions so now send it to the ospray geometry
    std::cout << "#osp: creating 'InSituSpheres' geometry, #InSituSpheres = "
      << model.position.size() << std::endl;

    if (model.position.size() >= (1ULL << 30)) {
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
    partikd.build(&model);

    ddspheres.positions = std::move(model.position);
    ddspheres.attributes = std::move(model.getAttribute(attribute_name)->value);
    // TODO: The positions data is being lost??
    Data *posData = new Data(ddspheres.positions.size(), OSP_FLOAT3, ddspheres.positions.data(),
        OSP_DATA_SHARED_BUFFER);
    // TODO: Will need to manually release these. Do we actually need to
    // manually increment the refcount?
    posData->refInc();
    ddspheres.pkd = new PartiKDGeometry;

    ddspheres.pkd->findParam("position", 1)->set(posData);
    if (!transferFunction) {
      std::cout << "NO TRANSFER FCN" << std::endl;
    }
    ddspheres.pkd->findParam("transferFunction", 1)->set(transferFunction);
    ddspheres.pkd->findParam("radius", 1)->set(model.radius);
    if (!ddspheres.attributes.empty()) {
      Data *attribData = new Data(ddspheres.attributes.size(), OSP_FLOAT, ddspheres.attributes.data(),
          OSP_DATA_SHARED_BUFFER);
      attribData->refInc();
      ddspheres.pkd->findParam("attribute", 1)->set(attribData);
    } else {
      std::cout << "No attributes on particles" << std::endl;
    }
  }

  OSP_REGISTER_GEOMETRY(InSituSpheres,InSituSpheres);

} // ::ospray
