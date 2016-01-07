#pragma once

#include <vector>
#include "sg/geometry/Geometry.h"

namespace ospray {
	namespace sg {
		struct InSituSpheres : public sg::Geometry {
			float radius;
			OSPGeometry geometry;
			OSPData positionData;
			box3f particleBounds;
			std::vector<vec3f> particles;

			InSituSpheres();
			~InSituSpheres();
			box3f getBounds();
			void render(RenderContext &ctx);
		};
	}
}

