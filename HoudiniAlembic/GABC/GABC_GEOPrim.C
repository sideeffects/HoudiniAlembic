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

#include "GABC_GEOPrim.h"
#if !defined(GABC_PACKED)
#include "GABC_Util.h"
#include <GA/GA_IntrinsicMacros.h>
#include <GA/GA_PrimitiveJSON.h>
#include <GA/GA_SaveMap.h>
#include <GA/GA_Defragment.h>
#include <GA/GA_RangeMemberQuery.h>
#include <GA/GA_MergeMap.h>
#include <GT/GT_Transform.h>
#include <GEO/GEO_Detail.h>
#include <GEO/GEO_PrimType.h>
#include <UT/UT_Lock.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_JSONWriter.h>

#include <Alembic/AbcGeom/All.h>

#define GABC_SHARED_DATA_NAMEMAP	0

using namespace GABC_NAMESPACE;

namespace
{
    static bool
    saveEmptyNameMap(UT_JSONWriter &w)
    {
	bool	ok = w.jsonBeginMap();
	return ok && w.jsonEndMap();
    }

#if 0
    static bool
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
#endif
}

GABC_GEOPrim::GABC_GEOPrim(GEO_Detail *d, GA_Offset offset)
    : GEO_Primitive(d, offset)
    , myFilename()
    , myObjectPath()
    , myObject()
    , myFrame(0)
    , myGTPrimitive(new GABC_GTPrimitive(this))
    , myVertex(GA_INVALID_OFFSET)
    , myViewportLOD(GABC_VIEWPORT_FULL)
    , myUseTransform(true)
    , myUseVisibility(true)
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
    myUseVisibility = src.myUseVisibility;
    myGTPrimitive->copyFrom(*src.myGTPrimitive);

    setViewportLOD(src.viewportLOD());

    // TODO: NEED TO CLEAR THE TRANSFORM on the cache!

    if (!myUseTransform)
    {
	// Should be an identity
	UT_Matrix4D	xform;
	myGTTransform = src.myGTTransform;
	myGTTransform->getMatrix(xform);
	myGTPrimitive->updateTransform(xform);
    }
    else
    {
	myGTTransform = GT_TransformHandle();
    }
}

GABC_GEOPrim::~GABC_GEOPrim()
{
    if (GAisValid(myVertex))
	destroyVertex(myVertex);
    myGTPrimitive->decref();	// Old school reference counting
}

void
GABC_GEOPrim::clearForDeletion()
{
    myVertex = GA_INVALID_OFFSET;
    myGTPrimitive->clear();
    myGeoTransform = GT_Transform::identity();
}

void
GABC_GEOPrim::stashed(int onoff, GA_Offset offset)
{
    myGeoTransform = GT_Transform::identity();
    myGTPrimitive->clear();
    GEO_Primitive::stashed(onoff, offset);
    myVertex = onoff ? GA_INVALID_OFFSET : allocateVertex();
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
    return GAisValid(myVertex) ? 1 : 0;
}

GA_Offset
GABC_GEOPrim::getVertexOffset(GA_Size idx) const
{
    UT_ASSERT_P(idx == 0);
    return idx == 0 ? myVertex : GA_INVALID_OFFSET;
}

void
GABC_GEOPrim::ensureVertexCreated()
{
    if (!GAisValid(myVertex))
	myVertex = allocateVertex();
}

void
GABC_GEOPrim::setVertexPoint(GA_Offset point)
{
    if (!GAisValid(myVertex))
	ensureVertexCreated();
    wireVertex(myVertex, point);
}

void
GABC_GEOPrim::assignVertex(GA_Offset vtx, bool update_topology)
{
    if (myVertex != vtx)
    {
	if (GAisValid(myVertex))
	    destroyVertex(myVertex);
	myVertex = vtx;
	if (update_topology)
	    registerVertex(myVertex);
    }
}

int
GABC_GEOPrim::detachPoints(GA_PointGroup &grp)
{
    return GAisValid(myVertex)
	    && grp.containsOffset(getParent()->vertexPoint(myVertex)) ? -2 : 0;
}

GA_Primitive::GA_DereferenceStatus
GABC_GEOPrim::dereferencePoint(GA_Offset pt, bool)
{
    return getParent()->vertexPoint(myVertex) == pt
		? GA_DEREFERENCE_DESTROY
		: GA_DEREFERENCE_OK;
}

GA_Primitive::GA_DereferenceStatus
GABC_GEOPrim::dereferencePoints(const GA_RangeMemberQuery &pq, bool)
{
    return pq.contains(getParent()->vertexPoint(myVertex))
		? GA_DEREFERENCE_DESTROY
		: GA_DEREFERENCE_OK;
}

///
/// JSON methods
///

namespace
{

/// Return true of the attribute name map needs to be saved
static bool
nameMapSharedKey(UT_WorkBuffer &key, const GABC_GEOPrim *prim)
{
    const GABC_NameMapPtr	&map = prim->attributeNameMap();
    if (!map || !map->entries())
    {
	key.clear();
	return false;
    }
    key.sprintf("amap:%p", prim->attributeNameMap().get());
    return true;
}

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
	geo_VERTEX,
	geo_USETRANSFORM,
	geo_USEVISIBILITY,
	geo_ATTRIBUTEMAP_SHARED,
	geo_ATTRIBUTEMAP_UNIQUE,
	geo_ENTRIES
    };

    const GABC_GEOPrim	*abc(const GA_Primitive *p) const
			    { return UTverify_cast<const GABC_GEOPrim *>(p); }
    GABC_GEOPrim	*abc(GA_Primitive *p) const
			    { return UTverify_cast<GABC_GEOPrim *>(p); }

    virtual int		getEntries() const	{ return geo_ENTRIES; }
    virtual const char	*getKeyword(int i) const
    {
	switch (i)
	{
	    case geo_FILENAME:			return "filename";
	    case geo_OBJECTPATH:		return "object";
	    case geo_FRAME:			return "frame";
	    case geo_TRANSFORM:			return "ltransform";
	    case geo_USETRANSFORM:		return "usetransform";
	    case geo_USEVISIBILITY:		return "usevisibility";
	    case geo_ATTRIBUTEMAP_UNIQUE:	return "attributemap";
	    case geo_ATTRIBUTEMAP_SHARED:	return "sharedattributemap";
	    case geo_VERTEX:			return "vertex";
	    case geo_ENTRIES:			break;
	}
	UT_ASSERT(0);
	return NULL;
    }
    virtual bool shouldSaveField(const GA_Primitive *pr, int i,
			    const GA_SaveMap &map) const
    {
	UT_WorkBuffer	nmapkey;
	switch (i)
	{
	    case geo_TRANSFORM:
		return !abc(pr)->geoTransform()->isIdentity();
	    case geo_ATTRIBUTEMAP_SHARED:
		if (!nameMapSharedKey(nmapkey, abc(pr)))
		    return false;
		return true;
	    case geo_ATTRIBUTEMAP_UNIQUE:
		return false;
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

	    case geo_USEVISIBILITY:
		return w.jsonBool(abc(pr)->useVisibility());

	    case geo_ATTRIBUTEMAP_UNIQUE:
	    {
		if (abc(pr)->attributeNameMap())
		    return abc(pr)->attributeNameMap()->save(w);
		return saveEmptyNameMap(w);
	    }
	    case geo_ATTRIBUTEMAP_SHARED:
	    {
		UT_WorkBuffer	nmapkey;
		nameMapSharedKey(nmapkey, abc(pr));
		return w.jsonString(nmapkey.buffer());
	    }
	    case geo_VERTEX:
	    {
		GA_Offset	vtx = abc(pr)->getVertexOffset(0);
		return w.jsonInt(GA_Size(map.getVertexIndex(vtx)));
	    }

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
	int64		ival;
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
	    case geo_USEVISIBILITY:
		if (!p.parseBool(bval))
		    return false;
		abc(pr)->setUseVisibility(bval);
		return true;
	    case geo_ATTRIBUTEMAP_UNIQUE:
		if (!GABC_NameMap::load(amap, p))
		    return false;
		abc(pr)->setAttributeNameMap(amap);
		return true;
	    case geo_ATTRIBUTEMAP_SHARED:
	    {
		UT_WorkBuffer	key;
		if (!p.parseString(key))
		    return false;
		if (key.length())
		{
		    map.needSharedData(key.buffer(), pr,
				    GABC_SHARED_DATA_NAMEMAP);
		}
		return true;
	    }
	    case geo_VERTEX:
		if (!p.parseInt(ival))
		    return false;
		ival = map.getVertexOffset(ival);
		abc(pr)->assignVertex(GA_Offset(ival), false);
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
	    case geo_USEVISIBILITY:
		return abc(a)->useVisibility() ==
			abc(b)->useVisibility();
	    case geo_ATTRIBUTEMAP_SHARED:
		// Just compare pointers directly
		return abc(a)->attributeNameMap().get() ==
			abc(b)->attributeNameMap().get();
	    case geo_ATTRIBUTEMAP_UNIQUE:
	    case geo_ENTRIES:
	    case geo_VERTEX:
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

bool
GABC_GEOPrim::saveSharedLoadData(UT_JSONWriter &w, GA_SaveMap &save) const
{
    UT_WorkBuffer	key;
    bool		ok = true;
    if (nameMapSharedKey(key, this))
    {
	if (!save.hasSavedSharedData(key.buffer()))
	{
	    save.setSavedSharedData(key.buffer());
	    UT_ASSERT(attributeNameMap());
	    ok = ok && w.jsonStringToken(getTypeName());
	    ok = ok && w.jsonBeginArray();
	    ok = ok && w.jsonStringToken("namemap");
	    ok = ok && w.jsonStringToken(key.buffer());
	    ok = ok && attributeNameMap()->save(w);
	    ok = ok && w.jsonEndArray();
	}
    }
    return ok;
}

bool
GABC_GEOPrim::loadSharedLoadData(int dtype, const GA_SharedLoadData *vitem)
{
    UT_ASSERT(dtype == GABC_SHARED_DATA_NAMEMAP);
    const GABC_NameMap::LoadContainer	*item;
    item = UTverify_cast<const GABC_NameMap::LoadContainer *>(vitem);
    if (item)
	setAttributeNameMap(item->myNameMap);
    return true;
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
    myGTPrimitive->updateAnimation(myUseTransform, myUseVisibility);
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
    if (!myObject.valid())
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

bool
GABC_GEOPrim::needTransform() const
{
    if (myUseTransform)			// If we us abc transforms
    {
	return true;
    }
    if (!myGeoTransform->isIdentity())	// check if there's a local xform
    {
	return true;
    }
    if (GAisValid(myVertex))
    {
	const GA_Detail	&gdp = getDetail();
	GA_Offset	pt = gdp.vertexPoint(myVertex);
	if (GAisValid(pt))
	{
	    UT_Vector3	P = gdp.getPos3(pt);
	    if (!P.equalZero())
	    {
		return true;
	    }
	}
    }
    return false;
}

GT_PrimitiveHandle
GABC_GEOPrim::gtPointCloud() const
{
    GABC_AnimationType	atype;
    GT_PrimitiveHandle	result;

    result = myObject.getPointCloud(myFrame, atype);

    if (needTransform())
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

    if (needTransform())
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
    if (!myGTPrimitive->visible())
	return GT_PrimitiveHandle();
    if (needTransform())
    {
	UT_Matrix4D	xform;
	if (getTransform(xform))
	    myGTPrimitive->updateTransform(xform);
    }
    return GT_PrimitiveHandle(myGTPrimitive);
}

GT_PrimitiveHandle
GABC_GEOPrim::gtPrimitive(const GT_PrimitiveHandle &attrib_prim,
				const UT_StringMMPattern *vertex,
				const UT_StringMMPattern *point,
				const UT_StringMMPattern *uniform,
				const UT_StringMMPattern *detail,
				const GT_RefineParms *parms) const
{
    if (!myGTPrimitive->visible())
	return GT_PrimitiveHandle();
    if (!attrib_prim || (!vertex && !point && !uniform && !detail))
	return gtPrimitive();

    // Get the base primitive
    GT_PrimitiveHandle		 base = gtPrimitive();
    const GABC_GTPrimitive	*p0 = UTverify_cast<const GABC_GTPrimitive *>(base.get());
    GT_PrimitiveHandle		 result = p0->getRefined(parms);

    GT_Primitive		*p1 = NULL;
    GT_PrimitiveHandle		 tmp;

    const GABC_GTPrimitive	*abc = dynamic_cast<const GABC_GTPrimitive *>(attrib_prim.get());
    if (abc)
    {
	// If it's an Alembic primitive, get its refined geometry
	tmp = abc->getRefined(parms);
	p1 = tmp.get();
    }
    else
    {
	// Assume it's the right type of primitive
	p1 = attrib_prim.get();
    }
    if (!p1)
	return result;
    return result->attributeMerge(*p1, vertex, point, uniform, detail);
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
    if (GAisValid(myVertex))
    {
	// If we're tied to a point, the point will be transformed, so we don't
	// want to pick up the translates.
	UT_Matrix3	m3(xform);
	setGeoTransform(GT_TransformHandle(myGeoTransform->multiply(m3)));
    }
    else
    {
	// Otherwise, we perform a full translation.
	setGeoTransform(GT_TransformHandle(myGeoTransform->multiply(xform)));
    }
}

void
GABC_GEOPrim::getLocalTransform(UT_Matrix3D &matrix) const
{
    UT_Matrix4D	m4;
    myGeoTransform->getMatrix(m4);
    matrix = m4;
}

void
GABC_GEOPrim::setLocalTransform(const UT_Matrix3D &matrix)
{
    UT_Matrix4D	m4;
    m4 = matrix;
    setGeoTransform(GT_TransformHandle(new GT_Transform(&m4, 1)));
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
GABC_GEOPrim::copyOffsetPrimitive(const GEO_Primitive *psrc, GA_Index)
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
GABC_GEOPrim::copyUnwiredForMerge(const GA_Primitive *psrc,
	const GA_MergeMap &map)
{
    UT_ASSERT( psrc != this );
    const GABC_GEOPrim	*src = UTverify_cast<const GABC_GEOPrim *>(psrc);

    copyMemberDataFrom(*src);

    // Assign my vertex based on the source vertex.
    if (GAisValid(myVertex))
    {
	destroyVertex(myVertex);
	myVertex = GA_INVALID_OFFSET;
    }
    if (GAisValid(src->myVertex))
	myVertex = map.mapDestFromSource(GA_ATTRIB_VERTEX, src->myVertex);
}

void
GABC_GEOPrim::swapVertexOffsets(const GA_Defragment &defrag)
{
    myVertex = defrag.mapOffset(myVertex);
}

enum
{
    geo_INTRINSIC_ABC_TYPE,	// Alembic primitive type
    geo_INTRINSIC_ABC_FILE,	// Filename
    geo_INTRINSIC_ABC_PATH,	// Object path
    geo_INTRINSIC_ABC_FRAME,	// Time
    geo_INTRINSIC_ABC_ANIMATION,	// Animation type
    geo_INTRINSIC_ABC_USEVISIBILITY,	// Whether prim respects visibility
    geo_INTRINSIC_ABC_VISIBILITY,	// Visibility (on node)
    geo_INTRINSIC_ABC_FULLVISIBILITY,	// Full visibility (including parent)
    geo_INTRINSIC_ABC_LOCALXFORM,	// Alembic file local transform
    geo_INTRINSIC_ABC_WORLDXFORM,	// Alembic file world transform
    geo_INTRINSIC_ABC_GEOXFORM,		// Geometry transform
    geo_INTRINSIC_ABC_TRANSFORM,	// Combined transform
    geo_INTRINSIC_ABC_VIEWPORTLOD,	// Viewport LOD

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
    intrinsicViewportLOD(const GABC_GEOPrim *p)
    {
	return GABCviewportLOD(p->viewportLOD());
    }
    static bool
    intrinsicNodeVisibility(const GABC_GEOPrim *p)
    {
	bool	animated;
	return p->object().visibility(animated, p->frame(), false);
    }
    static bool
    intrinsicFullVisibility(const GABC_GEOPrim *p)
    {
	bool	animated;
	return p->object().visibility(animated, p->frame(), true);
    }
    static const char *
    intrinsicAnimation(const GABC_GEOPrim *p)
    {
	return GABCanimationType(p->animation());
    }
    static GA_Size
    intrinsicGeoTransform(const GABC_GEOPrim *p, fpreal64 *v, GA_Size size)
    {
	size = SYSmin(size, 9);
	UT_Matrix3D	xform;
	p->getLocalTransform(xform);
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
	if (size < 9)
	    return 0;
	UT_Matrix3D	m(v[0], v[1], v[2],
			  v[3], v[4], v[5],
			  v[6], v[7], v[8]);
	p->setLocalTransform(m);
	return 9;
    }
    static GA_Size
    intrinsicSetUseVisibility(GABC_GEOPrim *p, const int64 v)
    {
	p->setUseVisibility(v != 0);
	return 1;
    }
    static GA_Size
    intrinsicSetViewportLOD(GABC_GEOPrim *prim, const char *lod)
    {
	GABC_ViewportLOD	val = GABCviewportLOD(lod);
	if (val >= 0)
	{
	    prim->setViewportLOD(val);
	    return 1;
	}
	return 0;
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
    GA_INTRINSIC_METHOD_I(GABC_GEOPrim, geo_INTRINSIC_ABC_USEVISIBILITY,
	    "abcusevisibility", useVisibility)
    GA_INTRINSIC_I(GABC_GEOPrim, geo_INTRINSIC_ABC_VISIBILITY,
	    "abcvisibility", intrinsicNodeVisibility)
    GA_INTRINSIC_I(GABC_GEOPrim, geo_INTRINSIC_ABC_FULLVISIBILITY,
	    "abcfullvisibility", intrinsicFullVisibility)
    GA_INTRINSIC_TUPLE_F(GABC_GEOPrim, geo_INTRINSIC_ABC_LOCALXFORM,
	    "abclocaltransform", 16, intrinsicLocalTransform)
    GA_INTRINSIC_TUPLE_F(GABC_GEOPrim, geo_INTRINSIC_ABC_WORLDXFORM,
	    "abcworldtransform", 16, intrinsicWorldTransform)
    GA_INTRINSIC_TUPLE_F(GABC_GEOPrim, geo_INTRINSIC_ABC_GEOXFORM,
	    "abcgeotransform", 9, intrinsicGeoTransform)
    GA_INTRINSIC_TUPLE_F(GABC_GEOPrim, geo_INTRINSIC_ABC_TRANSFORM,
	    "transform", 16, intrinsicTransform)
    GA_INTRINSIC_S(GABC_GEOPrim, geo_INTRINSIC_ABC_VIEWPORTLOD,
	    "abcviewportlod", intrinsicViewportLOD)

    GA_INTRINSIC_SET_TUPLE_F(GABC_GEOPrim, geo_INTRINSIC_ABC_GEOXFORM,
		    intrinsicSetGeoTransform);
    GA_INTRINSIC_SET_I(GABC_GEOPrim, geo_INTRINSIC_ABC_USEVISIBILITY,
		    intrinsicSetUseVisibility)
    GA_INTRINSIC_SET_METHOD_F(GABC_GEOPrim, geo_INTRINSIC_ABC_FRAME, setFrame)
    GA_INTRINSIC_SET_S(GABC_GEOPrim, geo_INTRINSIC_ABC_VIEWPORTLOD,
		    intrinsicSetViewportLOD)

GA_END_INTRINSIC_DEF(GABC_GEOPrim, GEO_Primitive)

void
GABC_GEOPrim::setUseTransform(bool v)
{
    myUseTransform = v;
    if (!myUseTransform)
	myGTTransform = GT_Transform::identity();
}

void
GABC_GEOPrim::setUseVisibility(bool v)
{
    if (v != myUseVisibility)
    {
	myUseVisibility = v;
	if (!myUseVisibility)
	    myGTPrimitive->setVisibilityCache(NULL);
	updateAnimation();
    }
}

void
GABC_GEOPrim::setViewportLOD(GABC_ViewportLOD vlod)
{
    if (vlod >= 0 && vlod != myViewportLOD)
    {
	myViewportLOD = vlod;
	myGTPrimitive->clear();	// Clear cache
    }
}

void
GABC_GEOPrim::init(const std::string &filename,
	    const std::string &objectpath,
	    fpreal frame,
	    bool use_transform,
	    bool use_visibility)
{
    myFilename = filename;
    myObjectPath = objectpath;
    myFrame = frame;
    myUseTransform = use_transform;
    setUseTransform(use_transform);
    setUseVisibility(use_visibility);
    setViewportLOD(GABC_VIEWPORT_FULL);
    resolveObject();
}

void
GABC_GEOPrim::init(const std::string &filename,
		const GABC_IObject &object,
		fpreal frame,
		bool use_transform,
		bool use_visibility)
{
    myFilename = filename;
    myObject = object;
    myObjectPath = object.getFullName();
    setUseTransform(use_transform);
    setUseVisibility(use_visibility);
    setViewportLOD(GABC_VIEWPORT_FULL);
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
    if (f != myFrame)
    {
	bool	update = false;

	// If we don't have any transforms involved, we can do a quick check
	// and clamp the time to the animation time.  If the time stamp is
	// outside the animation time, we don't need to reset the GT primitive
	// cache.
	if (myUseTransform)
	{
	    update = true;
	}
	else if (myObject.valid())
	{
	    update = myObject.clampTime(myFrame) != myObject.clampTime(f);
	}
	myFrame = f;
	if (update)
	    myGTPrimitive->updateAnimation(myUseTransform, myUseVisibility);
    }
}

bool
GABC_GEOPrim::getTransform(UT_Matrix4D &xform) const
{
    if (!getABCWorldTransform(xform))
	xform.identity();

    if (!myGeoTransform->isIdentity())
    {
	UT_Matrix4D	lxform;
	myGeoTransform->getMatrix(lxform, 0);
	xform *= lxform;
    }
    if (GAisValid(myVertex))
    {
	const GA_Detail	&gdp = getDetail();
	GA_Offset	pt = gdp.vertexPoint(myVertex);
	UT_Vector3	P = getParent()->getPos3(pt);
	xform.translate(P.x(), P.y(), P.z());
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

    bool		is_const = false;
    bool		inheritsXform;
    return myObject.localTransform(myFrame, xform, is_const, inheritsXform);
}

void
GABC_GEOPrim::resolveObject()
{
    // TODO: Set myObject based on myFilename/myObjectPath
    myObject = GABC_Util::findObject(myFilename, myObjectPath);
    updateAnimation();
}
#endif
