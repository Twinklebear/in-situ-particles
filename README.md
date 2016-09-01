
Reference for making the loader script to be used instead
```c++
// TODO: Leaving this in for testing but will eventually remove for scripting
// the module in the glut viewer
std::cout << "InSituSpheres usage: --iss <server> <port> <sphere radius> <poll rate (s)>\n";
// Run InSituSpheres rendering
const char *server = argv[++argID];
const int port = atoi(argv[++argID]);
const float radius = atof(argv[++argID]);
const float poll_rate = atof(argv[++argID]);
Ref<sg::TransferFunction> fn = new sg::TransferFunction();
fn->name = "Attribute Transfer Function";
world->node.push_back(sg::loadInSituSpheres(server, port, radius, poll_rate, fn));
world->node.push_back(fn.cast<sg::Node>());
```

