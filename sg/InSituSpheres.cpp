#include <iostream>
#include <thread>
#include "ospray/mpi/MPICommon.h"
#include "libIS/is_render.h"
#include "sg/module/Module.h"
#include "sg/common/Integrator.h"
#include "InSituSpheres.h"

#include "../testing_defines.h"

namespace ospray {
	namespace sg {
		InSituSpheres::InSituSpheres() : Geometry("InSituSpheres"), radius(0.01f), geometry(NULL),
			bounds(embree::empty), poller_exit(false), transfer_fn(NULL)
		{}
		InSituSpheres::~InSituSpheres(){
			if (geometry){
				ospRelease(geometry);
				geometry = NULL;
			}
			if (polling_thread.get_id() != std::thread::id()){
				poller_exit = true;
				polling_thread.join();
			}
		}
		box3f InSituSpheres::getBounds(){
			return bounds;
		}
		void InSituSpheres::render(RenderContext &ctx){
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
					vec3f kd(.9f);
					vec3f ks(.1f);
					ospSet3fv(mat,"kd", &kd.x);
					ospSet3fv(mat,"ks", &ks.x);
					ospSet1f(mat,"Ns", 99.f);
					ospCommit(mat);
				}
				const std::string server = reinterpret_cast<const ParamT<const std::string>*>(getParam("server_name"))->value;
				const int port = reinterpret_cast<const ParamT<const int>*>(getParam("port"))->value;
				const float poll_rate = reinterpret_cast<ParamT<float>*>(getParam("poll_rate"))->value;
				radius = reinterpret_cast<ParamT<float>*>(getParam("radius"))->value;
				transfer_fn = reinterpret_cast<ParamT<Ref<TransferFunction>>*>(getParam("transfer_function"))->value;
				assert(transfer_fn);
				ospSetMaterial(geometry, mat);
				ospSet1f(geometry, "radius", radius);
				ospSet1f(geometry, "poll_rate", poll_rate);
				ospSet1i(geometry, "port", port);
				ospSetString(geometry, "server_name", server.c_str());

				transfer_fn->render(ctx);
				if (transfer_fn->getLastCommitted() >= lastCommitted){
					ospSetObject(geometry, "transferFunction", transfer_fn->getOSPHandle());
				}

				ospCommit(geometry);
				lastCommitted = TimeStamp::now();
				ospAddGeometry(ctx.world->ospModel, geometry);
				// Launch a thread that waits for the first worker to send us updated world bounds
				// indicating that they have new data and we're dirty
				if (ospray::mpi::world.rank == 0 && polling_thread.get_id() == std::thread::id()){
					polling_thread = std::thread([&](){
						while (!poller_exit){
							std::cout << "sg::InSituSpheres: MASTER Waiting to recieve world bounds\n";
							MPI_CALL(Recv(&bounds, 6, MPI_FLOAT, 1, 1,
										ospray::mpi::world.comm, MPI_STATUS_IGNORE));
							std::cout << "sg::InSituSpheres: MASTER recieved world bounds: "
								<< bounds << "\n";
							lastModified = TimeStamp::now();
						#if POLL_ONCE
							break;
						#endif
						}
					});
				}
				std::cout << "Geometry added\n";
			} else {
				ospCommit(geometry);
				lastCommitted = TimeStamp::now();
			}
		}
		void InSituSpheres::setTransferFunction(Ref<TransferFunction> fn){
			transfer_fn = fn;
			lastModified = TimeStamp::now();
		}

		OSP_REGISTER_SG_NODE(InSituSpheres);
	}
}

