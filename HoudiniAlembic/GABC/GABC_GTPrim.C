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
#include "GABC_Util.h"
#include "GABC_GTArray.h"
#include <Alembic/AbcGeom/All.h>
#include <UT/UT_StackBuffer.h>
#include <GT/GT_Refine.h>
#include <GT/GT_GEOPrimitive.h>
#include <GT/GT_Util.h>
#include <GT/GT_DANumeric.h>
#include <GT/GT_DAConstantValue.h>
#include <GT/GT_DAIndexedString.h>
#include <GT/GT_PrimPolygonMesh.h>
#include <GT/GT_PrimCurveMesh.h>
#include <GT/GT_PrimPointMesh.h>
#include <GT/GT_PrimNuPatch.h>
#include <GT/GT_TrimNuCurves.h>
#include <GT/GT_AttributeList.h>
#include <GT/GT_Transform.h>

using namespace Alembic::AbcGeom;

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

// Implementation of GT primitives for ABC

// ABC_T := A shared pointer to a TypedArraySample
template <typename ABC_T, typename POD_T, GT_Storage STORE_T>
static GT_DataArrayHandle
getDataArray(const ABC_T &array, int tuple_size, GT_Type type=GT_TYPE_NONE)
{
    if (!array)
	return GT_DataArrayHandle();

    GABC_GTNumericArray<ABC_T, POD_T>	*data;
    data = new GABC_GTNumericArray<ABC_T, POD_T>(array);
    return GT_DataArrayHandle(data);
}

template <typename ABC_T>
static GT_DataArrayHandle
getStringArray(const ABC_T &, int, GT_Type type=GT_TYPE_NONE)
{
    return GT_DataArrayHandle();
}

template <>
GT_DataArrayHandle
getStringArray<StringArraySamplePtr>(const StringArraySamplePtr &array,
		int tuple_size, GT_Type type)
{
    if (!array)
	return GT_DataArrayHandle();
    GT_DAIndexedString	*data = new GT_DAIndexedString(array->size(), tuple_size);
    const std::string	*src = (const std::string *)array->getData();

    // Copy strings over to 
    for (exint i = 0; i < array->size(); ++i)
    {
	for (int j = 0; j < tuple_size; ++j, ++src)
	    data->setString(i, j, src->c_str());
    }
    return GT_DataArrayHandle(data);
}

template <typename T>
static GT_DataArrayHandle
convertArray(const T &array)
{
    GT_Storage	store = GABC_GTUtil::getGTStorage(array->getDataType());
    int		tsize = GABC_GTUtil::getGTTupleSize(array->getDataType());

    // We're passed a boost shared pointer to the underlying array.
    // We can get the underlying array type with
    //    boost::shared_ptr::element_type
    // From the array type, we can get the traits type using
    //	  Abc::TypedArraySample::traits_type
    // This has a static method to get the interpretation of the array
    const char	*interp = T::element_type::traits_type::interpretation();
    // We can use the interpretation to get the type information
    GT_Type	tinfo = GABC_GTUtil::getGTTypeInfo(interp, tsize);

    switch (store)
    {
	case GT_STORE_UINT8:
	    return getDataArray<T, uint8, GT_STORE_UINT8>(array, tsize, tinfo);
	case GT_STORE_INT32:
	    return getDataArray<T, int32, GT_STORE_INT32>(array, tsize, tinfo);
	case GT_STORE_INT64:
	    return getDataArray<T, int64, GT_STORE_INT64>(array, tsize, tinfo);
	case GT_STORE_REAL16:
	    return getDataArray<T, fpreal16, GT_STORE_REAL16>(
				array, tsize, tinfo);
	case GT_STORE_REAL32:
	    return getDataArray<T, fpreal32, GT_STORE_REAL32>(
				array, tsize, tinfo);
	case GT_STORE_REAL64:
	    return getDataArray<T, fpreal64, GT_STORE_REAL64>(
				array, tsize, tinfo);
	case GT_STORE_STRING:
	    return getStringArray(array, tsize, tinfo);
	default:
	    UT_ASSERT(0);
	    break;
    }

    return GT_DataArrayHandle();
}

template <typename T>
static GT_DataArrayHandle
convertGeomParam(T &param, ISampleSelector &sample)
{
    typename T::sample_type	psample;
    param.getExpanded(psample, sample);
    return convertArray(psample.getVals());
}

template <typename T>
static GT_DataArrayHandle
convertArbitraryPropertyT(ICompoundProperty parent,
		const PropertyHeader &propHeader,
		ISampleSelector &sample)
{
    T	param(parent, propHeader.getName());
    return convertGeomParam(param, sample);
}

#define MATCH_CONVERT_PROPERTY(TYPENAME)	\
    if (TYPENAME::matches(propHeader)) \
	return convertArbitraryPropertyT<TYPENAME>(parent, propHeader, sample);

static GT_DataArrayHandle
convertArbitraryProperty(ICompoundProperty parent,
		const PropertyHeader &propHeader,
		ISampleSelector &sample)
{
    // Yuck.
    MATCH_CONVERT_PROPERTY(IBoolGeomParam);
    MATCH_CONVERT_PROPERTY(IUcharGeomParam);
    MATCH_CONVERT_PROPERTY(ICharGeomParam);
    MATCH_CONVERT_PROPERTY(IUInt16GeomParam);
    MATCH_CONVERT_PROPERTY(IInt16GeomParam);
    MATCH_CONVERT_PROPERTY(IUInt32GeomParam);
    MATCH_CONVERT_PROPERTY(IInt32GeomParam);
    MATCH_CONVERT_PROPERTY(IUInt64GeomParam);
    MATCH_CONVERT_PROPERTY(IInt64GeomParam);
    MATCH_CONVERT_PROPERTY(IHalfGeomParam);
    MATCH_CONVERT_PROPERTY(IFloatGeomParam);
    MATCH_CONVERT_PROPERTY(IDoubleGeomParam);
    MATCH_CONVERT_PROPERTY(IStringGeomParam);
    MATCH_CONVERT_PROPERTY(IWstringGeomParam);
    MATCH_CONVERT_PROPERTY(IV2sGeomParam);
    MATCH_CONVERT_PROPERTY(IV2iGeomParam);
    MATCH_CONVERT_PROPERTY(IV2fGeomParam);
    MATCH_CONVERT_PROPERTY(IV2dGeomParam);
    MATCH_CONVERT_PROPERTY(IV3sGeomParam);
    MATCH_CONVERT_PROPERTY(IV3iGeomParam);
    MATCH_CONVERT_PROPERTY(IV3fGeomParam);
    MATCH_CONVERT_PROPERTY(IV3dGeomParam);
    MATCH_CONVERT_PROPERTY(IP2sGeomParam);
    MATCH_CONVERT_PROPERTY(IP2iGeomParam);
    MATCH_CONVERT_PROPERTY(IP2fGeomParam);
    MATCH_CONVERT_PROPERTY(IP2dGeomParam);
    MATCH_CONVERT_PROPERTY(IP3sGeomParam);
    MATCH_CONVERT_PROPERTY(IP3iGeomParam);
    MATCH_CONVERT_PROPERTY(IP3fGeomParam);
    MATCH_CONVERT_PROPERTY(IP3dGeomParam);
    MATCH_CONVERT_PROPERTY(IBox2sGeomParam);
    MATCH_CONVERT_PROPERTY(IBox2iGeomParam);
    MATCH_CONVERT_PROPERTY(IBox2fGeomParam);
    MATCH_CONVERT_PROPERTY(IBox2dGeomParam);
    MATCH_CONVERT_PROPERTY(IBox3sGeomParam);
    MATCH_CONVERT_PROPERTY(IBox3iGeomParam);
    MATCH_CONVERT_PROPERTY(IBox3fGeomParam);
    MATCH_CONVERT_PROPERTY(IBox3dGeomParam);
    MATCH_CONVERT_PROPERTY(IM33fGeomParam);
    MATCH_CONVERT_PROPERTY(IM33dGeomParam);
    MATCH_CONVERT_PROPERTY(IM44fGeomParam);
    MATCH_CONVERT_PROPERTY(IM44dGeomParam);
    MATCH_CONVERT_PROPERTY(IQuatfGeomParam);
    MATCH_CONVERT_PROPERTY(IQuatdGeomParam);
    MATCH_CONVERT_PROPERTY(IC3hGeomParam);
    MATCH_CONVERT_PROPERTY(IC3fGeomParam);
    MATCH_CONVERT_PROPERTY(IC3cGeomParam);
    MATCH_CONVERT_PROPERTY(IC4hGeomParam);
    MATCH_CONVERT_PROPERTY(IC4fGeomParam);
    MATCH_CONVERT_PROPERTY(IC4cGeomParam);
    MATCH_CONVERT_PROPERTY(IN2fGeomParam);
    MATCH_CONVERT_PROPERTY(IN2dGeomParam);
    MATCH_CONVERT_PROPERTY(IN3fGeomParam);
    MATCH_CONVERT_PROPERTY(IN3dGeomParam);

    UT_ASSERT(0 && "Unhandled attribute type");
    return GT_DataArrayHandle();
}

static GeometryScope
getArbitraryPropertyScope(const PropertyHeader &propHeader)
{
    return GetGeometryScope(propHeader.getMetaData());
}

static bool
replaceDataArray(const GT_AttributeListHandle &alist,
		const char *name, const GT_DataArrayHandle &array,
		int segment=0)
{
    // NOTE:  This is quite dangerous.  What we should likely do is to create
    // a a new GT_AttributeListHandle instead.  We're changing the contents of
    // the attribute list which may be referenced by other users.
    int	idx = alist->getIndex(name);
    if (idx < 0)
	return false;
    UT_ASSERT(alist->get(idx)->entries() == array->entries());
    alist->set(idx, array, 0);
    return true;
}


static void
setAttributeData(GT_AttributeList &alist,
	const char *name,
	const GT_DataArrayHandle &data,
	bool *filled)
{
    int		idx = alist.getIndex(name);
    if (idx >= 0 && data && !filled[idx])
    {
	alist.set(idx, data, 0);
	filled[idx] = true;
    }
    else
    {
	UT_ASSERT(idx < 0 || filled[idx]);
    }
}

static bool
matchScope(GeometryScope needle, const GeometryScope *haystack, int haysize)
{
    for (int i = 0; i < haysize; ++i)
	if (needle == haystack[i])
	    return true;
    return false;
}

static void
fillHoudiniAttributes(GT_AttributeList &alist,
		const GABC_GEOPrim *prim,
		GA_AttributeOwner owner,
		bool *filled)
{
    const GA_Detail		&gdp = prim->getDetail();
    const GA_AttributeDict	&dict = gdp.getAttributes().getDict(owner);
    const GA_IndexMap		&indexmap = gdp.getIndexMap(owner);
    GA_Offset			 offset;
    if (owner == GA_ATTRIB_DETAIL)
	offset = GA_Offset(0);
    else
	offset = prim->getMapOffset();
    GA_Range	range(indexmap, offset, GA_Offset(offset+1));
    for (GA_AttributeDict::iterator it = dict.begin(GA_SCOPE_PUBLIC);
	    !it.atEnd(); ++it)
    {
	int			idx = alist.getIndex((*it)->getName());
	if (idx >= 0 && !filled[idx])
	{
	    GT_DataArrayHandle	h = GT_Util::extractAttribute(*(*it), range);
	    alist.set(idx, h, 0);
	    filled[idx] = true;
	}
    }
}

#define SET_ARRAY(VAR, NAME)	\
    if (VAR && *VAR) { \
	setAttributeData(alist, NAME, \
		convertArray(VAR->getValue(sample)), filled); \
    }
#define SET_GEOM_PARAM(VAR, NAME)	\
    if (VAR && VAR->valid() && matchScope(VAR->getScope(), scope, scope_size)){\
	setAttributeData(alist, NAME, \
		convertGeomParam(*VAR, sample), filled); \
    }

static void
fillAttributeList(GT_AttributeList &alist,
	const GABC_GEOPrim *prim,
	ISampleSelector &sample,
	const GeometryScope *scope,
	int scope_size,
	ICompoundProperty arb,
	Abc::IP3fArrayProperty *P = NULL,
	Abc::IV3fArrayProperty *v = NULL,
	IN3fGeomParam *N = NULL,
	IV2fGeomParam *uvs = NULL,
	Abc::IUInt64ArrayProperty *ids = NULL,
	IFloatGeomParam *widths = NULL)
{
    if (!alist.entries())
	return;

    UT_StackBuffer<bool>	filled(alist.entries());
    memset(filled, 0, sizeof(bool)*alist.entries());
    const GEO_ABCNameMapPtr	&namemap = prim->attributeNameMap();
    SET_ARRAY(P, "P")
    SET_ARRAY(v, "v")
    SET_ARRAY(ids, "id")
    SET_GEOM_PARAM(N, "N")
    SET_GEOM_PARAM(uvs, "uv")
    SET_GEOM_PARAM(widths, "width")
    if (matchScope(kConstantScope, scope, scope_size))
    {
	setAttributeData(alist, "__primitive_id",
		new GT_IntConstant(1, prim->getMapOffset()), filled);
    }
    if (arb)
    {
	for (size_t i = 0; i < arb.getNumProperties(); ++i)
	{
	    const PropertyHeader	&propHeader = arb.getPropertyHeader(i);
	    if (!matchScope(getArbitraryPropertyScope(propHeader),
			scope, scope_size))
	    {
		continue;
	    }

	    const char		*name = propHeader.getName().c_str();
	    if (namemap)
	    {
		name = namemap->getName(name);
		if (!name)
		    continue;
	    }
	    GT_Storage	store = GABC_GTUtil::getGTStorage(propHeader.getDataType());
	    if (store == GT_STORE_INVALID)
		continue;
	    setAttributeData(alist, name,
		    convertArbitraryProperty(arb, propHeader, sample), filled);
	}
    }
    // We need to fill Houdini attributes last.  Otherwise, when converting two
    // primitives, the Houdini attributes override the first primitive
    // converted.
    if (matchScope(kConstantScope, scope, scope_size))
    {
	fillHoudiniAttributes(alist, prim, GA_ATTRIB_PRIMITIVE, filled);
	fillHoudiniAttributes(alist, prim, GA_ATTRIB_GLOBAL, filled);
    }
}

static void
initializeHoudiniAttributes(const GABC_GEOPrim *prim, GT_AttributeMap *map,
			    GA_AttributeOwner owner)
{
    const GA_Detail		&gdp = prim->getDetail();
    const GA_AttributeDict	&dict = gdp.getAttributes().getDict(owner);
    for (GA_AttributeDict::iterator it = dict.begin(GA_SCOPE_PUBLIC);
	    !it.atEnd(); ++it)
    {
	const GA_Attribute	*attrib = it.attrib();
	if (attrib->getAIFTuple() || attrib->getAIFStringTuple())
	    map->add(attrib->getName(), false);
    }
}

#define REPLACE_ARRAY(VAR, NAME)	\
    if (VAR && *VAR) { \
	UT_VERIFY(replaceDataArray(src, NAME, \
		    convertArray(VAR->getValue(sample)))); \
    }
#define REPLACE_GEOM_PARAM(VAR, NAME)	\
    if (VAR && VAR->valid() && matchScope(VAR->getScope(), scope, scope_size)){\
	UT_VERIFY(replaceDataArray(src, NAME, \
		    convertGeomParam(*VAR, sample))); \
    }

static GT_AttributeListHandle
reuseAttributeList(const GABC_GEOPrim *prim,
	ISampleSelector &sample,
	const GT_AttributeListHandle &src,
	const GeometryScope *scope,
	int scope_size,
	ICompoundProperty arb,
	Abc::IP3fArrayProperty *P = NULL,
	Abc::IV3fArrayProperty *v = NULL,
	IN3fGeomParam *N = NULL,
	IV2fGeomParam *uvs = NULL,
	Abc::IUInt64ArrayProperty *ids = NULL,
	IFloatGeomParam *widths = NULL)
{
    if (!src || !src->entries())
	return src;

    const GEO_ABCNameMapPtr	&namemap = prim->attributeNameMap();
    REPLACE_ARRAY(P, "P")
    REPLACE_ARRAY(v, "v")
    REPLACE_ARRAY(ids, "id")
    REPLACE_GEOM_PARAM(N, "N")
    REPLACE_GEOM_PARAM(uvs, "uv")
    REPLACE_GEOM_PARAM(widths, "width")
    if (arb)
    {
	for (size_t i = 0; i < arb.getNumProperties(); ++i)
	{
	    const PropertyHeader	&propHeader = arb.getPropertyHeader(i);
	    if (!matchScope(getArbitraryPropertyScope(propHeader),
			scope, scope_size))
	    {
		continue;
	    }

	    const char		*name = propHeader.getName().c_str();
	    if (namemap)
	    {
		name = namemap->getName(name);
		if (!name)
		    continue;
	    }
	    GT_Storage	store = GABC_GTUtil::getGTStorage(propHeader.getDataType());
	    if (store == GT_STORE_INVALID)
		continue;
	    UT_VERIFY(replaceDataArray(src, name,
		    convertArbitraryProperty(arb, propHeader, sample)));
	}
    }
    return src;
}

static GT_AttributeListHandle
reuseAttributeList(const GABC_GEOPrim *prim,
	ISampleSelector &sample,
	const GT_AttributeListHandle &src,
	const GeometryScope scope,
	ICompoundProperty arb,
	Abc::IP3fArrayProperty *P = NULL,
	Abc::IV3fArrayProperty *v = NULL,
	IN3fGeomParam *N = NULL,
	IV2fGeomParam *uvs = NULL)
{
    return reuseAttributeList(prim, sample, src, &scope, 1, arb, P, v, N, uvs);
}


static GT_AttributeListHandle
initializeAttributeList(const GABC_GEOPrim *prim,
	ISampleSelector &sample,
	const GeometryScope *scope,
	int scope_size,
	ICompoundProperty arb,
	Abc::IP3fArrayProperty *P = NULL,
	Abc::IV3fArrayProperty *v = NULL,
	IN3fGeomParam *N = NULL,
	IV2fGeomParam *uvs = NULL,
	Abc::IUInt64ArrayProperty *ids = NULL,
	IFloatGeomParam *widths = NULL)
{
    GT_AttributeMap		*map = new GT_AttributeMap();
    const GEO_ABCNameMapPtr	&namemap = prim->attributeNameMap();

    if (P && *P)
	map->add("P", true);
    if (v && *v)
	map->add("v", true);
    if (ids && *ids)
	map->add("id", true);
    if (N && N->valid() && matchScope(N->getScope(), scope, scope_size))
	map->add("N", true);
    if (uvs && uvs->valid() && matchScope(uvs->getScope(), scope, scope_size))
	map->add("uv", true);
    if (widths && widths->valid()
		&& matchScope(widths->getScope(), scope, scope_size))
    {
	map->add("width", true);
    }

    // Primitive id
    if (matchScope(kConstantScope, scope, scope_size))
    {
	map->add("__primitive_id", true);
    }
    if (arb)
    {
	for (size_t i = 0; i < arb.getNumProperties(); ++i)
	{
	    const PropertyHeader	&propHeader = arb.getPropertyHeader(i);
	    if (!matchScope(getArbitraryPropertyScope(propHeader), scope, scope_size))
	    {
		continue;
	    }

	    const char		*name = propHeader.getName().c_str();
	    if (namemap)
	    {
		name = namemap->getName(name);
		if (!name)
		    continue;
	    }
	    GT_Storage	store = GABC_GTUtil::getGTStorage(propHeader.getDataType());
	    if (store == GT_STORE_INVALID)
		continue;
	    map->add(name, false);
	}
    }
    if (matchScope(kConstantScope, scope, scope_size))
    {
	initializeHoudiniAttributes(prim, map, GA_ATTRIB_PRIMITIVE);
	initializeHoudiniAttributes(prim, map, GA_ATTRIB_GLOBAL);
    }

    GT_AttributeList	*alist = NULL;
    if (!map->entries())
	delete map;
    else
	alist = new GT_AttributeList(GT_AttributeMapHandle(map));

    if (alist)
    {
	fillAttributeList(*alist, prim, sample, scope, scope_size,
		arb, P, v, N, uvs, ids, widths);
    }

    return GT_AttributeListHandle(alist);
}

static GT_AttributeListHandle
initializeAttributeList(const GABC_GEOPrim *prim,
	ISampleSelector &sample,
	const GeometryScope scope,
	ICompoundProperty arb,
	Abc::IP3fArrayProperty *P = NULL,
	Abc::IV3fArrayProperty *v = NULL,
	IN3fGeomParam *N = NULL,
	IV2fGeomParam *uvs = NULL)
{
    return initializeAttributeList(prim, sample, &scope, 1, arb, P, v, N, uvs);
}

static GeometryScope	theConstantUnknownScope[] = {
			    kConstantScope,
			    kUnknownScope
};

static GT_DataArrayHandle
joinVector3Array(const GT_DataArrayHandle &x,
		const GT_DataArrayHandle &y,
		const GT_DataArrayHandle &z,
		GT_Type type = GT_TYPE_POINT)
{
    exint	n = x->entries();
    UT_ASSERT(n == y->entries() && n == z->entries());
    GT_Real32Array	*xyz = new GT_Real32Array(n, 3, type);
    fpreal32		*data = xyz->data();
    for (exint i = 0; i < n; ++i, data += 3)
    {
	data[0] = x->getF32(i);
	data[1] = y->getF32(i);
	data[2] = z->getF32(i);
    }
    return GT_DataArrayHandle(xyz);
}

static GT_PrimitiveHandle
buildNuPatch(const GABC_GEOPrim *abc,
	const Alembic::AbcGeom::IObject &object,
	ISampleSelector selector)
{
    INuPatch			 nupatch(object, kWrapExisting);
    INuPatchSchema		&ss = nupatch.getSchema();
    INuPatchSchema::Sample	 sample = ss.getValue(selector);

    int			 uorder = sample.getUOrder();
    int			 vorder = sample.getVOrder();
    GT_DataArrayHandle	 uknots;
    GT_DataArrayHandle	 vknots;
    GT_PrimNuPatch	*patch;

    uknots = getDataArray<FloatArraySamplePtr, float, GT_STORE_REAL32>
			(sample.getUKnot(), 1);
    vknots = getDataArray<FloatArraySamplePtr, float, GT_STORE_REAL32>
			(sample.getVKnot(), 1);

    Abc::IP3fArrayProperty	 P = ss.getPositionsProperty();
    Abc::IV3fArrayProperty	 v = ss.getVelocitiesProperty();
    GT_AttributeListHandle	 vertex;
    GT_AttributeListHandle	 detail;
    GeometryScope	vertex_scope[3] = { kVertexScope, kVaryingScope, kFacevaryingScope };
    GeometryScope	detail_scope[3] = { kUniformScope, kConstantScope, kUnknownScope };

    vertex = initializeAttributeList(abc, selector,
		    vertex_scope, 3, ss.getArbGeomParams(),
		    &P, &v, &ss.getNormalsParam(), &ss.getUVsParam());
    detail = initializeAttributeList(abc, selector,
		    detail_scope, 3, ss.getArbGeomParams());

    patch = new GT_PrimNuPatch(uorder, uknots, vorder, vknots, vertex, detail);

    if (ss.hasTrimCurve())
    {
#if 0
	if (!ss.trimCurveTopologyIsConstant())
	{
	    if (!ss.trimCurveTopologyIsHomogenous())
		topology = kHeterogenousTopology;
	    else if (topology != kHeterogenousTopology)
		topology = kHomogenousTopology;
	}
#endif
	GT_DataArrayHandle	loopCount;
	GT_DataArrayHandle	curveCount;
	GT_DataArrayHandle	curveOrders;
	GT_DataArrayHandle	curveKnots;
	GT_DataArrayHandle	curveMin;
	GT_DataArrayHandle	curveMax;
	GT_DataArrayHandle	curveU, curveV, curveW, curveUVW;
	loopCount = getDataArray<Int32ArraySamplePtr, int32, GT_STORE_INT32>
			(sample.getTrimNumCurves(), 1);
	curveCount = getDataArray<Int32ArraySamplePtr, int32, GT_STORE_INT32>
			(sample.getTrimNumVertices(), 1);
	curveOrders = getDataArray<Int32ArraySamplePtr, int32, GT_STORE_INT32>
			(sample.getTrimOrders(), 1);
	curveKnots = getDataArray<FloatArraySamplePtr, fpreal32,GT_STORE_REAL32>
			(sample.getTrimKnots(), 1);
	curveMin = getDataArray<FloatArraySamplePtr, fpreal32, GT_STORE_REAL32>
			(sample.getTrimMins(), 1);
	curveMax = getDataArray<FloatArraySamplePtr, fpreal32, GT_STORE_REAL32>
			(sample.getTrimMaxes(), 1);
	curveU = getDataArray<FloatArraySamplePtr, fpreal32, GT_STORE_REAL32>
			(sample.getTrimU(), 1);
	curveV = getDataArray<FloatArraySamplePtr, fpreal32, GT_STORE_REAL32>
			(sample.getTrimV(), 1);
	curveW = getDataArray<FloatArraySamplePtr, fpreal32, GT_STORE_INT32>
			(sample.getTrimW(), 1);
	curveUVW = joinVector3Array(curveU, curveV, curveW);

	GT_TrimNuCurves	*trims = new GT_TrimNuCurves(loopCount, curveCount,
				curveOrders, curveKnots,
				curveMin, curveMax, curveUVW);
	if (!trims->isValid())
	{
	    delete trims;
	    trims = NULL;
	}
	patch->adoptTrimCurves(trims);
    }

    return GT_PrimitiveHandle(patch);
}

static GT_PrimitiveHandle
buildCurves(const GABC_GEOPrim *abc,
	    const Alembic::AbcGeom::IObject &object,
	    ISampleSelector selector)
{
    ICurves			 curves(object, kWrapExisting);
    ICurvesSchema		&ss = curves.getSchema();
    ICurvesSchema::Sample	 sample = ss.getValue(selector);

#if 0
    topology = ss.getTopologyVariance();

    // Work around bug in Alembic 1.0.5 that doesn't properly detect
    // heterogenous topology.
    if (topology == kHomogenousTopology &&
	    !ss.getNumVerticesProperty().isConstant())
	topology = kHeterogenousTopology;
#endif

    GT_DataArrayHandle	 counts;
    GT_PrimCurveMesh	*cmesh;

    counts = getDataArray<Int32ArraySamplePtr, int32, GT_STORE_INT32>
			(sample.getCurvesNumVertices(), 1);

    GT_AttributeListHandle	vertex;
    GT_AttributeListHandle	uniform;
    GT_AttributeListHandle	detail;
    Abc::IP3fArrayProperty	P = ss.getPositionsProperty();
    Abc::IV3fArrayProperty	v = ss.getVelocitiesProperty();
    IFloatGeomParam		&widths = ss.getWidthsParam();
    GeometryScope	 vertex_scope[3] = { kVertexScope, kVaryingScope, kFacevaryingScope };

    vertex = initializeAttributeList(abc, selector,
			vertex_scope, 3, ss.getArbGeomParams(),
			&P, &v, &ss.getNormalsParam(), &ss.getUVsParam(),
			NULL, &widths);
    uniform = initializeAttributeList(abc, selector,
			kUniformScope, ss.getArbGeomParams());
    detail = initializeAttributeList(abc, selector,
			theConstantUnknownScope, 2, ss.getArbGeomParams());

    GT_Basis	basis;
    switch (sample.getBasis())
    {
	case Alembic::AbcGeom::kBezierBasis:
	    basis = GT_BASIS_BEZIER;
	    break;
	default:
	    basis = GT_BASIS_LINEAR;
	    break;
    }

    cmesh = new GT_PrimCurveMesh(basis, counts,
		    vertex, uniform, detail,
		    false);

    return GT_PrimitiveHandle(cmesh);
}

static GT_PrimitiveHandle
buildPoints(const GABC_GEOPrim *abc,
	    const Alembic::AbcGeom::IObject &object,
	    ISampleSelector selector)
{
    IPoints			 points(object, kWrapExisting);
    IPointsSchema		&ss = points.getSchema();
    IPointsSchema::Sample	 sample = ss.getValue(selector);
    GT_PrimPointMesh		*pmesh;

    GT_AttributeListHandle	vertex;
    GT_AttributeListHandle	detail;
    Abc::IP3fArrayProperty	P = ss.getPositionsProperty();
    Abc::IV3fArrayProperty	v = ss.getVelocitiesProperty();
    Abc::IUInt64ArrayProperty	ids = ss.getIdsProperty();
    IFloatGeomParam		widths = ss.getWidthsParam();
    GeometryScope	vertex_scope[3] = { kVertexScope, kVaryingScope, kFacevaryingScope };
    GeometryScope	detail_scope[3] = { kUniformScope, kConstantScope, kUnknownScope };

    vertex = initializeAttributeList(abc, selector,
		    vertex_scope, 3, ss.getArbGeomParams(),
		    &P, &v, NULL, NULL, &ids, &widths);
    detail = initializeAttributeList(abc, selector,
		    detail_scope, 3, ss.getArbGeomParams());

    pmesh = new GT_PrimPointMesh(vertex, detail);

    return GT_PrimitiveHandle(pmesh);
}

static void
initializeM44d(Imath::M44d &d, const UT_Matrix4D &s)
{
    const double	*src = s.data();
    for (int r = 0; r < 4; ++r)
	for (int c = 0; c < 4; ++c)
	    d.x[r][c] = src[r*4+c];
}

static GT_PrimitiveHandle
reuseLocator(const GABC_GEOPrim *abc,
	    const GT_PrimitiveHandle &srcprim,
	    const Alembic::AbcGeom::IObject &object,
	    ISampleSelector selector)
{
    IXform				 xform(object, kWrapExisting);
    Alembic::Abc::IScalarProperty	 loc(xform.getProperties(), "locator");
    Alembic::AbcGeom::IObject		 parent;
    UT_Matrix4D				 pxform;
    bool				 pxform_const = false;

    parent = xform.getParent();
    if (!GABC_Util::getWorldTransform(abc->getFilename(),
				parent,
				selector.getRequestedTime(),
				pxform,
				pxform_const))
    {
	pxform.identity();
	pxform_const = true;
    }
    Imath::M44d	pmat;
    Imath::V3d	psval, phval, prval, pxval;
    double	locdata[6];
    initializeM44d(pmat, pxform);
    if (!Imath::extractSHRT(pmat, psval, phval, prval, pxval))
    {
	psval = Imath::V3d(1,1,1);
	prval = Imath::V3d(0,0,0);
	pxval = Imath::V3d(0,0,0);
    }
    loc.get(locdata, selector);

    GT_DataArrayHandle	P;
    GT_DataArrayHandle	localPosition;
    GT_DataArrayHandle	localScale;
    GT_DataArrayHandle	parentTrans;
    GT_DataArrayHandle	parentRot;
    GT_DataArrayHandle	parentScale;

    const GT_AttributeListHandle	&point = srcprim->getPointAttributes();
    const GT_AttributeListHandle	&detail= srcprim->getDetailAttributes();

    P = GT_DataArrayHandle(new GT_RealConstant(1, locdata, 3));
    parentTrans = GT_DataArrayHandle(new GT_RealConstant(1, pxval.x, 3));
    parentRot = GT_DataArrayHandle(new GT_RealConstant(1, prval.x, 3));
    parentScale = GT_DataArrayHandle(new GT_RealConstant(1, psval.x, 3));
    localPosition = GT_DataArrayHandle(new GT_RealConstant(1, locdata, 3));
    localScale = GT_DataArrayHandle(new GT_RealConstant(1, locdata+3,3));

    UT_VERIFY(replaceDataArray(point, "P", P));
    UT_VERIFY(replaceDataArray(detail, "localPosition", localPosition));
    UT_VERIFY(replaceDataArray(detail, "localScale", localScale));
    UT_VERIFY(replaceDataArray(detail, "parentTrans", parentTrans));
    UT_VERIFY(replaceDataArray(detail, "parentRot", parentRot));
    UT_VERIFY(replaceDataArray(detail, "parentScale", parentScale));

    return srcprim;
}

static GT_PrimitiveHandle
buildLocator(const GABC_GEOPrim *abc,
	    const Alembic::AbcGeom::IObject &object,
	    ISampleSelector selector)
{
    IXform				 xform(object, kWrapExisting);
    GT_PrimPointMesh			*pmesh = NULL;
    Alembic::Abc::IScalarProperty	 loc(xform.getProperties(), "locator");
    Alembic::AbcGeom::IObject		 parent;
    UT_Matrix4D				 pxform;
    bool				 pxform_const = false;

    parent = xform.getParent();
    if (!GABC_Util::getWorldTransform(abc->getFilename(),
				parent,
				selector.getRequestedTime(),
				pxform,
				pxform_const))
    {
	pxform.identity();
	pxform_const = true;
    }
    Imath::M44d	pmat;
    Imath::V3d	psval, phval, prval, pxval;
    double	locdata[6];
    initializeM44d(pmat, pxform);
    if (!Imath::extractSHRT(pmat, psval, phval, prval, pxval))
    {
	psval = Imath::V3d(1,1,1);
	prval = Imath::V3d(0,0,0);
	pxval = Imath::V3d(0,0,0);
    }

    if (loc.isConstant())
	loc.get(locdata, 0);
    else
	loc.get(locdata, selector);

    GT_AttributeMapHandle	vmap(new GT_AttributeMap());
    GT_AttributeMapHandle	umap(new GT_AttributeMap());
    GT_AttributeListHandle	vertex;
    GT_AttributeListHandle	detail;

    int	Pidx = vmap->add("P", true);
    int lP = umap->add("localPosition", true);
    int lS = umap->add("localScale", true);
    int pX = umap->add("parentTrans", true);
    int pR = umap->add("parentRot", true);
    int pS = umap->add("parentScale", true);

    vertex = GT_AttributeListHandle(new GT_AttributeList(vmap));
    vertex->set(Pidx, GT_DataArrayHandle(new GT_RealConstant(1, locdata, 3)), 0);
    detail = GT_AttributeListHandle(new GT_AttributeList(umap));
    detail->set(pX, GT_DataArrayHandle(new GT_RealConstant(1, pxval.x, 3)), 0);
    detail->set(pR, GT_DataArrayHandle(new GT_RealConstant(1, prval.x, 3)), 0);
    detail->set(pS, GT_DataArrayHandle(new GT_RealConstant(1, psval.x, 3)), 0);
    detail->set(lP, GT_DataArrayHandle(new GT_RealConstant(1, locdata, 3)), 0);
    detail->set(lS, GT_DataArrayHandle(new GT_RealConstant(1, locdata+3,3)), 0);

    pmesh = new GT_PrimPointMesh(vertex, detail);

    return GT_PrimitiveHandle(pmesh);
}

static IN3fGeomParam *
getNormalsParam(IPolyMeshSchema &ss)
{
    return &ss.getNormalsParam();
}
static IN3fGeomParam *
getNormalsParam(ISubDSchema &ss)
{
    return NULL;
}

template <typename ABC_T, typename SCHEMA_T>
static GT_PrimitiveHandle
buildMesh(const GABC_GEOPrim *abc,
	    const Alembic::AbcGeom::IObject &object,
	    ISampleSelector selector)
{
    ABC_T			 prim(object, kWrapExisting);
    SCHEMA_T			&ss = prim.getSchema();
    typename SCHEMA_T::Sample	 sample = ss.getValue(selector);

    GT_DataArrayHandle		 counts;
    GT_DataArrayHandle		 indices;
    GT_PrimPolygonMesh		*pmesh;

    counts = getDataArray<Int32ArraySamplePtr, int32, GT_STORE_INT32>
			(sample.getFaceCounts(), 1);
    indices = getDataArray<Int32ArraySamplePtr, int32, GT_STORE_INT32>
			(sample.getFaceIndices(), 1);

    GT_AttributeListHandle	 point;
    GT_AttributeListHandle	 vertex;
    GT_AttributeListHandle	 uniform;
    GT_AttributeListHandle	 detail;
    Abc::IP3fArrayProperty	 P = ss.getPositionsProperty();
    Abc::IV3fArrayProperty	 v = ss.getVelocitiesProperty();
    GeometryScope	point_scope[2] = { kVaryingScope, kVertexScope };
    GeometryScope	vertex_scope[1] = { kFacevaryingScope };

    point = initializeAttributeList(abc, selector,
			point_scope, 2, ss.getArbGeomParams(),
			&P,
			&v,
			getNormalsParam(ss),
			&ss.getUVsParam());

    vertex = initializeAttributeList(abc, selector,
			vertex_scope, 1, ss.getArbGeomParams(),
			NULL,
			&v,
			getNormalsParam(ss),
			&ss.getUVsParam());
    uniform = initializeAttributeList(abc, selector,
			kUniformScope, ss.getArbGeomParams());
    detail = initializeAttributeList(abc, selector,
			theConstantUnknownScope, 2, ss.getArbGeomParams());

    pmesh = new GT_PrimPolygonMesh(counts, indices,
		    point, vertex, uniform, detail);

    return GT_PrimitiveHandle(pmesh);
}

static GT_PrimitiveHandle
reusePoints(const GABC_GEOPrim *abc,
	    const GT_PrimitiveHandle &srcprim,
	    const Alembic::AbcGeom::IObject &object,
	    ISampleSelector selector)
{
    IPoints			 prim(object, kWrapExisting);
    IPointsSchema		&ss = prim.getSchema();
    Abc::IP3fArrayProperty	 P = ss.getPositionsProperty();
    Abc::IV3fArrayProperty	 v = ss.getVelocitiesProperty();
    Abc::IUInt64ArrayProperty	 ids = ss.getIdsProperty();
    IFloatGeomParam		 widths = ss.getWidthsParam();
    GeometryScope	vertex_scope[3] = { kVertexScope, kVaryingScope, kFacevaryingScope };
    GeometryScope	detail_scope[3] = { kUniformScope, kConstantScope, kUnknownScope };

    reuseAttributeList(abc, selector, srcprim->getPointAttributes(),
	    vertex_scope, 3, ss.getArbGeomParams(),
	    &P, &v, NULL, NULL, &ids, &widths);
    reuseAttributeList(abc, selector, srcprim->getDetailAttributes(),
	    detail_scope, 3, ss.getArbGeomParams());

    return srcprim;
}

static GT_PrimitiveHandle
reuseNuPatch(const GABC_GEOPrim *abc,
	    const GT_PrimitiveHandle &srcprim,
	    const Alembic::AbcGeom::IObject &object,
	    ISampleSelector selector)
{
    INuPatch			 prim(object, kWrapExisting);
    INuPatchSchema		&ss = prim.getSchema();
    INuPatchSchema::Sample	 sample = ss.getValue(selector);

    // TODO: Deal with animated bases?
    Abc::IP3fArrayProperty	 P = ss.getPositionsProperty();
    Abc::IV3fArrayProperty	 v = ss.getVelocitiesProperty();
    GeometryScope	vertex_scope[3] = { kVertexScope, kVaryingScope, kFacevaryingScope };
    GeometryScope	detail_scope[3] = { kUniformScope, kConstantScope, kUnknownScope };

    reuseAttributeList(abc, selector, srcprim->getVertexAttributes(),
		    vertex_scope, 3, ss.getArbGeomParams(),
		    &P, &v, &ss.getNormalsParam(), &ss.getUVsParam());
    reuseAttributeList(abc, selector, srcprim->getDetailAttributes(),
		    detail_scope, 3, ss.getArbGeomParams());
    return srcprim;
}

static GT_PrimitiveHandle
reuseCurves(const GABC_GEOPrim *abc,
	    const GT_PrimitiveHandle &srcprim,
	    const Alembic::AbcGeom::IObject &object,
	    ISampleSelector selector)
{
    ICurves			 prim(object, kWrapExisting);
    ICurvesSchema		&ss = prim.getSchema();
    ICurvesSchema::Sample	 sample = ss.getValue(selector);
    Abc::IP3fArrayProperty	P = ss.getPositionsProperty();
    Abc::IV3fArrayProperty	v = ss.getVelocitiesProperty();
    IFloatGeomParam		&widths = ss.getWidthsParam();
    GeometryScope	 vertex_scope[3] = { kVertexScope, kVaryingScope, kFacevaryingScope };

    reuseAttributeList(abc, selector, srcprim->getVertexAttributes(),
			vertex_scope, 3, ss.getArbGeomParams(),
			&P, &v, &ss.getNormalsParam(), &ss.getUVsParam(),
			NULL, &widths);
    reuseAttributeList(abc, selector, srcprim->getUniformAttributes(),
			kUniformScope, ss.getArbGeomParams());
    reuseAttributeList(abc, selector, srcprim->getDetailAttributes(),
			theConstantUnknownScope, 2, ss.getArbGeomParams());

    return srcprim;
}

template <typename ABC_T, typename SCHEMA_T>
static GT_PrimitiveHandle
reuseMesh(const GABC_GEOPrim *abc,
	    const GT_PrimitiveHandle &srcprim,
	    const Alembic::AbcGeom::IObject &object,
	    ISampleSelector selector)
{
    ABC_T			 prim(object, kWrapExisting);
    SCHEMA_T			&ss = prim.getSchema();
    typename SCHEMA_T::Sample	 sample = ss.getValue(selector);

    Abc::IP3fArrayProperty	 P = ss.getPositionsProperty();
    Abc::IV3fArrayProperty	 v = ss.getVelocitiesProperty();
    GeometryScope	point_scope[2] = { kVaryingScope, kVertexScope };
    GeometryScope	vertex_scope[1] = { kFacevaryingScope };

    reuseAttributeList(abc, selector, srcprim->getPointAttributes(),
			point_scope, 2, ss.getArbGeomParams(),
			&P,
			&v,
			getNormalsParam(ss),
			&ss.getUVsParam());
    reuseAttributeList(abc, selector, srcprim->getVertexAttributes(),
			vertex_scope, 1, ss.getArbGeomParams(),
			NULL,
			&v,
			getNormalsParam(ss),
			&ss.getUVsParam());
    reuseAttributeList(abc, selector, srcprim->getUniformAttributes(),
			kUniformScope, ss.getArbGeomParams());
    reuseAttributeList(abc, selector, srcprim->getDetailAttributes(),
			theConstantUnknownScope, 2, ss.getArbGeomParams());

    return srcprim;
}

GT_PrimitiveHandle
GABC_GEOPrim::gtPrimitive() const
{
    if (!myObject.valid())
	return GT_PrimitiveHandle();

    GT_PrimitiveHandle	result;
    ISampleSelector	sampleSelector(myFrame);

    if ((myAnimation == GABC_Util::ANIMATION_CONSTANT ||
	 myAnimation == GABC_Util::ANIMATION_TRANSFORM) && myGTPrimitive)
    {
	result = myGTPrimitive;
    }
    else if (myAnimation == GABC_Util::ANIMATION_ATTRIBUTE && myGTPrimitive)
    {
	// Update attributes
	if (ISubD::matches(myObject.getHeader()))
	{
	    result = reuseMesh<ISubD, ISubDSchema>(this, myGTPrimitive,
			myObject, sampleSelector);
	}
	else if (IPolyMesh::matches(myObject.getHeader()))
	{
	    result = reuseMesh<IPolyMesh, IPolyMeshSchema>(this, myGTPrimitive,
			myObject, sampleSelector);
	}
	else if (ICurves::matches(myObject.getHeader()))
	{
	    result = reuseCurves(this, myGTPrimitive, myObject, sampleSelector);
	}
	else if (IPoints::matches(myObject.getHeader()))
	{
	    result = reusePoints(this, myGTPrimitive, myObject, sampleSelector);
	}
	else if (INuPatch::matches(myObject.getHeader()))
	{
	    result = reuseNuPatch(this, myGTPrimitive, myObject, sampleSelector);
	}
	else if (GABC_Util::isMayaLocator(myObject))
	{
	    result = reuseLocator(this, myGTPrimitive, myObject, sampleSelector);
	}
    }
    else
    {
	UT_ASSERT(myAnimation == GABC_Util::ANIMATION_TOPOLOGY || !myGTPrimitive);
	if (ISubD::matches(myObject.getHeader()))
	{
	    result = buildMesh<ISubD, ISubDSchema>(this, myObject,
				sampleSelector);
	}
	else if (IPolyMesh::matches(myObject.getHeader()))
	{
	    result = buildMesh<IPolyMesh, IPolyMeshSchema>(this, myObject,
				sampleSelector);
	}
	else if (ICurves::matches(myObject.getHeader()))
	{
	    result = buildCurves(this, myObject, sampleSelector);
	}
	else if (IPoints::matches(myObject.getHeader()))
	{
	    result = buildPoints(this, myObject, sampleSelector);
	}
	else if (INuPatch::matches(myObject.getHeader()))
	{
	    result = buildNuPatch(this, myObject, sampleSelector);
	}
	else if (GABC_Util::isMayaLocator(myObject))
	{
	    result = buildLocator(this, myObject, sampleSelector);
	}
	else
	{
	    fprintf(stderr, "Unsupported primitive: %s\n", myObject.getName().c_str());
	}
    }
    // Transform the result
    if (result)
    {
	// Pick up loacal transform and abc transform
	UT_Matrix4D	xform;
	if (getTransform(xform))
	{
	    GT_Transform	*x = new GT_Transform(&xform, 1);
	    result->setPrimitiveTransform(GT_TransformHandle(x));
	}
    }

#if 0
    // Enable this to turn on pre-convexing for faster viewport rendering
    if (result && myAnimation != GABC_Util::ANIMATION_TOPOLOGY)
    {
	if (result && result->getPrimitiveType() == GT_PRIM_POLYGON_MESH)
	{
	    const GT_PrimPolygonMesh	*pmesh;
	    pmesh = static_cast<const GT_PrimPolygonMesh *>(result.get());
	    myGTPrimitive = pmesh->convex(3);
	    result = myGTPrimitive;
	}
    }
#endif
    myGTPrimitive = result;

    return result;
}
