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
 * NAME:	GABC_PackedImpl.h (GABC Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_PackedImpl.h"
#include "GABC_PackedGT.h"
#include "GABC_Visibility.h"
#include <UT/UT_JSONParser.h>
#include <UT/UT_Debug.h>
#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GT/GT_Primitive.h>
#include <GT/GT_Util.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_DAConstantValue.h>
#include <GT/GT_PrimPointMesh.h>
#include <GT/GT_PrimitiveBuilder.h>

#if !defined(GABC_PRIMITIVE_TOKEN)
    #define GABC_PRIMITIVE_TOKEN	"AlembicRef"
    #define GABC_PRIMITIVE_LABEL	"Alembic Delayed Load"
#endif

using namespace GABC_NAMESPACE;

namespace
{

static GT_PrimitiveHandle	theNullPrimitive;

class AlembicFactory : public GU_PackedFactory
{
public:
    AlembicFactory()
	: GU_PackedFactory(GABC_PRIMITIVE_TOKEN, GABC_PRIMITIVE_LABEL)
    {
	registerIntrinsic("abctypename",
	    StringGetterCast(&GABC_PackedImpl::intrinsicNodeType));
	registerIntrinsic("abcfilename",
	    StdStringGetterCast(&GABC_PackedImpl::intrinsicFilename),
	    StdStringSetterCast(&GABC_PackedImpl::setFilename));
	registerIntrinsic("abcobjectpath",
	    StdStringGetterCast(&GABC_PackedImpl::intrinsicObjectPath),
	    StdStringSetterCast(&GABC_PackedImpl::setObjectPath));
	registerIntrinsic("abcframe",
	    FloatGetterCast(&GABC_PackedImpl::frame),
	    FloatSetterCast(&GABC_PackedImpl::setFrame));
	registerIntrinsic("abcanimation",
	    StringGetterCast(&GABC_PackedImpl::intrinsicAnimation));
	registerIntrinsic("abcusevisibility",
	    BoolGetterCast(&GABC_PackedImpl::useVisibility),
	    BoolSetterCast(&GABC_PackedImpl::setUseVisibility));
	registerIntrinsic("abcvisibility",
	    IntGetterCast(&GABC_PackedImpl::intrinsicVisibility));
	registerIntrinsic("abcfullvisibility",
	    IntGetterCast(&GABC_PackedImpl::intrinsicFullVisibility));
	registerIntrinsic("abcusetransform",
	    BoolGetterCast(&GABC_PackedImpl::useTransform),
	    BoolSetterCast(&GABC_PackedImpl::setUseTransform));
    }
    virtual ~AlembicFactory()
    {
    }

    virtual GU_PackedImpl	*create() const
				    { return new GABC_PackedImpl(); }
};

static AlembicFactory	*theFactory = NULL;

}

GU_PrimPacked *
GABC_PackedImpl::build(GU_Detail &gdp,
			const std::string &filename,
			const GABC_IObject &obj,
			fpreal frame,
			bool useTransform,
			bool useVisibility)
{
    UT_ASSERT(theFactory);
    GA_Primitive	*prim;
    GU_PrimPacked	*pack;
    GABC_PackedImpl	*abc;
    
    prim = gdp.getPrimitiveFactory().create(theFactory->typeId(), gdp);
    pack = UTverify_cast<GU_PrimPacked *>(prim);
    abc = UTverify_cast<GABC_PackedImpl *>(pack->implementation());
    abc->setFilename(filename);
    abc->setObject(obj);
    abc->setFrame(frame);
    abc->setUseTransform(useTransform);
    abc->setUseVisibility(useVisibility);
    return pack;
}

void
GABC_PackedImpl::install(GA_PrimitiveFactory *gafactory)
{
    UT_DBGOUT(("== Using Alembic Packed Primitives == \n"));
    UT_ASSERT(!theFactory);
    if (theFactory)
	return;
    theFactory = new AlembicFactory();
    GU_PrimPacked::registerProcedural(gafactory, theFactory);

    // Now, register the GT primitive
    GABC_CollectPacked	*gt = new GABC_CollectPacked();
    gt->bind(theFactory->typeId());
}

bool
GABC_PackedImpl::isInstalled()
{
    return theFactory != NULL;
}

const GA_PrimitiveTypeId &
GABC_PackedImpl::typeId()
{
    UT_ASSERT(theFactory);
    return theFactory->typeId();
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

bool
GABC_PackedImpl::load(const UT_Options &options, const GA_LoadMap &map)
{
    clearData();
    bool	bval;
    if (!options.importOption("filename", myFilename))
	myFilename = "";
    if (!options.importOption("object", myObjectPath))
	myObjectPath = "";
    if (!options.importOption("frame", myFrame))
	myFrame = 0;
    if (!options.importOption("usetransform", bval))
	bval = true;
    setUseTransform(bval);
    if (!options.importOption("usevisibility", bval))
	bval = true;
    setUseVisibility(bval);
    return true;
}

void
GABC_PackedImpl::update(const UT_Options &options)
{
    bool	changed = false;
    bool	bval;
    changed |= options.importOption("filename", myFilename);
    changed |= options.importOption("object", myObjectPath);
    changed |= options.importOption("frame", myFrame);
    changed |= options.importOption("usetransform", bval);
    setUseTransform(bval);
    changed |= options.importOption("usevisibility", bval);
    setUseVisibility(bval);
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
    UT_WorkBuffer	sval;
    fpreal64		fval;
    bool		bval;
    if (!strcmp(token, "filename"))
    {
	if (!p.parseString(sval))
	    return false;
	myFilename = sval.toStdString();
    }
    else if (!strcmp(token, "object"))
    {
	if (!p.parseString(sval))
	    return false;
	myObjectPath = sval.toStdString();
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
    const GABC_IObject	&obj = object();
    if (!obj.valid())
	return 0;

    bool	isconst;
    return obj.getBoundingBox(box, myFrame, isconst) ? 1 : 0;
}

bool
GABC_PackedImpl::getRenderingBounds(UT_BoundingBox &box) const
{
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
    GT_DataArrayHandle	v = myObject.getVelocity(myFrame, atype);
    if (v)
    {
	for (int i = 0; i < 3; ++i)
	{
	    fpreal	fmin, fmax;
	    if (i >= v->getTupleSize())
	    {
		fmin = fmax = 0;
	    }
	    else
	    {
		v->getRange(fmin, fmax, i);
	    }
	    vmin(i) = fmin;
	    vmax(i) = fmax;
	}
    }
}

void
GABC_PackedImpl::getPrimitiveName(UT_WorkBuffer &wbuf) const
{
    if (UTisstring(objectPath().c_str()))
	wbuf.strcpy(objectPath().c_str());
    else
	GU_PackedImpl::getPrimitiveName(wbuf);
}

bool
GABC_PackedImpl::getLocalTransform(UT_Matrix4D &m) const
{
    if (!myUseTransform || !myObject.valid())
	return false;

    GEO_AnimationType	atype;
    myObject.getWorldTransform(m, myFrame, atype);
    return true;
}

bool
GABC_PackedImpl::unpackGeometry(GU_Detail &destgdp, bool allow_psoup) const
{
    GT_PrimitiveHandle	prim = fullGT();
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

GT_PrimitiveHandle
GABC_PackedImpl::fullGT() const
{
    if (!object().valid())
	return GT_PrimitiveHandle();
    return myCache.full(this);
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

const GABC_IObject &
GABC_PackedImpl::object() const
{
    if (!myObject.valid())
	myObject = GABC_Util::findObject(myFilename, myObjectPath);
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
GABC_PackedImpl::setFilename(const std::string &v)
{
    if (myFilename != v)
    {
	myFilename = v;
	myCache.clear();
    }
}

void
GABC_PackedImpl::setObjectPath(const std::string &v)
{
    if (myObjectPath != v)
    {
	myObjectPath = v;
	myCache.clear();
    }
}

void
GABC_PackedImpl::setFrame(fpreal f)
{
    myFrame = f;
    myCache.updateFrame(f);
}

void
GABC_PackedImpl::setUseTransform(bool v)
{
    if (v != myUseTransform)
    {
	myUseTransform = v;
	// This can affect animation type
	myCache.clear();
    }
}

void
GABC_PackedImpl::setUseVisibility(bool v)
{
    if (v != myUseVisibility)
    {
	myUseVisibility = v;
	// This can affect animation type
	myCache.clear();
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
    delete myVisibility;
    myVisibility = NULL;
    myPrim = GT_PrimitiveHandle();
    myTransform = GT_TransformHandle();
    myRep = GEO_VIEWPORT_INVALID_MODE;
    myAnimationType = GEO_ANIMATION_INVALID;
    myFrame = 0;
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
GABC_PackedImpl::GTCache::full(const GABC_PackedImpl *abc)
{
    if (!visible(abc))
	return theNullPrimitive;
    if (!myPrim || myRep != GEO_VIEWPORT_FULL || myFrame != abc->frame())
    {
	const GABC_IObject	&o = abc->object();
	if (o.valid())
	{
	    GEO_AnimationType	atype;
	    myFrame = abc->frame();
	    myRep = GEO_VIEWPORT_FULL;
	    myPrim = o.getPrimitive(abc->getPrim(), myFrame, atype,
		    abc->getPrim()->attributeNameMap());
	    if (atype > myAnimationType)
		myAnimationType = atype;
	}
    }
    if (myPrim)
	updateTransform(abc);
    return myPrim;
}

const GT_PrimitiveHandle &
GABC_PackedImpl::GTCache::points(const GABC_PackedImpl *abc)
{
    if (!visible(abc))
	return theNullPrimitive;
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
		myAnimationType = atype;
	}
    }
    if (myPrim)
	updateTransform(abc);
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
		myPrim = GT_PrimitiveBuilder::wireBox(err, box, NULL);
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
    if (myAnimationType == GEO_ANIMATION_INVALID)
	points(abc);	// Update lightest weight cache
    return myAnimationType;
}

bool
GABC_PackedImpl::GTCache::visible(const GABC_PackedImpl *abc)
{
    if (!abc->useVisibility())
    {
	delete myVisibility;
	myVisibility = NULL;
	return true;
    }
    if (!myVisibility)
    {
	const GABC_IObject	&o = abc->object();
	if (!o.valid())
	    return false;
	myVisibility = o.visibilityCache();
    }
    UT_ASSERT(myVisibility);
    myVisibility->update(abc->frame());
    return myVisibility->visible();
}

GABC_VisibilityType
GABC_PackedImpl::computeVisibility(bool check_parent) const
{
    const GABC_IObject	&o = object();
    if (!o.valid())
	return GABC_VISIBLE_HIDDEN;

    bool		 animated;
    return o.visibility(animated, frame(), check_parent);
}

void
GABC_PackedImpl::GTCache::updateTransform(const GABC_PackedImpl *abc)
{
    UT_ASSERT(myPrim);
    if (!myTransform)
    {
	const GABC_IObject	&o = abc->object();
	if (myAnimationType == GEO_ANIMATION_CONSTANT
		&& abc->useVisibility()
		&& o.visibilityCache()->animated())
	{
	    // Mark animated visibility as animated transforms
	    myAnimationType = GEO_ANIMATION_TRANSFORM;
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
    myPrim->setPrimitiveTransform(myTransform);
}
