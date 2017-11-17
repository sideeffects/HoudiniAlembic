/*
 * Copyright (c) 2017
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

#include "GABC_PackedImpl.h"
#include "GABC_PackedGT.h"

#include <UT/UT_JSONParser.h>
#include <UT/UT_Debug.h>
#include <UT/UT_MemoryCounter.h>
#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GT/GT_Primitive.h>
#include <GT/GT_Util.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_DAConstantValue.h>
#include <GT/GT_PrimPointMesh.h>
#include <GT/GT_PrimitiveBuilder.h>
#include <GT/GT_PackedGeoCache.h>

#if !defined(GABC_PRIMITIVE_TOKEN)
    #define GABC_PRIMITIVE_TOKEN	"AlembicRef"
    #define GABC_PRIMITIVE_LABEL	"Packed Alembic"
#endif

using namespace GABC_NAMESPACE;

namespace
{

static GT_PrimitiveHandle	theNullPrimitive;

class AlembicFactory : public GU_PackedFactory
{
public:
    AlembicFactory()
	: GU_PackedFactory(GABC_PRIMITIVE_TOKEN, GABC_PRIMITIVE_LABEL,
	                   "SOP_alembic")
    {
	registerIntrinsic("abctypename",
	    StringGetterCast(&GABC_PackedImpl::intrinsicNodeType));
	registerIntrinsic("abcfilename",
	    StringHolderGetterCast(&GABC_PackedImpl::intrinsicFilename),
	    StringHolderSetterCast(&GABC_PackedImpl::setFilename));
	registerIntrinsic("abcobjectpath",
	    StringHolderGetterCast(&GABC_PackedImpl::intrinsicObjectPath),
	    StringHolderSetterCast(&GABC_PackedImpl::setObjectPath));
	registerIntrinsic("abcsourcepath",
	    StringHolderGetterCast(&GABC_PackedImpl::intrinsicSourcePath));
	registerIntrinsic("abcpointcount",
	    IntGetterCast(&GABC_PackedImpl::intrinsicPointCount));
	registerIntrinsic("abcframe",
	    FloatGetterCast(&GABC_PackedImpl::intrinsicFrame),
	    FloatSetterCast(&GABC_PackedImpl::setFrame));
	registerIntrinsic("abcanimation",
	    StringGetterCast(&GABC_PackedImpl::intrinsicAnimation));
	registerIntrinsic("abcusevisibility",
	    BoolGetterCast(&GABC_PackedImpl::intrinsicUseVisibility),
	    BoolSetterCast(&GABC_PackedImpl::setUseVisibility));
	registerIntrinsic("abcvisibility",
	    IntGetterCast(&GABC_PackedImpl::intrinsicVisibility));
	registerIntrinsic("abcfullvisibility",
	    IntGetterCast(&GABC_PackedImpl::intrinsicFullVisibility));
	registerIntrinsic("abcusetransform",
	    BoolGetterCast(&GABC_PackedImpl::intrinsicUseTransform),
	    BoolSetterCast(&GABC_PackedImpl::setUseTransform));
        registerIntrinsic("abcpoint", 
            StringHolderGetterCast(&GABC_PackedImpl::intrinsicPoint));
        registerIntrinsic("abcvertex",
            StringHolderGetterCast(&GABC_PackedImpl::intrinsicVertex));
        registerIntrinsic("abcprimitive",
            StringHolderGetterCast(&GABC_PackedImpl::intrinsicPrimitive));
        registerIntrinsic("abcdetail",
            StringHolderGetterCast(&GABC_PackedImpl::intrinsicDetail));
        registerIntrinsic("abcfaceset",
            StringHolderGetterCast(&GABC_PackedImpl::intrinsicFaceSet));
    }
    virtual ~AlembicFactory()
    {
    }

    virtual GU_PackedImpl	*create() const
				    { return new GABC_PackedImpl(); }
};

static AlembicFactory	*theFactory = NULL;

}

GA_PrimitiveTypeId GABC_PackedImpl::theTypeId(-1);

GU_PrimPacked *
GABC_PackedImpl::build(GU_Detail &gdp,
			const UT_StringHolder &filename,
			const GABC_IObject &obj,
			fpreal frame,
			bool useTransform,
			bool useVisibility)
{
    UT_ASSERT(theFactory);

    GA_Primitive *prim = gdp.appendPrimitive(theFactory->typeDef().getId());
    // Note:  The primitive is invalid until you do something like
    //   prim->setVertexPoint(gdp.appendPointOffset());
    // The Alembic creation code has an option to have a shared point for all
    // Alembic primitives, so this is handled separately.
    GU_PrimPacked *pack = UTverify_cast<GU_PrimPacked *>(prim);
    GABC_PackedImpl *abc = UTverify_cast<GABC_PackedImpl *>(pack->implementation());
    abc->setFilename(pack, filename);
    abc->setObject(obj);
    abc->setFrame(pack, frame);
    abc->setUseTransform(pack, useTransform);
    abc->setUseVisibility(pack, useVisibility);
    return pack;
}

void
GABC_PackedImpl::install(GA_PrimitiveFactory *gafactory)
{
    UT_ASSERT(!theFactory);
    if (theFactory)
	return;
    theFactory = new AlembicFactory();
    GU_PrimPacked::registerPacked(gafactory, theFactory);
    theTypeId = theFactory->typeDef().getId();

    // Now, register the GT primitive
    GABC_CollectPacked	*gt = new GABC_CollectPacked();
    gt->bind(theFactory->typeDef().getId());
}

bool
GABC_PackedImpl::isInstalled()
{
    return theFactory != NULL;
}



GABC_PackedImpl::GABC_PackedImpl()
    : GU_PackedImpl()
    , myObject()
    , myCache()
    , myFilename()
    , myObjectPath()
    , myFrame(0)
    , myUseTransform(true)
    , myUseVisibility(true)
    , myCachedUniqueID(false)
    , myUniqueID(0)
    , myConstVisibility(GABC_VISIBLE_DEFER)
    , myHasConstBounds(false)
    , myViewportCache(nullptr)
{
}

GABC_PackedImpl::GABC_PackedImpl(const GABC_PackedImpl &src)
    : GU_PackedImpl(src)
    , myObject(src.myObject)
    , myCache()
    , myFilename(src.myFilename)
    , myObjectPath(src.myObjectPath)
    , myFrame(src.myFrame)
    , myUseTransform(src.myUseTransform)
    , myUseVisibility(src.myUseVisibility)
    , myCachedUniqueID(src.myCachedUniqueID)
    , myUniqueID(src.myUniqueID)
    , myConstVisibility(src.myConstVisibility)
    , myHasConstBounds(src.myHasConstBounds)
    , myConstBounds(src.myConstBounds)
    , myViewportCache(src.myViewportCache)
{
}

GABC_PackedImpl::~GABC_PackedImpl()
{
}

GU_PackedFactory *
GABC_PackedImpl::getFactory() const
{
    return theFactory;
}

GU_PackedImpl *
GABC_PackedImpl::copy() const
{
    return new GABC_PackedImpl(*this);
}

int64
GABC_PackedImpl::getMemoryUsage(bool inclusive) const
{
    int64 mem = inclusive ? sizeof(*this) : 0;
    
    // NOTE: This is currently very slow to compute, and slows down Alembic
    //       playback noticably. 
    // mem += myCache.getMemoryUsage(false);
    
    mem += myFilename.getMemoryUsage(false);
    mem += myObjectPath.getMemoryUsage(false);
    return mem;
}

void
GABC_PackedImpl::countMemory(UT_MemoryCounter &counter, bool inclusive) const
{
    if (counter.mustCountUnshared())
    {
        size_t mem = getMemoryUsage(inclusive);
        UT_MEMORY_DEBUG_LOG("GABC_PackedImpl", int64(mem));
        counter.countUnshared(mem);
    }
}

bool
GABC_PackedImpl::isValid() const
{
    return myObject.valid();
}

void
GABC_PackedImpl::clearData()
{
    myObject = GABC_IObject();
    myFilename.clear();
    myObjectPath.clear();
    myFrame = 0;
    myUseTransform = true;
    myUseVisibility = true;
    myCache.clear();
}

template <typename T>
bool
GABC_PackedImpl::loadFrom(GU_PrimPacked *prim, const T &options, const GA_LoadMap &map)
{
    clearData();
    bool bval;
    if (!import(options, "filename", myFilename))
	myFilename = "";
    if (!import(options, "object", myObjectPath))
	myObjectPath = "";
    if (!import(options, "frame", myFrame))
	myFrame = 0;
    if (!import(options, "usetransform", bval))
	bval = true;
    setUseTransform(prim, bval);
    if (!import(options, "usevisibility", bval))
	bval = true;
    setUseVisibility(prim, bval);
    return true;
}

void
GABC_PackedImpl::update(GU_PrimPacked *prim, const UT_Options &options)
{
    bool	changed = false;
    bool	bval;
    changed |= options.importOption("filename", myFilename);
    changed |= options.importOption("object", myObjectPath);
    changed |= options.importOption("frame", myFrame);
    changed |= options.importOption("usetransform", bval);
    setUseTransform(prim, bval);
    changed |= options.importOption("usevisibility", bval);
    setUseVisibility(prim, bval);
}

bool
GABC_PackedImpl::save(UT_Options &options, const GA_SaveMap &map) const
{
    options.setOptionS("filename", myFilename);
    options.setOptionS("object", myObjectPath);
    options.setOptionF("frame", myFrame);
    options.setOptionB("usetransform", myUseTransform);
    options.setOptionB("usevisibility", myUseVisibility);
    return true;
}

bool
GABC_PackedImpl::loadUnknownToken(const char *token,
	UT_JSONParser &p, const GA_LoadMap &map)
{
    UT_StringHolder	sval;
    fpreal64		fval;
    bool		bval;
    if (!strcmp(token, "filename"))
    {
	if (!p.parseString(sval))
	    return false;
	myFilename = sval;
    }
    else if (!strcmp(token, "object"))
    {
	if (!p.parseString(sval))
	    return false;
	myObjectPath = sval;
    }
    else if (!strcmp(token, "frame"))
    {
	if (!p.parseNumber(fval))
	    return false;
	myFrame = fval;
    }
    else if (!strcmp(token, "usetransform"))
    {
	if (!p.parseBool(bval))
	    return false;
	myUseTransform = bval;
    }
    else if (!strcmp(token, "usevisibility"))
    {
	if (!p.parseBool(bval))
	    return false;
	myUseVisibility = bval;
    }
    else
    {
	return GU_PackedImpl::loadUnknownToken(token, p, map);
    }
    return true;
}

bool
GABC_PackedImpl::getBounds(UT_BoundingBox &box) const
{
    const GABC_IObject	&iobj = object();
    if (!iobj.valid())
	return 0;

    bool	isconst;
    if (iobj.getBoundingBox(box, myFrame, isconst))
    {
	setBoxCache(box);
	return true;
    }
    return false;
}

bool
GABC_PackedImpl::getRenderingBounds(UT_BoundingBox &box) const
{
    switch (nodeType())
    {
	case GABC_POINTS:
	case GABC_CURVES:
	    break;
	default:
	    // Calling the primitive to get the "bounds" (not rendering bounds)
	    // will use the box cache if possible.
	    return getPrim()->getUntransformedBounds(box);
	return false;
	    break;
    }
    return object().getRenderingBoundingBox(box, myFrame);
}

void
GABC_PackedImpl::getVelocityRange(UT_Vector3 &vmin, UT_Vector3 &vmax) const
{
    vmin = 0;
    vmax = 0;
    if (!myObject.valid())
	return;

    GEO_AnimationType	atype;
    GT_Primitive::computeVelocityRange(vmin, vmax,
		    myObject.getVelocity(myFrame, atype));
}

void
GABC_PackedImpl::getWidthRange(fpreal &wmin, fpreal &wmax) const
{
    wmin = wmax = 0;
    if (!myObject.valid())
	return;

    GEO_AnimationType	atype;
    GT_DataArrayHandle	w = myObject.getWidth(myFrame, atype);
    if (w)
	w->getRange(wmin, wmax);
}

void
GABC_PackedImpl::getPrimitiveName(const GU_PrimPacked *prim, UT_WorkBuffer &wbuf) const
{
    if (UTisstring(objectPath().c_str()))
	wbuf.strcpy(objectPath().c_str());
    else
	GU_PackedImpl::getPrimitiveName(prim, wbuf);
}

bool
GABC_PackedImpl::getLocalTransform(UT_Matrix4D &m) const
{
    if (!myUseTransform || !myObject.valid())
	return false;

    GEO_AnimationType	atype;
    if(myViewportCache)
    {
	if(!myViewportCache->getTransform(myFrame, m))
	{
	    myObject.getWorldTransform(m, myFrame, atype);

	    bool animated = (atype!=GEO_ANIMATION_CONSTANT);
	    
	    myViewportCache->setTransformAnimated(animated);
	    myViewportCache->cacheTransform(myFrame, m);
	}
    }
    else
	myObject.getWorldTransform(m, myFrame, atype);
    
    return true;
}

bool
GABC_PackedImpl::unpackGeometry(GU_Detail &destgdp, bool allow_psoup) const
{
    int loadstyle = GABC_IObject::GABC_LOAD_FULL;
    // We don't want to copy over the attributes from the Houdini geometry
    loadstyle &= ~(GABC_IObject::GABC_LOAD_HOUDINI);

    GT_PrimitiveHandle	prim = fullGT(loadstyle);
    if (prim)
    {
	UT_Array<GU_Detail *>	details;
	GT_RefineParms		rparms;
	if (!allow_psoup)
	    rparms.setAllowPolySoup(false);

	GT_Util::makeGEO(details, prim, &rparms);
	for (exint i = 0; i < details.entries(); ++i)
	{
	    copyPrimitiveGroups(*details(i), false);
	    // Don't transform since GT conversion has already done that for us
	    unpackToDetail(destgdp, details(i), false);
	    delete details(i);
	}
    }
    return true;
}

bool
GABC_PackedImpl::unpack(GU_Detail &destgdp) const
{
    return unpackGeometry(destgdp, true);
}

bool
GABC_PackedImpl::unpackUsingPolygons(GU_Detail &destgdp) const
{
    return unpackGeometry(destgdp, false);
}


bool
GABC_PackedImpl::visibleGT(bool *is_animated) const
{
    if (!object().valid())
	return false;

    return myCache.visible(this, is_animated);
}

GT_PrimitiveHandle
GABC_PackedImpl::fullGT(int load_style) const
{
    if (!object().valid())
	return GT_PrimitiveHandle();
    
    return myCache.full(this, load_style);
}

GT_PrimitiveHandle
GABC_PackedImpl::instanceGT(bool ignore_visibility) const
{
    UT_AutoLock	lock(myLock);
    int		loadstyle = GABC_IObject::GABC_LOAD_FULL;
    // We don't want to copy over the attributes from the Houdini geometry
    loadstyle &= ~(GABC_IObject::GABC_LOAD_HOUDINI);
    // We don't want to transform into world space
    loadstyle |=  (GABC_IObject::GABC_LOAD_FORCE_UNTRANSFORMED);

    if(ignore_visibility)
	loadstyle |= (GABC_IObject::GABC_LOAD_IGNORE_VISIBILITY);
	
    return fullGT(loadstyle);
}

GT_PrimitiveHandle
GABC_PackedImpl::pointGT() const
{
    if (!object().valid())
	return GT_PrimitiveHandle();
    return myCache.points(this);
}

GT_PrimitiveHandle
GABC_PackedImpl::boxGT() const
{
    if (!object().valid())
	return GT_PrimitiveHandle();
    return myCache.box(this);
}

GT_PrimitiveHandle
GABC_PackedImpl::centroidGT() const
{
    if (!object().valid())
	return GT_PrimitiveHandle();
    return myCache.centroid(this);
}

GT_TransformHandle
GABC_PackedImpl::xformGT() const
{
    if (!object().valid())
	return GT_TransformHandle();
    return myCache.xform(this);
}

const GABC_IObject &
GABC_PackedImpl::object() const
{
    if (!myObject.valid())
    {
	myObject = GABC_Util::findObject(myFilename.toStdString(),
					myObjectPath.toStdString());
    }
    return myObject;
}

void
GABC_PackedImpl::setObject(const GABC_IObject &v)
{
    myObject = v;
    myObjectPath = myObject.objectPath();
    myCache.clear();
}

void
GABC_PackedImpl::setFilename(GU_PrimPacked *prim, const UT_StringHolder &v)
{
    if (myFilename != v)
    {
	myFilename = v;
	myCache.clear();
	myObject.purge();
	markDirty(prim);
    }
}

void
GABC_PackedImpl::setObjectPath(GU_PrimPacked *prim, const UT_StringHolder &v)
{
    if (myObjectPath != v)
    {
	myObjectPath = v;
	myCache.clear();
	myObject.purge();
	markDirty(prim);
    }
}

void
GABC_PackedImpl::setFrame(GU_PrimPacked *prim, fpreal f)
{
    myFrame = f;
    myCache.updateFrame(f);
    markDirty(prim);
}

void
GABC_PackedImpl::setUseTransform(GU_PrimPacked *prim, bool v)
{
    if (v != myUseTransform)
    {
	myUseTransform = v;
	// This can affect animation type
	myCache.clear();
	markDirty(prim);
    }
}

void
GABC_PackedImpl::setUseVisibility(GU_PrimPacked *prim, bool v)
{
    if (v != myUseVisibility)
    {
	myUseVisibility = v;
	// This can affect animation type
	myCache.clear();
	markDirty(prim);
    }
}

GEO_AnimationType
GABC_PackedImpl::animationType() const
{
    return myCache.animationType(this);
}


    
void
GABC_PackedImpl::GTCache::clear()
{
    myPrim = GT_PrimitiveHandle();
    myTransform = GT_TransformHandle();
    myRep = GEO_VIEWPORT_INVALID_MODE;
    myAnimationType = GEO_ANIMATION_INVALID;
    myFrame = 0;
    myLoadStyle = GABC_IObject::GABC_LOAD_FULL;
}

int64
GABC_PackedImpl::GTCache::getMemoryUsage(bool inclusive) const
{
    int64 mem = inclusive ? sizeof(*this) : 0;
    if (myPrim)
        mem += myPrim->getMemoryUsage();
    if (myTransform)
        mem += myTransform->getMemoryUsage();

    return mem;
}


void
GABC_PackedImpl::GTCache::updateFrame(fpreal frame)
{
    if (frame == myFrame)
	return;
    myFrame = frame;
    switch (myAnimationType)
    {
	case GEO_ANIMATION_ATTRIBUTE:
	case GEO_ANIMATION_TOPOLOGY:
	    myPrim = GT_PrimitiveHandle();
	case GEO_ANIMATION_TRANSFORM:
	    myTransform = GT_TransformHandle();
	case GEO_ANIMATION_CONSTANT:
	case GEO_ANIMATION_INVALID:
	    break;
    }
}


const GT_PrimitiveHandle &
GABC_PackedImpl::GTCache::full(const GABC_PackedImpl *abc,
			       int load_style)
{
    if(!(load_style&GABC_IObject::GABC_LOAD_IGNORE_VISIBILITY) &&
       !visible(abc))
    {
	return theNullPrimitive;
    }
    if (!myPrim
	    || myRep != GEO_VIEWPORT_FULL
	    || myLoadStyle != load_style
	    || myFrame != abc->frame())
    {
	const GABC_IObject	&o = abc->object();
	if (o.valid())
	{
	    UT_StringHolder cache_name;
	    const int64 version = 0;
	    bool cached = false;
	    GEO_AnimationType	atype = GEO_ANIMATION_INVALID;
	    
	    myFrame = abc->frame();
	    myRep = GEO_VIEWPORT_FULL;
	    myLoadStyle = load_style;
	    
	    if(GT_PackedGeoCache::isCachingAvailable())
	    {
		GT_PackedGeoCache::buildAlembicName(
					    cache_name,
					    o.getSourcePath().c_str(),
					    o.archive()->filename().c_str(),
					    abc->frame());
		myPrim = GT_PackedGeoCache::findInstance(cache_name, version,
							 load_style, &atype);
		if(myPrim)
		{
		    //UTdebugPrint("got cached for", cache_name);
		    cached = true;

		    if (atype > myAnimationType)
			myAnimationType = atype;
		}
	    }

	    if(!cached)
	    {
		myPrim = o.getPrimitive(abc->getPrim(), 
					myFrame, 
					atype,
					abc->getPrim()->attributeNameMap(),
					abc->getPrim()->facesetAttribute(), 
					myLoadStyle);

		if (atype > myAnimationType)
		    myAnimationType = atype;

		if(myPrim &&
		   (myLoadStyle & GABC_IObject::GABC_LOAD_GL_OPTIMIZED) &&
		   (myPrim->getPrimitiveType() == GT_PRIM_POLYGON_MESH ||
		    myPrim->getPrimitiveType() == GT_PRIM_SUBDIVISION_MESH))
		{
		    GU_ConstDetailHandle dtl;
		    myPrim = GT_Util::optimizePolyMeshForGL(myPrim, dtl);
		}

		if(GT_PackedGeoCache::isCachingAvailable() && myPrim)
		{
		    GT_PackedGeoCache::cacheInstance(cache_name, myPrim,
						     version, load_style,
						     atype);
		}
	    }
	    
	}
    }

    if (myPrim)
    {
	updateTransform(abc);
    }

    return myPrim;
}

const GT_PrimitiveHandle &
GABC_PackedImpl::GTCache::points(const GABC_PackedImpl *abc)
{
    if (!visible(abc))
    {
	return theNullPrimitive;
    }
    if (!myPrim || myRep != GEO_VIEWPORT_POINTS || myFrame != abc->frame())
    {
	const GABC_IObject	&o = abc->object();
	if (o.valid())
	{
	    GEO_AnimationType	atype;
	    myFrame = abc->frame();
	    myRep = GEO_VIEWPORT_POINTS;
	    myPrim = o.getPointCloud(myFrame, atype);

	    if (atype > myAnimationType)
	    {
		myAnimationType = atype;
            }
	}
    }

    if (myPrim)
    {
	updateTransform(abc);
    }

    return myPrim;
}

const GT_PrimitiveHandle &
GABC_PackedImpl::GTCache::box(const GABC_PackedImpl *abc)
{
    if (!visible(abc))
	return theNullPrimitive;
    if (!myPrim || myRep != GEO_VIEWPORT_BOX || myFrame != abc->frame())
    {
	const GABC_IObject	&o = abc->object();
	if (o.valid())
	{
	    bool		isconst;
	    UT_BoundingBox	box;
	    myFrame = abc->frame();
	    myRep = GEO_VIEWPORT_BOX;
	    if (o.getBoundingBox(box, myFrame, isconst))
	    {
		GT_BuilderStatus	err;
		myPrim = GT_PrimitiveBuilder::wireBox(err, box);
		if (myAnimationType < GEO_ANIMATION_ATTRIBUTE && !isconst)
		    myAnimationType = GEO_ANIMATION_ATTRIBUTE;
	    }
	}
    }
    if (myPrim)
	updateTransform(abc);
    return myPrim;
}

const GT_PrimitiveHandle &
GABC_PackedImpl::GTCache::centroid(const GABC_PackedImpl *abc)
{
    if (!visible(abc))
	return theNullPrimitive;
    if (!myPrim || myRep != GEO_VIEWPORT_CENTROID || myFrame != abc->frame())
    {
	const GABC_IObject	&o = abc->object();
	if (o.valid())
	{
	    bool		isconst;
	    UT_BoundingBox	box;
	    myFrame = abc->frame();
	    myRep = GEO_VIEWPORT_CENTROID;
	    if (o.getBoundingBox(box, myFrame, isconst))
	    {
		fpreal64		pos[3];
		pos[0] = box.xcenter();
		pos[1] = box.ycenter();
		pos[2] = box.zcenter();
		GT_AttributeListHandle	pt;
		pt = GT_AttributeList::createAttributeList(
			"P", new GT_RealConstant(1, pos, 3, GT_TYPE_POINT),
			NULL);
		myPrim = new GT_PrimPointMesh(pt,GT_AttributeListHandle());
		if (myAnimationType < GEO_ANIMATION_ATTRIBUTE && !isconst)
		    myAnimationType = GEO_ANIMATION_ATTRIBUTE;
	    }
	}
    }
    if (myPrim)
	updateTransform(abc);
    return myPrim;
}

GEO_AnimationType
GABC_PackedImpl::GTCache::animationType(const GABC_PackedImpl *abc)
{
    if (myAnimationType == GEO_ANIMATION_INVALID && visible(abc))
    {
	const GABC_IObject	&o = abc->object();
	if (o.valid())
	{
	    if(GT_PackedGeoCache::isCachingAvailable())
	    {
		UT_StringHolder cache_name;
		const int64 version = 0;
		GEO_AnimationType	atype = GEO_ANIMATION_INVALID;
		GT_PackedGeoCache::buildAlembicName(
		    cache_name,
		    o.getSourcePath().c_str(),
		    o.archive()->filename().c_str(),
		    abc->frame());

		if(GT_PackedGeoCache::findAnimation(cache_name, version, atype))
		{
		    if (atype > myAnimationType)
			myAnimationType = atype;
		    return atype;
		}
	    }

	    auto atype = abc->object().getAnimationType(true);
	    // The call to visible() might have set the animation type which we
	    // don't want to lose.
	    if (atype > myAnimationType)
		myAnimationType = atype;
	}
    }

    return myAnimationType;
}

bool
GABC_PackedImpl::GTCache::visible(const GABC_PackedImpl *abc,
				  bool *is_animated )
{
    if (!abc->useVisibility())
    {
	if(is_animated)
	    *is_animated = false;
	return true;
    }

    bool animated;
    bool vis = (GABC_Util::getVisibility(abc->object(), abc->frame(), animated,
					 true) != GABC_VISIBLE_HIDDEN);

    if(is_animated)
	*is_animated = animated;

    if (myAnimationType <= GEO_ANIMATION_CONSTANT && animated)
	myAnimationType = GEO_ANIMATION_TRANSFORM;
    return vis;
}

GABC_VisibilityType
GABC_PackedImpl::computeVisibility(bool check_parent) const
{
    bool animated;
    return GABC_Util::getVisibility(object(), frame(), animated, check_parent);
}

UT_StringHolder 
GABC_PackedImpl::getAttributeNames(GT_Owner owner) const
{
    if (myObject.valid())
        return myObject.getAttributes(getPrim()->attributeNameMap(), GABC_IObject::GABC_LOAD_FULL, owner);

    UT_String emptyString;
    return emptyString;
}

UT_StringHolder
GABC_PackedImpl::getFaceSetNames() const
{
    if (myObject.valid())
        return myObject.getFaceSets(getPrim()->facesetAttribute(), 0, GABC_IObject::GABC_LOAD_FULL);

    UT_String emptyString;
    return emptyString;
}

void
GABC_PackedImpl::GTCache::refreshTransform(const GABC_PackedImpl *abc)
{
    UT_AutoLock	lock(abc->myLock);
    if (!myTransform)
    {
	if (myAnimationType == GEO_ANIMATION_CONSTANT && abc->useVisibility())
	{
	    bool animated;
	    GABC_Util::getVisibility(abc->object(), abc->frame(), animated, true);
	    if(animated)
	    {
		// Mark animated visibility as animated transforms
		myAnimationType = GEO_ANIMATION_TRANSFORM;
	    }
	}

	UT_Matrix4D		xform;
	abc->getPrim()->getFullTransform4(xform);
	myTransform.reset(new GT_Transform(&xform, 1));
	if (myAnimationType == GEO_ANIMATION_CONSTANT
		&& abc->useTransform()
		&& abc->object().isTransformAnimated())
	{
	    myAnimationType = GEO_ANIMATION_TRANSFORM;
	}
    }
}

void
GABC_PackedImpl::GTCache::updateTransform(const GABC_PackedImpl *abc)
{
    UT_ASSERT(myPrim);
    if (myLoadStyle & GABC_IObject::GABC_LOAD_FORCE_UNTRANSFORMED)
    {
	myPrim->setPrimitiveTransform(GT_Transform::identity());
    }
    else
    {
	refreshTransform(abc);
	myPrim->setPrimitiveTransform(myTransform);
    }
}

void
GABC_PackedImpl::markDirty(GU_PrimPacked *prim)
{
    if (!prim)
        return;

    // Don't compute the animation type
    switch (myCache.animationType())
    {
	case GEO_ANIMATION_CONSTANT:
	    break;
	case GEO_ANIMATION_TRANSFORM:
            prim->transformDirty();
	    break;
	case GEO_ANIMATION_INVALID:
	case GEO_ANIMATION_ATTRIBUTE:
            prim->attributeDirty();
	    break;
	default:
            prim->topologyDirty();
	    break;
    }
}

int64
GABC_PackedImpl::getPropertiesHash() const
{
    if(!myCachedUniqueID)
    {
	if(!myObject.getPropertiesHash(myUniqueID))
	{
	    // HDF, likely. Hash the object path & filename to get an id.
	    const int64 pathhash = UT_String::hash(objectPath().c_str());
	    const int64 filehash = UT_String::hash(filename().c_str());

	    myUniqueID = pathhash + SYSwang_inthash64(filehash);
	}
	myCachedUniqueID = true;
    }
    
    return myUniqueID;
}

void
GABC_PackedImpl::setViewportCache(GABC_AlembicCache *cache) const
{
    myViewportCache = cache;
}
