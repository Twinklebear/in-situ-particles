#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include "sg/geometry/Geometry.h"

namespace ospray {
	namespace sg {
		struct InSituSpheres : public sg::Geometry {
			float radius;
			OSPGeometry geometry;
			box3f bounds;
			std::atomic<bool> poller_exit;
			std::thread polling_thread;

			InSituSpheres();
			~InSituSpheres();
			box3f getBounds();
			void render(RenderContext &ctx);
		};
	}
}

