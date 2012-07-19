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
#include <GA/GA_IntrinsicDef.h>
#include <GA/GA_IntrinsicEval.h>
#include <GA/GA_PrimitiveJSON.h>
#include <GT/GT_Transform.h>
#include <GEO/GEO_Detail.h>
#include <GEO/GEO_PrimType.h>
#include <UT/UT_Lock.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_JSONWriter.h>

#include <Alembic/AbcGeom/All.h>

static UT_Lock	theH5Lock;

GEO_ABCNameMap::GEO_ABCNameMap()
    : myMap()
    , myRefCount(0)
{
}

GEO_ABCNameMap::~GEO_ABCNameMap()
{
}

bool
GEO_ABCNameMap::isEqual(const GEO_ABCNameMap &src) const
{
    if (myMap.entries() != src.myMap.entries())
	return false;
    for (MapType::iterator it = myMap.begin(); !it.atEnd(); ++it)
    {
	UT_String	item;
	if (!src.myMap.findSymbol(it.name(), &item))
	    return false;
	if (item != it.thing())
	    return false;
    }
    return true;
}

const char *
GEO_ABCNameMap::getName(const char *name) const
{
    UT_String	entry;
    if (myMap.findSymbol(name, &entry))
    {
	return (const char *)entry;
    }
    return name;
}

void
GEO_ABCNameMap::addMap(const char *abcName, const char *houdiniName)
{
    UT_String	&entry = myMap[abcName];
    if (UTisstring(houdiniName))
	entry.harden(houdiniName);	// Replace existing entry
    else
	entry = NULL;
}

static bool
saveEmptyNameMap(UT_JSONWriter &w)
{
    bool	ok = w.jsonBeginMap();
    return ok && w.jsonEndMap();
}

bool
GEO_ABCNameMap::save(UT_JSONWriter &w) const
{
    bool	ok = true;
    ok = w.jsonBeginMap();
    for (MapType::iterator it = myMap.begin(); ok && !it.atEnd(); ++it)
    {
	const UT_String &str = it.thing();
	const char	*value = str.isstring() ? (const char *)str : "";
	ok = ok && w.jsonKeyToken(it.name());
	ok = ok && w.jsonStringToken(value);
    }
    return ok && w.jsonEndMap();
}

bool
GEO_ABCNameMap::load(GEO_ABCNameMapPtr &result, UT_JSONParser &p)
{
    GEO_ABCNameMap	*map = new GEO_ABCNameMap();
    bool		 ok = true;
    UT_WorkBuffer	 key, value;
    for (UT_JSONParser::iterator it = p.beginMap(); ok && !it.atEnd(); ++it)
    {
	ok = ok && it.getKey(key);
	ok = ok && p.parseString(value);
	map->addMap(key.buffer(), value.buffer());
    }
    if (!ok || map->entries() == 0)
	delete map;
    else
	result = map;
    return ok;
}

GABC_GEOPrim::GABC_GEOPrim(GEO_Detail *d, GA_Offset offset)
    : GEO_Primitive(d, offset)
    , myFilename()
    , myObjectPath()
    , myObject()
    , myFrame(0)
    , myAnimation(GABC_Util::ANIMATION_TOPOLOGY)
    , myUseTransform(true)
    , myGTPrimitive()
{
    myBox.makeInvalid();
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

    // Data which shouldn't be copied
    myBox.makeInvalid();
    myAnimation = GABC_Util::ANIMATION_TOPOLOGY;
    myGTPrimitive = GT_PrimitiveHandle();

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
}

void
GABC_GEOPrim::clearForDeletion()
{
    myGTPrimitive = GT_PrimitiveHandle();
    myGeoTransform = GT_Transform::identity();
}

void
GABC_GEOPrim::stashed(int onoff, GA_Offset offset)
{
    myGeoTransform = GT_Transform::identity();
    myGTPrimitive = GT_PrimitiveHandle();
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
GABC_GEOPrim::calcVolume(const UT_Vector3 &) const
{
    UT_BoundingBox	box;
    if (getBBox(&box))
	return box.volume();
    return 0;
}

fpreal
GABC_GEOPrim::calcArea() const
{
    UT_BoundingBox	box;
    if (getBBox(&box))
	return box.area();
    return 0;
}

fpreal
GABC_GEOPrim::calcPerimeter() const
{
    UT_BoundingBox	box;
    if (getBBox(&box))
	return box.area();
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
			    { return static_cast<const GABC_GEOPrim *>(p); }
    GABC_GEOPrim		*abc(GA_Primitive *p) const
			    { return static_cast<GABC_GEOPrim *>(p); }

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
			    const std::string	&f = abc(pr)->getFilename();
			    return w.jsonStringToken(f.c_str(), f.length());
			}
			case geo_OBJECTPATH:
			{
			    const std::string	&f = abc(pr)->getObjectPath();
			    return w.jsonStringToken(f.c_str(), f.length());
			}
			case geo_FRAME:
			    return w.jsonValue(abc(pr)->getFrame());

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
		    GEO_ABCNameMapPtr	amap;
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
			    if (!GEO_ABCNameMap::load(amap, p))
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
			    return abc(a)->getFilename() == abc(b)->getFilename();
			case geo_OBJECTPATH:
			    return abc(a)->getObjectPath() == abc(b)->getObjectPath();
			case geo_FRAME:
			    return abc(a)->getFrame() == abc(b)->getFrame();

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
    typedef Alembic::Abc::IObject		IObject;
    typedef Alembic::Abc::ISampleSelector	ISampleSelector;
    typedef Alembic::AbcGeom::ICompoundProperty	ICompoundProperty;
    typedef Alembic::AbcGeom::IXform		IXform;
    typedef Alembic::AbcGeom::IXformSchema	IXformSchema;
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

    static void
    assignBox(UT_BoundingBox &utbox, const Box3d &abcbox)
    {
	utbox.setBounds(abcbox.min[0], abcbox.min[1], abcbox.min[2],
			abcbox.max[0], abcbox.max[1], abcbox.max[2]);
    }

    template <typename ABC_T, typename SCHEMA_T>
    static void
    abcBounds(const IObject &obj, UT_BoundingBox &box,
		const ISampleSelector &iss)
    {
	UT_AutoLock			 lock(theH5Lock);
	ABC_T				 prim(obj, Alembic::Abc::kWrapExisting);
	SCHEMA_T			&ss = prim.getSchema();
	typename SCHEMA_T::Sample	 sample = ss.getValue(iss);
	assignBox(box, sample.getSelfBounds());
    }

#if 0
    template <typename ABC_T, typename SCHEMA_T>
    static bool
    abcIsConst(const IObject &obj)
    {
	ABC_T	prim(obj, Alembic::Abc::kWrapExisting);
	return prim.getSchema().isConstant();
    }

    static bool
    abcArbsAreAnimated(ICompoundProperty arb)
    {
	exint	narb = arb ? arb.getNumProperties() : 0;
	for (exint i = 0; i < narb; ++i)
	{
	    if (!GABC_Util::isConstant(arb, i))
		return true;
	}
	return false;
    }

    template <typename ABC_T, typename SCHEMA_T>
    static GABC_GEOPrim::AnimationType
    getAnimation(const IObject &obj)
    {
	ABC_T				 prim(obj, Alembic::Abc::kWrapExisting);
	SCHEMA_T			&schema = prim.getSchema();
	GABC_GEOPrim::AnimationType	 atype;
	
	switch (schema.getTopologyVariance())
	{
	    case Alembic::AbcGeom::kConstantTopology:
		atype = GABC_GEOPrim::AnimationConstant;
		if (abcArbsAreAnimated(schema.getArbGeomParams()))
		    atype = GABC_GEOPrim::AnimationAttribute;
		break;
	    case Alembic::AbcGeom::kHomogenousTopology:
		atype = GABC_GEOPrim::AnimationAttribute;
		break;
	    case Alembic::AbcGeom::kHeterogenousTopology:
		atype = GABC_GEOPrim::AnimationTopology;
		break;
	}
	return atype;
    }

    template <>
    GABC_GEOPrim::AnimationType
    getAnimation<IPoints, IPointsSchema>(const IObject &obj)
    {
	IPoints			 prim(obj, Alembic::Abc::kWrapExisting);
	IPointsSchema		&schema = prim.getSchema();
	if (!schema.isConstant())
	    return GABC_GEOPrim::AnimationTopology;
	if (abcArbsAreAnimated(schema.getArbGeomParams()))
	    return GABC_GEOPrim::AnimationAttribute;
	return GABC_GEOPrim::AnimationConstant;
    }

    template <>
    GABC_GEOPrim::AnimationType
    getAnimation<IXform, IXformSchema>(const IObject &obj)
    {
	IXform			 prim(obj, Alembic::Abc::kWrapExisting);
	IXformSchema		&schema = prim.getSchema();
	if (!schema.isConstant())
	    return GABC_GEOPrim::AnimationTopology;
	if (abcArbsAreAnimated(schema.getArbGeomParams()))
	    return GABC_GEOPrim::AnimationAttribute;
	return GABC_GEOPrim::AnimationConstant;
    }


    static GABC_GEOPrim::AnimationType
    getLocatorAnimation(const IObject &obj)
    {
	IXform				prim(obj, Alembic::Abc::kWrapExisting);
	Alembic::Abc::IScalarProperty	loc(prim.getProperties(), "locator");
	return loc.isConstant() ? GABC_GEOPrim::AnimationConstant
				: GABC_GEOPrim::AnimationAttribute;
    }

    static bool
    abcCurvesChangingTopology(const IObject &obj)
    {
	// There's a bug in Alembic 1.0.5 which doesn't properly detect
	// heterogenous topology.
	ICurves		 curves(obj, Alembic::Abc::kWrapExisting);
	ICurvesSchema	&schema = curves.getSchema();
	if (!schema.getNumVerticesProperty().isConstant())
	    return true;
	return false;
    }
#endif
}

void
GABC_GEOPrim::updateAnimation()
{
    UT_ASSERT_P(myObject.valid());
    // Set the topology based on a combination of conditions.
    // We initialize based on the shape topology, but if the shape topology is
    // constant, there are still various factors which can make the primitive
    // non-constant (i.e. Homogeneous).
    myAnimation = GABC_Util::getAnimationType(myFilename,
			myObject, myUseTransform);
#if 0
    switch (GABC_Util::getNodeType(myObject))
    {
	case GABC_Util::GABC_POLYMESH:
	    myAnimation = getAnimation<IPolyMesh, IPolyMeshSchema>(myObject);
	    break;
	case GABC_Util::GABC_SUBD:
	    myAnimation = getAnimation<ISubD, ISubDSchema>(myObject);
	    break;
	case GABC_Util::GABC_CURVES:
	    myAnimation = getAnimation<ICurves, ICurvesSchema>(myObject);
	    // There's a bug in Alembic 1.0.5 detecting changing topology on
	    // curves.
	    if (myAnimation != AnimationTopology
		    && abcCurvesChangingTopology(myObject))
	    {
		myAnimation = AnimationTopology;
	    }
	    break;
	case GABC_Util::GABC_POINTS:
	    myAnimation = getAnimation<IPoints, IPointsSchema>(myObject);
	    break;
	case GABC_Util::GABC_NUPATCH:
	    myAnimation = getAnimation<INuPatch, INuPatchSchema>(myObject);
	    break;
	case GABC_Util::GABC_XFORM:
	    if (GABC_Util::isMayaLocator(myObject))
		myAnimation = getLocatorAnimation(myObject);
	    else
		myAnimation = getAnimation<IXform, IXformSchema>(myObject);
	    break;
	default:
	    myAnimation = AnimationTopology;
	    return;
    }
    if (myAnimation == AnimationConstant && myUseTransform)
    {
	UT_Matrix4D	xform;
	bool		is_const;
	IObject		 parent = myObject.getParent();
	// Check to see if transform is non-constant
	if (GABC_Util::getWorldTransform(myFilename, parent.getFullName(),
			myFrame, xform, is_const))
	{
	    if (!is_const)
		myAnimation = AnimationTransform;
	}
    }
#endif
}

#if 0
bool
GABC_GEOPrim::isConstant() const
{
    switch (GABC_Util::getNodeType(myObject))
    {
	case GABC_Util::GABC_POLYMESH:
	    return abcIsConst<IPolyMesh, IPolyMeshSchema>(myObject);
	case GABC_Util::GABC_SUBD:
	    return abcIsConst<ISubD, ISubDSchema>(myObject);
	case GABC_Util::GABC_CURVES:
	    return abcIsConst<ICurves, ICurvesSchema>(myObject);
	case GABC_Util::GABC_POINTS:
	    return abcIsConst<IPoints, IPointsSchema>(myObject);
	case GABC_Util::GABC_NUPATCH:
	    return abcIsConst<INuPatch, INuPatchSchema>(myObject);
	case GABC_Util::GABC_XFORM:
	    if (GABC_Util::isMayaLocator(myObject))
		return myTopology == Alembic::AbcGeom::kConstantTopology;
	    return abcIsConst<IXform, IXformSchema>(myObject);
	default:
	    break;	// Unsupported object type
    }

    return false;
}
#endif

int
GABC_GEOPrim::getBBox(UT_BoundingBox *bbox) const
{
    // Return the bounding box area
    if (!myObject.valid())
	return 0;

    if (!myBox.isValid())
    {
	ISampleSelector	sample(myFrame);

	switch (GABC_Util::getNodeType(myObject))
	{
	    case GABC_Util::GABC_POLYMESH:
		abcBounds<IPolyMesh, IPolyMeshSchema>(myObject, *bbox, sample);
		break;
	    case GABC_Util::GABC_SUBD:
		abcBounds<ISubD, ISubDSchema>(myObject, *bbox, sample);
		break;
	    case GABC_Util::GABC_CURVES:
		abcBounds<ICurves, ICurvesSchema>(myObject, *bbox, sample);
		break;
	    case GABC_Util::GABC_POINTS:
		abcBounds<IPoints, IPointsSchema>(myObject, *bbox, sample);
		break;
	    case GABC_Util::GABC_NUPATCH:
		abcBounds<INuPatch, INuPatchSchema>(myObject, *bbox, sample);
		break;
	    case GABC_Util::GABC_XFORM:
		bbox->initBounds(0, 0, 0);	// Locator is just a point
		break;
	    default:
		return 0;	// Unsupported object type
	}
	myBox = *bbox;
    }
    else
    {
	*bbox = myBox;
    }
    UT_Matrix4D	xform;
    getTransform(xform);
    bbox->transform(xform);
    return 1;
}

#if 0
static void
dump(const GT_TransformHandle &x, const char *msg)
{
    UT_WorkBuffer	tmp;
    UT_AutoJSONWriter	w(tmp);
    x->save(w);
    fprintf(stderr, "%s: %s\n", msg, tmp.buffer());
}
#endif

void
GABC_GEOPrim::setGeoTransform(const GT_TransformHandle &x)
{
    myGeoTransform = x;
    if (myGTPrimitive)
    {
	UT_Matrix4D	xform;
	if (getTransform(xform))
	{
	    GT_Transform	*x = new GT_Transform(&xform, 1);
	    myGTPrimitive->setPrimitiveTransform(GT_TransformHandle(x));
	}
    }
}

void
GABC_GEOPrim::setAttributeNameMap(const GEO_ABCNameMapPtr &m)
{
    myAttributeNameMap = m;
    myGTPrimitive = GT_PrimitiveHandle();	// Rebuild primitive
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

    const GABC_GEOPrim	 *src = static_cast<const GABC_GEOPrim *>(psrc);

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
	GABC_GEOPrim	*abc = static_cast<GABC_GEOPrim *>(clone);
	abc->copyMemberDataFrom(*this);
    }
    return clone;
}

void
GABC_GEOPrim::copyUnwiredForMerge(const GA_Primitive *psrc, const GA_MergeMap &)
{
    UT_ASSERT( psrc != this );
    const GABC_GEOPrim	*src = static_cast<const GABC_GEOPrim *>(psrc);

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
    geo_INTRINSIC_ABC_TRANSFORM,	// Transform
};

static GA_IntrinsicDef	theABCIntrinsics;

GA_IntrinsicManager::Registrar
GABC_GEOPrim::registerIntrinsics(GA_PrimitiveDefinition &defn)
{
    // Register base class intrinsics
    GA_IntrinsicManager::Registrar r = GEO_Primitive::registerIntrinsics(defn);

    if (r.start(theABCIntrinsics))
    {
	r.addAttribute(GA_STORECLASS_STRING, "abctypename",
		geo_INTRINSIC_ABC_TYPE, true);
	r.addAttribute(GA_STORECLASS_STRING, "abcfilename",
		geo_INTRINSIC_ABC_FILE, true);
	r.addAttribute(GA_STORECLASS_STRING, "abcobjectpath",
		geo_INTRINSIC_ABC_PATH, true);
	r.addAttribute(GA_STORECLASS_FLOAT, "abcframe",
		geo_INTRINSIC_ABC_FRAME, true);
	r.addAttribute(GA_STORECLASS_STRING, "abcanimation",
		geo_INTRINSIC_ABC_ANIMATION, true);
	r.addAttribute(GA_STORECLASS_STRING, "abcworldtransform",
		geo_INTRINSIC_ABC_TRANSFORM, true);
    }
    return r;
}

int
GABC_GEOPrim::localIntrinsicTupleSize(const GA_IntrinsicEval &eval) const
{
    switch (eval.getUserId(theABCIntrinsics))
    {
	case geo_INTRINSIC_ABC_TYPE:
	case geo_INTRINSIC_ABC_FILE:
	case geo_INTRINSIC_ABC_PATH:
	case geo_INTRINSIC_ABC_FRAME:
	case geo_INTRINSIC_ABC_ANIMATION:
	    return 1;
	case geo_INTRINSIC_ABC_TRANSFORM:
	    return 16;
    }
    return GEO_Primitive::localIntrinsicTupleSize(eval);
}

int
GABC_GEOPrim::localGetIntrinsicF(const GA_IntrinsicEval &eval,
	fpreal *val, GA_Size sz) const
{
    switch (eval.getUserId(theABCIntrinsics))
    {
	case geo_INTRINSIC_ABC_FRAME:
	    val[0] = myFrame;
	    return 1;

	case geo_INTRINSIC_ABC_TRANSFORM:
	    if (sz >= 16)
	    {
		UT_Matrix4D	xform;
		getTransform(xform);
		for (int i = 0; i < 16; ++i)
		    val[i] = xform.data()[i];
		return 16;
	    }
	    break;
    }
    return GEO_Primitive::localGetIntrinsicF(eval, val, sz);
}

int
GABC_GEOPrim::localGetIntrinsicS(const GA_IntrinsicEval &eval,
	UT_String &value) const
{
    switch (eval.getUserId(theABCIntrinsics))
    {
	case geo_INTRINSIC_ABC_TYPE:
	    {
		GABC_Util::NodeType	type = GABC_Util::getNodeType(myObject);
		value = GABC_Util::nodeType(type);
	    }
	    return 1;
	case geo_INTRINSIC_ABC_FILE:
	    value.harden(myFilename.c_str());
	    return 1;
	case geo_INTRINSIC_ABC_PATH:
	    value.harden(myObjectPath.c_str());
	    return 1;
	case geo_INTRINSIC_ABC_ANIMATION:
	    switch (myAnimation)
	    {
		case GABC_Util::ANIMATION_CONSTANT:
		    value = "constant";
		    break;
		case GABC_Util::ANIMATION_TRANSFORM:
		    value = "transform";
		    break;
		case GABC_Util::ANIMATION_ATTRIBUTE:
		    value = "attribute";
		    break;
		case GABC_Util::ANIMATION_TOPOLOGY:
		    value = "topology";
		    break;
		default:
		    value = "<unknown>";
		    break;
	    }
	    return 1;
    }
    return GEO_Primitive::localGetIntrinsicS(eval, value);
}

int
GABC_GEOPrim::localGetIntrinsicSA(const GA_IntrinsicEval &eval,
	UT_StringArray &strings) const
{
    UT_String	single;
    switch (eval.getUserId(theABCIntrinsics))
    {
	case geo_INTRINSIC_ABC_TYPE:
	case geo_INTRINSIC_ABC_FILE:
	case geo_INTRINSIC_ABC_PATH:
	case geo_INTRINSIC_ABC_ANIMATION:
	    if (localGetIntrinsicS(eval, single) == 1)
	    {
		strings.append(single);
		return 1;
	    }
	    break;
    }
    return GEO_Primitive::localGetIntrinsicSA(eval, strings);
}

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
		const IObject &object,
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
    if (myAnimation != GABC_Util::ANIMATION_CONSTANT)
	myBox.makeInvalid();
}

bool
GABC_GEOPrim::getTransform(UT_Matrix4D &xform) const
{
    if (!getABCTransform(xform))
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
GABC_GEOPrim::getABCTransform(UT_Matrix4D &xform) const
{
    if (myGTTransform)
    {
	myGTTransform->getMatrix(xform, 0);
	return true;
    }
    //fprintf(stderr, "%p Non const: %s (%p)\n", this, myObjectPath.c_str(), myGTTransform.get());
    if (!myObject.valid())
	return false;

    UT_AutoLock		 lock(theH5Lock);
    bool		 is_const = false;
    IObject		 parent = const_cast<IObject &>(myObject).getParent();
    if (GABC_Util::getWorldTransform(myFilename, parent.getFullName(),
			myFrame, xform, is_const))
    {
	if (is_const)
	    myGTTransform = GT_TransformHandle(new GT_Transform(&xform, 1));
	return true;
    }
    return false;
}

void
GABC_GEOPrim::resolveObject()
{
    // TODO: Set myObject based on myFilename/myObjectPath
    myObject = GABC_Util::findObject(myFilename, myObjectPath);
    updateAnimation();
}
