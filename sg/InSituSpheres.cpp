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
				std::cout << "Geometry added\n";
			}
		}
		OSP_REGISTER_SG_NODE(InSituSpheres);
	}
}

