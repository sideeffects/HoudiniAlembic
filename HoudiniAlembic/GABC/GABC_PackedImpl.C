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
#include <UT/UT_JSONParser.h>
#include <UT/UT_Debug.h>
#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GT/GT_Primitive.h>
#include <GT/GT_Util.h>
#include <GT/GT_RefineParms.h>

#if !defined(GABC_PRIMITIVE_TOKEN)
    #define GABC_PRIMITIVE_TOKEN	"AlembicRef"
    #define GABC_PRIMITIVE_LABEL	"Alembic Delayed Load"
#endif

using namespace GABC_NAMESPACE;

namespace
{

class AlembicFactory : public GU_PackedFactory
{
public:
    AlembicFactory()
	: GU_PackedFactory(GABC_PRIMITIVE_TOKEN, GABC_PRIMITIVE_LABEL)
    {
    }
    virtual ~AlembicFactory()
    {
    }

    virtual GU_PackedImpl	*create() const
				    { return new GABC_PackedImpl(); }

#if 0
    /// Query how many intrinsic attributes are supported by this primitive type
    /// Default: return 0
    virtual int		getIntrinsicCount() const;
    /// Get information about the definition of the Nth intrinsic
    /// Default: assert
    virtual void	getIntrinsicDefinition(int idx,
				UT_String &name,
				GA_StorageClass &storage,
				bool &read_only) const;
    /// Get the intrinsic tuple size (which might be varying)
    /// Default: return 0
    virtual GA_Size	getIntrinsicTupleSize(const GU_PackedImpl *proc,
				int idx) const;
    /// @{
    /// Get/Set intrinsic attribute values for a primitive
    /// Default: return 0
    virtual GA_Size	getIntrinsicValue(const GU_PackedImpl *proc,
				int idx, UT_StringArray &strings) const;
    virtual GA_Size	getIntrinsicValue(const GU_PackedImpl *proc,
				int idx, int64 *val, GA_Size vsize) const;
    virtual GA_Size	getIntrinsicValue(const GU_PackedImpl *proc,
				int idx, fpreal64 *val, GA_Size vsize) const;
    virtual GA_Size	setIntrinsicValue(GU_PackedImpl *proc,
				int idx, const UT_StringArray &values);
    virtual GA_Size	setIntrinsicValue(GU_PackedImpl *proc,
				int idx, const int64 *val, int vsize);
    virtual GA_Size	setIntrinsicValue(GU_PackedImpl *proc,
				int idx, const fpreal64 *val, int vsize);
    /// @}
#endif
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
}

bool
GABC_PackedImpl::load(const UT_Options &options, const GA_LoadMap &map)
{
    clearData();
    if (!options.importOption("filename", myFilename))
	myFilename = "";
    if (!options.importOption("object", myObjectPath))
	myObjectPath = "";
    if (!options.importOption("frame", myFrame))
	myFrame = 0;
    if (!options.importOption("usetransform", myUseTransform))
	myUseTransform = true;
    if (!options.importOption("usevisibility", myUseVisibility))
	myUseVisibility = true;
    return true;
}

void
GABC_PackedImpl::update(const UT_Options &options)
{
    bool	changed = false;
    changed |= options.importOption("filename", myFilename);
    changed |= options.importOption("object", myObjectPath);
    changed |= options.importOption("frame", myFrame);
    changed |= options.importOption("usetransform", myUseTransform);
    changed |= options.importOption("usevisibility", myUseVisibility);
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
GABC_PackedImpl::unpack(GU_Detail &destgdp) const
{
    GT_PrimitiveHandle	prim = fullGT();
    if (prim)
    {
	UT_Array<GU_Detail *>	details;
	GT_RefineParms		rparms;

	GT_Util::makeGEO(details, prim, &rparms);
	for (exint i = 0; i < details.entries(); ++i)
	{
	    copyPrimitiveGroups(*details(i), false);
	    unpackToDetail(destgdp, details(i));
	    delete details(i);
	}
    }
    return true;
}

GT_PrimitiveHandle
GABC_PackedImpl::fullGT() const
{
    const GABC_IObject	&o = object();
    if (!o.valid())
	return GT_PrimitiveHandle();

    GEO_AnimationType	atype;
    GT_PrimitiveHandle	prim =  o.getPrimitive(getPrim(), frame(), atype,
					getPrim()->attributeNameMap());
    if (prim)
    {
	UT_Matrix4D		xform;
	getPrim()->getTransform(xform);
	prim->setPrimitiveTransform(new GT_Transform(&xform, 1));
    }
    return prim;
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
    myObjectPath = v.getFullName();
}

GEO_AnimationType
GABC_PackedImpl::animationType() const
{
    GEO_AnimationType	atype;
    atype = object().getAnimationType(myUseTransform);
    if (atype == GEO_ANIMATION_CONSTANT && myUseVisibility)
    {
	if (object().visibilityCache()->animated())
	    atype = GEO_ANIMATION_TRANSFORM;
    }
    return atype;
}
