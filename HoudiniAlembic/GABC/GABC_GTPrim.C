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

#include "GABC_GTPrim.h"
#include "GABC_GEOPrim.h"
#include <UT/UT_JSONWriter.h>
#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>

GABC_GTPrimitive::~GABC_GTPrimitive()
{
}

const char *
GABC_GTPrimitive::className() const
{
    return "GABC_GTPrimitive";
}

void
GABC_GTPrimitive::enlargeBounds(UT_BoundingBox boxes[], int nseg) const
{
    bool		isconst;
    UT_BoundingBox	box;
    myPrimitive->object().getBoundingBox(box, myCacheFrame, isconst);
    for (int i = 0; i < nseg; ++i)
	boxes[i].enlargeBounds(box);
}

GABC_GTPrimitive::QLOD
GABC_GTPrimitive::getLOD(const GT_RefineParms *parms)
{
    fpreal	lod = GT_RefineParms::getLOD(parms);
    if (lod > 0.75)
	return LOD_SURFACE;
    if (lod > 0.49)
	return LOD_POINTS;
    return LOD_BOXES;
}

void
GABC_GTPrimitive::updateTransform(const UT_Matrix4D &xform)
{
    GT_TransformHandle	x(new GT_Transform(&xform, 1));
    setPrimitiveTransform(x);
    if (myCache)
	myCache->setPrimitiveTransform(x);
}

void
GABC_GTPrimitive::updateAnimation(bool consider_transform)
{
    myAnimation = myPrimitive->object().getAnimationType(consider_transform);
}

void
GABC_GTPrimitive::updateCache(const GT_RefineParms *parms)
{
    const GABC_IObject	&obj = myPrimitive->object();
    QLOD		 lod = getLOD(parms);
    fpreal		 frame = myPrimitive->frame();

    if (!myCache || lod != myCacheLOD || myAnimation == GABC_ANIMATION_TOPOLOGY)
    {
	myCacheLOD = lod;
	myCacheFrame = frame;
	switch (lod)
	{
	    default:
	    case LOD_SURFACE:
		myCache = obj.getPrimitive(myPrimitive, frame,
				myAnimation, myPrimitive->attributeNameMap());
		break;
	    case LOD_POINTS:
		myCache = obj.getPointCloud(frame, myAnimation);
		break;
	    case LOD_BOXES:
		myCache = obj.getBoxGeometry(frame, myAnimation);
		break;
	}
	myCache->setPrimitiveTransform(getPrimitiveTransform());
    }
    else if (myAnimation == GABC_ANIMATION_ATTRIBUTE && myCacheFrame != frame)
    {
	UT_ASSERT(myCache);
	myCacheFrame = frame;
	switch (myCacheLOD)
	{
	    case LOD_SURFACE:
		myCache = obj.updatePrimitive(myCache, myPrimitive, frame,
			myPrimitive->attributeNameMap());
		break;
	    case LOD_POINTS:
		myCache = obj.getPointCloud(frame, myAnimation);
		break;
	    case LOD_BOXES:
		myCache = obj.getBoxGeometry(frame, myAnimation);
		break;
	}
    }
    if (myCache)
	myCache->setPrimitiveTransform(getPrimitiveTransform());
}

bool
GABC_GTPrimitive::refine(GT_Refine &refiner, const GT_RefineParms *parms) const
{
    const_cast<GABC_GTPrimitive *>(this)->updateCache(parms);
    if (myCache)
    {
	refiner.addPrimitive(myCache);
	return true;
    }
    return false;
}

int
GABC_GTPrimitive::getMotionSegments() const
{
    return 1;
}

int64
GABC_GTPrimitive::getMemoryUsage() const
{
    int64	mem = sizeof(*this);
    if (myCache)
	mem += myCache->getMemoryUsage();
    return mem;
}

bool
GABC_GTPrimitive::save(UT_JSONWriter &w) const
{
    jsonWriter	j(w, "AlembicShape");
    bool	ok = true;
    ok = ok && w.jsonBeginMap();
    ok = ok && w.jsonKeyToken("cacheFrame");
    ok = ok && w.jsonValue(myCacheFrame);
    ok = ok && w.jsonKeyToken("cacheLOD");
    ok = ok && w.jsonValue(myCacheLOD);
    if (myCache)
    {
	ok = ok && w.jsonKeyToken("cacheGeometry");
	ok = ok && myCache->save(w);
    }
    return ok && w.jsonEndMap();
}

//-------------------------------------------------------------------

void
GABC_GTPrimCollect::registerPrimitive(const GA_PrimitiveTypeId &id)
{
    // Just construct.  The constructor registers itself.
    new GABC_GTPrimCollect(id);
}

GABC_GTPrimCollect::GABC_GTPrimCollect(const GA_PrimitiveTypeId &id)
    : myId(id)
{
    // Bind this collector to the given primitive id.  When GT refines
    // primitives and hits the given primitive id, this collector will be
    // invoked.
    bind(myId);
}

GABC_GTPrimCollect::~GABC_GTPrimCollect()
{
}

GT_GEOPrimCollectData *
GABC_GTPrimCollect::beginCollecting(const GT_GEODetailListHandle &geometry,
	const GT_RefineParms *parms) const
{
    return NULL;
}

GT_PrimitiveHandle
GABC_GTPrimCollect::collect(const GT_GEODetailListHandle &,
	const GEO_Primitive *const*plist,
	int,
	GT_GEOPrimCollectData *) const
{
    if (plist && plist[0])
    {
	const GABC_GEOPrim *abc = UTverify_cast<const GABC_GEOPrim *>(plist[0]);
	return abc->gtPrimitive();
    }
    return GT_PrimitiveHandle();
}

GT_PrimitiveHandle
GABC_GTPrimCollect::endCollecting(const GT_GEODetailListHandle &,
				GT_GEOPrimCollectData *) const
{
    return GT_PrimitiveHandle();
}
