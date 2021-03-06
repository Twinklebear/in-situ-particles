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

// ospray
#include "math/vec.ih"
#include "math/box.ih"
#include "common/Ray.ih"
#include "common/Model.ih"
#include "geometry/Geometry.ih"
#include "transferFunction/LinearTransferFunction.ih"
// this module
#include "PKDGeometry.ih"
// embree
#include "embree2/rtcore.isph"
#include "embree2/rtcore_scene.isph"
#include "embree2/rtcore_geometry_user.isph"

static void PartiKDGeometry_postIntersect(uniform Geometry *uniform geometry,
                                          uniform Model *uniform model,
                                          varying DifferentialGeometry &dg,
                                          const varying Ray &ray,
                                          uniform int64 flags)
{
  uniform PartiKDGeometry *uniform THIS = (uniform PartiKDGeometry *uniform)geometry;
  dg.geometry = geometry;
  dg.material = geometry->material;
  dg.P = ray.org + ray.t * ray.dir;
  dg.Ng = dg.Ns = ray.Ng;
  dg.material = NULL;

  if ((flags & DG_COLOR) && (THIS->attribute != NULL) && (THIS->transferFunction != NULL)) {
    uniform float *uniform attribArray = THIS->attribute;
    const uniform float attrib_lo = THIS->attr_lo;
    const uniform float attrib_hi = THIS->attr_hi;

    float attrib_org;
    uint64 primID64 = (uint32)ray.primID_hi64;
    primID64 <<= 32;
    primID64 += (uint32)ray.primID;
    foreach_unique(pID in primID64) {
      uniform float *uniform ptr = attribArray + pID;
      attrib_org = ptr[0];
    }
    const float attrib
      = (attrib_org - attrib_lo)
      * rcp(attrib_hi - attrib_lo + 1e-10f);
    // if (attrib >= 1.f || attrib <= 0.f) 
    //   print("ATTRIB OUT OF RANGE:\n org %\n remapped %\n ID %:%\n range % %\n",
    //         attrib_org,attrib,primID64,ray.primID,attrib_lo,attrib_hi);
    // compute alpha value from attribute value
    const float alpha = THIS->transferFunction->getOpacityForValue(THIS->transferFunction,
                                                                   attrib);
    const vec3f color = THIS->transferFunction->getColorForValue(THIS->transferFunction,
                                                                 attrib);
    dg.color = make_vec4f(color.x,color.y,color.z,alpha);
  }

// some useful combinations; enums unfortunately don't work :-(
#define  DG_NG_FACEFORWARD (DG_NG | DG_FACEFORWARD)
#define  DG_NS_FACEFORWARD (DG_NS | DG_FACEFORWARD)
#define  DG_NG_NORMALIZE   (DG_NG | DG_NORMALIZE)
#define  DG_NS_NORMALIZE   (DG_NS | DG_NORMALIZE)

  if ((flags & DG_NG_NORMALIZE) == DG_NG_NORMALIZE)
    dg.Ng = safe_normalize(dg.Ng);
  if ((flags & DG_NS_NORMALIZE) == DG_NS_NORMALIZE)
    dg.Ns = safe_normalize(dg.Ns);
  
  if ((flags & DG_NG_FACEFORWARD) == DG_NG_FACEFORWARD)
    if (dot(ray.dir,dg.Ng) >= 0.f) dg.Ng = neg(dg.Ng);
  if ((flags & DG_NS_FACEFORWARD) == DG_NS_FACEFORWARD)
    if (dot(ray.dir,dg.Ns) >= 0.f) dg.Ns = neg(dg.Ns);

#undef  DG_NG_FACEFORWARD
#undef  DG_NS_FACEFORWARD
#undef  DG_NG_NORMALIZE
#undef  DG_NS_NORMALIZE
}

unmasked void PartiKDGeometry_bounds(uniform PartiKDGeometry *uniform geometry,
                            uniform size_t primID,
                            uniform box3fa &bbox)
{
  bbox = make_box3fa(geometry->sphereBounds.lower,geometry->sphereBounds.upper);
}


/*! creates a new pkd geometry */
export void *uniform PartiKDGeometry_create(void           *uniform cppEquivalent)
{
  uniform PartiKDGeometry *uniform geom = uniform new uniform PartiKDGeometry;
  Geometry_Constructor(&geom->geometry,cppEquivalent,
                       PartiKDGeometry_postIntersect,
                       NULL,0,NULL);
  return geom;
}

/*! helper function that recomputes the bin-bits from the transfer
    function for parameter range culling. note this function does
    _NOT_ change anything in the tree itself, it only re-bins the
    transfer function*/
export void PartiKDGeometry_updateTransferFunction(void *uniform _THIS,
                                                   void *uniform _transferFunction)
{
  PartiKDGeometry *uniform THIS = (PartiKDGeometry *uniform)_THIS;
  TransferFunction *uniform transferFunction
    = (TransferFunction *uniform)_transferFunction;
  THIS->transferFunction_activeBinBits = 0;
  for (uniform int i=0;i<32;i++) {
    uniform float a0 = i/32.f;
    uniform float a1 = (i+1)/32.f - 1e-5f;
    vec2f range = make_vec2f(a0,a1);
    uniform float alphaRange
      = extract(transferFunction->getMaxOpacityInRange(transferFunction,range),0);
    if (alphaRange >= .5f)
      THIS->transferFunction_activeBinBits |= (1UL << i);
  }
}

/*! 'constructor' for a newly created pkd geometry */
export void PartiKDGeometry_set(void       *uniform _geom,
                                void           *uniform _model,
                                uniform bool isQuantized,
                                uniform bool useSPMD,
                                void           *uniform transferFunction,
                                float           uniform particleRadius,
                                uniform uint64  numParticles,
                                uniform uint64  numInnerNodes,
                                PKDParticle    *uniform particle,
                                float          *uniform attribute,
                                uint32         *uniform innerNode_attributeMask,
                                uniform box3f &sphereBounds,
                                uniform float attr_lo, 
                                uniform float attr_hi)
{
  uniform PartiKDGeometry *uniform geom = (uniform PartiKDGeometry *uniform)_geom;
  uniform Model *uniform model = (uniform Model *uniform)_model;

  uniform uint32 geomID = rtcNewUserGeometry(model->embreeSceneHandle,1);
  
  geom->geometry.model  = model;
  geom->isQuantized     = isQuantized;
  geom->geometry.geomID = geomID;
  geom->particleRadius  = particleRadius;
  geom->particle        = particle;
  geom->numParticles    = numParticles;
  geom->numInnerNodes   = numInnerNodes;
  geom->sphereBounds    = sphereBounds;
  geom->attribute       = attribute;
  geom->attr_lo         = attr_lo;
  geom->attr_hi         = attr_hi;
  geom->innerNode_attributeMask   = innerNode_attributeMask;

  geom->transferFunction  = (TransferFunction *uniform)transferFunction;

  rtcSetUserData(model->embreeSceneHandle,geomID,geom);
  rtcSetBoundsFunction(model->embreeSceneHandle,geomID,
                       (uniform RTCBoundsFunc)&PartiKDGeometry_bounds);

  if (useSPMD) {
    rtcSetIntersectFunction
      (model->embreeSceneHandle,geomID,
       (uniform RTCIntersectFuncVarying)&PartiKDGeometry_intersect_spmd);
    rtcSetOccludedFunction
      (model->embreeSceneHandle,geomID,
       (uniform RTCOccludedFuncVarying)&PartiKDGeometry_occluded_spmd);
  } else {
    rtcSetIntersectFunction
      (model->embreeSceneHandle,geomID,
       (uniform RTCIntersectFuncVarying)&PartiKDGeometry_intersect_packet);
    rtcSetOccludedFunction
      (model->embreeSceneHandle,geomID,
       (uniform RTCOccludedFuncVarying)&PartiKDGeometry_occluded_packet);
  }
  if (transferFunction) 
    PartiKDGeometry_updateTransferFunction(geom,transferFunction);
}
