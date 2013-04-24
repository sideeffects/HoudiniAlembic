/*
 * Copyright (c) 2013
 *	Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of Houdini Development Kit samples in source and
 * binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. The name of Side Effects Software may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *----------------------------------------------------------------------------
 */

#ifndef __GABC_GTPrim__
#define __GABC_GTPrim__

#include "GABC_API.h"
#include <GT/GT_Primitive.h>
#include <GT/GT_GEOPrimCollect.h>
#include "GABC_Types.h"

namespace GABC_NAMESPACE
{

class GABC_GEOPrim;
class GABC_VisibilityCache;

/// Simple wrapper for Alembic primitives
class GABC_GTPrimitive : public GT_Primitive
{
public:
    // Quantized LOD
    GABC_GTPrimitive(const GABC_GEOPrim *prim)
	: GT_Primitive()
	, myCache()
	, myPrimitive(prim)
	, myVisibilityCache(NULL)
	, myCacheFrame(0)
	, myAnimation(GABC_ANIMATION_TOPOLOGY)
	, myCacheLOD(GABC_VIEWPORT_FULL)
    {
    }
    GABC_GTPrimitive	&operator=(const GABC_GTPrimitive &src)
			{
			    copyFrom(src);
			    return *this;
			}
    void		copyFrom(const GABC_GTPrimitive &src)
			{
			    myCache = src.myCache;
			    // We do *not* copy myPrimitive
			    setVisibilityCache(src.myVisibilityCache);
			    myCacheFrame = src.myCacheFrame;
			    myAnimation = src.myAnimation;
			    myCacheLOD = src.myCacheLOD;
			}

    virtual ~GABC_GTPrimitive();
    /// @{
    /// Methods defined on GT_Primitive
    virtual const char	*className() const;
    virtual void	 enlargeBounds(UT_BoundingBox boxes[], int nseg) const;
    virtual bool	 refine(GT_Refine &refiner,
				    const GT_RefineParms *parms) const;
    virtual int		 getMotionSegments() const;
    virtual int64	 getMemoryUsage() const;
    virtual bool	 save(UT_JSONWriter &w) const;
    virtual const GT_ViewportRefineOptions	&viewportRefineOptions() const;
    virtual GT_PrimitiveHandle	doSoftCopy() const
				    { return new GABC_GTPrimitive(*this); }
    /// @}

    static GABC_ViewportLOD		 getLOD(const GABC_GEOPrim &prim,
				const GT_RefineParms *parms);

    void			 clear()
				 {
				     myCache = GT_PrimitiveHandle();
				 }
    const GABC_GEOPrim		*primitive() const	{ return myPrimitive; }
    const GT_PrimitiveHandle	&cache() const		{ return myCache; }
    fpreal			 cacheFrame() const	{ return myCacheFrame; }
    GABC_AnimationType		 animation() const	{ return myAnimation; }
    GABC_ViewportLOD			 cacheLOD() const	{ return myCacheLOD; }
    bool			 visible() const;
    const GABC_VisibilityCache	*visibilityCache() const
					{ return myVisibilityCache; }

    void	 updateTransform(const UT_Matrix4D &xform);
    void	 updateAnimation(bool consider_transform,
			bool consider_visibility);

    void	 setVisibilityCache(const GABC_VisibilityCache *src);

    /// Return the "atomic" primitive.  This is the base GT primitive and it
    /// will be one of:
    ///	 - GT_PrimPointMesh
    ///	 - GT_PrimCurveMesh
    ///	 - GT_PrimPolygonMesh
    ///	 - GT_PrimSubdivisionMesh
    ///	 - GT_PrimNuPatch
    /// If there are errors, this may return a NULL pointer
    const GT_PrimitiveHandle	&getRefined(const GT_RefineParms *parms) const;
private:
    GABC_GTPrimitive(const GABC_GTPrimitive &src)
	: GT_Primitive(src)
	, myCache()
	, myPrimitive(src.myPrimitive)
	, myVisibilityCache(NULL)
	, myCacheFrame(0)
	, myAnimation(GABC_ANIMATION_TOPOLOGY)
	, myCacheLOD(GABC_VIEWPORT_FULL)
    {
	UT_ASSERT(0 && "Copy c-tor -- should be a soft copy");
	copyFrom(src);
    }
    void		 updateCache(const GT_RefineParms *parms);

    GT_PrimitiveHandle		 myCache;
    const GABC_GEOPrim		*myPrimitive;
    GABC_VisibilityCache	*myVisibilityCache;
    fpreal			 myCacheFrame;
    GABC_AnimationType		 myAnimation;
    GABC_ViewportLOD		 myCacheLOD;
};

/// Hook to handle tesselation of Alembic primitives
class GABC_API GABC_GTPrimCollect : public GT_GEOPrimCollect
{
public:
    /// Register the GT collector
    static void	registerPrimitive(const GA_PrimitiveTypeId &id);

    /// Constructor.  The @c id is used to bind the collector to the proper
    /// primitive type.
    GABC_GTPrimCollect(const GA_PrimitiveTypeId &id);

    /// Destructor
    virtual ~GABC_GTPrimCollect();

    /// Return a structure to capture all the tet primitives
    virtual GT_GEOPrimCollectData *
		beginCollecting(const GT_GEODetailListHandle &geometry,
				const GT_RefineParms *parms) const;

    /// When refining a single tet primitive, we add it to the container
    virtual GT_PrimitiveHandle
		collect(const GT_GEODetailListHandle &geometry,
				const GEO_Primitive *const* prim_list,
				int nsegments,
				GT_GEOPrimCollectData *data) const;
    /// At the end of collecting, the single outside skin is generated
    virtual GT_PrimitiveHandle
		endCollecting(const GT_GEODetailListHandle &geometry,
				GT_GEOPrimCollectData *data) const;
private:
    GA_PrimitiveTypeId	myId;	// Primitive ID of the GA_PrimABC type
};
}

#endif
