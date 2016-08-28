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
      box3f sphereBounds;
      int firstOwner;
      int numOwners;
      int isMine;

      // TODO: The special renderer will know how to deal with the
      // clipping of primary rays against the actual domain, the PKD
      // geometry shouldn't have to care about this at all!
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
    //! Transfer function used to color the spheres
    Ref<TransferFunction> transferFunction;
    vec3i grid;

    // TODO: We need to store DDBlock's of particle data like the data-distrib
    // volume rendering code.
    PartiKD *pkd;
    std::vector<uint32> binBitsArray;
    std::string server;
    int port;

    InSituSpheres();
    virtual ~InSituSpheres();
    virtual void dependencyGotChanged(ManagedObject *object);

  private:
    PartiKD *next_pkd;
    box3f next_actual_bounds;

    // Repeatedly poll from the simulation until poller_exit
    // is set true
    // Worker nodes should run this on a separate thread and call it repeatedly
    // once we actual have the data from the sim tell the master that we're dirty and should
    // re-commit. Then when re-committing we'll build the p-kd tree
    void pollSimulation();
    // Fetch new data from the simulation
    void getTimeStep();
  };
  /*! @} */

} // ::ospray

