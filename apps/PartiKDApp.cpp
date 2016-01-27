// ======================================================================== //
// Copyright 2009-2014 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "PartiKD.h"

using std::endl;
using std::cout;

namespace ospray {

  void partiKDMain(int ac, char **av)
  {
    std::vector<std::string> input;
    std::string output, outputQuantized;
    ParticleModel model;
    bool roundRobin = false;

    for (int i=1;i<ac;i++) {
      std::string arg = av[i];
      if (arg[0] == '-') {
        if (arg == "-o") {
          output = av[++i];
        } else if (arg == "--radius") {
          model.radius = atof(av[++i]);
        } else if (arg == "--quantize") {
          if (i+1 >= ac || av[i+1][0] == '-')
            throw std::runtime_error("no filename passed to '--quantize'");
          outputQuantized = av[++i];
        } else if (arg == "--round-robin") {
          roundRobin = true;
        } else {
          throw std::runtime_error("unknown parameter '"+arg+"'");
        }
      } else {
        input.push_back(arg);
      }
    }
    if (input.empty()) {
      throw std::runtime_error("no input file(s) specified");
    }
    if (output == "")
      throw std::runtime_error("no output file specified");
    
    if (model.radius == 0.f)
      std::cout << "#osp:pkd: no radius specified on command line" << std::endl;

    // load the input(s)
    for (int i=0;i<input.size();i++) {
      cout << "#osp:pkd: loading " << input[i] << endl;
      model.load(input[i]);
    }

    if (model.radius == 0.f) {
      throw std::runtime_error("no radius specified via either command line or model file");
    }

    double before = getSysTime();
    std::cout << "#osp:pkd: building tree ..." << std::endl;
    PartiKD partiKD(roundRobin);
    partiKD.build(&model);
    double after = getSysTime();
    std::cout << "#osp:pkd: tree built (" << (after-before) << " sec)" << std::endl;

    std::cout << "#osp:pkd: writing binary data to " << output << endl;
    partiKD.saveOSP(output);
    if (outputQuantized != "") {
      std::cout << "#osp:pkd: writing QUANTIZED binary data to " << outputQuantized << endl;
      partiKD.saveOSPQuantized(outputQuantized);
    }

    std::cout << "#osp:pkd: done." << endl;
  }
}

int main(int ac, char **av)
{
  try {
    ospray::partiKDMain(ac,av);
  } catch (std::runtime_error(e)) {
    cout << "#osp:pkd (fatal): " << e.what() << endl;
    cout << "usage:" << endl;
    cout << "./ospPartiKD <inputfile(s)> -o output.pkd [--round-robin] [--quantize quantized.pkd]\n" << endl;
    
  }
}

