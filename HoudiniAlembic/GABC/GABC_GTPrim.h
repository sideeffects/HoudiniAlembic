/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Side Effects Software Inc
 *	123 Front Street West, Suite 1401
 *	Toronto, Ontario
 *	Canada   M5J 2M2
 *	416-504-9876
 *
 * NAME:	GABC_GTPrim.h ( GT Library, C++)
 *
 * COMMENTS:
 */

#ifndef __GABC_GTPrim__
#define __GABC_GTPrim__

#include "GABC_API.h"
#include <GT/GT_Primitive.h>
#include <GT/GT_GEOPrimCollect.h>
#include "GABC_Types.h"

class GABC_GEOPrim;

/// Simple wrapper for Alembic primitives
class GABC_GTPrimitive : public GT_Primitive
{
public:
    // Quantized LOD
    enum QLOD
    {
	LOD_SURFACE,
	LOD_POINTS,
	LOD_BOXES
    };

    GABC_GTPrimitive(const GABC_GEOPrim *prim)
	: myPrimitive(prim)
	, myCacheLOD(LOD_SURFACE)
	, myCache()
	, myAnimation(GABC_ANIMATION_TOPOLOGY)
    {
    }
    GABC_GTPrimitive	&operator=(const GABC_GTPrimitive &src)
			{
			    copyFrom(src);
			    return *this;
			}
    void		copyFrom(const GABC_GTPrimitive &src)
			{
			    // We do *not* copy the primitive
			    myCache = src.myCache;
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
    /// @}

    static QLOD		 getLOD(const GT_RefineParms *parms);

    void			 clear()
				 {
				     myCache = GT_PrimitiveHandle();
				 }
    const GABC_GEOPrim		*primitive() const	{ return myPrimitive; }
    const GT_PrimitiveHandle	&cache() const		{ return myCache; }
    fpreal			 cacheFrame() const	{ return myCacheFrame; }
    GABC_AnimationType		 animation() const	{ return myAnimation; }
    QLOD			 cacheLOD() const	{ return myCacheLOD; }

    void			 updateTransform(const UT_Matrix4D &xform);
    void			 updateAnimation(bool consider_transform);
private:
    GABC_GTPrimitive(const GABC_GTPrimitive &src)
	: GT_Primitive(src)
	, myPrimitive(NULL)
	, myCacheLOD(src.myCacheLOD)
	, myCache(src.myCache)
	, myAnimation(src.myAnimation)
    {
	UT_ASSERT(0 && "Copy c-tor");
    }
    void		 updateCache(const GT_RefineParms *parms);

    const GABC_GEOPrim	*myPrimitive;
    GT_PrimitiveHandle	 myCache;
    fpreal		 myCacheFrame;
    GABC_AnimationType	 myAnimation;
    QLOD		 myCacheLOD;
};

/// Hook to handle tesselation of tetra primitives
///
/// When rendering tet primitives, collect all the tet primitives together,
/// then a polygonal mesh is generated for the external hull, removing any
/// shared faces on the inside.
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

#endif
