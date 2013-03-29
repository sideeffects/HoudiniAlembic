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

#include "GABC_GTPrim.h"
#include "GABC_GEOPrim.h"
#include "GABC_Visibility.h"
#include <UT/UT_JSONWriter.h>
#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>

GABC_GTPrimitive::~GABC_GTPrimitive()
{
    setVisibilityCache(NULL);
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
    myPrimitive->object().getBoundingBox(box, myPrimitive->frame(), isconst);
    for (int i = 0; i < nseg; ++i)
	boxes[i].enlargeBounds(box);
}

GABC_ViewportLOD
GABC_GTPrimitive::getLOD(const GABC_GEOPrim &prim, const GT_RefineParms *parms)
{
    if (!GT_RefineParms::getDelayedLoadViewportLOD(parms))
	return GABC_VIEWPORT_FULL;
    return prim.viewportLOD();
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
GABC_GTPrimitive::updateAnimation(bool consider_transform,
		    bool consider_visibility)
{
    myAnimation = myPrimitive->object().getAnimationType(consider_transform);
    if (consider_visibility)
    {
	setVisibilityCache(myPrimitive->object().visibilityCache());
	if (myAnimation == GABC_ANIMATION_CONSTANT &&
		myVisibilityCache->animated())
	{
	    // Mark that we're dependent on "transform" for lack of a better
	    // flag
	    myAnimation = GABC_ANIMATION_TRANSFORM;
	}
    }
    else
    {
	if (myVisibilityCache)
	    setVisibilityCache(NULL);
    }
}

bool
GABC_GTPrimitive::visible() const
{
    return myVisibilityCache ? myVisibilityCache->visible() : true;
}

void
GABC_GTPrimitive::setVisibilityCache(const GABC_VisibilityCache *src)
{
    delete myVisibilityCache;
    myVisibilityCache = NULL;
    if (src)
	myVisibilityCache = new GABC_VisibilityCache(*src);
}

void
GABC_GTPrimitive::updateCache(const GT_RefineParms *parms)
{
    const GABC_IObject	&obj = myPrimitive->object();
    GABC_ViewportLOD	 lod = getLOD(*myPrimitive, parms);
    fpreal		 frame = myPrimitive->frame();

    if (myVisibilityCache)
	myVisibilityCache->update(frame);
    if (!myCache || lod != myCacheLOD || myAnimation == GABC_ANIMATION_TOPOLOGY)
    {
	myCacheLOD = lod;
	myCacheFrame = frame;
	switch (lod)
	{
	    case GABC_VIEWPORT_FULL:
		myCache = obj.getPrimitive(myPrimitive, frame,
				myAnimation, myPrimitive->attributeNameMap());
		break;
	    case GABC_VIEWPORT_POINTS:
		myCache = obj.getPointCloud(frame, myAnimation);
		break;
	    case GABC_VIEWPORT_BOX:
		myCache = obj.getBoxGeometry(frame, myAnimation);
		break;
	    case GABC_VIEWPORT_HIDDEN:
	    case GABC_VIEWPORT_CENTROID:
		myCache = obj.getCentroidGeometry(frame, myAnimation);
		break;
	}
	if (myCache)
	    myCache->setPrimitiveTransform(getPrimitiveTransform());
    }
    else if (myAnimation == GABC_ANIMATION_ATTRIBUTE && myCacheFrame != frame)
    {
	UT_ASSERT(myCache);
	myCacheFrame = frame;
	switch (myCacheLOD)
	{
	    case GABC_VIEWPORT_FULL:
		myCache = obj.updatePrimitive(myCache, myPrimitive, frame,
			myPrimitive->attributeNameMap());
		break;
	    case GABC_VIEWPORT_POINTS:
		myCache = obj.getPointCloud(frame, myAnimation);
		break;
	    case GABC_VIEWPORT_BOX:
		myCache = obj.getBoxGeometry(frame, myAnimation);
		break;
	    case GABC_VIEWPORT_HIDDEN:
	    case GABC_VIEWPORT_CENTROID:
		myCache = obj.getCentroidGeometry(frame, myAnimation);
		break;
	}
    }
    if (myCache)
	myCache->setPrimitiveTransform(getPrimitiveTransform());
}

const GT_PrimitiveHandle &
GABC_GTPrimitive::getRefined(const GT_RefineParms *parms) const
{
    const_cast<GABC_GTPrimitive *>(this)->updateCache(parms);
    return myCache;
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
    jsonWriter		j(w, "AlembicShape");
    UT_BoundingBox	box;
    bool		ok = true;
    std::string	filename = myPrimitive->filename();
    std::string	objectPath = myPrimitive->objectPath();
    ok = ok && w.jsonBeginMap();
    if (UTisstring(filename.c_str()))
    {
	ok = ok && w.jsonKeyToken("primFile");
	ok = ok && w.jsonStringToken(filename.c_str());
    }
    if (UTisstring(objectPath.c_str()))
    {
	ok = ok && w.jsonKeyToken("primObject");
	ok = ok && w.jsonStringToken(objectPath.c_str());
    }
    box.initBounds();
    enlargeBounds(&box, 1);
    ok = ok && w.jsonKeyToken("bounds");
    ok = ok && box.save(w);
    if (myCache)
    {
	ok = ok && w.jsonKeyToken("primFrame");
	ok = ok && w.jsonValue(myPrimitive->frame());
	ok = ok && w.jsonKeyToken("cacheFrame");
	ok = ok && w.jsonValue(myCacheFrame);
	ok = ok && w.jsonKeyToken("cacheLOD");
	ok = ok && w.jsonValue(myCacheLOD);
	ok = ok && w.jsonKeyToken("cacheGeometry");
	ok = ok && myCache->save(w);
    }
    return ok && w.jsonEndMap();
}

const GT_ViewportRefineOptions &
GABC_GTPrimitive::viewportRefineOptions() const
{
    static GT_ViewportRefineOptions     vopt(true);
    return vopt;
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
