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
 * NAME:	GABC_OGTGeometry.h
 *
 * COMMENTS:
 */

#include "GABC_OGTGeometry.h"
#include "GABC_OProperty.h"
#include "GABC_OError.h"
#include "GABC_OOptions.h"
#include <stdarg.h>
#include <UT/UT_StackBuffer.h>
#include <GT/GT_DANumeric.h>
#include <GT/GT_DARange.h>
#include <GT/GT_DAConstantValue.h>
#include <GT/GT_PrimPolygonMesh.h>
#include <GT/GT_PrimCurveMesh.h>
#include <GT/GT_PrimPolygon.h>
#include <GT/GT_PrimCurve.h>
#include <GT/GT_PrimSubdivisionMesh.h>
#include <GT/GT_PrimNuPatch.h>
#include <GT/GT_PrimNuCurves.h>
#include <GT/GT_TrimNuCurves.h>
#include <GT/GT_PrimPointMesh.h>

// For simple refinement
#include <GT/GT_GEOPrimTPSurf.h>

namespace
{
    typedef GABC_OGTGeometry::IntrinsicCache	IntrinsicCache;
    typedef GABC_OGTGeometry::SecondaryCache	SecondaryCache;
    typedef UT_SymbolMap<GABC_OProperty *>	PropertyMap;
    typedef Alembic::Abc::V2f			V2f;
    typedef Alembic::Abc::V3f			V3f;
    typedef Alembic::Abc::Int32ArraySample	Int32ArraySample;
    typedef Alembic::Abc::UInt64ArraySample	UInt64ArraySample;
    typedef Alembic::Abc::FloatArraySample	FloatArraySample;
    typedef Alembic::Abc::V2fArraySample	V2fArraySample;
    typedef Alembic::Abc::V3fArraySample	V3fArraySample;
    typedef Alembic::Abc::P3fArraySample	P3fArraySample;
    typedef Alembic::Abc::OCompoundProperty	OCompoundProperty;
    typedef Alembic::Abc::TimeSamplingPtr	TimeSamplingPtr;
    typedef Alembic::AbcGeom::OFaceSet		OFaceSet;
    typedef Alembic::AbcGeom::OPolyMesh		OPolyMesh;
    typedef Alembic::AbcGeom::OSubD		OSubD;
    typedef Alembic::AbcGeom::OCurves		OCurves;
    typedef Alembic::AbcGeom::OPoints		OPoints;
    typedef Alembic::AbcGeom::ONuPatch		ONuPatch;
    typedef Alembic::AbcGeom::OFaceSetSchema	OFaceSetSchema;
    typedef Alembic::AbcGeom::OPolyMeshSchema	OPolyMeshSchema;
    typedef Alembic::AbcGeom::OSubDSchema	OSubDSchema;
    typedef Alembic::AbcGeom::OCurvesSchema	OCurvesSchema;
    typedef Alembic::AbcGeom::OPointsSchema	OPointsSchema;
    typedef Alembic::AbcGeom::ONuPatchSchema	ONuPatchSchema;
    typedef Alembic::AbcGeom::OP3fGeomParam	OP3fGeomParam;
    typedef Alembic::AbcGeom::OV2fGeomParam	OV2fGeomParam;
    typedef Alembic::AbcGeom::OV3fGeomParam	OV3fGeomParam;
    typedef Alembic::AbcGeom::ON3fGeomParam	ON3fGeomParam;
    typedef Alembic::AbcGeom::OFloatGeomParam	OFloatGeomParam;

    Int32ArraySample
    int32Array(const GT_DataArrayHandle &data, GT_DataArrayHandle &storage)
    {
	return Int32ArraySample(data->getI32Array(storage), data->entries());
    }
    UInt64ArraySample
    uint64Array(const GT_DataArrayHandle &data, GT_DataArrayHandle &storage)
    {
	return UInt64ArraySample((const uint64 *)data->getI64Array(storage),
				data->entries());
    }
    FloatArraySample
    floatArray(const GT_DataArrayHandle &data, GT_DataArrayHandle &storage)
    {
	return FloatArraySample(data->getF32Array(storage), data->entries());
    }
    Int32ArraySample
    int32Array(const GT_PrimSubdivisionMesh::Tag &tag,
	    GT_DataArrayHandle &storage, int index=0)
    {
	return int32Array(tag.intArray(index), storage);
    }
    FloatArraySample
    floatArray(const GT_PrimSubdivisionMesh::Tag &tag,
	    GT_DataArrayHandle &storage, int index=0)
    {
	return floatArray(tag.intArray(index), storage);
    }

    static void
    splitVector3(const GT_DataArrayHandle &xyz,
		GT_DataArrayHandle &x,
		GT_DataArrayHandle &y,
		GT_DataArrayHandle &z)
    {
	exint		len = xyz->entries();
	UT_ASSERT(xyz->getTupleSize() == 3);
	UT_StackBuffer<UT_Vector3>	v3(len);
	xyz->fillArray((fpreal32 *)v3.array(), 0, len, 3);
	GT_Real32Array	*xn = new GT_Real32Array(len, 1);
	GT_Real32Array	*yn = new GT_Real32Array(len, 1);
	GT_Real32Array	*zn = new GT_Real32Array(len, 1);
	x.reset(xn);
	y.reset(yn);
	z.reset(zn);
	for (exint i = 0; i < len; ++i)
	{
	    xn->data()[i] = v3[i].x();
	    yn->data()[i] = v3[i].y();
	    zn->data()[i] = v3[i].z();
	}
    }


    /// A simple set of strings
    class SkipList
    {
    public:
	SkipList()
	    : myStrings()
	{
	}
	// Strings are passed in the c-tor
	SkipList(const char *arg0, ...)
	    : myStrings()
	{
	    va_list	 args;
	    va_start(args, arg0);
	    for (const char *s = arg0; s; s = va_arg(args, const char *))
	    {
		myStrings.insert(s, (void *)0);
	    }
	    va_end(args);
	}
	~SkipList() {}

	bool	contains(const char *token) const
		    { return myStrings.count(token) > 0; }

    private:
	UT_SymbolMap<void *, false>	myStrings;
    };
    static SkipList	thePolyMeshSkip("P", "v", "N", "uv", NULL);
    static SkipList	theSubDSkip("P", "v", "uv", "creaseweight", NULL);
    static SkipList	theCurvesSkip("P", "v", "N", "uv", "width", NULL);
    static SkipList	thePointsSkip("P", "v", "id", "width", NULL);
    static SkipList	theNuPatchSkip("P", "Pw", "v", "N", "uv", NULL);
    static SkipList	theEmptySkip("P", NULL);

    /// Create compound properties definition from an attribute list handle
    static bool
    makeCompoundProperties(PropertyMap &table,
			const GT_AttributeListHandle &attribs,
			OCompoundProperty &cp,
			Alembic::AbcGeom::GeometryScope scope,
			const GABC_OOptions &ctx,
			const SkipList &skips = SkipList())
    {
	int		 nwritten = 0;
	GABC_OProperty	*prop;

	if (!attribs)
	    return true;
	for (exint i = 0; i < attribs->entries(); ++i)
	{
	    const char			*name = attribs->getName(i);
	    const GT_DataArrayHandle	&data = attribs->get(i);
	    if (!data || skips.contains(name))
		continue;
	    if (data->getTypeInfo() == GT_TYPE_HIDDEN)
		continue;
	    if (table.findSymbol(name, &prop))
		continue;

	    prop = new GABC_OProperty(scope);
	    if (!prop->start(cp, name, data, ctx))
	    {
		delete prop;
	    }
	    else
	    {
		table.addSymbol(name, prop);
		nwritten++;
	    }
	}
	return nwritten > 0;
    }

    static void
    writeCompoundProperties(const PropertyMap &table,
		const GT_AttributeListHandle &attribs,
		const GABC_OOptions &ctx)
    {
	UT_Thing	thing;
	if (!attribs || !table.entries())
	    return;

	for (int i = 0; i < attribs->entries(); ++i)
	{
	    const char		*name = attribs->getName(i);
	    GABC_OProperty	*prop;
	    if (table.findSymbol(name, &prop))
		prop->update(attribs->get(i), ctx);
	}
    }

    template <typename POD_T>
    static const POD_T *
    extractArray(const GT_DataArrayHandle &from, GT_DataArrayHandle &storage)
    {
	UT_ASSERT(0 && "Not specialized");
	return NULL;
    }
    #define EXTRACT_ARRAY(POD_T, METHOD)	\
    template <> const POD_T * \
    extractArray<POD_T>(const GT_DataArrayHandle &a, GT_DataArrayHandle &store) \
		    { return a->METHOD(store); } \
	
    EXTRACT_ARRAY(uint8, getU8Array);
    EXTRACT_ARRAY(int32, getI32Array);
    EXTRACT_ARRAY(int64, getI64Array);
    EXTRACT_ARRAY(fpreal16, getF16Array);
    EXTRACT_ARRAY(fpreal32, getF32Array);
    EXTRACT_ARRAY(fpreal64, getF64Array);
    
    template <typename POD_T, GT_Storage T_STORAGE>
    static const POD_T *
    fillArray(const GT_DataArrayHandle &gt, GT_DataArrayHandle &store, int tsize)
    {
	if (!gt || gt->getTupleSize() < tsize)
	    return NULL;
	if (gt->getTupleSize() == tsize)
	{
	    store = gt;
	    return extractArray<POD_T>(gt, store);
	}

	// Compact the array by trimming off extra tuple entries
	GT_DANumeric<POD_T, T_STORAGE>	*num;
	num = new GT_DANumeric<POD_T, T_STORAGE>(gt->entries(), tsize,
			    gt->getTypeInfo());
	store.reset(num);
	POD_T	*values = num->data();
	gt->fillArray(values, 0, gt->entries(), tsize);
	return values;
    }

    template <typename POD_T, GT_Storage T_STORAGE,
	     typename GeomParamSample, typename TRAITS>
    static bool
    fillAttributeFromList(GeomParamSample &sample,
	    IntrinsicCache &cache,
	    const GABC_OOptions &ctx,
	    const char *name,
	    GT_DataArrayHandle &store,
	    const GT_AttributeListHandle &alist,
	    Alembic::AbcGeom::GeometryScope scope)
    {
	GT_DataArrayHandle	data;

	if (!alist || !(data = alist->get(name)))
	    return false;
	if (!cache.needWrite(ctx, name, data))
	    return true;

	typedef Alembic::Abc::TypedArraySample<TRAITS>	ArraySample;
	typedef typename TRAITS::value_type		ValueType;
	int			 tsize = TRAITS::dataType().getExtent();
	const POD_T		*vals;
	if (!(vals = fillArray<POD_T, T_STORAGE>(data, store, tsize)))
	    return false;
	sample.setScope(scope);
	sample.setVals(ArraySample((const ValueType *)vals, data->entries()));
	return true;
    }

    template <typename POD_T, GT_Storage T_STORAGE,
	     typename GeomParamSample, typename TRAITS>
    static bool
    fillAttribute(GeomParamSample &sample,
	    IntrinsicCache &cache,
	    const GABC_OOptions &ctx,
	    const char *name,
	    GT_DataArrayHandle &store,
	    const GT_AttributeListHandle &point,
	    const GT_AttributeListHandle &vertex = GT_AttributeListHandle(),
	    const GT_AttributeListHandle &uniform = GT_AttributeListHandle(),
	    const GT_AttributeListHandle &detail = GT_AttributeListHandle())
    {
	if (fillAttributeFromList<POD_T, T_STORAGE, GeomParamSample, TRAITS>(
		    sample, cache, ctx, name, store,
		    point, Alembic::AbcGeom::kVertexScope))
	{
	    return true;
	}
	if (fillAttributeFromList<POD_T, T_STORAGE, GeomParamSample, TRAITS>(
		    sample, cache, ctx, name, store,
		    vertex, Alembic::AbcGeom::kFacevaryingScope))
	{
	    return true;
	}
	if (fillAttributeFromList<POD_T, T_STORAGE, GeomParamSample, TRAITS>(
		    sample, cache, ctx, name, store,
		    uniform, Alembic::AbcGeom::kUniformScope))
	{
	    return true;
	}
	if (fillAttributeFromList<POD_T, T_STORAGE, GeomParamSample, TRAITS>(
		    sample, cache, ctx, name, store,
		    detail, Alembic::AbcGeom::kConstantScope))
	{
	    return true;
	}
	return false;
    }

    #define TYPED_FILL(METHOD, GEOM_PARAM, TRAITS, H_TYPE, GT_STORAGE) \
	static bool METHOD(Alembic::AbcGeom::GEOM_PARAM::Sample &sample, \
	    IntrinsicCache &cache, \
	    const GABC_OOptions &ctx, \
	    const char *name, \
	    GT_DataArrayHandle &store, \
	    const GT_AttributeListHandle &point, \
	    const GT_AttributeListHandle &vertex = GT_AttributeListHandle(), \
	    const GT_AttributeListHandle &uniform = GT_AttributeListHandle(), \
	    const GT_AttributeListHandle &detail = GT_AttributeListHandle()) \
	{ \
	    return fillAttribute<H_TYPE, GT_STORAGE, \
		Alembic::AbcGeom::GEOM_PARAM::Sample, Alembic::Abc::TRAITS>( \
				sample, cache, ctx, name, store, \
				point, vertex, uniform, detail); \
	}
    TYPED_FILL(fillP3f, OP3fGeomParam, P3fTPTraits, fpreal32, GT_STORE_REAL32);
    TYPED_FILL(fillV2f, OV2fGeomParam, V2fTPTraits, fpreal32, GT_STORE_REAL32);
    TYPED_FILL(fillV3f, OV3fGeomParam, V3fTPTraits, fpreal32, GT_STORE_REAL32);
    TYPED_FILL(fillN3f, ON3fGeomParam, N3fTPTraits, fpreal32, GT_STORE_REAL32);
    TYPED_FILL(fillF32, OFloatGeomParam, Float32TPTraits, fpreal32, GT_STORE_REAL32);

    static GT_AttributeListHandle
    transformedAttributes(const GT_Primitive &prim,
			    const GT_AttributeListHandle &a)
    {
	// If the primitive has a transform, we need to get the transformed
	// primitive attributes.
	const GT_TransformHandle	&x = prim.getPrimitiveTransform();
	if (a && x && !x->isIdentity())
	    return a->transform(x);
	return a;
    }

    static GT_AttributeListHandle
    pointAttributes(const GT_Primitive &prim)
    {
	return transformedAttributes(prim, prim.getPointAttributes());
    }
    static GT_AttributeListHandle
    vertexAttributes(const GT_Primitive &prim)
    {
	return transformedAttributes(prim, prim.getVertexAttributes());
    }
    static GT_AttributeListHandle
    uniformAttributes(const GT_Primitive &prim)
    {
	return transformedAttributes(prim, prim.getUniformAttributes());
    }
    static GT_AttributeListHandle
    detailAttributes(const GT_Primitive &prim)
    {
	return transformedAttributes(prim, prim.getDetailAttributes());
    }

    template <typename ABC_TYPE>
    static void
    fillFaceSets(const UT_Array<std::string> &names,
	    ABC_TYPE &dest, const GT_FaceSetMapPtr &src)
    {
	for (exint i = 0; i < names.entries(); ++i)
	{
	    const GT_FaceSetPtr	&set = src->find(names(i).c_str());
	    if (set)
	    {
		UT_ASSERT(dest.hasFaceSet(names(i)));
		OFaceSet		fset = dest.getFaceSet(names(i));
		OFaceSetSchema		&ss = fset.getSchema();
		OFaceSetSchema::Sample	sample;
		GT_DataArrayHandle	items = set->extractMembers();
		GT_DataArrayHandle	store;
		sample.setFaces(int32Array(items, store));
		ss.set(sample);
	    }
	}
    }

    static void
    fillPolyMesh(OPolyMesh &dest,
	    const GT_PrimPolygonMesh &src,
	    IntrinsicCache &cache, const GABC_OOptions &ctx)
    {
	Int32ArraySample	iInd;
	Int32ArraySample	iCnt;
	OP3fGeomParam::Sample	iPos;
	OV2fGeomParam::Sample	iUVs;
	ON3fGeomParam::Sample	iNml;
	OV3fGeomParam::Sample	iVel;
	GT_DataArrayHandle	counts;
	IntrinsicCache		storage;

	const GT_AttributeListHandle	&pt = pointAttributes(src);
	const GT_AttributeListHandle	&vtx = vertexAttributes(src);

	counts = src.getFaceCountArray().extractCounts();
	if (cache.needVertex(ctx, src.getVertexList()))
	    iInd = int32Array(src.getVertexList(), storage.vertexList());
	if (cache.needCounts(ctx, counts))
	    iCnt = int32Array(counts, storage.counts());
	fillP3f(iPos, cache, ctx, "P", storage.P(), pt);
	fillV2f(iUVs, cache, ctx, "uv", storage.uv(), pt, vtx);
	fillN3f(iNml, cache, ctx, "N", storage.N(), pt, vtx);
	fillV3f(iVel, cache, ctx, "v", storage.v(), pt, vtx);

	OPolyMeshSchema::Sample	sample(iPos.getVals(), iInd, iCnt, iUVs, iNml);
	if (iVel.valid())
	    sample.setVelocities(iVel.getVals());
	dest.getSchema().set(sample);
    }

    static void
    fillSubD(GABC_OGTGeometry &geo, OSubD &dest,
	    const GT_PrimSubdivisionMesh &src,
	    IntrinsicCache &cache, const GABC_OOptions &ctx)
    {
	Int32ArraySample	iInd;
	Int32ArraySample	iCnt;
	Int32ArraySample	iCreaseIndices;
	Int32ArraySample	iCreaseLengths;
	FloatArraySample	iCreaseSharpnesses;
	Int32ArraySample	iCornerIndices;
	FloatArraySample	iCornerSharpnesses;
	Int32ArraySample	iHoles;
	OP3fGeomParam::Sample	iPos;
	OV2fGeomParam::Sample	iUVs;
	OV3fGeomParam::Sample	iVel;
	GT_DataArrayHandle	counts;
	IntrinsicCache		storage;
	SecondaryCache		tstorage;	// Tag storage

	const GT_AttributeListHandle	&pt = pointAttributes(src);
	const GT_AttributeListHandle	&vtx = vertexAttributes(src);

	counts = src.getFaceCountArray().extractCounts();
	if (cache.needVertex(ctx, src.getVertexList()))
	    iInd = int32Array(src.getVertexList(), storage.vertexList());
	if (cache.needCounts(ctx, counts))
	    iCnt = int32Array(counts, storage.counts());
	fillP3f(iPos, cache, ctx, "P", storage.P(), pt);
	fillV2f(iUVs, cache, ctx, "uv", storage.uv(), pt, vtx);
	fillV3f(iVel, cache, ctx, "v", storage.v(), pt, vtx);

	const GT_PrimSubdivisionMesh::Tag	*tag;
	tag = src.findTag("crease");
	if (tag && tag->intCount() == 1 && tag->realCount() == 1)
	{
	    SecondaryCache		&cache2 = geo.getSecondaryCache();
	    const GT_DataArrayHandle	&idx = tag->intArray(0);
	    const GT_DataArrayHandle	&sharp = tag->realArray(0);
	    bool			 needcounts = false;
	    if (cache2.needCreaseIndices(ctx, idx))
	    {
		iCreaseIndices = int32Array(idx, tstorage.creaseIndices());
		needcounts = true;
	    }
	    if (cache2.needCreaseSharpnesses(ctx, sharp))
	    {
		iCreaseSharpnesses = floatArray(sharp,
				tstorage.creaseSharpnesses());
	    }
	    if (needcounts)
	    {
		// Counts need to be an array of 2's -- one for each sharpness
		GT_DataArrayHandle	two;
		two.reset(new GT_IntConstant(sharp->entries(), 2));
		iCreaseLengths = int32Array(two, tstorage.creaseLengths());
	    }
	}
	tag = src.findTag("corner");
	if (tag && tag->intCount() == 1 && tag->realCount() == 1)
	{
	    SecondaryCache		&cache2 = geo.getSecondaryCache();
	    const GT_DataArrayHandle	&idx = tag->intArray(0);
	    const GT_DataArrayHandle	&sharp = tag->realArray(0);
	    if (cache2.needCornerIndices(ctx, idx))
	    {
		iCornerIndices = int32Array(idx, tstorage.cornerIndices());
	    }
	    if (cache2.needCornerSharpnesses(ctx, sharp))
	    {
		iCornerSharpnesses = floatArray(sharp,
				tstorage.cornerSharpnesses());
	    }
	}
	tag = src.findTag("hole");
	if (tag && tag->intCount() == 1)
	{
	    SecondaryCache		&cache2 = geo.getSecondaryCache();
	    const GT_DataArrayHandle	&idx = tag->intArray(0);
	    if (cache2.needHoleIndices(ctx, idx))
		iHoles = int32Array(idx, tstorage.holeIndices());
	}

	OSubDSchema::Sample	sample(iPos.getVals(), iInd, iCnt,
			iCreaseIndices, iCreaseLengths, iCreaseSharpnesses,
			iCornerIndices, iCornerSharpnesses,
			iHoles);
	if (iUVs.valid())
	    sample.setUVs(iUVs);
	if (iVel.valid())
	    sample.setVelocities(iVel.getVals());
	dest.getSchema().set(sample);
    }


    static void
    fillCurves(OCurves &dest, const GT_PrimCurveMesh &src,
	    IntrinsicCache &cache, const GABC_OOptions &ctx)
    {
	OP3fGeomParam::Sample			iPos;
	Int32ArraySample			iCnt;
	OFloatGeomParam::Sample			iWidths;
	OV2fGeomParam::Sample			iUVs;
	ON3fGeomParam::Sample			iNml;
	OV3fGeomParam::Sample			iVel;
	GT_DataArrayHandle			counts;
	IntrinsicCache				storage;
	Alembic::AbcGeom::CurveType		iDegree;
	Alembic::AbcGeom::CurvePeriodicity	iPeriod;
	Alembic::AbcGeom::BasisType		iBasis;

	switch (src.getBasis())
	{
	    case GT_BASIS_BEZIER:
		iDegree = Alembic::AbcGeom::kCubic;
		iBasis = Alembic::AbcGeom::kBezierBasis;
	    case GT_BASIS_LINEAR:
	    default:
		iDegree = Alembic::AbcGeom::kLinear;
		iBasis = Alembic::AbcGeom::kNoBasis;
		break;
	}
	iPeriod = src.getWrap() ? Alembic::AbcGeom::kPeriodic
				: Alembic::AbcGeom::kNonPeriodic;

	const GT_AttributeListHandle	&pt = vertexAttributes(src);
	const GT_AttributeListHandle	&uniform = uniformAttributes(src);
	const GT_AttributeListHandle	&detail = detailAttributes(src);

	counts = src.getCurveCountArray().extractCounts();
	if (cache.needCounts(ctx, counts))
	    iCnt = int32Array(counts, storage.counts());
	fillP3f(iPos, cache, ctx, "P", storage.P(), pt);
	fillV2f(iUVs, cache, ctx, "uv", storage.uv(), pt);
	fillN3f(iNml, cache, ctx, "N", storage.N(), pt);
	fillV3f(iVel, cache, ctx, "v", storage.v(), pt);
	fillF32(iWidths, cache, ctx, "width", storage.width(), pt,
		GT_AttributeListHandle(), uniform, detail);

	OCurvesSchema::Sample	sample(iPos.getVals(), iCnt,
		iDegree, iPeriod, iWidths, iUVs, iNml, iBasis);
	if (iVel.valid())
	    sample.setVelocities(iVel.getVals());
	dest.getSchema().set(sample);
    }

    static void
    fillPoints(OPoints &dest, const GT_PrimPointMesh &src,
	    IntrinsicCache &cache, const GABC_OOptions &ctx)
    {
	OP3fGeomParam::Sample			iPos;
	UInt64ArraySample			iId;
	OFloatGeomParam::Sample			iWidths;
	OV3fGeomParam::Sample			iVel;
	GT_DataArrayHandle			ids;
	IntrinsicCache				storage;

	const GT_AttributeListHandle	&pt = pointAttributes(src);
	const GT_AttributeListHandle	&detail = detailAttributes(src);
	ids = pt->get("id");
	if (!ids)
	{
	    ids = pt->get("__vertex_id");
	    if (!ids)
	    {
		ids = pt->get("__point_id");
		if (!ids)
		    ids.reset(new GT_DARange(0, src.getPointCount()));
	    }
	}
	iId = uint64Array(ids, storage.id());
	fillP3f(iPos, cache, ctx, "P", storage.P(), pt);
	fillV3f(iVel, cache, ctx, "v", storage.v(), pt);
	fillF32(iWidths, cache, ctx, "width", storage.width(), pt,
		GT_AttributeListHandle(), GT_AttributeListHandle(), detail);

	if (cache.needWrite(ctx, "id", ids))
	{
	    // Topology changed
	    OPointsSchema::Sample	sample(iPos.getVals(),
						iId, iVel.getVals(), iWidths);
	    dest.getSchema().set(sample);
	}
	else
	{
	    // Topology unchanged
	    OPointsSchema::Sample	sample(iPos.getVals(),
						iVel.getVals(), iWidths);
	    dest.getSchema().set(sample);
	}
    }

    static void
    fillNuPatch(GABC_OGTGeometry &geo,
	    ONuPatch &dest, const GT_PrimNuPatch &src,
	    IntrinsicCache &cache, const GABC_OOptions &ctx)
    {
	FloatArraySample	iUKnot;
	FloatArraySample	iVKnot;
	FloatArraySample	iPosWeight;
	OP3fGeomParam::Sample	iPos;
	ON3fGeomParam::Sample	iNml;
	OV2fGeomParam::Sample	iUVs;
	OV3fGeomParam::Sample	iVel;
	IntrinsicCache		storage;

	const GT_AttributeListHandle		&pt = vertexAttributes(src);
	const GT_DataArrayHandle		&Pw = pt->get("Pw");

	fillP3f(iPos, cache, ctx, "P", storage.P(), pt);
	fillP3f(iPos, cache, ctx, "P", storage.P(), pt);
	fillV2f(iUVs, cache, ctx, "uv", storage.uv(), pt);
	fillN3f(iNml, cache, ctx, "N", storage.N(), pt);
	fillV3f(iVel, cache, ctx, "v", storage.v(), pt);
	if (cache.needWrite(ctx, "uknots", src.getUKnots()))
	    iUKnot = floatArray(src.getUKnots(), storage.uknots());
	if (cache.needWrite(ctx, "vknots", src.getVKnots()))
	    iVKnot = floatArray(src.getVKnots(), storage.vknots());
	if (Pw && cache.needWrite(ctx, "Pw", Pw))
	    iPosWeight = floatArray(Pw, storage.Pw());

	ONuPatchSchema::Sample	sample(iPos.getVals(),
			src.getNu(), src.getNv(),
			src.getUOrder(), src.getVOrder(),
			iUKnot, iVKnot,
			iNml, iUVs, iPosWeight);
	if (iVel.valid())
	    sample.setVelocities(iVel.getVals());

	// The trim storage needs to be declared out of the trim scope since it
	// needs to be maintained until the sample is written.
	SecondaryCache		tstore;
	GT_DataArrayHandle	trimNcurves, trimN, trimU, trimV, trimW;
	if (src.isTrimmed())
	{
	    SecondaryCache		&cache2=geo.getSecondaryCache();
	    const GT_TrimNuCurves	*trims;
	    Int32ArraySample		 iTrim_nCurves, iTrim_n, iTrim_order;
	    FloatArraySample		 iTrim_knot, iTrim_min, iTrim_max;
	    FloatArraySample		 iTrim_u, iTrim_v, iTrim_w;

	    UT_VERIFY(trims = src.getTrimCurves());
	    trimNcurves = trims->getLoopCountArray().extractCounts();
	    trimN = trims->getCurveCountArray().extractCounts();
	    splitVector3(trims->getUV(), trimU, trimV, trimW);
	    if (cache2.needTrimNCurves(ctx, trimNcurves))
		iTrim_nCurves = int32Array(trimNcurves, tstore.trimNCurves());
	    if (cache2.needTrimN(ctx, trimN))
		iTrim_n = int32Array(trimN, tstore.trimN());
	    if (cache2.needTrimOrder(ctx, trims->getOrders()))
		iTrim_order = int32Array(trims->getOrders(), tstore.trimOrder());
	    if (cache2.needTrimKnot(ctx, trims->getKnots()))
		iTrim_knot = floatArray(trims->getKnots(), tstore.trimKnot());
	    if (cache2.needTrimMin(ctx, trims->getMin()))
		iTrim_min = floatArray(trims->getMin(), tstore.trimMin());
	    if (cache2.needTrimMax(ctx, trims->getMax()))
		iTrim_max = floatArray(trims->getMax(), tstore.trimMax());
	    if (cache2.needTrimU(ctx, trimU))
		iTrim_u = floatArray(trimU, tstore.trimU());
	    if (cache2.needTrimU(ctx, trimV))
		iTrim_v = floatArray(trimV, tstore.trimV());
	    if (cache2.needTrimU(ctx, trimW))
		iTrim_w = floatArray(trimW, tstore.trimW());
	    sample.setTrimCurve(trimNcurves->entries(),
		    iTrim_nCurves, iTrim_n, iTrim_order, iTrim_knot,
		    iTrim_min, iTrim_max, iTrim_u, iTrim_v, iTrim_w);
	}

	dest.getSchema().set(sample);
    }



    GT_PrimitiveHandle
    getPrimitive(const GT_PrimitiveHandle &prim, int &expected_type)
    {
	UT_ASSERT(prim);
	GT_PrimitiveHandle	refined;
	switch (prim->getPrimitiveType())
	{
	    case GT_PRIM_POLYGON_MESH:
	    case GT_PRIM_SUBDIVISION_MESH:
	    case GT_PRIM_CURVE_MESH:
	    case GT_PRIM_POINT_MESH:
	    case GT_PRIM_NUPATCH:
		refined = prim;
		break;
	    case GT_GEO_PRIMTPSURF:
		refined = ((const GT_GEOPrimTPSurf *)(prim.get()))->buildNuPatch();
		break;
	    case GT_PRIM_CURVE:
		refined.reset(new GT_PrimCurveMesh(*(const GT_PrimCurve *)(prim.get())));
		break;
	    case GT_PRIM_POLYGON:
		refined.reset(new GT_PrimPolygonMesh(*(const GT_PrimPolygon *)(prim.get())));
		break;
	    case GT_PRIM_NUCURVES:
		refined = ((const GT_PrimNuCurves *)(prim.get()))->buildHull();
		break;
	    default:
		break;
	}
	if (refined)
	{
	    if (expected_type == GT_PRIM_UNDEFINED)
		expected_type = refined->getPrimitiveType();
	    if (expected_type != refined->getPrimitiveType())
		refined.reset(NULL);
	}
	return refined;
    }

    static bool
    cacheNeedWrite(const GABC_OOptions &ctx, const GT_DataArrayHandle &data,
	    GT_DataArrayHandle &cache)
    {
	if (ctx.optimizeSpace() < GABC_OOptions::OPTIMIZE_TOPOLOGY || !data)
	    return true;
	if (!cache || !cache->isEqual(*data))
	{
	    cache = data->harden();
	    return true;
	}
	return false;
    }
}

bool
GABC_OGTGeometry::IntrinsicCache::needWrite(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &data,
			GT_DataArrayHandle &cache)
{
    return cacheNeedWrite(ctx, data, cache);
}

void
GABC_OGTGeometry::IntrinsicCache::clear()
{
    myVertexList.reset(NULL);
    myCounts.reset(NULL);
    myP.reset(NULL);
    myPw.reset(NULL);
    myN.reset(NULL);
    myUV.reset(NULL);
    myVel.reset(NULL);
    myId.reset(NULL);
}

bool
GABC_OGTGeometry::IntrinsicCache::needWrite(const GABC_OOptions &ctx,
	const char *name, const GT_DataArrayHandle &data)
{
    if (!strcmp(name, "P"))
	return needWrite(ctx, data, myP);
    if (!strcmp(name, "v"))
	return needWrite(ctx, data, myVel);
    if (!strcmp(name, "uv"))
	return needWrite(ctx, data, myUV);
    if (!strcmp(name, "N"))
	return needWrite(ctx, data, myN);
    if (!strcmp(name, "id"))
	return needWrite(ctx, data, myId);
    if (!strcmp(name, "Pw"))
	return needWrite(ctx, data, myPw);
    if (!strcmp(name, "width"))
	return needWrite(ctx, data, myWidth);
    if (!strcmp(name, "uknots"))
	return needWrite(ctx, data, myUKnots);
    if (!strcmp(name, "vknots"))
	return needWrite(ctx, data, myVKnots);
    UT_ASSERT(0);
    return true;
}

SecondaryCache &
GABC_OGTGeometry::getSecondaryCache()
{
    // TODO: Double lock
    if (!mySecondaryCache)
	mySecondaryCache = new SecondaryCache();
    return *mySecondaryCache;
}

void
GABC_OGTGeometry::SecondaryCache::clear()
{
    for (int i = 0; i < 9; ++i)
	myData[i].reset(NULL);
}

bool
GABC_OGTGeometry::SecondaryCache::needWrite(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &data,
			GT_DataArrayHandle &cache)
{
    return cacheNeedWrite(ctx, data, cache);
}

//!-----------------------------------------------------------------------

GABC_OGTGeometry::GABC_OGTGeometry(const std::string &name)
    : myName(name)
    , myType(GT_PRIM_UNDEFINED)
    , mySecondaryCache(NULL)
{
    myShape.myVoidPtr = NULL;
}

GABC_OGTGeometry::~GABC_OGTGeometry()
{
    clearProperties();
    clearShape();
    clearCache();
}

void
GABC_OGTGeometry::clearProperties()
{
    for (int i = 0; i < MAX_PROPERTIES; ++i)
    {
	for (PropertyMap::iterator it = myProperties[i].begin(); !it.atEnd(); ++it)
	{
	    delete it.thing();
	}
	myProperties[i].clear();
    }
}

void
GABC_OGTGeometry::clearCache()
{
    myCache.clear();
    delete mySecondaryCache;
    mySecondaryCache = NULL;
}

void
GABC_OGTGeometry::clearShape()
{
    switch (myType)
    {
	case GT_PRIM_POLYGON_MESH:
	    delete myShape.myPolyMesh;
	    break;
	case GT_PRIM_SUBDIVISION_MESH:
	    delete myShape.mySubD;
	    break;
	case GT_PRIM_POINT_MESH:
	    delete myShape.myPoints;
	    break;
	case GT_PRIM_CURVE_MESH:
	    delete myShape.myCurves;
	    break;
	case GT_PRIM_NUPATCH:
	    delete myShape.myNuPatch;
	    break;
	default:
	    break;
    }
    myShape.myVoidPtr = NULL;
}

#if 0
bool
GABC_OGTGeometry::SecondaryCache::needTrim(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &nCurves,
			const GT_DataArrayHandle &n,
			const GT_DataArrayHandle &order,
			const GT_DataArrayHandle &knot,
			const GT_DataArrayHandle &min,
			const GT_DataArrayHandle &max,
			const GT_DataArrayHandle &u,
			const GT_DataArrayHandle &v,
			const GT_DataArrayHandle &w)
{
    bool	changed = false;
    // We can't put these in an individual if since the condition could be
    // short-circuited.  We need to ensure all values in the cache are updated.
    changed |= needWrite(ctx, nCurves, trimNCurves());
    changed |= needWrite(ctx, n, trimN());
    changed |= needWrite(ctx, order, trimOrder());
    changed |= needWrite(ctx, knot, trimKnot());
    changed |= needWrite(ctx, min, trimMin());
    changed |= needWrite(ctx, max, trimMax());
    changed |= needWrite(ctx, u, trimU());
    changed |= needWrite(ctx, v, trimV());
    changed |= needWrite(ctx, w, trimW());
    return changed;
}
#endif

void
GABC_OGTGeometry::writeProperties(const GT_PrimitiveHandle &prim,
			const GABC_OOptions &ctx)
{
    writeCompoundProperties(myProperties[VERTEX_PROPERTIES],
	    vertexAttributes(*prim), ctx);
    writeCompoundProperties(myProperties[POINT_PROPERTIES],
	    pointAttributes(*prim), ctx);
    writeCompoundProperties(myProperties[UNIFORM_PROPERTIES],
	    uniformAttributes(*prim), ctx);
    writeCompoundProperties(myProperties[DETAIL_PROPERTIES],
	    detailAttributes(*prim), ctx);
}

static GT_FaceSetMapPtr
getFaceSetMap(const GT_PrimitiveHandle &prim)
{
    GT_FaceSetMapPtr	facesets;
    switch (prim->getPrimitiveType())
    {
	case GT_PRIM_POLYGON_MESH:
	case GT_PRIM_SUBDIVISION_MESH:
	    {
		const GT_PrimPolygonMesh	*p;
		p = UTverify_cast<const GT_PrimPolygonMesh *>(prim.get());
		facesets = p->faceSetMap();
	    }
	    break;
	case GT_PRIM_CURVE_MESH:
	    {
		const GT_PrimCurveMesh	*p;
		p = UTverify_cast<const GT_PrimCurveMesh *>(prim.get());
		facesets = p->faceSetMap();
	    }
	    break;
    }
    return facesets;
}

void
GABC_OGTGeometry::makeFaceSets(const GT_PrimitiveHandle &prim,
	const GABC_OOptions &ctx)
{
    if (ctx.faceSetMode() == GABC_OOptions::FACESET_NONE)
    {
	return;
    }
    GT_DataArrayHandle	ids;
    GT_FaceSetMapPtr	facesets = getFaceSetMap(prim);
    if (!facesets)
	return;
    for (GT_FaceSetMap::iterator it = facesets->begin(); !it.atEnd(); ++it)
    {
	std::string		 name = it.name();
	const GT_FaceSetPtr	&set = it.faceSet();
	if (ctx.faceSetMode() == GABC_OOptions::FACESET_ALL_GROUPS ||
		set->entries() != 0)
	{
	    myFaceSetNames.append(name);
	    switch (myType)
	    {
		case GT_PRIM_POLYGON_MESH:
		    {
			OPolyMeshSchema	&ss = myShape.myPolyMesh->getSchema();
			ss.createFaceSet(name);
		    }
		    break;
		case GT_PRIM_SUBDIVISION_MESH:
		    {
			OSubDSchema	&ss = myShape.mySubD->getSchema();
			ss.createFaceSet(name);
		    }
		    break;
	    }
	}
    }
}

void
GABC_OGTGeometry::makeProperties(const GT_PrimitiveHandle &prim,
			const GABC_OOptions &ctx)
{
    clearProperties();
    if (!ctx.saveAttributes())
	return;

    const SkipList			*skip = &theEmptySkip;
    OCompoundProperty			 cp;
    Alembic::AbcGeom::GeometryScope	 pt = Alembic::AbcGeom::kUnknownScope;
    Alembic::AbcGeom::GeometryScope	 vtx = Alembic::AbcGeom::kUnknownScope;
    Alembic::AbcGeom::GeometryScope	 uni = Alembic::AbcGeom::kUnknownScope;
    switch (myType)
    {
	case GT_PRIM_POLYGON_MESH:
	    vtx = Alembic::AbcGeom::kFacevaryingScope;
	    pt = Alembic::AbcGeom::kVaryingScope;
	    uni = Alembic::AbcGeom::kUniformScope;
	    skip = &thePolyMeshSkip;
	    cp = myShape.myPolyMesh->getSchema().getArbGeomParams();
	    break;
	case GT_PRIM_SUBDIVISION_MESH:
	    vtx = Alembic::AbcGeom::kFacevaryingScope;
	    pt = Alembic::AbcGeom::kVaryingScope;
	    uni = Alembic::AbcGeom::kUniformScope;
	    skip = &theSubDSkip;
	    cp = myShape.mySubD->getSchema().getArbGeomParams();
	    break;
	case GT_PRIM_POINT_MESH:
	    pt = Alembic::AbcGeom::kVaryingScope;
	    uni = Alembic::AbcGeom::kUniformScope;
	    skip = &thePointsSkip;
	    cp = myShape.myPoints->getSchema().getArbGeomParams();
	    break;
	case GT_PRIM_CURVE_MESH:
	    vtx = Alembic::AbcGeom::kVertexScope;
	    uni = Alembic::AbcGeom::kUniformScope;
	    skip = &theCurvesSkip;
	    cp = myShape.myCurves->getSchema().getArbGeomParams();
	    break;
	case GT_PRIM_NUPATCH:
	    vtx = Alembic::AbcGeom::kFacevaryingScope;
	    uni = Alembic::AbcGeom::kUniformScope;
	    skip = &theNuPatchSkip;
	    cp = myShape.myNuPatch->getSchema().getArbGeomParams();
	    break;
	default:
	    UT_ASSERT(0);
    }

    if (vtx != Alembic::AbcGeom::kUnknownScope)
    {
	// We don't need to transform the attributes to build the Alembic
	// objects, only when we write their values.
	makeCompoundProperties(myProperties[VERTEX_PROPERTIES],
		    prim->getVertexAttributes(), cp, vtx, ctx, *skip);
    }
    if (pt != Alembic::AbcGeom::kUnknownScope)
    {
	makeCompoundProperties(myProperties[POINT_PROPERTIES],
		    prim->getPointAttributes(), cp, pt, ctx, *skip);
    }
    if (uni != Alembic::AbcGeom::kUnknownScope)
    {
	makeCompoundProperties(myProperties[UNIFORM_PROPERTIES],
		prim->getUniformAttributes(), cp, uni, ctx, *skip);
    }
    makeCompoundProperties(myProperties[DETAIL_PROPERTIES],
		prim->getDetailAttributes(),
		cp, Alembic::AbcGeom::kConstantScope, ctx, *skip);
}

bool
GABC_OGTGeometry::isPrimitiveSupported(const GT_PrimitiveHandle &prim)
{
    if (!prim)
	return false;
    switch (prim->getPrimitiveType())
    {
	case GT_PRIM_POLYGON_MESH:
	case GT_PRIM_SUBDIVISION_MESH:
	case GT_PRIM_CURVE_MESH:
	case GT_PRIM_POINT_MESH:
	case GT_PRIM_NUPATCH:
	case GT_GEO_PRIMTPSURF:
	case GT_PRIM_CURVE:
	case GT_PRIM_POLYGON:
	case GT_PRIM_NUCURVES:
	    return true;
	default:
	    break;
    }
    return false;
}

bool
GABC_OGTGeometry::start(const GT_PrimitiveHandle &src,
	const OObject &parent,
	GABC_OError &err,
	const GABC_OOptions &ctx)
{
    UT_ASSERT(src);
    myCache.clear();
    myType = GT_PRIM_UNDEFINED;
    GT_PrimitiveHandle	prim = getPrimitive(src, myType);
    if (!prim)
    {
	UT_ASSERT(myType == GT_PRIM_UNDEFINED);
	return false;
    }
    switch (prim->getPrimitiveType())
    {
	// Direct mapping to Alembic primitives
	case GT_PRIM_POLYGON_MESH:
	    {
		myShape.myPolyMesh = new OPolyMesh(parent, myName,
					ctx.timeSampling());
		makeProperties(prim, ctx);
		makeFaceSets(prim, ctx);
		return update(prim, err, ctx);
	    }
	    return true;
	case GT_PRIM_SUBDIVISION_MESH:
	    {
		myShape.mySubD = new OSubD(parent, myName,
					ctx.timeSampling());
		makeProperties(prim, ctx);
		makeFaceSets(prim, ctx);
		return update(prim, err, ctx);
	    }
	    return true;
	case GT_PRIM_POINT_MESH:
	    {
		myShape.myPoints = new OPoints(parent, myName,
					ctx.timeSampling());
		makeProperties(prim, ctx);
		return update(prim, err, ctx);
	    }
	    return true;
	case GT_PRIM_CURVE_MESH:
	    {
		myShape.myCurves = new OCurves(parent, myName,
					ctx.timeSampling());
		makeProperties(prim, ctx);
		return update(prim, err, ctx);
	    }
	    return true;
	case GT_PRIM_NUPATCH:
	    {
		myShape.myNuPatch = new ONuPatch(parent, myName,
					ctx.timeSampling());
		makeProperties(prim, ctx);
		return update(prim, err, ctx);
	    }
	    return true;

	default:
	    UT_ASSERT(0);
	    break;
    }
    UT_ASSERT(0 && "Unhandled primitive");
    return false;
}

bool
GABC_OGTGeometry::update(const GT_PrimitiveHandle &src,
	GABC_OError &err, const GABC_OOptions &ctx)
{
    UT_ASSERT(src);
    if (myType == GT_PRIM_UNDEFINED)
    {
	err.error("Need to save first frame!");
	return false;
    }
    GT_PrimitiveHandle	prim = getPrimitive(src, myType);
    if (!prim)
	return false;
    switch (myType)
    {
	case GT_PRIM_POLYGON_MESH:
	    fillPolyMesh(*myShape.myPolyMesh,
			*(GT_PrimPolygonMesh *)(prim.get()),
			myCache, ctx);
	    writeProperties(prim, ctx);
	    fillFaceSets(myFaceSetNames,
			myShape.myPolyMesh->getSchema(),
			((GT_PrimPolygonMesh *)(prim.get()))->faceSetMap());
	    return true;
	case GT_PRIM_SUBDIVISION_MESH:
	    fillSubD(*this, *myShape.mySubD,
			*(GT_PrimSubdivisionMesh *)(prim.get()),
			myCache, ctx);
	    writeProperties(prim, ctx);
	    fillFaceSets(myFaceSetNames,
			myShape.mySubD->getSchema(),
			((GT_PrimSubdivisionMesh *)(prim.get()))->faceSetMap());
	    return true;
	case GT_PRIM_POINT_MESH:
	    fillPoints(*myShape.myPoints,
			*(GT_PrimPointMesh *)(prim.get()),
			myCache, ctx);
	    writeProperties(prim, ctx);
	    return true;
	case GT_PRIM_CURVE_MESH:
	    fillCurves(*myShape.myCurves,
			*(GT_PrimCurveMesh *)(prim.get()),
			myCache, ctx);
	    writeProperties(prim, ctx);
	    return true;
	case GT_PRIM_NUPATCH:
	    fillNuPatch(*this, *myShape.myNuPatch,
			*(GT_PrimNuPatch *)(prim.get()),
			myCache, ctx);
	    writeProperties(prim, ctx);
	    return true;
	default:
	    UT_ASSERT(0);
	    break;
    }
    err.error("Invalid primitive type (%s): %d", prim->className(), prim->getPrimitiveType());
    return false;
}
