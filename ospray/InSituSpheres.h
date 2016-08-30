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

#pragma once

#include <atomic>
#include <thread>
#include "ospray/geometry/Geometry.h"
#include "ospray/transferFunction/TransferFunction.h"
#include "libIS/is_render.h"
// pkd builder library
#include "apps/ParticleModel.h"
#include "apps/PartiKD.h"
#include "PKDGeometry.h"

namespace ospray {

  /*! \brief A geometry for a set of InSituSpheres
    Implements the \ref geometry_InSituSpheres geometry
    */
  struct InSituSpheres : public Geometry {
    // A data-distributed block of particle data that we may or may not own.
    // this mirrors the style of the DDBlock in datadistributedblockedvolume
    struct DDSpheres {
      // The actual grid domain assigned for this block
      box3f actualDomain;
      // The full domain spanned by the particles in the block, accounting
      // for ghost regions + particle radii
      // TODO: I don't think we need this, the pkd tracks this
      //box3f sphereBounds;
      int firstOwner;
      int numOwners;
      int isMine;
      // The position and attribute data shared with the pkd geometry
      std::vector<vec3f> positions;
      std::vector<float> attributes;

      Ref<PartiKDGeometry> pkd;
      void *ispc_pkd;
    };

    //! \brief common function to help printf-debugging
    virtual std::string toString() const { return "ospray::InSituSpheres"; }
    /*! \brief integrates this geometry's primitives into the respective
      model's acceleration structure */
    virtual void finalize(Model *model);
    box3f getBounds() const;

    //! radius for the spheres
    float radius;
    /*! frequency (in seconds) to check for new timesteps, each timestep
     * query is blocking, so after getting a new timestep we'll wait
     * poll_rate seconds before requesting a new one. Default is 10s
     */
    float poll_delay;
    vec3i grid;

    // TODO: We need to store DDBlock's of particle data like the data-distrib
    // volume rendering code.
    std::vector<DDSpheres> ddSpheres;
    std::string server;
    int port;

    InSituSpheres();
    virtual ~InSituSpheres();

  private:
    // The next set of particle data that we'll be switching too
    std::vector<DDSpheres> nextDDSpheres;
    // Repeatedly poll from the simulation until poller_exit
    // is set true
    // Worker nodes should run this on a separate thread and call it repeatedly
    // once we actual have the data from the sim tell the master that we're dirty and should
    // re-commit. Then when re-committing we'll build the p-kd tree
    void pollSimulation();
    // Fetch new data from the simulation
    void getTimeStep();
    // Build the PKD tree on the block of particles in the grid into the DDSpheres passed
    void buildPKDBlock(const DomainGrid::Block &b, DDSpheres &ddspheres) const;
  };
  /*! @} */

} // ::ospray

