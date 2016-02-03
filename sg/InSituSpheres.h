#pragma once

#include <vector>
#include "sg/geometry/Geometry.h"

namespace ospray {
	namespace sg {
		struct InSituSpheres : public sg::Geometry {
			float radius;
			OSPGeometry geometry;

			InSituSpheres();
			~InSituSpheres();
			box3f getBounds();
			void render(RenderContext &ctx);
		};
	}
}

