#include <iostream>
#include "libIS/is_render.h"
#include "sg/module/Module.h"
#include "sg/common/Integrator.h"
#include "InSituSpheres.h"

namespace ospray {
	namespace sg {
		InSituSpheres::InSituSpheres() : Geometry("InSituSpheres"), radius(0.01f), positionData(NULL), geometry(NULL)
		{}
		InSituSpheres::~InSituSpheres(){
			if (geometry){
				ospRelease(geometry);
				geometry = NULL;
			}
		}
		box3f InSituSpheres::getBounds(){
			box3f bounds = embree::empty;
			for (size_t i = 0; i < particles.size(); ++i){
				bounds.extend(particles[i]);
			}
			bounds.lower -= vec3f(radius);
			bounds.upper += vec3f(radius);
			std::cout << "bounds = " << bounds << "\n";
			return bounds;
		}
		void InSituSpheres::render(RenderContext &ctx){
			assert(!geometry);
			// Check if data has changed
			if (lastModified <= lastCommitted){
				return;
			}
			if (!geometry){
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
				ospSetMaterial(geometry, mat);
				ospAddGeometry(ctx.world->ospModel, geometry);
			}
			if (!positionData){
				// Fetch the position data from the simulation
				// TODO: Must send these args through from command line
				// How would pulling within _finalize work vs. doing it here?
				const char *servName = "localhost";
				int servPort = 290374;

				DomainGrid *dd = ospIsPullRequest(MPI_COMM_WORLD, const_cast<char*>(servName), servPort, vec3i(1), .01f);

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
							particles = b.particle;
						}
						std::cout << std::flush;
						fflush(0);
						usleep(200);
					}
					MPI_CALL(Barrier(MPI_COMM_WORLD));
				}
				// We've got our positions so now send it to the ospray geometry
				positionData = ospNewData(particles.size(), OSP_FLOAT3, &particles[0], OSP_DATA_SHARED_BUFFER);
				ospCommit(positionData);
				ospSetData(geometry, "InSituSpheres", positionData);
				ospSet1f(geometry, "radius", radius);
				ospCommit(geometry);
				lastCommitted = TimeStamp::now();
			}
		}
		OSP_REGISTER_SG_NODE(InSituSpheres);
	}
}

