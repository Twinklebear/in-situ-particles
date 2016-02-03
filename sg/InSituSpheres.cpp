#include <iostream>
#include <thread>
#include "ospray/mpi/MPICommon.h"
#include "libIS/is_render.h"
#include "sg/module/Module.h"
#include "sg/common/Integrator.h"
#include "InSituSpheres.h"

namespace ospray {
	namespace sg {
		InSituSpheres::InSituSpheres() : Geometry("InSituSpheres"), radius(0.01f), geometry(NULL)
		{}
		InSituSpheres::~InSituSpheres(){
			if (geometry){
				ospRelease(geometry);
				geometry = NULL;
			}
		}
		box3f InSituSpheres::getBounds(){
			// TODO WILL: We now have the first worker send us the bounds
			// of the geometry once it's been loaded but how can we update
			// the renderer & camera?
			box3f bounds = embree::empty;
			bounds.extend(vec3f(0.11, 0.11, 0.11));
			bounds.extend(vec3f(0.89, 0.89, 0.89));
			bounds.lower -= vec3f(radius);
			bounds.upper += vec3f(radius);
			return bounds;
		}
		void InSituSpheres::render(RenderContext &ctx){
			assert(!geometry);
			// Check if data has changed
			if (lastModified <= lastCommitted){
				return;
			}
			if (!geometry){
				std::cout << "#osp:sg:InSituSpheres: setting up InSituSpheres geometry\n";
				ospLoadModule("pkd");
				geometry = ospNewGeometry("InSituSpheres");
				if (!geometry){
					throw std::runtime_error("#osp:sg:InSituSpheres: could not create ospray 'InSituSpheres' geometry");
				}
				// assign a default material (for now.... eventually we might
				// want to do a 'real' material
				OSPMaterial mat = ospNewMaterial(ctx.integrator ? ctx.integrator->getOSPHandle() : NULL, "default");
				if (mat) {
					vec3f kd(.7f);
					vec3f ks(.3f);
					ospSet3fv(mat,"kd", &kd.x);
					ospSet3fv(mat,"ks", &ks.x);
					ospSet1f(mat,"Ns", 99.f);
					ospCommit(mat);
				}
				const std::string server = reinterpret_cast<const ParamT<const std::string>*>(getParam("server_name"))->value;
				const int port = reinterpret_cast<const ParamT<const int>*>(getParam("port"))->value;
				ospSetMaterial(geometry, mat);
				ospSet1f(geometry, "radius", radius);
				ospSet1i(geometry, "port", port);
				ospSetString(geometry, "server_name", server.c_str());
				// TODO: The commit and additon of the geometry is performed asynchronously. So how
				// can we tell the viewer that the bounds of the geometry have changed?
				ospCommit(geometry);
				lastCommitted = TimeStamp::now();
				ospAddGeometry(ctx.world->ospModel, geometry);
				// TODO WILL: Wait for the first worker to send us the world bounds?
				if (ospray::mpi::world.rank == 0){
					std::thread test_thread{[&](){
						std::cout << "Waiting to recieve world bounds\n";
						box3f world_bounds = embree::empty;
						MPI_CALL(Recv(&world_bounds, 6, MPI_FLOAT, 1, 1,
									ospray::mpi::world.comm, MPI_STATUS_IGNORE));
						std::cout << "MASTER recieved world bounds: " << world_bounds << "\n";
					}};
					test_thread.detach();
				}
				std::cout << "Geometry added\n";
			}
		}
		OSP_REGISTER_SG_NODE(InSituSpheres);
	}
}

