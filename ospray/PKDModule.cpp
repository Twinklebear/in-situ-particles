// ======================================================================== //
// Copyright 2009-2014 Intel Corporation                                    //
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

#include <thread>
#include <chrono>

#include <ospray/ospray.h>
#include <ospcommon/vec.h>
#include <ospcommon/box.h>

#include <OSPRayScriptHandler.h>
#include <ospray/mpi/MPICommon.h>

using namespace ospcommon;

namespace pkd {
  namespace cs = chaiscript;

  ospray::cpp::Geometry setupInSituSpheres(ospray::cpp::Renderer renderer, const std::string &server,
      const int port, const float radius)
  {
    using namespace ospray::cpp;
    std::cout << "#osp:pkd:InSituSpheres: setting up InSituSpheres geometry\n";

    Geometry geometry("InSituSpheres");

    // assign a default material (for now.... eventually we might
    // want to do a 'real' material
    Material mat = renderer.newMaterial("default");
    // The Uintah stuff from Josh looks good with a slightly unrealistic
    // brighter material than this one. Otherwise it gets a bit too dark
    mat.set("Kd", vec3f(0.9f));
    mat.set("Ks", vec3f(0.1f));
    mat.set("Ns", 99.f);
    mat.commit();

    TransferFunction transfer_fn("piecewise_linear");
    const std::vector<vec3f> colors {vec3f(0.f), vec3f(0.9f)};
    const std::vector<float> opacities = {0.7f, 1.f};

    Data colorsData = Data(colors.size(), OSP_FLOAT3, colors.data());
    Data opacityData = Data(opacities.size(), OSP_FLOAT, opacities.data());
    transfer_fn.set("opacities", opacityData);
    transfer_fn.set("colors", colorsData);
    transfer_fn.commit();

    geometry.setMaterial(mat);
    geometry.set("radius", radius);
    geometry.set("port", port);
    geometry.set("server_name", server);
    geometry.set("transferFunction", transfer_fn);
    return geometry;
  }

  ospray::cpp::Geometry pollSim(ospray::cpp::Renderer renderer, const std::string &server,
      const int port, const float radius, const float pollRate, box3f &bounds,
      std::function<void (ospray::cpp::Geometry, const box3f&)> callback)
  {
    using namespace ospray::cpp;
    Geometry geometry = setupInSituSpheres(renderer, server, port, radius);
    geometry.set("poll_rate", pollRate);
    geometry.commit();

    // Do a single blocking query then spawn a thread to notify us in the future
    MPI_CALL(Recv(&bounds, 6, MPI_FLOAT, 1, 1,
          ospray::mpi::world.comm, MPI_STATUS_IGNORE));

    // TODO: This is a C++14 thing, so we'd want to push it behind some handle we move into
    // and then give to the thread.
    std::thread poller([callback, geometry](){
        box3f newBounds;
        while (true){
          MPI_CALL(Recv(&newBounds, 6, MPI_FLOAT, 1, 1,
              ospray::mpi::world.comm, MPI_STATUS_IGNORE));
          callback(geometry, newBounds);
        }
    });
    poller.detach();

    return geometry;
  }

  ospray::cpp::Geometry pollOnce(ospray::cpp::Renderer renderer, const std::string &server,
      const int port, const float radius, box3f &bounds)
  {
    using namespace ospray::cpp;
    Geometry geometry = setupInSituSpheres(renderer, server, port, radius);
    geometry.set("poll_rate", -1);
    geometry.commit();

    // Wait for the geometry to actually get data from the simulation
    MPI_CALL(Recv(&bounds, 6, MPI_FLOAT, 1, 1,
          ospray::mpi::world.comm, MPI_STATUS_IGNORE));
    return geometry;
  }
  void registerModule(cs::ChaiScript &engine) {
    engine.add(cs::fun(&pollOnce), "pkdPollOnce");
    engine.add(cs::fun(&pollSim), "pkdPollSim");
  }
  void printHelp() {
    std::cout << "==PKD Module (InSituSpheres) Help==\n"
      << "  Functions:\n"
      << "    Geometry pkdPollOnce(renderer, server, port, radius, bounds):\n"
      << "      Connect to the simulation running on 'server' listening at 'port'\n"
      << "      and query particle data from it. Returns the geometry and world bounds\n"
      << "====\n";
  }
  extern "C" void ospray_init_module_pkd() {
    std::cout << "#osp:pkd: loading 'pkd' module" << std::endl;
    ospray::script::register_module(pkd::registerModule, pkd::printHelp);
  }
}

