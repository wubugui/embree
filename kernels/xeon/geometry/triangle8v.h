// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
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

#pragma once

#include "primitive.h"

namespace embree
{
  /*! Precalculated representation for 8 triangles. Stores for each
      triangle a base vertex, two edges, and the geometry normal to
      speed up intersection calculations. */
  struct Triangle8v
  {
#if defined __AVX__
    typedef avxb simdb;
    typedef avxf simdf;
    typedef avxi simdi;
#endif

  public:
    struct Type : public PrimitiveType 
    {
      Type ();
      size_t size(const char* This) const;
    };
    static Type type;

public:

    /*! returns maximal number of stored triangles */
    static __forceinline size_t max_size() { return 8; }
    
     /*! returns required number of primitive blocks for N primitives */
    static __forceinline size_t blocks(size_t N) { return (N+max_size()-1)/max_size(); }
   

#if defined __AVX__
  public:

    /*! Default constructor. */
    __forceinline Triangle8v () {}

    /*! Construction from vertices and IDs. */
    __forceinline Triangle8v (const avx3f& v0, const avx3f& v1, const avx3f& v2, const avxi& geomIDs, const avxi& primIDs)
      : v0(v0), v1(v1), v2(v2),  geomIDs(geomIDs), primIDs(primIDs) {}

    /*! Returns a mask that tells which triangles are valid. */
    __forceinline avxb valid() const { return geomIDs != avxi(-1); }

    /*! Returns if the specified triangle is valid. */
    __forceinline bool valid(const size_t i) const { assert(i<8); return geomIDs[i] != -1; }

    /*! Returns the number of stored triangles. */
    __forceinline unsigned int size() const { return __bsf(~movemask(valid())); }

    /*! returns the geometry IDs */
    __forceinline avxi geomID() const { return geomIDs; }
    __forceinline int  geomID(const size_t i) const { assert(i<8); return geomIDs[i]; }

    /*! returns the primitive IDs */
    __forceinline avxi primID() const { return primIDs; }
    __forceinline int  primID(const size_t i) const { assert(i<8); return primIDs[i]; }

    /*! calculate the bounds of the triangle */
    __forceinline BBox3fa bounds() const 
    {
      avx3f p0 = v0;
      avx3f p1 = v1;
      avx3f p2 = v2;
      avx3f lower = min(p0,p1,p2);
      avx3f upper = max(p0,p1,p2);
      avxb mask = valid();
      lower.x = select(mask,lower.x,avxf(pos_inf));
      lower.y = select(mask,lower.y,avxf(pos_inf));
      lower.z = select(mask,lower.z,avxf(pos_inf));
      upper.x = select(mask,upper.x,avxf(neg_inf));
      upper.y = select(mask,upper.y,avxf(neg_inf));
      upper.z = select(mask,upper.z,avxf(neg_inf));
      return BBox3fa(Vec3fa(reduce_min(lower.x),reduce_min(lower.y),reduce_min(lower.z)),
                     Vec3fa(reduce_max(upper.x),reduce_max(upper.y),reduce_max(upper.z)));
    }

    /*! non temporal store */
    __forceinline static void store_nt(Triangle8v* dst, const Triangle8v& src)
    {
      store8f_nt(&dst->v0.x,src.v0.x);
      store8f_nt(&dst->v0.y,src.v0.y);
      store8f_nt(&dst->v0.z,src.v0.z);
      store8f_nt(&dst->v1.x,src.v1.x);
      store8f_nt(&dst->v1.y,src.v1.y);
      store8f_nt(&dst->v1.z,src.v1.z);
      store8f_nt(&dst->v2.x,src.v2.x);
      store8f_nt(&dst->v2.y,src.v2.y);
      store8f_nt(&dst->v2.z,src.v2.z);
      store8i_nt(&dst->geomIDs,src.geomIDs);
      store8i_nt(&dst->primIDs,src.primIDs);
    }

    /*! fill triangle from triangle list */
    __forceinline void fill(atomic_set<PrimRefBlock>::block_iterator_unsafe& prims, Scene* scene, const bool list)
    {
      avxi vgeomID = -1, vprimID = -1;
      avx3f v0 = zero, v1 = zero, v2 = zero;
      
      for (size_t i=0; i<8 && prims; i++, prims++)
      {
	const PrimRef& prim = *prims;
	const size_t geomID = prim.geomID();
        const size_t primID = prim.primID();
        const TriangleMesh* __restrict__ const mesh = scene->getTriangleMesh(geomID);
        const TriangleMesh::Triangle& tri = mesh->triangle(primID);
        const Vec3fa& p0 = mesh->vertex(tri.v[0]);
        const Vec3fa& p1 = mesh->vertex(tri.v[1]);
        const Vec3fa& p2 = mesh->vertex(tri.v[2]);
        vgeomID [i] = geomID;
        vprimID [i] = primID;
        v0.x[i] = p0.x; v0.y[i] = p0.y; v0.z[i] = p0.z;
        v1.x[i] = p1.x; v1.y[i] = p1.y; v1.z[i] = p1.z;
        v2.x[i] = p2.x; v2.y[i] = p2.y; v2.z[i] = p2.z;
      }
      Triangle8v::store_nt(this,Triangle8v(v0,v1,v2,vgeomID,vprimID));
    }

    /*! fill triangle from triangle list */
    __forceinline void fill(const PrimRef* prims, size_t& begin, size_t end, Scene* scene, const bool list)
    {
      avxi vgeomID = -1, vprimID = -1;
      avx3f v0 = zero, v1 = zero, v2 = zero;
      
      for (size_t i=0; i<8 && begin<end; i++, begin++)
      {
	const PrimRef& prim = prims[begin];
        const size_t geomID = prim.geomID();
        const size_t primID = prim.primID();
        const TriangleMesh* __restrict__ const mesh = scene->getTriangleMesh(geomID);
        const TriangleMesh::Triangle& tri = mesh->triangle(primID);
        const Vec3fa& p0 = mesh->vertex(tri.v[0]);
        const Vec3fa& p1 = mesh->vertex(tri.v[1]);
        const Vec3fa& p2 = mesh->vertex(tri.v[2]);
        vgeomID [i] = geomID;
        vprimID [i] = primID;
        v0.x[i] = p0.x; v0.y[i] = p0.y; v0.z[i] = p0.z;
        v1.x[i] = p1.x; v1.y[i] = p1.y; v1.z[i] = p1.z;
        v2.x[i] = p2.x; v2.y[i] = p2.y; v2.z[i] = p2.z;
      }
      Triangle8v::store_nt(this,Triangle8v(v0,v1,v2,vgeomID,vprimID));
    }

    /*! updates the primitive */
    __forceinline BBox3fa update(TriangleMesh* mesh)
    {
      BBox3fa bounds = empty;
      avxi vgeomID = -1, vprimID = -1;
      avx3f v0 = zero, v1 = zero, v2 = zero;
      
      for (size_t i=0; i<8; i++)
      {
        if (primID(i) == -1) break;
        const unsigned geomId = geomID(i);
        const unsigned primId = primID(i);
        const TriangleMesh::Triangle& tri = mesh->triangle(primId);
        const Vec3fa p0 = mesh->vertex(tri.v[0]);
        const Vec3fa p1 = mesh->vertex(tri.v[1]);
        const Vec3fa p2 = mesh->vertex(tri.v[2]);
        bounds.extend(merge(BBox3fa(p0),BBox3fa(p1),BBox3fa(p2)));
        vgeomID [i] = geomId;
        vprimID [i] = primId;
        v0.x[i] = p0.x; v0.y[i] = p0.y; v0.z[i] = p0.z;
        v1.x[i] = p1.x; v1.y[i] = p1.y; v1.z[i] = p1.z;
        v2.x[i] = p2.x; v2.y[i] = p2.y; v2.z[i] = p2.z;
      }
      Triangle8v::store_nt(this,Triangle8v(v0,v1,v2,vgeomID,vprimID));
      return bounds;
    }

  public:
    avx3f v0;       //!< Base vertex of the triangles
    avx3f v1;       //!< 1st edge of the triangles (v0-v1)
    avx3f v2;       //!< 2nd edge of the triangles (v2-v0)
    avxi geomIDs;   //!< geometry ID
    avxi primIDs;   //!< primitive ID
#endif
  };
}