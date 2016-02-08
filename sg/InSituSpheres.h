#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include "sg/geometry/Geometry.h"
#include "sg/geometry/Spheres.h"

namespace ospray {
	namespace sg {
		struct InSituSpheres : public sg::Geometry {
			float radius;
			OSPGeometry geometry;
			box3f bounds;
			//! Bool to flag the poller when it should exit
			std::atomic<bool> poller_exit;
			/*! The polling thread watches the render workers for a world bounds
			 * update which indicates the geometry from the simulation has changed */
			std::thread polling_thread;
			//! the transfer function we use for color-mapping the given attribute
			Ref<TransferFunction> transfer_fn;

			InSituSpheres();
			~InSituSpheres();
			box3f getBounds();
			void render(RenderContext &ctx);
			void setTransferFunction(Ref<TransferFunction> transfer_fn);
		};
	}
}

