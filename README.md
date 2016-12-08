OSPRay In Situ Particles Module
===

This is a module for the [OSPRay ray tracer](http://www.ospray.org/) and library for simulations
that implements the in situ particle rendering system described in the paper
[W. Usher, I. Wald, A. Knoll, M. Papka, V. Pascucci. "In Situ Exploration of Particle Simulations with CPU Ray Tracing",
Supercomputing Frontiers and Innovations, 2016](http://sci.utah.edu/~will/papers/in_situ_particles/in_situ_particles.pdf).

## Building the In Situ Library

To integrate the code in your simulation and build the rendering client in OSPRay you first need to build
the in situ data management library under `libIS/`. This will build the simulation side library `lib_is_sim`
and the client side library `lib_is_render` which you can use to integrate into your simulation and make a
rendering client respectively. Examples are provided in `libIS/test_sim.cpp` and `libIS/test_render.cpp`.

## Building the In Situ Rendering Client

We also provide an in situ particle rendering client built using `lib_is_render` which connects to simulations
using `lib_is_sim` and renders the particle data using [P-k-d trees](https://github.com/ingowald/ospray-module-pkd).
The P-k-d code has some modifications to extend it to a data-distributed setting along with a renderer based on
OSPRay's existing data-distributed volume renering. This code is built as a regular OSPRay module, to test it you
can run the test sim and connect using the module. OSPRay must also be built with app scripting enabled to
use the in situ particles code in the existing glut viewer.

The client requires v1.1.2 of OSPRay due to some recent reworking of the way devices are handled changing the
MPI device and distributed rendering code in OSPRay some.

### Running

Once the module is built you can run the test simulation and connect to it in the GLUT viewer. Once
running the simulation library will listen on port 29374, which you can then connect to and query
using functions exposed to the viewer scripting. First launch the GLUT viewer and load the in situ
particles module and use its renderer. In this case we run two worker processes and split the world in
a `2x1x1` grid with the `OSPRAY_DATA_PARALLEL` environment variable, so each worker will get half the world's
data to render. Note that you should not specify the `isp` renderer on the command line as it doesn't support
scenes without in situ particle data, so after creating the geometry you can make and set the renderer.

```
OSPRAY_DATA_PARALLEL=2x1x1 mpirun -np 3 ./ospGlutViewer \
  --module in_situ_particles --osp:mpi
```

To poll a timestep once from the simulation enter the following in the viewer's scripting console:
```js
var isp_renderer = Renderer("isp");
var bounds = box3f();
var iss = ispPollOnce(isp_renderer, "localhost", 29374, 0.015, bounds);
m.addGeometry(iss);

isp_renderer.set("camera", c);
isp_renderer.set("model", m);
// Note: This MUST be run as a single line so the previous renderer
// which doesn't support the ISP geometry doesn't see a partial/invalid
// world state.
isp_renderer.commit(); m.commit(); setRenderer(isp_renderer);

setWorldBounds(bounds);
print(bounds);
refresh();
```

The function `ispPollOnce` requests a single timestep from the simulation. Another function
`ispPollSim` is also available which will repeatedly get timesteps from the simulation and call a user provided
callback. Note that this requires a missing manual script locking feature in OSPRay which I implemented on a fork
for this project, the feature still works but cannot be used from a script. This is shown in
`insituspheres.chai`, which you can pass as a start script to the GLUT viewer
instead of typing the previous commands interactively:

```
OSPRAY_DATA_PARALLEL=2x1x1 mpirun -np 3 ./ospGlutViewer \
  --module in_situ_particles --osp:mpi --script insituparticles.chai
```

