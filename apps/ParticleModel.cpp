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

#include "ParticleModel.h"

namespace ospray {
  /*! helper function that creates a pseudo-random color for a given
      ID; this is used to generate initial colors for atom types, if
      the model file doesn't specify these colors explicitly */
  inline vec3f makeRandomColor(const int i)
  {
    const int mx = 13*17*43;
    const int my = 11*29;
    const int mz = 7*23*63;
    const uint32 g = (i * (3*5*127)+12312314);
    return vec3f((g % mx)*(1.f/(mx-1)),
                 (g % my)*(1.f/(my-1)),
                 (g % mz)*(1.f/(mz-1)));
  }

  uint32 ParticleModel::getAtomTypeID(const std::string &name)
  {
    if (atomTypeByName.find(name) == atomTypeByName.end()) {
      if (name != "Default") std::cout << "New atom type '"+name+"'" << std::endl;
      ParticleModel::AtomType *a = new ParticleModel::AtomType(name);
      a->color = makeRandomColor(atomType.size());
      atomTypeByName[name] = atomType.size();
      atomType.push_back(a);
    }
    return atomTypeByName[name];
  }

  //! helper function for parser error recovery: 'clamp' all attributes to largest non-empty attribute
  void ParticleModel::cullPartialData() 
  {
    size_t largestCompleteSize = position.size();
    for (std::vector<Attribute *>::const_iterator it=attribute.begin();
         it != attribute.end();it++)
      largestCompleteSize = std::min(largestCompleteSize,(*it)->value.size());
    
    if (position.size() > largestCompleteSize) {
      std::cout << "#osp:uintah: atoms w missing attribute(s): discarding" << std::endl;
      position.resize(largestCompleteSize);
    }
    if (type.size() > largestCompleteSize) {
      std::cout << "#osp:uintah: atoms w missing attribute(s): discarding" << std::endl;
      type.resize(largestCompleteSize);
    }
    for (std::vector<Attribute *>::const_iterator it=attribute.begin();
         it != attribute.end();it++) {
      if ((*it)->value.size() > largestCompleteSize) {
        std::cout << "#osp:uintah: attribute(s) w/o atom(s): discarding" << std::endl;
        (*it)->value.resize(largestCompleteSize);
      }
    }
  }

  //! return world bounding box of all particle *positions* (i.e., particles *ex* radius)
  box3f ParticleModel::getBounds() const
  {
    box3f bounds = ospcommon::empty;
    for (size_t i=0;i<position.size();i++)
      bounds.extend(position[i]);
    return bounds;
  }

  //! get attributeset of given name; create a new one if not yet exists */
  ParticleModel::Attribute *ParticleModel::getAttribute(const std::string &name)
  {
    for (int i=0;i<attribute.size();i++)
      if (attribute[i]->name == name) return attribute[i];
    attribute.push_back(new Attribute(name));
    return attribute[attribute.size()-1];
  }
  
  //! return if attribute of this name exists 
  bool ParticleModel::hasAttribute(const std::string &name)
  {
    for (int i=0;i<attribute.size();i++)
      if (attribute[i]->name == name) return true;
    return false;
  }
  
  //! add one attribute value to set of attributes of given name
  void ParticleModel::addAttribute(const std::string &name, float value)
  {
    ParticleModel::Attribute *a = getAttribute(name);
    a->value.push_back(value);
    a->minValue = std::min(a->minValue,value);
    a->maxValue = std::max(a->maxValue,value);
  }

}
