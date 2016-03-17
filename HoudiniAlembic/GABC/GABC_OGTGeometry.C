/*
 * Copyright (c) 2016
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

#include "GABC_OArrayProperty.h"
#include "GABC_OError.h"
#include "GABC_OGTGeometry.h"
#include "GABC_OOptions.h"
#include "GABC_OProperty.h"
#include "GABC_Util.h"
#include <stdarg.h>
#include <GT/GT_DAConstant.h>
#include <GT/GT_DAConstantValue.h>
#include <GT/GT_DANumeric.h>
#include <GT/GT_DARange.h>
#include <GT/GT_PrimPolygon.h>
#include <GT/GT_PrimPolygonMesh.h>
#include <GT/GT_PrimSubdivisionMesh.h>
#include <GT/GT_PrimPointMesh.h>
#include <GT/GT_PrimCurve.h>
#include <GT/GT_PrimCurveMesh.h>
#include <GT/GT_PrimNuPatch.h>
#include <GT/GT_TrimNuCurves.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_StackBuffer.h>
#include <UT/UT_HashFunctor.h>
#include <UT/UT_FSATable.h>

// For simple refinement
#include <GT/GT_GEOPrimTPSurf.h>

using namespace GABC_NAMESPACE;

namespace
{
    typedef Alembic::Abc::OCompoundProperty	OCompoundProperty;
    typedef Alembic::Abc::TimeSamplingPtr	TimeSamplingPtr;

    typedef Alembic::Abc::V2f			V2f;
    typedef Alembic::Abc::V3f			V3f;
    typedef Alembic::Abc::UcharArraySample	UcharArraySample;
    typedef Alembic::Abc::UcharArraySample	UInt8ArraySample;
    typedef Alembic::Abc::Int32ArraySample	Int32ArraySample;
    typedef Alembic::Abc::UInt32ArraySample	UInt32ArraySample;
    typedef Alembic::Abc::UInt64ArraySample	UInt64ArraySample;
    typedef Alembic::Abc::FloatArraySample	FloatArraySample;
    typedef Alembic::Abc::V2fArraySample	V2fArraySample;
    typedef Alembic::Abc::V3fArraySample	V3fArraySample;
    typedef Alembic::Abc::P3fArraySample	P3fArraySample;
    typedef Alembic::Abc::P3fArraySamplePtr	P3fArraySamplePtr;

    typedef Alembic::AbcGeom::ObjectVisibility	ObjectVisibility;

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

    typedef Alembic::AbcGeom::OFloatGeomParam	OFloatGeomParam;
    typedef Alembic::AbcGeom::OP3fGeomParam	OP3fGeomParam;
    typedef Alembic::AbcGeom::OV2fGeomParam	OV2fGeomParam;
    typedef Alembic::AbcGeom::OV3fGeomParam	OV3fGeomParam;
    typedef Alembic::AbcGeom::ON3fGeomParam	ON3fGeomParam;

    typedef GABC_OGTGeometry::PropertyMap       PropertyMap;
    typedef GABC_OGTGeometry::PropertyMapInsert PropertyMapInsert;
    typedef GABC_OGTGeometry::IgnoreList        IgnoreList;
    typedef GABC_OGTGeometry::IntrinsicCache    IntrinsicCache;
    typedef GABC_OGTGeometry::SecondaryCache    SecondaryCache;

    static UT_FSATableT<GT_Owner, GT_OWNER_INVALID>	theXlateOwner(
	    GT_OWNER_POINT,	"pt",
	    GT_OWNER_VERTEX,	"vtx",
	    GT_OWNER_PRIMITIVE,	"prim",
	    GT_OWNER_DETAIL,	"detail",
	    GT_OWNER_INVALID,	NULL
    );

    class RiXlate
    {
    public:
	class Key
	{
	public:
	    Key(const UT_StringHolder &name, GT_Owner owner)
		: myName(name)
		, myOwner(owner)
	    {
	    }
	    uint	hash() const
	    {
		return myName.hash() ^ SYSwang_inthash(myOwner);
	    }
	    bool	operator==(const Key &k) const
	    {
		return myOwner == k.myOwner && myName == k.myName;
	    }
	    UT_StringHolder	myName;
	    GT_Owner		myOwner;
	};
	class Item
	{
	public:
	    Item()
		: myName()
		, myRibName()
		, myGTOwner(GT_OWNER_INVALID)
		, myScope(Alembic::AbcGeom::kUnknownScope)
	    {
	    }
	    Item(const UT_StringHolder &name,
		    const UT_StringHolder &ribname,
		    GT_Owner owner,
		    Alembic::AbcGeom::GeometryScope scope)
		: myName(name)
		, myRibName(ribname)
		, myGTOwner(owner)
		, myScope(scope)
	    {
	    }
	    const UT_StringHolder	&name() const { return myName; }
	    const UT_StringHolder	&ribname() const { return myRibName; }
	    GT_Owner			 owner() const { return myGTOwner; }
	    Alembic::AbcGeom::GeometryScope scope() const { return myScope; }
	private:
	    UT_StringHolder			myName;
	    UT_StringHolder			myRibName;
	    GT_Owner				myGTOwner;
	    Alembic::AbcGeom::GeometryScope	myScope;
	};
	typedef UT_Map<Key, Item, UT_HashFunctor<Key> >	MapType;

	RiXlate(const GT_Primitive &prim, int type)
	{
	    myParametric = (type == GT_PRIM_CURVE_MESH ||
			    type == GT_PRIM_NUPATCH);

	    GT_Owner		owner;
	    auto	rixlate = prim.findAttribute("rixlate", owner, 0);
	    if (rixlate && rixlate->entries() == 1 && rixlate->getTupleSize())
	    {
		int		nstrings = rixlate->getTupleSize();
		for (exint i = 0; i < nstrings; ++i)
		    addXlate(rixlate->getS(0, i));
	    }
	}

	void	addXlate(const char *src)
	{
	    UT_String				 str(src);
	    char				*f[8];

	    if (str.tokenize(f, 8, ':') < 4)
		return;

	    GT_Owner owner = theXlateOwner.findSymbol(f[0]);
	    if (owner == GT_OWNER_INVALID)
		return;

	    Alembic::AbcGeom::GeometryScope	scope;
	    switch (owner)
	    {
		case GT_OWNER_POINT:
		    scope = Alembic::AbcGeom::kVaryingScope;
		    break;
		case GT_OWNER_VERTEX:
		    if (!strncmp(f[3], "v_", 2))
			scope = Alembic::AbcGeom::kFacevaryingScope;
		    else
			scope = Alembic::AbcGeom::kVertexScope;
		    break;
		case GT_OWNER_UNIFORM:
		    scope = Alembic::AbcGeom::kUniformScope;
		    break;
		default:
		    scope = Alembic::AbcGeom::kConstantScope;
	    }
	    UT_StringHolder	name(f[1]);
	    UT_StringHolder	ribname(f[2]);
	    myMap[Key(name, owner)] = Item(name, ribname, owner, scope);
	}
	Alembic::AbcGeom::GeometryScope	getScope(const UT_StringHolder &name,
					    GT_Owner owner) const
	{
	    auto	it = myMap.find(Key(name, owner));
	    if (it != myMap.end())
	    {
		return it->second.scope();
	    }
	    switch (owner)
	    {
		case GT_OWNER_POINT:
		    return Alembic::AbcGeom::kVaryingScope;
		case GT_OWNER_VERTEX:
		    return myParametric
				? Alembic::AbcGeom::kVertexScope
				: Alembic::AbcGeom::kFacevaryingScope;
		case GT_OWNER_UNIFORM:
		    return Alembic::AbcGeom::kUniformScope;
		default:
		    return Alembic::AbcGeom::kConstantScope;
	    }
	}
    private:
	MapType	myMap;
	bool	myParametric;
    };

    UInt8ArraySample
    uint8Array(const GT_DataArrayHandle &data, GT_DataArrayHandle &storage)
    {
	return UInt8ArraySample(data->getU8Array(storage), data->entries());
    }
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

    static IgnoreList	thePolyMeshSkip("N", "uv", (void *)NULL);
    static IgnoreList	theSubDSkip("uv", "creaseweight", (void *)NULL);
    static IgnoreList	theCurvesSkip("Pw", "N", "uv", "width", (void *)NULL);
    static IgnoreList	thePointsSkip("id", "width", (void *)NULL);
    static IgnoreList	theNuPatchSkip("Pw", "N", "uv", (void *)NULL);
    static IgnoreList	theEmptySkip((const char *)NULL);

    /// Create arbGeomParams from an attribute list handle
    static bool
    makeGeomParams(PropertyMap &arb_map,
            const GT_AttributeListHandle &attribs,
            OCompoundProperty &cp,
            GT_Owner owner,
	    const RiXlate &rixlate,
            GABC_OError &err,
            const GABC_OOptions &ctx,
            const IgnoreList &skips)
    {
        GABC_OProperty     *prop;
        const IgnoreList   &default_skips = GABC_OGTGeometry::getDefaultSkip();

        if (!attribs)
        {
            return true;
        }

        for (exint i = 0; i < attribs->entries(); ++i)
        {
	    auto	 name = attribs->getName(i);
	    auto	 exp_name = attribs->getExportName(i);
	    auto	&data = attribs->get(i);
	    auto	 scope = rixlate.getScope(name, owner);

            if (!data
                    || skips.contains(exp_name)
                    || default_skips.contains(exp_name)
                    || !ctx.matchAttribute(scope, exp_name)
                    || data->getTupleSize() < 1
                    || data->getTypeInfo() == GT_TYPE_HIDDEN
                    || arb_map.count(exp_name))
            {
                continue;
            }

            prop = new GABC_OArrayProperty(scope);
            if (!prop->start(cp, exp_name, data, err, ctx))
            {
                delete prop;
                return false;
            }
            else
            {
                arb_map.insert(PropertyMapInsert(name, prop));
            }
        }

        return true;
    }

    static bool
    writeGeomProperties(const PropertyMap &arb_map,
            const GT_AttributeListHandle &attribs,
            GABC_OError &err,
            const GABC_OOptions &ctx)
    {
        bool        result = true;

        if (!attribs || !arb_map.size())
        {
            return true;
        }

        for (PropertyMap::const_iterator it = arb_map.begin();
                result && it != arb_map.end();
                ++it)
        {
            int index = attribs->getIndex(it->first.c_str());

            if (index >= 0)
            {
                result &= it->second->update(attribs->get(index), err, ctx);
            }
            else
            {
                exint   num_samples = it->second->getNumSamples();
                if (num_samples)
                {
                    result &= it->second->updateFromPrevious();
                }
            }
        }

        return result;
    }

    static void
    writePropertiesFromPrevious(const PropertyMap &pmap)
    {
        if (!pmap.size())
        {
            return;
        }

        exint   num_samples;

        // Write a new sample using the previous sample (if a previous
        // sample exists)
        for (PropertyMap::const_iterator it = pmap.begin();
                it != pmap.end();
                ++it)
        {
            num_samples = it->second->getNumSamples();
            if (num_samples)
            {
                it->second->updateFromPrevious();
            }
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

    template <typename GeomParamSample, typename TRAITS>
    static bool
    fillAttributeFromList(GeomParamSample &sample,
	    IntrinsicCache &cache,
	    const GABC_OOptions &ctx,
	    const char *name,
	    GT_DataArrayHandle &store,
	    const GT_AttributeListHandle &alist,
	    const RiXlate &rixlate,
	    GT_Owner owner,
	    UT_ValArray<uint32> *uv_idx_storage,
	    UT_Fpreal32Array *uv_data_storage)
    {
	GT_DataArrayHandle	data;

	if (!alist || !(data = alist->get(name)))
	    return false;
	if (!cache.needWrite(ctx, name, data))
	    return true;

	typedef Alembic::Abc::TypedArraySample<TRAITS>	ArraySample;
	typedef typename TRAITS::value_type		ValueType;
	int tsize = TRAITS::dataType().getExtent();
	int n = data->entries();
	const fpreal32 *flatarray =
		fillArray<fpreal32, GT_STORE_REAL32>(data, store, tsize);
	if(!flatarray)
	    return false;

	sample.setScope(rixlate.getScope(name, owner));

	// Use indexed samples for UV as other tools use this to identify
	// shared vertices.
	if(uv_idx_storage && uv_data_storage && tsize == 2)
	{
	    uv_idx_storage->clear();
	    uv_data_storage->clear();
	    UT_Map<std::pair<fpreal32, fpreal32>, int> map;

	    int idx = 0;
	    for(int i = 0; i < n; ++i)
	    {
		fpreal32 u = flatarray[idx++];
		fpreal32 v = flatarray[idx++];
		std::pair<fpreal32, fpreal32> key(u, v);

		int j;
		auto it = map.find(key);
		if(it != map.end())
		    j = it->second;
		else
		{
		    uv_data_storage->append(u);
		    uv_data_storage->append(v);
		    j = map.size();
		    map.emplace(key, j);
		}
		uv_idx_storage->append(j);
	    }

	    sample.setIndices(UInt32ArraySample(uv_idx_storage->array(),
						uv_idx_storage->entries()));
	    sample.setVals(ArraySample((const ValueType *)uv_data_storage->array(),
				       map.size()));
	}
	else
	    sample.setVals(ArraySample((const ValueType *)flatarray, n));

	return true;
    }

    static bool
    matchAttribute(const GABC_OOptions &ctx,
	    const char *name,
	    const GT_AttributeListHandle &point,
	    const GT_AttributeListHandle &vertex = GT_AttributeListHandle(),
	    const GT_AttributeListHandle &uniform = GT_AttributeListHandle(),
	    const GT_AttributeListHandle &detail = GT_AttributeListHandle())
    {
	if (point && point->getIndex(name) >= 0)
	    return ctx.matchAttribute(GA_ATTRIB_POINT, name);
	if (vertex && vertex->getIndex(name) >= 0)
	    return ctx.matchAttribute(GA_ATTRIB_VERTEX, name);
	if (uniform && uniform->getIndex(name) >= 0)
	    return ctx.matchAttribute(GA_ATTRIB_PRIMITIVE, name);
	if (detail && detail->getIndex(name) >= 0)
	    return ctx.matchAttribute(GA_ATTRIB_DETAIL, name);
	return true;
    }

    template <typename GeomParamSample, typename TRAITS>
    static bool
    fillAttribute(GeomParamSample &sample,
	    IntrinsicCache &cache,
	    const GABC_OOptions &ctx,
	    const char *name,
	    GT_DataArrayHandle &store,
	    const RiXlate &rixlate,
	    const GT_AttributeListHandle &point,
	    const GT_AttributeListHandle &vertex,
	    const GT_AttributeListHandle &uniform,
	    const GT_AttributeListHandle &detail,
	    UT_ValArray<uint32> *uv_idx_storage,
	    UT_Fpreal32Array *uv_data_storage)
    {
	// Fill vertex attributes first
	if (fillAttributeFromList<GeomParamSample, TRAITS>(
		    sample, cache, ctx, name, store,
		    vertex, rixlate, GT_OWNER_VERTEX,
		    uv_idx_storage, uv_data_storage))
	{
	    return true;
	}
	// Followed by point attributes
	if (fillAttributeFromList<GeomParamSample, TRAITS>(
		    sample, cache, ctx, name, store,
		    point, rixlate, GT_OWNER_POINT, 0, 0))
	{
	    return true;
	}
	// Followed by uniform
	if (fillAttributeFromList<GeomParamSample, TRAITS>(
		    sample, cache, ctx, name, store,
		    uniform, rixlate, GT_OWNER_UNIFORM, 0, 0))
	{
	    return true;
	}
	// Followed by detail attribs
	if (fillAttributeFromList<GeomParamSample, TRAITS>(
		    sample, cache, ctx, name, store,
		    detail, rixlate, GT_OWNER_DETAIL, 0, 0))
	{
	    return true;
	}
	return false;
    }

    #define TYPED_FILL(METHOD, GEOM_PARAM, TRAITS) \
	static bool METHOD(Alembic::AbcGeom::GEOM_PARAM::Sample &sample, \
	    IntrinsicCache &cache, \
	    const GABC_OOptions &ctx, \
	    const char *name, \
	    GT_DataArrayHandle &store, \
	    const RiXlate &rixlate, \
	    const GT_AttributeListHandle &point, \
	    const GT_AttributeListHandle &vertex = GT_AttributeListHandle(), \
	    const GT_AttributeListHandle &uniform = GT_AttributeListHandle(), \
	    const GT_AttributeListHandle &detail = GT_AttributeListHandle(), \
	    UT_ValArray<uint32> *uv_idx_storage = 0, \
	    UT_Fpreal32Array *uv_data_storage = 0) \
	{ \
	    return fillAttribute<Alembic::AbcGeom::GEOM_PARAM::Sample, \
		Alembic::Abc::TRAITS>(sample, cache, ctx, name, \
				      store, rixlate, \
				      point, vertex, uniform, detail, \
				      uv_idx_storage, uv_data_storage); \
	}
    TYPED_FILL(fillP3f, OP3fGeomParam, P3fTPTraits);
    TYPED_FILL(fillV2f, OV2fGeomParam, V2fTPTraits);
    TYPED_FILL(fillV3f, OV3fGeomParam, V3fTPTraits);
    TYPED_FILL(fillN3f, ON3fGeomParam, N3fTPTraits);
    TYPED_FILL(fillF32, OFloatGeomParam, Float32TPTraits);

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

    static P3fArraySamplePtr
    homogenize(OP3fGeomParam::Sample &sample, const FloatArraySample &weights)
    {
        if(sample.getVals().size() != weights.size())
        {
            UT_ASSERT(0);
            return P3fArraySamplePtr();
        }

        size_t              size = weights.size();
        const V3f           *sample_vals = sample.getVals().get();
        const fpreal32      *weight_vals = weights.get();
        V3f                 *homogenized_vals = new V3f[size];

        for(exint i = 0; i < size; ++i)
        {
            homogenized_vals[i] = sample_vals[i] * weight_vals[i];
        }

        // Use custom destructor to free homogenized_vals memory
        return P3fArraySamplePtr(new P3fArraySample(homogenized_vals, size),
                        Alembic::AbcCoreAbstract::TArrayDeleter<V3f>());
    }

    template <typename ABC_TYPE>
    static void
    fillFaceSets(const UT_StringArray &names,
	    ABC_TYPE &dest, const GT_FaceSetMapPtr &src)
    {
	for (exint i = 0; i < names.entries(); ++i)
	{
	    GT_FaceSetPtr	set;
	    
	    if (src)
		set = src->find(names(i));
	    UT_ASSERT(dest.hasFaceSet(names(i).buffer()));
	    OFaceSet			 fset = dest.getFaceSet(names(i).buffer());
	    OFaceSetSchema		&ss = fset.getSchema();
	    OFaceSetSchema::Sample	 sample;
	    GT_DataArrayHandle		 items;
	    GT_DataArrayHandle		 store;
	    if (set)
	    {
		items = set->extractMembers();
		sample.setFaces(int32Array(items, store));
	    }
	    else
	    {
		sample.setFaces(Int32ArraySample(NULL, 0));
	    }
	    ss.set(sample);
	}
    }

    template <typename ABC_TYPE>
    static void
    fillFaceSetsFromPrevious(const UT_StringArray &names,
            ABC_TYPE &dest)
    {
	for (exint i = 0; i < names.entries(); ++i)
	{
	    UT_ASSERT(dest.hasFaceSet(names(i).buffer()));

	    OFaceSet			 fset = dest.getFaceSet(names(i).buffer());
	    OFaceSetSchema		&ss = fset.getSchema();
	    OFaceSetSchema::Sample	 sample;

	    sample.setFaces(Int32ArraySample(NULL, 0));
	    ss.set(sample);
	}
    }

    template <typename T>
    bool
    isConstantArray(const T *array, const GT_DataArrayHandle &data)
    {
	auto	 tsize = data->getTupleSize();
	const T	*end = (array + tsize);
	const T *item = end;
	for (exint i = 1, n = data->entries(); i < n; ++i)
	{
	    if (!std::equal(array, end, item))
		return false;
	    item += tsize;
	}
	return true;
    }

    static bool
    isConstant(const GT_DataArrayHandle &data)
    {
	if (data->entries() == 1)
	    return true;
	GT_DataArrayHandle	buffer;	// This shouldn't actually be used
	switch (data->getStorage())
	{
	    case GT_STORE_UINT8:
		return isConstantArray(data->getU8Array(buffer), data);
	    case GT_STORE_INT32:
		return isConstantArray(data->getI32Array(buffer), data);
	    case GT_STORE_INT64:
		return isConstantArray(data->getI64Array(buffer), data);
	    case GT_STORE_REAL16:
		return isConstantArray(data->getF16Array(buffer), data);
	    case GT_STORE_REAL32:
		return isConstantArray(data->getF32Array(buffer), data);
	    case GT_STORE_REAL64:
		return isConstantArray(data->getF64Array(buffer), data);
	    case GT_STORE_STRING:
	    {
		auto &&s0 = data->getS(0);
		for (exint i = 1, n = data->entries(); i < n; ++i)
		{
		    if (strcmp(s0, data->getS(i)) != 0)
			return false;
		}
	    }
	    case GT_STORE_INVALID:
	    case GT_NUM_STORAGE_TYPES:
		UT_ASSERT(0);
		break;
	}
	return false;
    }

    template <typename T>
    const T &
    promotePrimToDetail(const T &mesh,
	    const GABC_OOptions &ctx,
	    GT_PrimitiveHandle &storage)
    {
	auto	 pattern = ctx.uniformToDetailPattern();
	auto	 force_constant = ctx.forcePrimToDetail();
	auto	&uniform = mesh.getUniformAttributes();
	if (!uniform)
	    return mesh;
	UT_StackBuffer<int>	convert(uniform->entries());
	int			nconvert = 0;
	for (int i = 0; i < uniform->entries(); ++i)
	{
	    UT_String	name(uniform->getName(i));
	    if (name.multiMatch(pattern)
		    && (force_constant || isConstant(uniform->get(i))))
	    {
		convert[nconvert] = i;
		nconvert++;
	    }
	}
	if (!nconvert)
	    return mesh;

	GT_AttributeListHandle	clist;
	GT_AttributeListHandle	ulist;
	if (mesh.getDetailAttributes())
	    clist.reset(new GT_AttributeList(*mesh.getDetailAttributes()));
	else
	    clist.reset(new GT_AttributeList(new GT_AttributeMap, 1));
	int cidx = 0;
	for (int i = 0; i < uniform->entries(); ++i)
	{
	    auto	  name = uniform->getName(i);
	    auto	&&data = uniform->get(i);
	    if (convert[cidx] == i)
	    {
		// Here, we want to convert the uniform attributes to constant
		GT_DataArrayHandle	cval(new GT_DAConstant(data, 0, 1));
		UT_ASSERT(cidx < nconvert);
		clist = clist->addAttribute(name, cval, true);
		cidx++;
	    }
	    else
	    {
		if (!ulist)
		{
		    ulist = GT_AttributeList::createAttributeList(
					name, data.get(), nullptr);
		}
		else
		{
		    ulist = ulist->addAttribute(name, data, true);
		}
	    }
	}
	// Now, we have new uniform and constant attribute lists.
	T	*newmesh = new T(mesh, mesh.getPointAttributes(),
				mesh.getVertexAttributes(),
				ulist,
				clist);
	storage.reset(newmesh);	// Keep a reference to the new mesh
	return *newmesh;
    }

    static void
    fillPolyMesh(OPolyMesh &dest,
	    const GT_PrimPolygonMesh &src_mesh,
	    IntrinsicCache &cache,
	    const GABC_OOptions &ctx)
    {
	GT_PrimitiveHandle		tmpprim;
	auto &&src = promotePrimToDetail(src_mesh, ctx, tmpprim);

	Int32ArraySample                iInd;
	Int32ArraySample                iCnt;
	OP3fGeomParam::Sample           iPos;
	OV2fGeomParam::Sample           iUVs;
	ON3fGeomParam::Sample           iNml;
	OV3fGeomParam::Sample           iVel;
	GT_DataArrayHandle              counts;
	IntrinsicCache                  storage;
	const GT_AttributeListHandle   &pt = pointAttributes(src);
	const GT_AttributeListHandle   &vtx = vertexAttributes(src);
	const GT_AttributeListHandle   &uniform = uniformAttributes(src);
	RiXlate				rixlate(src, GT_PRIM_POLYGON_MESH);

	counts = src.getFaceCountArray().extractCounts();
	if (cache.needVertex(ctx, src.getVertexList()))
	    iInd = int32Array(src.getVertexList(), storage.vertexList());

	if (cache.needCounts(ctx, counts))
	    iCnt = int32Array(counts, storage.counts());

	fillP3f(iPos, cache, ctx, "P", storage.P(), rixlate, pt);

	UT_ValArray<uint32> uv_idx_storage;
	UT_Fpreal32Array uv_data_storage;
	if (matchAttribute(ctx, "uv", pt, vtx, uniform))
	{
	    fillV2f(iUVs, cache, ctx, "uv", storage.uv(), rixlate, pt, vtx,
		    uniform, GT_AttributeListHandle(),
		    &uv_idx_storage, &uv_data_storage);
	}

	if (matchAttribute(ctx, "N", pt, vtx))
	    fillN3f(iNml, cache, ctx, "N", storage.N(), rixlate, pt, vtx);

	if (matchAttribute(ctx, "v", pt, vtx))
	    fillV3f(iVel, cache, ctx, "v", storage.v(), rixlate, pt, vtx);

	OPolyMeshSchema::Sample	sample(iPos.getVals(), iInd, iCnt, iUVs, iNml);
	if (iVel.valid())
	    sample.setVelocities(iVel.getVals());
	dest.getSchema().set(sample);
    }

    static void
    fillSubD(GABC_OGTGeometry &geo, OSubD &dest,
	    const GT_PrimSubdivisionMesh &src_mesh,
	    IntrinsicCache &cache,
	    const GABC_OOptions &ctx)
    {
	GT_PrimitiveHandle	tmpprim;
	auto &&src = promotePrimToDetail(src_mesh, ctx, tmpprim);

	Int32ArraySample                iInd;
	Int32ArraySample                iCnt;
	Int32ArraySample                iCreaseIndices;
	Int32ArraySample                iCreaseLengths;
	FloatArraySample                iCreaseSharpnesses;
	Int32ArraySample                iCornerIndices;
	FloatArraySample                iCornerSharpnesses;
	Int32ArraySample                iHoles;
	OP3fGeomParam::Sample           iPos;
	OV2fGeomParam::Sample           iUVs;
	OV3fGeomParam::Sample           iVel;
	GT_DataArrayHandle              counts;
	IntrinsicCache                  storage;
	SecondaryCache                  tstorage;   // Tag storage
	const GT_AttributeListHandle   &pt = pointAttributes(src);
	const GT_AttributeListHandle   &vtx = vertexAttributes(src);
	const GT_AttributeListHandle   &uniform = uniformAttributes(src);
	RiXlate				rixlate(src, GT_PRIM_SUBDIVISION_MESH);

	counts = src.getFaceCountArray().extractCounts();
	if (cache.needVertex(ctx, src.getVertexList()))
	    iInd = int32Array(src.getVertexList(), storage.vertexList());
	if (cache.needCounts(ctx, counts))
	    iCnt = int32Array(counts, storage.counts());
	fillP3f(iPos, cache, ctx, "P", storage.P(), rixlate, pt);
	UT_ValArray<uint32> uv_idx_storage;
	UT_Fpreal32Array uv_data_storage;
	if (matchAttribute(ctx, "uv", pt, vtx, uniform))
	{
	    fillV2f(iUVs, cache, ctx, "uv", storage.uv(), rixlate, pt, vtx,
		    uniform, GT_AttributeListHandle(),
		    &uv_idx_storage, &uv_data_storage);
	}
	if (matchAttribute(ctx, "v", pt, vtx))
	    fillV3f(iVel, cache, ctx, "v", storage.v(), rixlate, pt, vtx);

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
	tag = src.findTag("interpolateboundary");
	if (tag && tag->intCount() == 1)
	{
	    const GT_DataArrayHandle	&val = tag->intArray(0);
	    if (val && val->entries() == 1)
		sample.setInterpolateBoundary(val->getI32(0));
	}
	tag = src.findTag("facevaryinginterpolateboundary");
	if (tag && tag->intCount() == 1)
	{
	    const GT_DataArrayHandle	&val = tag->intArray(0);
	    if (val && val->entries() == 1)
		sample.setFaceVaryingInterpolateBoundary(val->getI32(0));
	}
	tag = src.findTag("facevaryingpropagatecorners");
	if (tag && tag->intCount() == 1)
	{
	    const GT_DataArrayHandle	&val = tag->intArray(0);
	    if (val && val->entries() == 1)
		sample.setFaceVaryingPropagateCorners(val->getI32(0));
	}
	if (iUVs.valid())
	    sample.setUVs(iUVs);
	if (iVel.valid())
	    sample.setVelocities(iVel.getVals());
	dest.getSchema().set(sample);
    }

    static void
    fillPoints(OPoints &dest, const GT_PrimPointMesh &src,
	    IntrinsicCache &cache, const GABC_OOptions &ctx)
    {
	OP3fGeomParam::Sample           iPos;
	UInt64ArraySample               iId;
	OFloatGeomParam::Sample         iWidths;
	OV3fGeomParam::Sample           iVel;
	GT_DataArrayHandle              ids;
	IntrinsicCache                  storage;
	const GT_AttributeListHandle   &pt = pointAttributes(src);
	const GT_AttributeListHandle   &detail = detailAttributes(src);
	RiXlate				rixlate(src, GT_PRIM_POINT_MESH);

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
	fillP3f(iPos, cache, ctx, "P", storage.P(), rixlate, pt);
	if (matchAttribute(ctx, "v", pt))
	    fillV3f(iVel, cache, ctx, "v", storage.v(), rixlate, pt);
	if (matchAttribute(ctx,
	        "width",
	        pt,
		GT_AttributeListHandle(),
		GT_AttributeListHandle(),
		detail))
	{
	    fillF32(iWidths,
	            cache,
	            ctx,
	            "width",
	            storage.width(),
		    rixlate,
	            pt,
		    GT_AttributeListHandle(),
		    GT_AttributeListHandle(),
		    detail);
	}
	if (cache.needWrite(ctx, "id", ids))
	{
	    // Topology changed
	    OPointsSchema::Sample   sample(iPos.getVals(),
                                            iId,
                                            iVel.getVals(),
                                            iWidths);
	    dest.getSchema().set(sample);
	}
	else
	{
	    // Topology unchanged
	    OPointsSchema::Sample   sample(iPos.getVals(),
                                            iVel.getVals(),
                                            iWidths);
	    dest.getSchema().set(sample);
	}
    }

    static void
    fillCurves(OCurves &dest, const GT_PrimCurveMesh &src,
	    IntrinsicCache &cache, const GABC_OOptions &ctx)
    {
	OP3fGeomParam::Sample			 iPos;
	FloatArraySample			 iPosWeight;
	Int32ArraySample			 iCnt;
	OFloatGeomParam::Sample			 iWidths;
	OV2fGeomParam::Sample			 iUVs;
	ON3fGeomParam::Sample			 iNml;
	OV3fGeomParam::Sample			 iVel;
	FloatArraySample			 iKnots;
	UInt8ArraySample			 iOrders;
	GT_DataArrayHandle			 counts;
	GT_DataArrayHandle			 orders, order_storage;
	GT_DataArrayHandle			 knots = src.knots();
	IntrinsicCache				 storage;
	Alembic::AbcGeom::CurveType		 iOrder;
	Alembic::AbcGeom::CurvePeriodicity	 iPeriod;
	Alembic::AbcGeom::BasisType		 iBasis;
	P3fArraySamplePtr			 homogenized_vals;
	const GT_AttributeListHandle		&vtx = vertexAttributes(src);
	const GT_AttributeListHandle		&uniform=uniformAttributes(src);
	const GT_AttributeListHandle		&detail=detailAttributes(src);
	const GT_DataArrayHandle		&Pw = vtx->get("Pw");
	RiXlate					 rixlate(src, GT_PRIM_CURVE_MESH);

	switch (src.getBasis())
	{
	    case GT_BASIS_BEZIER:
		iBasis = Alembic::AbcGeom::kBezierBasis;
		break;
	    case GT_BASIS_BSPLINE:
		iBasis = Alembic::AbcGeom::kBsplineBasis;
		break;
	    case GT_BASIS_CATMULLROM:
		iBasis = Alembic::AbcGeom::kCatmullromBasis;
		break;
	    case GT_BASIS_HERMITE:
		iBasis = Alembic::AbcGeom::kHermiteBasis;
		break;
	    case GT_BASIS_POWER:
		iBasis = Alembic::AbcGeom::kPowerBasis;
		break;
	    case GT_BASIS_LINEAR:
	    default:
		iBasis = Alembic::AbcGeom::kNoBasis;
		break;
	}
	switch (src.uniformOrder())
	{
	    case 2:
		iOrder = Alembic::AbcGeom::kLinear;
		break;
	    case 4:
		iOrder = Alembic::AbcGeom::kCubic;
		break;
	    default:
		iOrder = Alembic::AbcGeom::kVariableOrder;
		if (src.isUniformOrder())
		{
		    // Here, we have a uniform order, but it's not something
		    // that's supported implicitly by Alembic.  So, we just
		    // create a constant array to store the order.
		    // For example, if all curves are order 7
		    orders = new GT_IntConstant(src.getCurveCount(),
					src.uniformOrder());
		}
		else
		{
		    orders = src.varyingOrders();
		}
		// TODO: We should likely cache these
		iOrders = uint8Array(orders, order_storage);
		break;
	}
	iPeriod = src.getWrap()
			? Alembic::AbcGeom::kPeriodic
			: Alembic::AbcGeom::kNonPeriodic;

	counts = src.getCurveCountArray().extractCounts();
	if (cache.needCounts(ctx, counts))
	    iCnt = int32Array(counts, storage.counts());

	// We pass in "vtx" for the point attributes since we can't
	// differentiate between them at this point.
	fillP3f(iPos, cache, ctx, "P", storage.P(), rixlate, vtx);

	UT_ValArray<uint32> uv_idx_storage;
	UT_Fpreal32Array uv_data_storage;
	if (matchAttribute(ctx,
                "uv",
                GT_AttributeListHandle(),
                vtx,
                uniform))
        {
            fillV2f(iUVs,
                    cache,
                    ctx,
                    "uv",
                    storage.uv(),
		    rixlate, 
                    GT_AttributeListHandle(),
                    vtx,
                    uniform,
		    GT_AttributeListHandle(),
		    &uv_idx_storage,
		    &uv_data_storage);
        }

	if (matchAttribute(ctx, "N", vtx))
        {
	    fillN3f(iNml,
	            cache,
	            ctx,
	            "N",
	            storage.N(),
		    rixlate,
	            GT_AttributeListHandle(),
	            vtx);
        }

	if (matchAttribute(ctx, "v", vtx))
	{
	    fillV3f(iVel,
	            cache,
	            ctx,
	            "v",
	            storage.v(),
		    rixlate,
	            GT_AttributeListHandle(),
	            vtx);
        }

	if (matchAttribute(ctx,
	        "width",
	        GT_AttributeListHandle(),
	        vtx,
	        uniform,
	        detail))
	{
	    fillF32(iWidths,
	            cache,
	            ctx,
	            "width",
	            storage.width(),
		    rixlate,
	            GT_AttributeListHandle(),
		    vtx,
		    uniform,
		    detail);
	}

	if (Pw && cache.needWrite(ctx, "Pw", Pw))
	{
	    iPosWeight = floatArray(Pw, storage.Pw());
            homogenized_vals = homogenize(iPos, iPosWeight);

            if(homogenized_vals)
                iPos.setVals(*homogenized_vals);
        }

	if (knots)
	    iKnots = floatArray(knots, storage.uknots());

	OCurvesSchema::Sample	sample(iPos.getVals(), iCnt,
		iOrder, iPeriod, iWidths, iUVs, iNml, iBasis,
		iPosWeight, iOrders, iKnots);
	if (iVel.valid())
	    sample.setVelocities(iVel.getVals());
	dest.getSchema().set(sample);
    }

    static void
    fillNuPatch(GABC_OGTGeometry &geo,
	    ONuPatch &dest, const GT_PrimNuPatch &src,
	    IntrinsicCache &cache, const GABC_OOptions &ctx)
    {
	FloatArraySample                iUKnot;
	FloatArraySample                iVKnot;
	FloatArraySample                iPosWeight;
	OP3fGeomParam::Sample           iPos;
	ON3fGeomParam::Sample           iNml;
	OV2fGeomParam::Sample           iUVs;
	OV3fGeomParam::Sample           iVel;
	IntrinsicCache                  storage;
	P3fArraySamplePtr               homogenized_vals;
	const GT_AttributeListHandle   &vtx = vertexAttributes(src);
	const GT_AttributeListHandle   &detail = detailAttributes(src);
	const GT_DataArrayHandle       &Pw = vtx->get("Pw");
	RiXlate				rixlate(src, GT_PRIM_NUPATCH);

	// We pass in "vtx" for the point attributes since we can't
	// differentiate between them at this point.
	//
	// Also pass in "detail" for primitive attributes.
	fillP3f(iPos, cache, ctx, "P", storage.P(), rixlate, vtx);

	UT_ValArray<uint32> uv_idx_storage;
	UT_Fpreal32Array uv_data_storage;
	if (matchAttribute(ctx,
	        "uv",
	        GT_AttributeListHandle(),
	        vtx,
	        detail))
	{
	    fillV2f(iUVs,
	            cache,
	            ctx,
	            "uv",
	            storage.uv(),
		    rixlate,
	            GT_AttributeListHandle(),
	            vtx,
	            detail,
		    GT_AttributeListHandle(),
		    &uv_idx_storage,
		    &uv_data_storage);
        }

	if (matchAttribute(ctx, "N", vtx, vtx))
	    fillN3f(iNml, cache, ctx, "N", storage.N(), rixlate, vtx, vtx);

	if (matchAttribute(ctx, "v", vtx, vtx))
	    fillV3f(iVel, cache, ctx, "v", storage.v(), rixlate, vtx, vtx);

	if (cache.needWrite(ctx, "uknots", src.getUKnots()))
	    iUKnot = floatArray(src.getUKnots(), storage.uknots());

	if (cache.needWrite(ctx, "vknots", src.getVKnots()))
	    iVKnot = floatArray(src.getVKnots(), storage.vknots());

	if (Pw && cache.needWrite(ctx, "Pw", Pw))
	{
	    iPosWeight = floatArray(Pw, storage.Pw());
            homogenized_vals = homogenize(iPos, iPosWeight);

            if(homogenized_vals)
                iPos.setVals(*homogenized_vals);
	}

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
    cacheNeedWrite(const GABC_OOptions &ctx,
            const GT_DataArrayHandle &data,
	    GT_DataArrayHandle &cache)
    {
	if (!data || !cache || !cache->isEqual(*data))
	{
	    cache = data->harden();
	    return true;
	}
	return false;
    }
}

//-----------------------------------------------
//  IntrinsicCache
//-----------------------------------------------

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

//-----------------------------------------------
//  SecondaryCache
//-----------------------------------------------

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

//-----------------------------------------------
//  IgnoreList
//-----------------------------------------------

GABC_OGTGeometry::IgnoreList::IgnoreList(const char *arg0, ...)
     : UT_StringSet()
{
    if (arg0)
    {
        va_list	 args;
        va_start(args, arg0);
        for (const char *s = arg0; s; s = va_arg(args, const char *))
        {
            insert(UTmakeUnsafeRef(s));
        }
        va_end(args);
    }
}

//-----------------------------------------------
//  GABC_OGTGeometry
//-----------------------------------------------

GABC_OGTGeometry::IgnoreList &
GABC_OGTGeometry::getDefaultSkip()
{
    // Construct on first use to avoid the static initialization order fiasco
    static IgnoreList *def = NULL;
    
    if (!def)
    {
	def = new IgnoreList("P",
            "v",
            "__topology",
            "__primitive_id",
            "__point_id",
            "__vertex_id",
            "varmap",
            (void *)NULL);
	// Add these after the initial construction.
	def->addSkip(GABC_Util::theUserPropsValsAttrib);
	def->addSkip(GABC_Util::theUserPropsMetaAttrib);
    }

    return *def;
}

GABC_OGTGeometry::GABC_OGTGeometry(const std::string &name)
    : myName(name)
    , myType(GT_PRIM_UNDEFINED)
    , mySecondaryCache(NULL)
    , myUserPropState(UNSET)
    , myElapsedFrames(0)
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
    clearArbProperties();
    clearUserProperties();
}

void
GABC_OGTGeometry::clearArbProperties()
{
    for (int i = 0; i < MAX_PROPERTIES; ++i)
    {
	for (PropertyMap::const_iterator it = myArbProperties[i].begin();
	        it != myArbProperties[i].end();
	        ++it)
	{
	    delete it->second;
	}
	myArbProperties[i].clear();
    }
}

void
GABC_OGTGeometry::clearUserProperties()
{
    for (PropertyMap::const_iterator it = myUserProperties.begin();
            it != myUserProperties.end();
            ++it)
    {
        delete it->second;
    }
    myUserProperties.clear();
    myUserPropState = UNSET;
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
			OFaceSet &fset = ss.createFaceSet(name);
			OFaceSetSchema	&fss = fset.getSchema();
			fss.setTimeSampling(ctx.timeSampling());
		    }
		    break;
		case GT_PRIM_SUBDIVISION_MESH:
		    {
			OSubDSchema	&ss = myShape.mySubD->getSchema();
			OFaceSet &fset = ss.createFaceSet(name);
			OFaceSetSchema	&fss = fset.getSchema();
			fss.setTimeSampling(ctx.timeSampling());
		    }
		    break;
	    }
	}
    }
}

bool
GABC_OGTGeometry::makeArbProperties(const GT_PrimitiveHandle &prim,
        GABC_OError &err,
        const GABC_OOptions &ctx)
{
    clearArbProperties();

    if (!ctx.saveAttributes())
    {
        return true;
    }

    const IgnoreList	*skip = &theEmptySkip;
    OCompoundProperty	 cp;
    bool		 result = true;
    RiXlate		 rixlate(*prim, myType);

    switch (myType)
    {
	case GT_PRIM_POLYGON_MESH:
	    skip = &thePolyMeshSkip;
	    cp = myShape.myPolyMesh->getSchema().getArbGeomParams();
	    break;

	case GT_PRIM_SUBDIVISION_MESH:
	    skip = &theSubDSkip;
	    cp = myShape.mySubD->getSchema().getArbGeomParams();
	    break;

	case GT_PRIM_POINT_MESH:
	    skip = &thePointsSkip;
	    cp = myShape.myPoints->getSchema().getArbGeomParams();
	    break;

	case GT_PRIM_CURVE_MESH:
	    skip = &theCurvesSkip;
	    cp = myShape.myCurves->getSchema().getArbGeomParams();
	    break;

	case GT_PRIM_NUPATCH:
	    skip = &theNuPatchSkip;
	    cp = myShape.myNuPatch->getSchema().getArbGeomParams();
	    break;

	default:
	    UT_ASSERT(0 && "Type error");
	    return false;
    }

    result = result && makeGeomParams(myArbProperties[POINT_PROPERTIES],
                prim->getPointAttributes(),
                cp,
                GT_OWNER_POINT,
		rixlate,
                err,
                ctx,
                *skip);
    result = result && makeGeomParams(myArbProperties[VERTEX_PROPERTIES],
                prim->getVertexAttributes(),
                cp,
                GT_OWNER_VERTEX,
		rixlate,
                err,
                ctx,
                *skip);
    result = result && makeGeomParams(myArbProperties[UNIFORM_PROPERTIES],
		prim->getUniformAttributes(),
		cp,
		GT_OWNER_UNIFORM,
		rixlate,
		err,
		ctx,
		*skip);
    result = result && makeGeomParams(myArbProperties[DETAIL_PROPERTIES],
		prim->getDetailAttributes(),
		cp,
		GT_OWNER_DETAIL,
		rixlate,
		err,
		ctx,
		*skip);

    return result;
}

bool
GABC_OGTGeometry::writeArbProperties(const GT_PrimitiveHandle &prim,
        GABC_OError &err,
        const GABC_OOptions &ctx)
{
    bool    result = true;

    result &= writeGeomProperties(myArbProperties[VERTEX_PROPERTIES],
	    vertexAttributes(*prim),
	    err,
	    ctx);
    result &= writeGeomProperties(myArbProperties[POINT_PROPERTIES],
	    pointAttributes(*prim),
	    err,
	    ctx);
    result &= writeGeomProperties(myArbProperties[UNIFORM_PROPERTIES],
	    uniformAttributes(*prim),
	    err,
	    ctx);
    result &= writeGeomProperties(myArbProperties[DETAIL_PROPERTIES],
	    detailAttributes(*prim),
	    err,
	    ctx);

    return result;
}

void
GABC_OGTGeometry::writeArbPropertiesFromPrevious()
{
    writePropertiesFromPrevious(myArbProperties[VERTEX_PROPERTIES]);
    writePropertiesFromPrevious(myArbProperties[POINT_PROPERTIES]);
    writePropertiesFromPrevious(myArbProperties[UNIFORM_PROPERTIES]);
    writePropertiesFromPrevious(myArbProperties[DETAIL_PROPERTIES]);
}

bool
GABC_OGTGeometry::makeUserProperties(const GT_PrimitiveHandle &prim,
        OCompoundProperty *parent,
        GABC_OError &err,
        const GABC_OOptions &ctx)
{
    GT_AttributeListHandle  attribs_handle;
    GT_DataArrayHandle      meta_attrib;
    GT_DataArrayHandle      vals_attrib;
    const char             *meta_buffer;
    const char             *vals_buffer;

    clearUserProperties();

    attribs_handle = prim->getUniformAttributes();
    if (!attribs_handle)
    {
        // Primitive level attributes are sometimes merged with detail level
        // attributes.
        attribs_handle = prim->getDetailAttributes();
    }
    if(!attribs_handle)
    {
        myUserPropState = NO_USER_PROPERTIES;
        return true;
    }

    meta_attrib = attribs_handle->get(GABC_Util::theUserPropsMetaAttrib, 0);
    vals_attrib = attribs_handle->get(GABC_Util::theUserPropsValsAttrib, 0);

    if (!meta_attrib && !vals_attrib)
    {
        myUserPropState = NO_USER_PROPERTIES;
    }
    else if (!vals_attrib)
    {
        meta_buffer = meta_attrib->getS(0,0);

        if (!meta_buffer || !(*meta_buffer))
        {
            myUserPropState = NO_USER_PROPERTIES;
        }
        else
        {
            myUserPropState = ERROR_READING_PROPERTIES;
            err.warning("Found user properties metadata attribute, but not "
                    "data attribute. Ignoring user properties.");
        }
    }
    else if (!meta_attrib)
    {
        vals_buffer = vals_attrib->getS(0,0);

        if (!vals_buffer || !(*vals_buffer))
        {
            myUserPropState = NO_USER_PROPERTIES;
        }
        else
        {
            myUserPropState = ERROR_READING_PROPERTIES;
            err.warning("Found user properties data attribute, but not "
                    "metadata attribute. Ignoring user properties.");
        }
    }
    else
    {
        meta_buffer = meta_attrib->getS(0,0);
        vals_buffer = vals_attrib->getS(0,0);

        if ((!meta_buffer || !(*meta_buffer))
                && (!vals_buffer || !(*vals_buffer)))
        {
            myUserPropState = NO_USER_PROPERTIES;
        }
        else if (!meta_buffer || !(*meta_buffer))
        {
            myUserPropState = ERROR_READING_PROPERTIES;
            err.warning("User properties metadata attribute empty. Ignoring "
                    "user properties.");
        }
        else if (!vals_buffer || !(*vals_buffer))
        {
            myUserPropState = ERROR_READING_PROPERTIES;
            err.warning("User properties data attribute empty. Ignoring "
                    "user properties.");
        }
        else
        {
            UT_AutoJSONParser   meta_data(meta_buffer, strlen(meta_buffer));
            UT_AutoJSONParser   vals_data(vals_buffer, strlen(vals_buffer));
            UT_WorkBuffer       err_msg;

            meta_data->setBinary(false);
            vals_data->setBinary(false);
            if (!GABC_Util::readUserPropertyDictionary(meta_data,
                    vals_data,
                    myUserProperties,
                    parent,
                    err,
                    ctx))
            {
                err.warning("Ignoring user properties (%s).",
                        myName.c_str());
                myUserPropState = ERROR_READING_PROPERTIES;
            }
            else
            {
                myUserPropState = WRITE_USER_PROPERTIES;
            }
        }
    }

    return true;
}

bool
GABC_OGTGeometry::writeUserProperties(const GT_PrimitiveHandle &prim,
        GABC_OError &err,
        const GABC_OOptions &ctx)
{
    if (myUserPropState == UNSET)
    {
        UT_ASSERT(0 && "writeUserProperties called before makeUserProperties");
        return false;
    }

    if (myUserPropState != ERROR_READING_PROPERTIES)
    {
        GT_AttributeListHandle  attribs_handle;
        GT_DataArrayHandle      meta_attrib;
        GT_DataArrayHandle      vals_attrib;

        attribs_handle = prim->getUniformAttributes();
        if (!attribs_handle)
        {
            // Primitive level attributes are sometimes merged with detail level
            // attributes.
            attribs_handle = prim->getDetailAttributes();
        }
        if (!attribs_handle)
        {
            if (myUserPropState == WRITE_USER_PROPERTIES)
            {
                err.warning("Missing user property information for %s on frame "
                        "%" SYS_PRId64 ". Reusing previous samples.",
                        myName.c_str(),
                        (ctx.firstFrame() + myElapsedFrames + 1));
                writePropertiesFromPrevious(myUserProperties);
            }

            return true;
        }

        meta_attrib = attribs_handle->get(GABC_Util::theUserPropsMetaAttrib, 0);
        vals_attrib = attribs_handle->get(GABC_Util::theUserPropsValsAttrib, 0);

        if (!meta_attrib && !vals_attrib)
        {
            if (myUserPropState == WRITE_USER_PROPERTIES)
            {
                err.warning("Missing user property information for %s on frame "
                        "%" SYS_PRId64 ". Reusing previous samples.",
                        myName.c_str(),
                        (ctx.firstFrame() + myElapsedFrames + 1));
                writePropertiesFromPrevious(myUserProperties);
            }
        }
        else if (!meta_attrib || !vals_attrib)
        {
            if (myUserPropState == WRITE_USER_PROPERTIES)
            {
                err.warning("Missing user property information for %s on frame "
                        "%" SYS_PRId64 ". Reusing previous samples.",
                        myName.c_str(),
                        (ctx.firstFrame() + myElapsedFrames + 1));
                writePropertiesFromPrevious(myUserProperties);
            }
            else
            {
                UT_ASSERT(myUserPropState == NO_USER_PROPERTIES);

                if (myElapsedFrames)
                {
                    err.warning("Partial user property information detected for %s "
                            "on frame %" SYS_PRId64 ", but none detected "
                            "for previous frames. Ignoring it.",
                            myName.c_str(),
                            (ctx.firstFrame() + myElapsedFrames + 1));
                    myUserPropState = ERROR_READING_PROPERTIES;
                }
            }
        }
        else
        {
            const char *meta_buffer = meta_attrib->getS(0,0);
            const char *vals_buffer = vals_attrib->getS(0,0);

            if (myUserPropState == WRITE_USER_PROPERTIES)
            {
                if (!meta_buffer
                        || !(*meta_buffer)
                        || !vals_buffer
                        || !(*vals_buffer))
                {
                    err.warning("Missing user property information for %s on "
                            "frame %" SYS_PRId64 ". Reusing previous samples.",
                            myName.c_str(),
                            (ctx.firstFrame() + myElapsedFrames + 1));
                    writePropertiesFromPrevious(myUserProperties);
                }
                else
                {
                    UT_AutoJSONParser   meta_data(meta_buffer, strlen(meta_buffer));
                    UT_AutoJSONParser   vals_data(vals_buffer, strlen(vals_buffer));
                    UT_WorkBuffer       err_msg;

                    meta_data->setBinary(false);
                    vals_data->setBinary(false);
                    if (!GABC_Util::readUserPropertyDictionary(meta_data,
                            vals_data,
                            myUserProperties,
                            NULL,
                            err,
                            ctx))
                    {
                        err.warning("Reusing previous user property samples "
                                "(%s, frame %" SYS_PRId64 ")",
                                myName.c_str(),
                                (ctx.firstFrame() + myElapsedFrames + 1));
                        writePropertiesFromPrevious(myUserProperties);
                    }
                }
            }
            else
            {
                UT_ASSERT(myUserPropState == NO_USER_PROPERTIES);

                if ((meta_buffer && *meta_buffer) || (vals_buffer && *vals_buffer))
                {
                    err.warning("User property information detected for %s on "
                            "frame %" SYS_PRId64 ", but none detected "
                            "for previous frames. Ignoring it.",
                            myName.c_str(),
                            (ctx.firstFrame() + myElapsedFrames + 1));
                    myUserPropState = ERROR_READING_PROPERTIES;
                }
            }
        }
    }

    return true;
}

void
GABC_OGTGeometry::writeUserPropertiesFromPrevious()
{
    writePropertiesFromPrevious(myUserProperties);
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
	    return true;
	default:
	    break;
    }
    return false;
}

bool
GABC_OGTGeometry::start(const GT_PrimitiveHandle &src,
	const OObject &parent,
	const GABC_OOptions &ctx,
	GABC_OError &err,
	ObjectVisibility vis)
{
    GT_PrimitiveHandle	prim;
    OCompoundProperty   user_props;

    UT_ASSERT(src);
    myCache.clear();
    myType = GT_PRIM_UNDEFINED;
    prim = getPrimitive(src, myType);
    myElapsedFrames = 0;

    if (!prim)
    {
	UT_ASSERT(myType == GT_PRIM_UNDEFINED);
	return false;
    }

    switch (myType)
    {
	// Direct mapping to Alembic primitives
	case GT_PRIM_POLYGON_MESH:
	    // Promoting uniform to detail may change the prim.
	    promotePrimToDetail(
		    *UTverify_cast<const GT_PrimPolygonMesh *>(prim.get()),
		    ctx, prim);
            myShape.myPolyMesh = new OPolyMesh(parent,
                    myName,
                    ctx.timeSampling());
            user_props = myShape.myPolyMesh->getSchema().getUserProperties();
	    myVisibility = Alembic::AbcGeom::CreateVisibilityProperty(
                    *(myShape.myPolyMesh),
                    ctx.timeSampling());
            makeFaceSets(prim, ctx);
            break;

	case GT_PRIM_SUBDIVISION_MESH:
	    // Promoting uniform to detail may change the prim.
	    promotePrimToDetail(
		    *UTverify_cast<const GT_PrimSubdivisionMesh *>(prim.get()),
		    ctx, prim);
            myShape.mySubD = new OSubD(parent,
                    myName,
                    ctx.timeSampling());
            user_props = myShape.mySubD->getSchema().getUserProperties();
	    myVisibility = Alembic::AbcGeom::CreateVisibilityProperty(
                    *(myShape.mySubD),
                    ctx.timeSampling());
            makeFaceSets(prim, ctx);
            break;

	case GT_PRIM_POINT_MESH:
            myShape.myPoints = new OPoints(parent,
                    myName,
                    ctx.timeSampling());
            user_props = myShape.myPoints->getSchema().getUserProperties();
	    myVisibility = Alembic::AbcGeom::CreateVisibilityProperty(
                    *(myShape.myPoints),
                    ctx.timeSampling());
            break;

	case GT_PRIM_CURVE_MESH:
            myShape.myCurves = new OCurves(parent,
                    myName,
                    ctx.timeSampling());
            user_props = myShape.myCurves->getSchema().getUserProperties();
	    myVisibility = Alembic::AbcGeom::CreateVisibilityProperty(
                    *(myShape.myCurves),
                    ctx.timeSampling());
            break;

	case GT_PRIM_NUPATCH:
            myShape.myNuPatch = new ONuPatch(parent,
                    myName,
                    ctx.timeSampling());
            user_props = myShape.myNuPatch->getSchema().getUserProperties();
	    myVisibility = Alembic::AbcGeom::CreateVisibilityProperty(
                    *(myShape.myNuPatch),
                    ctx.timeSampling());
            break;

	default:
	    UT_ASSERT(0 && "Unhandled primitive");
            return false;
    }

    if (!makeArbProperties(prim, err, ctx))
    {
        err.error("Error saving attributes: ");
        return false;
    }
    if (!makeUserProperties(src, &user_props, err, ctx))
    {
        err.error("Error saving user properties: ");
        return false;
    }

    return update(prim, ctx, err, vis);
}

bool
GABC_OGTGeometry::update(const GT_PrimitiveHandle &src,
	const GABC_OOptions &ctx,
	GABC_OError &err,
	ObjectVisibility vis)
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

    myVisibility.set(vis);

    switch (myType)
    {
	case GT_PRIM_POLYGON_MESH:
	    fillPolyMesh(*myShape.myPolyMesh,
			*(GT_PrimPolygonMesh *)(prim.get()),
			myCache, ctx);
	    fillFaceSets(myFaceSetNames,
			myShape.myPolyMesh->getSchema(),
			((GT_PrimPolygonMesh *)(prim.get()))->faceSetMap());
	    break;

	case GT_PRIM_SUBDIVISION_MESH:
	    fillSubD(*this, *myShape.mySubD,
			*(GT_PrimSubdivisionMesh *)(prim.get()),
			myCache, ctx);
	    fillFaceSets(myFaceSetNames,
			myShape.mySubD->getSchema(),
			((GT_PrimSubdivisionMesh *)(prim.get()))->faceSetMap());
	    break;

	case GT_PRIM_POINT_MESH:
	    fillPoints(*myShape.myPoints,
			*(GT_PrimPointMesh *)(prim.get()),
			myCache, ctx);
	    break;

	case GT_PRIM_CURVE_MESH:
	    fillCurves(*myShape.myCurves,
			*(GT_PrimCurveMesh *)(prim.get()),
			myCache, ctx);
	    break;

	case GT_PRIM_NUPATCH:
	    fillNuPatch(*this, *myShape.myNuPatch,
			*(GT_PrimNuPatch *)(prim.get()),
			myCache, ctx);
	    break;

	default:
	    UT_ASSERT(0 && "Unhandled primitive");
	    return false;
    }

    if (!writeArbProperties(prim, err, ctx))
    {
        err.error("Error saving attributes: ");
        return false;
    }
    if (!writeUserProperties(prim, err, ctx))
    {
        err.error("Error saving user properties: ");
        return false;
    }

    ++myElapsedFrames;
    return true;
}

bool
GABC_OGTGeometry::updateFromPrevious(GABC_OError &err,
        ObjectVisibility vis,
        exint frames)
{
    if (myType == GT_PRIM_UNDEFINED)
    {
	err.error("Need to save first frame!");
	return false;
    }

    if (frames < 0)
    {
        UT_ASSERT(0 && "Attempted to update less than 0 frames.");
        return false;
    }

    switch (myType)
    {
	case GT_PRIM_POLYGON_MESH:
	    for (exint i = 0; i < frames; ++i) {
                myVisibility.set(vis);
                myShape.myPolyMesh->getSchema().setFromPrevious();
                writeArbPropertiesFromPrevious();
                writeUserPropertiesFromPrevious();
                fillFaceSetsFromPrevious(myFaceSetNames,
                        myShape.myPolyMesh->getSchema());
            }
	    break;

	case GT_PRIM_SUBDIVISION_MESH:
	    for (exint i = 0; i < frames; ++i) {
                myVisibility.set(vis);
                myShape.mySubD->getSchema().setFromPrevious();
                writeArbPropertiesFromPrevious();
                writeUserPropertiesFromPrevious();
                fillFaceSetsFromPrevious(myFaceSetNames,
                        myShape.mySubD->getSchema());
            }
	    break;

	case GT_PRIM_POINT_MESH:
	    for (exint i = 0; i < frames; ++i) {
                myVisibility.set(vis);
	        myShape.myPoints->getSchema().setFromPrevious();
                writeArbPropertiesFromPrevious();
                writeUserPropertiesFromPrevious();
            }
	    break;

	case GT_PRIM_CURVE_MESH:
	    for (exint i = 0; i < frames; ++i) {
                myVisibility.set(vis);
	        myShape.myCurves->getSchema().setFromPrevious();
                writeArbPropertiesFromPrevious();
                writeUserPropertiesFromPrevious();
            }
	    break;

	case GT_PRIM_NUPATCH:
	    for (exint i = 0; i < frames; ++i) {
                myVisibility.set(vis);
	        myShape.myNuPatch->getSchema().setFromPrevious();
                writeArbPropertiesFromPrevious();
                writeUserPropertiesFromPrevious();
            }
	    break;

	default:
	    UT_ASSERT(0 && "Unhandled primitive");
	    return false;
    }

    myElapsedFrames += frames;
    return true;
}

Alembic::Abc::OObject
GABC_OGTGeometry::getOObject() const
{
    switch (myType)
    {
	case GT_PRIM_POLYGON_MESH:
	    return *myShape.myPolyMesh;
	case GT_PRIM_SUBDIVISION_MESH:
	    return *myShape.mySubD;
	case GT_PRIM_POINT_MESH:
	    return *myShape.myPoints;
	case GT_PRIM_CURVE_MESH:
	    return *myShape.myCurves;
	case GT_PRIM_NUPATCH:
	    return *myShape.myNuPatch;
	default:
	    UT_ASSERT(0);
	    break;
    }
    return OObject();
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
GABC_OGTGeometry::dump(int indent) const
{
    printf("%*sGeometry[%s] = %s\n", indent, "", myName.c_str(),
	    GTprimitiveType(myType));
}
