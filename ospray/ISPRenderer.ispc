// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
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

#include "ospray/fb/FrameBuffer.ih"
#include "ospray/camera/Camera.ih"
#include "ospray/common/Ray.ih"
#include "ospray/render/util.ih"
#include "ospray/math/math.ih"
#include "ospray/math/random.ih"
#include "ospray/common/Model.ih"
#include "ospray/render/Renderer.ih"
#include "ospray/render/simpleAO/SimpleAOMaterial.ih"

#include "ospray/PKDGeometry.ih"

struct ISPRenderer {
  uniform Renderer super;
  uniform int samplesPerFrame;
  uniform float aoRayLength;
};

struct DDSpheresBlock {
  // The actual grid domain assigned for this block
  box3f actualDomain;
  int firstOwner;
  int numOwners;
  int isMine;
  int blockID;

  // TODO: The special renderer will know how to deal with the
  // clipping of primary rays against the actual domain, the PKD
  // geometry shouldn't have to care about this at all!
  void *uniform cpp_pkd;
  uniform PartiKDGeometry *uniform ispc_pkd;
};

struct PassInfo {
  // region to integrate over in this pass
  // TODO: This changes to box1f
  //varying box1f region;
  varying region1f region;
  // block to use in this pass - NULL for 'everything other than blocks'
  // TODO: shouldn't this be a uniform DDBVolumeBlock?
  uniform DDSpheresBlock *uniform block;
};

inline vec3f getShadingNormal(const Ray &ray)
{
  vec3f N = ray.Ng;
  float f = rcp(sqrt(dot(N,N)));
  if (dot(N,ray.dir) >= 0.f) f = -f;
  return f*N;
}

void getBinormals(vec3f &biNorm0, vec3f &biNorm1, const vec3f &gNormal)
{
  biNorm0 = make_vec3f(1.f,0.f,0.f);
  if (abs(dot(biNorm0,gNormal)) > .95f)
    biNorm0 = make_vec3f(0.f,1.f,0.f);
  biNorm1 = normalize(cross(biNorm0,gNormal));
  biNorm0 = normalize(cross(biNorm1,gNormal));
}

inline float rotate(float x, float dx)
{
  x += dx;
  if (x >= 1.f) x -= 1.f;
  return x;
}

inline uniform float rotate(uniform float x, uniform float dx)
{
  x += dx;
  if (x >= 1.f) x -= 1.f;
  return x;
}

inline vec3f getRandomDir(varying RandomTEA* uniform rng,
                          const vec3f biNorm0,
                          const vec3f biNorm1,
                          const vec3f gNormal,
                          const float rot_x, const float rot_y,
                          const uniform float epsilon)
{
  const vec2f rn = RandomTEA__getFloats(rng);
  const float r0 = rotate(rn.x, rot_x);
  const float r1 = rotate(rn.y, rot_y);

  const float w = sqrt(1.f-r1);
  const float x = cos((2.f*M_PI)*r0)*w;
  const float y = sin((2.f*M_PI)*r0)*w;
  const float z = sqrt(r1)+epsilon;
  return x*biNorm0+y*biNorm1+z*gNormal;
}

inline void shade_ao(uniform ISPRenderer *uniform self,
                     varying vec3f &color,
                     varying float &alpha,
                     int sampleCnt,
                     const Ray &ray,
                     const int32 pixel_x,
                     const int32 pixel_y,
                     const float rot_x,
                     const float rot_y,
                     void *uniform perFrameData)
{
  if (ray.geomID < 0) {
    color = self->super.backgroundEnabled ? self->super.bgColor :
                                            make_vec3f(0.f);
    alpha = self->super.backgroundEnabled ? 1.f : 0.f;
    return;
  }
  uniform PassInfo *uniform passInfo = (uniform PassInfo *uniform)perFrameData;

  DifferentialGeometry dg;
  if (passInfo) {
    PartiKDGeometry_postIntersect((uniform Geometry *uniform)passInfo->block->ispc_pkd,
        self->super.model, dg, ray,
        DG_NG|DG_NS|DG_NORMALIZE|DG_FACEFORWARD|DG_COLOR|DG_TEXCOORD);
  } else {
    postIntersect(self->super.model,dg,ray,
                  DG_NG|DG_NS|DG_NORMALIZE|DG_FACEFORWARD
                  |DG_MATERIALID|DG_COLOR|DG_TEXCOORD);
  }

  uniform SimpleAOMaterial *mat = ((uniform SimpleAOMaterial*)dg.material);
  vec3f superColor = make_vec3f(1.f);
  if (mat) {
    foreach_unique(m in mat) {
      superColor = m->Kd;
      if (m->map_Kd) {
        vec4f Kd_from_map = get4f(m->map_Kd,dg.st);
        superColor = superColor * make_vec3f(Kd_from_map);
      }
    }
  }
  // should be done in material:
  superColor = superColor * make_vec3f(dg.color);

  // init TEA RNG //
  uniform FrameBuffer *uniform fb = self->super.fb;
  RandomTEA rng_state;
  varying RandomTEA* const uniform rng = &rng_state;
  RandomTEA__Constructor(rng, (fb->size.x * pixel_y) + pixel_x, fb->accumID);

  int hits = 0;
  vec3f biNormU,biNormV;
  const vec3f N = dg.Ns;
  getBinormals(biNormU,biNormV,N);

  for (uniform int i = 0; i < sampleCnt; i++) {
    const vec3f ao_dir = getRandomDir(rng, biNormU, biNormV, N, rot_x,
                                      rot_y,self->super.epsilon);

    Ray ao_ray;
    setRay(ao_ray, dg.P, ao_dir);
    ao_ray.t0 = self->super.epsilon;
    ao_ray.t  = self->aoRayLength - self->super.epsilon;
    // TODO WILL: Reducing this from 0.05 to 0.015 helps reduce the shadowing on the Uintah model
    /*
    if (dot(ao_ray.dir, N) < 0.05f) {
      ++hits;
    }
    */
    if (passInfo) {
      PartiKDGeometry_occluded_packet(passInfo->block->ispc_pkd, ao_ray, passInfo->block->blockID);
      if (ao_ray.geomID >= 0) {
        ++hits;
      }
    }
    /*
    else if (isOccluded(self->super.model,ao_ray)) {
      ++hits;
    }
    */
  }

  const float diffuse = absf(dot(N, ray.dir));
  //superColor = (N + make_vec3f(1.f)) / 2.f;
  color = superColor * make_vec3f(diffuse) * (1.0f - (float)hits/sampleCnt);
  alpha = 1.f;
  return;
}

void ISPRenderer_renderSample(uniform Renderer *uniform _self,
                           void *uniform perFrameData,
                           varying ScreenSample &sample)
{
  uniform ISPRenderer *uniform self = (uniform ISPRenderer *uniform)_self;

  if (perFrameData) {
    uniform PassInfo *uniform passInfo = (uniform PassInfo *uniform)perFrameData;
    //sample.ray.t0 = passInfo->region.lower;
    //sample.ray.t = passInfo->region.upper;
    PartiKDGeometry_intersect_packet(passInfo->block->ispc_pkd, sample.ray, passInfo->block->blockID);
  } else {
    traceRay(self->super.model, sample.ray);
  }
  sample.z = sample.ray.t;
  uniform int samplesPerFrame = self->samplesPerFrame;
  uniform int framesPer16 = 16 / samplesPerFrame;
  int accumID = sample.sampleID.z / framesPer16;
  float rot_x = 1.f - precomputedHalton3(accumID);
  float rot_y = 1.f - precomputedHalton5(accumID);
  shade_ao(self,
      sample.rgb,
      sample.alpha,
      samplesPerFrame,
      sample.ray,
      sample.sampleID.x,
      sample.sampleID.y,
      rot_x,rot_y,
      perFrameData);
}

inline uniform bool shouldRenderBlock(DDSpheresBlock *uniform block,
                                      uniform int32 tileID,
                                      uniform int32 myRank)
{
  return block->isMine && (myRank - block->firstOwner) == (tileID % block->numOwners);
}

extern "C" Tile *uniform ISPCacheForTiles_getTile(void *uniform cache, uniform int32 blockID);

void ISPRenderer_renderSample(uniform ISPRenderer *uniform self,
                              ScreenSample &fgSample,
                              ScreenSample &bgSample,
                              void *uniform _tileCache,
                              uint32 pixelID,
                              uniform int numBlocks,
                              DDSpheresBlock *uniform block,
                              bool *uniform tileNeedsBlock,
                              uniform int32 tileID,
                              uniform int32 myRank,
                              uniform bool isMyTile)
{
  //box1f blockRegion = make_box1f(+inf,-inf);
  region1f blockRegion = make_region1f(+inf,-inf);
  float org_ray_t0 = fgSample.ray.t0;
  float org_ray_t1 = fgSample.ray.t;
  uniform int numBlocksMine = 0;
  for (uniform int blockID = 0; blockID < numBlocks; ++blockID) {
    float t0 = org_ray_t0;
    float t1 = org_ray_t1;
    intersectBox(fgSample.ray, block[blockID].actualDomain, t0, t1);
    if (t0 >= t1) {
      // ray does not intersect this block...
    } else {
      // ray hits this block, tile will need it rendered
      tileNeedsBlock[blockID] = true;
      blockRegion.lower = min(blockRegion.lower, t0);
      blockRegion.upper = max(blockRegion.upper, t1);

      // now, render this block if it's one of ours
      if (!block[blockID].isMine) {
        continue;
      }
      if (shouldRenderBlock(&block[blockID], tileID, myRank)) {
        Tile *uniform blockTile = ISPCacheForTiles_getTile(_tileCache, blockID);

        // -------------------------------------------------------
        // set up pass for 'this block'
        // -------------------------------------------------------
        uniform PassInfo passInfo;
        // set integration pass up for region from (at most) ray origin to
        // start of blocks
        //passInfo.region = make_box1f(t0,t1);
        passInfo.region = make_region1f(t0,t1);
        // do not use any block in this pass
        passInfo.block = &block[blockID];
        ++numBlocksMine;

        ISPRenderer_renderSample((uniform Renderer *uniform)self, &passInfo, fgSample);

        fgSample.ray.t0 = org_ray_t0;
        fgSample.ray.t  = org_ray_t1;
        setRGBAZ(*blockTile,pixelID,fgSample.rgb,fgSample.alpha,fgSample.z);
      }
    }
  }

  if (!isMyTile) return;

  // If we own the tile also report the background color, since we have no
  // replicated geometry in this renderer we don't need to send a foreground
  // tile at all.
  bgSample.rgb = make_vec3f(0.f,0.f,0.f);
  bgSample.z = inf;
  bgSample.alpha = 0.f;
  if (self->super.backgroundEnabled) {
    bgSample.rgb = self->super.bgColor;
  }
}

export void ISPRendererDataDistrib_renderTile(void *uniform _self,
                                              uniform Tile &bgTile,
                                              void *uniform _tileCache,
                                              uniform int numBlocks,
                                              void *uniform _block,
                                              uniform bool *uniform tileNeedsBlock,
                                              uniform int32 tileID,
                                              uniform int32 myRank,
                                              uniform bool isMyTile,
                                              uniform int taskIndex)
{
  uniform Renderer *uniform self = (uniform Renderer *uniform)_self;
  DDSpheresBlock *uniform block = (DDSpheresBlock *uniform)_block;

  uniform FrameBuffer *uniform fb     = self->fb;
  uniform Camera      *uniform camera = self->camera;

  float pixel_du = .5f, pixel_dv = .5f;
  float lens_du = 0.f,  lens_dv = 0.f;

  ScreenSample fgSample, bgSample;
  fgSample.z = inf;
  fgSample.alpha = 0.f;

  CameraSample cameraSample;

  const uniform int begin = taskIndex * RENDERTILE_PIXELS_PER_JOB;
  const uniform int end   = begin     + RENDERTILE_PIXELS_PER_JOB;
  const uniform int startSampleID = max(fb->accumID,0);

  for (uniform uint32 i=begin;i<end;i+=programCount) {
    const uint32 index = i + programIndex;
    fgSample.sampleID.x        = bgTile.region.lower.x + z_order.xs[index];
    fgSample.sampleID.y        = bgTile.region.lower.y + z_order.ys[index];

    if ((fgSample.sampleID.x >= fb->size.x) |
        (fgSample.sampleID.y >= fb->size.y))
      continue;

    const uint32 pixel = z_order.xs[index] + (z_order.ys[index] * TILE_SIZE);

    pixel_du = precomputedHalton2(startSampleID);
    pixel_dv = precomputedHalton3(startSampleID);
    fgSample.sampleID.z = startSampleID;

    cameraSample.screen.x = (fgSample.sampleID.x + pixel_du) * fb->rcpSize.x;
    cameraSample.screen.y = (fgSample.sampleID.y + pixel_dv) * fb->rcpSize.y;

    camera->initRay(camera,fgSample.ray,cameraSample);

    bgSample = fgSample;
    ISPRenderer_renderSample((ISPRenderer *uniform)self,
                      fgSample, bgSample, _tileCache, pixel,
                      numBlocks,block,tileNeedsBlock,tileID,myRank,isMyTile);

    setRGBAZ(bgTile,pixel,bgSample.rgb,bgSample.alpha,bgSample.z);
  }
}

export void *uniform ISPRenderer_create(void *uniform cppE,
                                     void *uniform _model,
                                     void *uniform _camera)
{
  uniform ISPRenderer *uniform self = uniform new uniform ISPRenderer;
  Renderer_Constructor(&self->super, cppE, _model, _camera, 1);
  self->super.renderSample = ISPRenderer_renderSample;
  return self;
}

export void ISPRenderer_set(void *uniform _self,
                         uniform int samplesPerFrame,
                         uniform float aoRayLength)
{
  uniform ISPRenderer *uniform self = (uniform ISPRenderer *uniform)_self;
  self->samplesPerFrame = samplesPerFrame;
  self->aoRayLength = aoRayLength;
}

