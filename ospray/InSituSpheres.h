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
#include "apps/ParticleModel.h"
// pkd builder library
#include "apps/PartiKD.h"

namespace ospray {
  /*! @{ \ingroup ospray_module_streamlines */

  /*! \defgroup geometry_InSituSpheres InSituSpheres ("InSituSpheres") 

    \ingroup ospray_supported_geometries

    \brief Geometry representing InSituSpheres with a per-sphere radius

    Implements a geometry consisting of individual InSituSpheres, each of
    which can have a radius.  To allow a variety of sphere
    representations this geometry allows a flexible way of specifying
    the offsets of origin, radius, and material ID within a data array
    of 32-bit floats.

    Parameters:
    <dl>
    <dt><code>float        radius = 0.01f</code></dt><dd>Base radius common to all InSituSpheres if 'offset_radius' is not used</dd>
    <dt><code>int32        materialID = 0</code></dt><dd>Material ID common to all InSituSpheres if 'offset_materialID' is not used</dd>
    <dt><code>int32        bytes_per_sphere = 3*sizeof(float)</code></dt><dd>Size (in bytes) of each sphere in the data array.</dd>
    <dt><code>int32        offset_center = 0</code></dt><dd>Offset (in bytes) of each sphere's 'vec3f center' value within the sphere</dd>
    <dt><code>int32        offset_radius = -1</code></dt><dd>Offset (in bytes) of each sphere's 'float radius' value within each sphere. Setting this value to -1 means that there is no per-sphere radius value, and that all InSituSpheres should use the (shared) 'radius' value instead</dd>
    <dt><code>int32        offset_materialID = -1</code></dt><dd>Offset (in bytes) of each sphere's 'int materialID' value within each sphere. Setting this value to -1 means that there is no per-sphere material ID, and that all InSituSpheres share the same per-geometry 'materialID'</dd>
    <dt><code>Data<float>  InSituSpheres</code></dt><dd> Array of data elements.</dd>
    </dl>

    The functionality for this geometry is implemented via the
    \ref ospray::InSituSpheres class.

  */

  /*! \brief A geometry for a set of InSituSpheres

    Implements the \ref geometry_InSituSpheres geometry

  */
  struct InSituSpheres : public Geometry {
    //! \brief common function to help printf-debugging 
    virtual std::string toString() const { return "ospray::InSituSpheres"; }
    /*! \brief integrates this geometry's primitives into the respective
      model's acceleration structure */
    virtual void finalize(Model *model);
    box3f getBounds() const;

    float radius;   //!< default radius, if no per-sphere radius was specified.
    int32 materialID;
    
    size_t numSpheres;
    size_t bytesPerSphere; //!< num bytes per sphere
    int64 offset_center;
    int64 offset_radius;
    int64 offset_materialID;
    int64 offset_colorID;

    Ref<Data> materialList;
    void     *_materialList;
    Ref<Data> colorData; /*!< sphere color array (vec3fa) */
	/*! We have two pkds that we swap between, once containing
	 * the data being rendered and one with the data coming from
	 * the next time step & being built */
	PartiKD pkds[2];
	int rendered_pkd;
	std::string server;
	int port;
	// Thread that will continue to poll new timesteps from
	// the simulation
	std::thread sim_poller;
	std::atomic<bool> poller_exit;

    InSituSpheres();
    ~InSituSpheres();

  private:
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

