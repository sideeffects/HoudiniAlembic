/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Side Effects Software Inc
 *	477 Richmond Street West
 *	Toronto, Ontario
 *	Canada   M5V 3E7
 *	416-504-9876
 *
 * NAME:	GABC_GEOPrim.C ( GEO Library, C++)
 *
 * COMMENTS:	Class for Alembic primitives.
 */

#include "GABC_GEOPrim.h"
#include "GABC_Util.h"
#include <GA/GA_IntrinsicMacros.h>
#include <GA/GA_PrimitiveJSON.h>
#include <GT/GT_Transform.h>
#include <GEO/GEO_Detail.h>
#include <GEO/GEO_PrimType.h>
#include <UT/UT_Lock.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_JSONWriter.h>

#include <Alembic/AbcGeom/All.h>

namespace
{
    static UT_Lock	theH5Lock;

    static bool
    saveEmptyNameMap(UT_JSONWriter &w)
    {
	bool	ok = w.jsonBeginMap();
	return ok && w.jsonEndMap();
    }

    bool
    setPrimitiveWorldTransform(const GABC_IObject &obj, GT_Primitive *prim,
		    fpreal t, GABC_AnimationType &atype)
    {
	UT_Matrix4D		xform;
	GABC_AnimationType	xtype;
	if (!obj.getWorldTransform(xform, t, xtype))
	    return false;
	prim->setPrimitiveTransform(
		GT_TransformHandle(new GT_Transform(&xform, 1)));
	return true;
    }
}

GABC_GEOPrim::GABC_GEOPrim(GEO_Detail *d, GA_Offset offset)
    : GEO_Primitive(d, offset)
    , myFilename()
    , myObjectPath()
    , myObject()
    , myFrame(0)
    , myUseTransform(true)
    , myGTPrimitive(new GABC_GTPrimitive(this))
{
    myGTPrimitive->incref();	// Old-school
    myGeoTransform = GT_Transform::identity();
}

void
GABC_GEOPrim::copyMemberDataFrom(const GABC_GEOPrim &src)
{
    myFilename = src.myFilename;
    myObjectPath = src.myObjectPath;
    myObject = src.myObject;
    myFrame = src.myFrame;
    myGeoTransform = src.myGeoTransform;
    myAttributeNameMap = src.myAttributeNameMap;
    myUseTransform = src.myUseTransform;
    myGTPrimitive->copyFrom(*src.myGTPrimitive);

    if (!myUseTransform)
    {
	// Should be an identity
	myGTTransform = src.myGTTransform;
    }
    else
    {
	myGTTransform = GT_TransformHandle();
    }

}

GABC_GEOPrim::~GABC_GEOPrim()
{
    myGTPrimitive->decref();	// Old school reference counting
}

void
GABC_GEOPrim::clearForDeletion()
{
    myGTPrimitive->clear();
    myGeoTransform = GT_Transform::identity();
}

void
GABC_GEOPrim::stashed(int onoff, GA_Offset offset)
{
    myGeoTransform = GT_Transform::identity();
    myGTPrimitive->clear();
    GEO_Primitive::stashed(onoff, offset);
}

bool
GABC_GEOPrim::evaluatePointRefMap(GA_Offset,
				GA_AttributeRefMap &,
				fpreal, fpreal, unsigned, unsigned) const
{
    UT_ASSERT(0);
    return false;
}


void
GABC_GEOPrim::reverse()
{
}

UT_Vector3
GABC_GEOPrim::computeNormal() const
{
    return UT_Vector3(0, 0, 0);
}

fpreal
GABC_GEOPrim::calcVolume(const UT_Vector3 &Pref) const
{
    const GT_PrimitiveHandle	gt = gtPrimitive();
    if (gt)
	return gt->computeVolume(Pref);
    return 0;
}

fpreal
GABC_GEOPrim::calcArea() const
{
    const GT_PrimitiveHandle	gt = gtPrimitive();
    if (gt)
	return gt->computeSurfaceArea();
    return 0;
}

fpreal
GABC_GEOPrim::calcPerimeter() const
{
    const GT_PrimitiveHandle	gt = gtPrimitive();
    if (gt)
	return gt->computePerimeter();
    return 0;
}

GA_Size
GABC_GEOPrim::getVertexCount(void) const
{
    return 0;
}

GA_Offset
GABC_GEOPrim::getVertexOffset(GA_Size) const
{
    UT_ASSERT(0);
    return GA_INVALID_OFFSET;
}

int
GABC_GEOPrim::detachPoints(GA_PointGroup &grp)
{
    UT_ASSERT(0);
    return 0;
}

GA_Primitive::GA_DereferenceStatus
GABC_GEOPrim::dereferencePoint(GA_Offset, bool)
{
    return GA_DEREFERENCE_FAIL;
}

GA_Primitive::GA_DereferenceStatus
GABC_GEOPrim::dereferencePoints(const GA_RangeMemberQuery &, bool)
{
    return GA_DEREFERENCE_FAIL;
}

///
/// JSON methods
///

namespace
{

class geo_PrimABCJSON : public GA_PrimitiveJSON
{
public:
    geo_PrimABCJSON()
    {
    }
    virtual ~geo_PrimABCJSON() {}

    enum
    {
	geo_FILENAME,
	geo_OBJECTPATH,
	geo_FRAME,
	geo_TRANSFORM,
	geo_USETRANSFORM,
	geo_ATTRIBUTEMAP,
	geo_ENTRIES
    };

    const GABC_GEOPrim	*abc(const GA_Primitive *p) const
			    { return UTverify_cast<const GABC_GEOPrim *>(p); }
    GABC_GEOPrim		*abc(GA_Primitive *p) const
			    { return UTverify_cast<GABC_GEOPrim *>(p); }

    virtual int		getEntries() const	{ return geo_ENTRIES; }
    virtual const char	*getKeyword(int i) const
			{
			    switch (i)
			    {
				case geo_FILENAME:	return "filename";
				case geo_OBJECTPATH:	return "object";
				case geo_FRAME:		return "frame";
				case geo_TRANSFORM:	return "ltransform";
				case geo_USETRANSFORM:	return "usetransform";
				case geo_ATTRIBUTEMAP:	return "attributemap";
				case geo_ENTRIES:	break;
			    }
			    UT_ASSERT(0);
			    return NULL;
			}
    virtual bool shouldSaveField(const GA_Primitive *pr, int i,
			    const GA_SaveMap &map) const
		{
		    switch (i)
		    {
			case geo_TRANSFORM:
			    return !abc(pr)->geoTransform()->isIdentity();
			case geo_ATTRIBUTEMAP:
			    return abc(pr)->attributeNameMap() &&
				    abc(pr)->attributeNameMap()->entries();
			default:
			    break;
		    }
		    return true;
		}
    virtual bool saveField(const GA_Primitive *pr, int i,
			UT_JSONWriter &w, const GA_SaveMap &map) const
		{
		    switch (i)
		    {
			case geo_FILENAME:
			{
			    const std::string	&f = abc(pr)->filename();
			    return w.jsonStringToken(f.c_str(), f.length());
			}
			case geo_OBJECTPATH:
			{
			    const std::string	&f = abc(pr)->objectPath();
			    return w.jsonStringToken(f.c_str(), f.length());
			}
			case geo_FRAME:
			    return w.jsonValue(abc(pr)->frame());

			case geo_TRANSFORM:
			    return abc(pr)->geoTransform()->save(w);

			case geo_USETRANSFORM:
			    return w.jsonBool(abc(pr)->useTransform());

			case geo_ATTRIBUTEMAP:
			    if (abc(pr)->attributeNameMap())
				return abc(pr)->attributeNameMap()->save(w);
			    return saveEmptyNameMap(w);

			case geo_ENTRIES:
			    break;
		    }
		    UT_ASSERT(0);
		    return false;
		}
    virtual bool saveField(const GA_Primitive *pr, int i,
			UT_JSONValue &v, const GA_SaveMap &map) const
		{
		    // Just use default JSON value saving
		    return GA_PrimitiveJSON::saveField(pr, i, v, map);
		}
    virtual bool loadField(GA_Primitive *pr, int i, UT_JSONParser &p,
			const GA_LoadMap &map) const
		{
		    UT_WorkBuffer	sval;
		    fpreal64		fval;
		    bool		bval;
		    GT_TransformHandle	xform;
		    GABC_NameMapPtr	amap;
		    switch (i)
		    {
			case geo_FILENAME:
			    if (!p.parseString(sval))
				return false;
			    abc(pr)->setFilename(sval.buffer());
			    return true;
			case geo_OBJECTPATH:
			    if (!p.parseString(sval))
				return false;
			    abc(pr)->setObjectPath(sval.buffer());
			    return true;
			case geo_FRAME:
			    if (!p.parseNumber(fval))
				return false;
			    abc(pr)->setFrame(fval);
			    return true;
			case geo_TRANSFORM:
			    if (!GT_Transform::load(xform, p))
				return false;
			    abc(pr)->setGeoTransform(xform);
			    return true;
			case geo_USETRANSFORM:
			    if (!p.parseBool(bval))
				return false;
			    abc(pr)->setUseTransform(bval);
			    return true;
			case geo_ATTRIBUTEMAP:
			    if (!GABC_NameMap::load(amap, p))
				return false;
			    abc(pr)->setAttributeNameMap(amap);
			    return true;
			case geo_ENTRIES:
			    break;
		    }
		    UT_ASSERT(0);
		    return false;
		}
    virtual bool loadField(GA_Primitive *pr, int i, UT_JSONParser &p,
			const UT_JSONValue &v, const GA_LoadMap &map) const
		{
		    // Just rely on default value parsing
		    return GA_PrimitiveJSON::loadField(pr, i, p, v, map);
		}
    virtual bool isEqual(int i, const GA_Primitive *a,
			const GA_Primitive *b) const
		{
		    switch (i)
		    {
			case geo_FILENAME:
			    return abc(a)->filename() == abc(b)->filename();
			case geo_OBJECTPATH:
			    return abc(a)->objectPath() == abc(b)->objectPath();
			case geo_FRAME:
			    return abc(a)->frame() == abc(b)->frame();

			case geo_TRANSFORM:
			    return (*abc(a)->geoTransform() ==
				    *abc(b)->geoTransform());
			case geo_USETRANSFORM:
			    return abc(a)->useTransform() ==
				    abc(b)->useTransform();
			case geo_ATTRIBUTEMAP:
			    // Just compare pointers directly
			    return abc(a)->attributeNameMap().get() ==
				    abc(b)->attributeNameMap().get();
			case geo_ENTRIES:
			    break;
		    }
		    return false;
		}
private:
};

static const GA_PrimitiveJSON *
abcJSON()
{
    static GA_PrimitiveJSON	*theJSON = NULL;

    if (!theJSON)
	theJSON = new geo_PrimABCJSON();
    return theJSON;
}
}

const GA_PrimitiveJSON *
GABC_GEOPrim::getJSON() const
{
    return abcJSON();
}

namespace
{
    typedef Alembic::Abc::Box3d			Box3d;
    typedef Alembic::Abc::ISampleSelector	ISampleSelector;
    typedef Alembic::AbcGeom::ICompoundProperty	ICompoundProperty;
    typedef Alembic::AbcGeom::IXform		IXform;
    typedef Alembic::AbcGeom::IXformSchema	IXformSchema;
    typedef Alembic::AbcGeom::XformSample	XformSample;
    typedef Alembic::AbcGeom::ISubD		ISubD;
    typedef Alembic::AbcGeom::ISubDSchema	ISubDSchema;
    typedef Alembic::AbcGeom::ICurves		ICurves;
    typedef Alembic::AbcGeom::ICurvesSchema	ICurvesSchema;
    typedef Alembic::AbcGeom::INuPatch		INuPatch;
    typedef Alembic::AbcGeom::INuPatchSchema	INuPatchSchema;
    typedef Alembic::AbcGeom::IPolyMesh		IPolyMesh;
    typedef Alembic::AbcGeom::IPolyMeshSchema	IPolyMeshSchema;
    typedef Alembic::AbcGeom::IPoints		IPoints;
    typedef Alembic::AbcGeom::IPointsSchema	IPointsSchema;
    typedef Alembic::AbcGeom::IFloatGeomParam	IFloatGeomParam;
}

void
GABC_GEOPrim::updateAnimation()
{
    if (!myObject.valid())
	return;
    // Set the topology based on a combination of conditions.
    // We initialize based on the shape topology, but if the shape topology is
    // constant, there are still various factors which can make the primitive
    // non-constant (i.e. Homogeneous).
    myGTPrimitive->updateAnimation(myUseTransform);
    //myAnimation = myObject.getAnimationType(myUseTransform);
}

int
GABC_GEOPrim::getBBox(UT_BoundingBox *bbox) const
{
    // Return the bounding box area
    if (!myObject.valid())
	return 0;

    bool	isconst;
    if (!myObject.getBoundingBox(*bbox, myFrame, isconst))
	return 0;

    UT_Matrix4D	xform;
    getTransform(xform);
    bbox->transform(xform);
    return 1;
}

bool
GABC_GEOPrim::getRenderingBounds(UT_BoundingBox &box) const
{
    if (!myObject.getRenderingBoundingBox(box, myFrame))
	return false;
    UT_Matrix4D	xform;
    getTransform(xform);
    box.transform(xform);
    return true;
}

void
GABC_GEOPrim::getVelocityRange(UT_Vector3 &vmin, UT_Vector3 &vmax) const
{
    vmin = 0;
    vmax = 0;
    if (myObject.valid())
	return;

    GABC_AnimationType	atype;
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
GABC_GEOPrim::clearGT()
{
    myGTPrimitive->clear();
}

GT_PrimitiveHandle
GABC_GEOPrim::gtPointCloud() const
{
    GABC_AnimationType	atype;
    GT_PrimitiveHandle	result;

    result = myObject.getPointCloud(myFrame, atype);

    if (myUseTransform || !myGeoTransform->isIdentity())
    {
	UT_Matrix4D	xform;
	if (getTransform(xform))
	{
	    result->setPrimitiveTransform(GT_TransformHandle(
			new GT_Transform(&xform, 1)));
	}
    }
    return result;
}

GT_PrimitiveHandle
GABC_GEOPrim::gtBox() const
{
    GABC_AnimationType	atype;
    GT_PrimitiveHandle	result;

    result = myObject.getBoxGeometry(myFrame, atype);

    if (myUseTransform || !myGeoTransform->isIdentity())
    {
	UT_Matrix4D	xform;
	if (getTransform(xform))
	{
	    result->setPrimitiveTransform(GT_TransformHandle(
			new GT_Transform(&xform, 1)));
	}
    }
    return result;
}

GT_PrimitiveHandle
GABC_GEOPrim::gtPrimitive() const
{
    if (myUseTransform || !myGeoTransform->isIdentity())
    {
	UT_Matrix4D	xform;
	if (getTransform(xform))
	    myGTPrimitive->updateTransform(xform);
    }
    return GT_PrimitiveHandle(myGTPrimitive);
}

void
GABC_GEOPrim::setGeoTransform(const GT_TransformHandle &x)
{
    UT_Matrix4D	xform;
    myGeoTransform = x;
    if (getTransform(xform))
	myGTPrimitive->updateTransform(xform);
}

void
GABC_GEOPrim::setAttributeNameMap(const GABC_NameMapPtr &m)
{
    myAttributeNameMap = m;
    myGTPrimitive->clear();
}

void
GABC_GEOPrim::enlargePointBounds(UT_BoundingBox &box) const
{
    UT_BoundingBox	primbox;
    if (getBBox(&primbox))
	box.enlargeBounds(primbox);
}

void
GABC_GEOPrim::transform(const UT_Matrix4 &xform)
{
    setGeoTransform(GT_TransformHandle(myGeoTransform->multiply(xform)));
}

void
GABC_GEOPrim::getLocalTransform(UT_Matrix4D &matrix) const
{
    myGeoTransform->getMatrix(matrix);
}

void
GABC_GEOPrim::setLocalTransform(const UT_Matrix4D &matrix)
{
    setGeoTransform(GT_TransformHandle(new GT_Transform(&matrix, 1)));
}

UT_Vector3
GABC_GEOPrim::baryCenter() const
{
    UT_BoundingBox	box;
    getBBox(&box);
    return box.center();
}

bool
GABC_GEOPrim::isDegenerate() const
{
    return !myObject.valid();
}

void
GABC_GEOPrim::copyPrimitive(const GEO_Primitive *psrc, GEO_Point **)
{
    if (psrc == this)
	return;

    const GABC_GEOPrim	 *src = UTverify_cast<const GABC_GEOPrim *>(psrc);

    copyMemberDataFrom(*src);
}

void
GABC_GEOPrim::copyOffsetPrimitive(const GEO_Primitive *psrc, int)
{
    copyPrimitive(psrc, NULL);
}

GEO_Primitive *
GABC_GEOPrim::copy(int preserve_shared_pts) const
{
    GEO_Primitive *clone = GEO_Primitive::copy(preserve_shared_pts);

    if (clone)
    {
	GABC_GEOPrim	*abc = UTverify_cast<GABC_GEOPrim *>(clone);
	abc->copyMemberDataFrom(*this);
    }
    return clone;
}

void
GABC_GEOPrim::copyUnwiredForMerge(const GA_Primitive *psrc, const GA_MergeMap &)
{
    UT_ASSERT( psrc != this );
    const GABC_GEOPrim	*src = UTverify_cast<const GABC_GEOPrim *>(psrc);

    copyMemberDataFrom(*src);
}

void
GABC_GEOPrim::swapVertexOffsets(const GA_Defragment &)
{
}

enum
{
    geo_INTRINSIC_ABC_TYPE,	// Alembic primitive type
    geo_INTRINSIC_ABC_FILE,	// Filename
    geo_INTRINSIC_ABC_PATH,	// Object path
    geo_INTRINSIC_ABC_FRAME,	// Time
    geo_INTRINSIC_ABC_ANIMATION,	// Animation type
    geo_INTRINSIC_ABC_LOCALXFORM,	// Alembic file local transform
    geo_INTRINSIC_ABC_WORLDXFORM,	// Alembic file world transform
    geo_INTRINSIC_ABC_GEOXFORM,		// Geometry transform
    geo_INTRINSIC_ABC_TRANSFORM,	// Combined transform

    geo_NUM_INTRINISCS
};

namespace
{
    static const char *
    intrinsicTypeName(const GABC_GEOPrim *p)
    {
	return GABCnodeType(p->abcNodeType());
    }
    static const char *
    intrinsicFilename(const GABC_GEOPrim *p)
    {
	return p->filename().c_str();
    }
    static const char *
    intrinsicObjectPath(const GABC_GEOPrim *p)
    {
	return p->objectPath().c_str();
    }
    static const char *
    intrinsicAnimation(const GABC_GEOPrim *p)
    {
	return GABCanimationType(p->animation());
    }
    static GA_Size
    intrinsicGeoTransform(const GABC_GEOPrim *p, fpreal64 *v, GA_Size size)
    {
	size = SYSmin(size, 16);
	UT_Matrix4D	xform;
	const GT_TransformHandle	gx = p->geoTransform();
	gx->getMatrix(xform, 0);
	for (int i = 0; i < size; ++i)
	    v[i] = xform.data()[i];
	return size;
    }
    static GA_Size
    intrinsicLocalTransform(const GABC_GEOPrim *p, fpreal64 *v, GA_Size size)
    {
	size = SYSmin(size, 16);
	UT_Matrix4D	xform;
	p->getABCLocalTransform(xform);
	for (int i = 0; i < size; ++i)
	    v[i] = xform.data()[i];
	return size;
    }
    static GA_Size
    intrinsicWorldTransform(const GABC_GEOPrim *p, fpreal64 *v, GA_Size size)
    {
	size = SYSmin(size, 16);
	UT_Matrix4D	xform;
	p->getABCWorldTransform(xform);
	for (int i = 0; i < size; ++i)
	    v[i] = xform.data()[i];
	return size;
    }
    static GA_Size
    intrinsicTransform(const GABC_GEOPrim *p, fpreal64 *v, GA_Size size)
    {
	size = SYSmin(size, 16);
	UT_Matrix4D	xform;
	p->getTransform(xform);
	for (int i = 0; i < size; ++i)
	    v[i] = xform.data()[i];
	return size;
    }
    static GA_Size
    intrinsicSetGeoTransform(GABC_GEOPrim *p, const fpreal64 *v, GA_Size size)
    {
	if (size < 16)
	    return 0;
	UT_Matrix4D	m(v[0], v[1], v[2], v[3],
			  v[4], v[5], v[6], v[7],
			  v[8], v[9], v[10], v[11],
			  v[12], v[13], v[14], v[15]);
	p->setGeoTransform(GT_TransformHandle(new GT_Transform(&m, 1)));
	return 16;
    }
}

GA_START_INTRINSIC_DEF(GABC_GEOPrim, geo_NUM_INTRINISCS)

    GA_INTRINSIC_S(GABC_GEOPrim, geo_INTRINSIC_ABC_TYPE,
	    "abctypename", intrinsicTypeName)
    GA_INTRINSIC_S(GABC_GEOPrim, geo_INTRINSIC_ABC_FILE,
	    "abcfilename", intrinsicFilename)
    GA_INTRINSIC_S(GABC_GEOPrim, geo_INTRINSIC_ABC_PATH,
	    "abcobjectpath", intrinsicObjectPath)
    GA_INTRINSIC_METHOD_F(GABC_GEOPrim, geo_INTRINSIC_ABC_FRAME,
	    "abcframe", frame)
    GA_INTRINSIC_S(GABC_GEOPrim, geo_INTRINSIC_ABC_ANIMATION,
	    "abcanimation", intrinsicAnimation)
    GA_INTRINSIC_TUPLE_F(GABC_GEOPrim, geo_INTRINSIC_ABC_LOCALXFORM,
	    "abclocaltransform", 16, intrinsicLocalTransform)
    GA_INTRINSIC_TUPLE_F(GABC_GEOPrim, geo_INTRINSIC_ABC_WORLDXFORM,
	    "abcworldtransform", 16, intrinsicWorldTransform)
    GA_INTRINSIC_TUPLE_F(GABC_GEOPrim, geo_INTRINSIC_ABC_GEOXFORM,
	    "abcgeotransform", 16, intrinsicGeoTransform)
    GA_INTRINSIC_TUPLE_F(GABC_GEOPrim, geo_INTRINSIC_ABC_TRANSFORM,
	    "transform", 16, intrinsicTransform)

    GA_INTRINSIC_SET_TUPLE_F(GABC_GEOPrim, geo_INTRINSIC_ABC_GEOXFORM,
		    intrinsicSetGeoTransform);
    GA_INTRINSIC_SET_METHOD_F(GABC_GEOPrim, geo_INTRINSIC_ABC_FRAME, setFrame)

GA_END_INTRINSIC_DEF(GABC_GEOPrim, GEO_Primitive)

void
GABC_GEOPrim::setUseTransform(bool v)
{
    myUseTransform = v;
    if (!myUseTransform)
	myGTTransform = GT_Transform::identity();
}

void
GABC_GEOPrim::init(const std::string &filename,
	    const std::string &objectpath,
	    fpreal frame,
	    bool use_transform)
{
    myFilename = filename;
    myObjectPath = objectpath;
    myFrame = frame;
    myUseTransform = use_transform;
    setUseTransform(use_transform);
    resolveObject();
}

void
GABC_GEOPrim::init(const std::string &filename,
		const GABC_IObject &object,
		fpreal frame,
		bool use_transform)
{
    myFilename = filename;
    myObject = object;
    myObjectPath = object.getFullName();
    myUseTransform = use_transform;
    setUseTransform(use_transform);
    myFrame = frame;
    updateAnimation();
}

void
GABC_GEOPrim::setFilename(const std::string &filename)
{
    myFilename = filename;
    resolveObject();
}

void
GABC_GEOPrim::setObjectPath(const std::string &path)
{
    myObjectPath = path;
    resolveObject();
}

void
GABC_GEOPrim::setFrame(fpreal f)
{
    myFrame = f;
}

bool
GABC_GEOPrim::getTransform(UT_Matrix4D &xform) const
{
    if (!getABCWorldTransform(xform))
	return false;
    if (!myGeoTransform->isIdentity())
    {
	UT_Matrix4D	lxform;
	myGeoTransform->getMatrix(lxform, 0);
	xform *= lxform;
    }
    return true;
}


bool
GABC_GEOPrim::getABCWorldTransform(UT_Matrix4D &xform) const
{
    if (myGTTransform)
    {
	myGTTransform->getMatrix(xform, 0);
	return true;
    }
    if (!myObject.valid())
	return false;

    bool		 is_const = false;
    bool		 inheritsXform;
    if (myObject.worldTransform(myFrame, xform, is_const, inheritsXform))
    {
	if (is_const)
	{
	    GABC_GEOPrim	*me = const_cast<GABC_GEOPrim *>(this);
	    me->myGTTransform = GT_TransformHandle(new GT_Transform(&xform, 1));
	}
	return true;
    }
    xform.identity();
    return false;
}

bool
GABC_GEOPrim::getABCLocalTransform(UT_Matrix4D &xform) const
{
    if (!myObject.valid())
	return false;

    UT_AutoLock		lock(theH5Lock);
    bool		is_const = false;
    bool		inheritsXform;
    GABC_IObject	obj = myObject;
    if (abcNodeType() != GABC_XFORM)
	obj = obj.getParent();
    if (GABC_Util::getLocalTransform(myFilename, obj.getFullName(),
			myFrame, xform, is_const, inheritsXform))
    {
	return true;
    }
    xform.identity();
    return false;
}

void
GABC_GEOPrim::resolveObject()
{
    // TODO: Set myObject based on myFilename/myObjectPath
    myObject = GABC_Util::findObject(myFilename, myObjectPath);
    updateAnimation();
}
