#pragma once

#include <vector>
#include <atomic>
#include "sg/geometry/Geometry.h"

namespace ospray {
	namespace sg {
		struct InSituSpheres : public sg::Geometry {
			float radius;
			OSPGeometry geometry;
			std::atomic<bool> have_world_bounds;
			box3f bounds;

			InSituSpheres();
			~InSituSpheres();
			box3f getBounds();
			void render(RenderContext &ctx);
		};
	}
}

