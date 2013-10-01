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

#include "GABC_IObject.h"
#include "GABC_IArray.h"
#include "GABC_IGTArray.h"
#include "GABC_IArchive.h"
#include "GABC_Util.h"
#include "GABC_GTUtil.h"
#include "GABC_ChannelCache.h"
#include <SYS/SYS_AtomicInt.h>
#include <GEO/GEO_PackedNameMap.h>
#include <GU/GU_Detail.h>
#include <GT/GT_DANumeric.h>
#include <GT/GT_DAIndirect.h>
#include <GT/GT_DABool.h>
#include <GT/GT_CurveEval.h>
#include <GT/GT_PrimitiveBuilder.h>
#include <GT/GT_DAConstantValue.h>
#include <GT/GT_Util.h>
#include <GT/GT_PrimPointMesh.h>
#include <GT/GT_PrimCurveMesh.h>
#include <GT/GT_PrimPolygonMesh.h>
#include <GT/GT_PrimSubdivisionMesh.h>
#include <GT/GT_PrimNuPatch.h>
#include <GT/GT_TrimNuCurves.h>
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcMaterial/All.h>
#include <Alembic/AbcCoreHDF5/All.h>
#include <UT/UT_StackBuffer.h>
#include <UT/UT_DoubleLock.h>
#include <UT/UT_ErrorLog.h>

using namespace GABC_NAMESPACE;

namespace
{
    typedef Alembic::Abc::index_t		index_t;
    typedef Alembic::Abc::chrono_t		chrono_t;
    typedef Alembic::Abc::DataType		DataType;
    typedef Alembic::Abc::M44d			M44d;
    typedef Alembic::Abc::V3d			V3d;
    typedef Alembic::Abc::Box3d			Box3d;
    typedef Alembic::Abc::Quatd			Quatd;
    typedef Alembic::Abc::IArchive		IArchive;
    typedef Alembic::Abc::IObject		IObject;
    typedef Alembic::Abc::ICompoundProperty	ICompoundProperty;
    typedef Alembic::Abc::ISampleSelector	ISampleSelector;
    typedef Alembic::Abc::ObjectHeader		ObjectHeader;
    typedef Alembic::Abc::TimeSamplingPtr	TimeSamplingPtr;
    typedef Alembic::Abc::PropertyHeader	PropertyHeader;
    typedef Alembic::Abc::IP3fArrayProperty	IP3fArrayProperty;
    typedef Alembic::Abc::IV3fArrayProperty	IV3fArrayProperty;
    typedef Alembic::Abc::IUInt64ArrayProperty	IUInt64ArrayProperty;
    typedef Alembic::Abc::IFloatArrayProperty	IFloatArrayProperty;
    typedef Alembic::Abc::IBox3dProperty	IBox3dProperty;
    typedef Alembic::Abc::IArrayProperty	IArrayProperty;
    typedef Alembic::Abc::IScalarProperty	IScalarProperty;
    typedef Alembic::Abc::ArraySamplePtr	ArraySamplePtr;
    typedef Alembic::Abc::Int32ArraySamplePtr	Int32ArraySamplePtr;
    typedef Alembic::Abc::FloatArraySamplePtr	FloatArraySamplePtr;
    typedef Alembic::Abc::WrapExistingFlag	WrapExistingFlag;
    typedef Alembic::AbcGeom::IXform		IXform;
    typedef Alembic::AbcGeom::IXformSchema	IXformSchema;
    typedef Alembic::AbcGeom::IPolyMesh		IPolyMesh;
    typedef Alembic::AbcGeom::ISubD		ISubD;
    typedef Alembic::AbcGeom::ICurves		ICurves;
    typedef Alembic::AbcGeom::IPoints		IPoints;
    typedef Alembic::AbcGeom::INuPatch		INuPatch;
    typedef Alembic::AbcGeom::ILight		ILight;
    typedef Alembic::AbcGeom::IFaceSet		IFaceSet;
    typedef Alembic::AbcGeom::IPolyMeshSchema	IPolyMeshSchema;
    typedef Alembic::AbcGeom::ISubDSchema	ISubDSchema;
    typedef Alembic::AbcGeom::ICurvesSchema	ICurvesSchema;
    typedef Alembic::AbcGeom::IPointsSchema	IPointsSchema;
    typedef Alembic::AbcGeom::INuPatchSchema	INuPatchSchema;
    typedef Alembic::AbcGeom::ILightSchema	ILightSchema;
    typedef Alembic::AbcGeom::IFaceSetSchema	IFaceSetSchema;
    typedef Alembic::AbcGeom::ICamera		ICamera;
    typedef Alembic::AbcGeom::ICameraSchema	ICameraSchema;
    typedef Alembic::AbcGeom::XformSample	XformSample;
    typedef Alembic::AbcGeom::GeometryScope	GeometryScope;
    typedef Alembic::AbcGeom::IN3fGeomParam	IN3fGeomParam;
    typedef Alembic::AbcGeom::IV2fGeomParam	IV2fGeomParam;
    typedef Alembic::AbcGeom::IFloatGeomParam	IFloatGeomParam;
    typedef Alembic::AbcGeom::IVisibilityProperty IVisibilityProperty;
    typedef Alembic::AbcMaterial::IMaterial	IMaterial;

    const WrapExistingFlag gabcWrapExisting = Alembic::Abc::kWrapExisting;
    const GeometryScope	gabcVaryingScope = Alembic::AbcGeom::kVaryingScope;
    const GeometryScope	gabcVertexScope = Alembic::AbcGeom::kVertexScope;
    const GeometryScope	gabcFacevaryingScope = Alembic::AbcGeom::kFacevaryingScope;
    const GeometryScope	gabcUniformScope = Alembic::AbcGeom::kUniformScope;
    const GeometryScope	gabcConstantScope = Alembic::AbcGeom::kConstantScope;
    const GeometryScope	gabcUnknownScope = Alembic::AbcGeom::kUnknownScope;
    GeometryScope	theConstantUnknownScope[2] =
					{ gabcConstantScope, gabcUnknownScope };

    static const fpreal	theDefaultWidth = 0.05;


    static GT_DataArrayHandle
    arrayFromSample(GABC_IArchive &arch, const ArraySamplePtr &param,
		int array_extent,
		GT_Type gttype)
    {
	if (!param)
	    return GT_DataArrayHandle();
	return GABC_NAMESPACE::GABCarray(GABC_IArray::getSample(arch, param,
			    gttype, array_extent));
    }
    static GT_DataArrayHandle
    simpleArrayFromSample(GABC_IArchive &arch, const ArraySamplePtr &param)
    {
	return arrayFromSample(arch, param, 1, GT_TYPE_NONE);
    }

    static bool
    matchScope(GeometryScope needle, const GeometryScope *haystack, int size)
    {
	for (int i = 0; i < size; ++i)
	    if (needle == haystack[i])
		return true;
	return false;
    }

    static GeometryScope
    getArbitraryPropertyScope(const PropertyHeader &header)
    {
	return Alembic::AbcGeom::GetGeometryScope(header.getMetaData());
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

    static void
    markFilled(GT_AttributeList &alist, const char *name, bool *filled)
    {
	int		idx = alist.getIndex(name);
	if (idx >= 0)
	    filled[idx] = true;
    }

    static const fpreal	timeBias = 0.0001;

    static fpreal
    getIndex(fpreal t,
	    const TimeSamplingPtr &itime,
	    exint nsamp,
	    index_t &i0,
	    index_t &i1)
    {
	nsamp  = SYSmax(nsamp, 1);
	std::pair<index_t, chrono_t> t0 = itime->getFloorIndex(t, nsamp);
	i0 = i1 = t0.first;
	if (nsamp == 1 || SYSisEqual(t, t0.second, timeBias))
	    return 0;
	std::pair<index_t, chrono_t> t1 = itime->getCeilIndex(t, nsamp);
	i1 = t1.first;
	if (i0 == i1)
	    return 0;
	fpreal	bias = (t - t0.second) / (t1.second - t0.second);
	if (SYSisEqual(bias, 1, timeBias))
	{
	    i0 = i1;
	    return 0;
	}
	return bias;
    }

    static GT_DataArrayHandle
    getArraySample(GABC_IArchive &arch, const IArrayProperty &prop,
	    index_t idx, GT_Type tinfo)
    {
	GABC_IArray	iarray = GABC_IArray::getSample(arch, prop, idx, tinfo);
	return GABC_NAMESPACE::GABCarray(iarray);
    }

    template <typename ABC_POD, typename GT_POD, GT_Storage GT_STORAGE>
    static GT_DataArrayHandle
    extractScalarProp(const IScalarProperty &prop, index_t idx)
    {
	const DataType	&dtype = prop.getDataType();
	const char	*interp = prop.getMetaData().get("interpretation").c_str();
	int		 tsize = dtype.getExtent();
	GT_Type		 tinfo = GABC_GTUtil::getGTTypeInfo(interp, tsize);
	UT_StackBuffer<ABC_POD>	src(tsize);
	UT_StackBuffer<GT_POD>	dest(tsize);

	prop.get(src, ISampleSelector(idx));
	for (int i = 0; i < tsize; ++i)
	    dest[i] = src[i];
	return new GT_DAConstantValue<GT_POD, GT_STORAGE>(1, dest, tsize, tinfo);
    }

    static GT_DataArrayHandle
    extractScalarString(const IScalarProperty &prop, index_t idx)
    {
	GT_DAIndexedString	*data = new GT_DAIndexedString(1);
	std::string		 val;
	prop.get(&val, ISampleSelector(idx));
	data->setString(0, 0, val.c_str());
	return data;
    }

    static GT_DataArrayHandle
    getScalarSample(GABC_IArchive &arch,
	    const IScalarProperty &prop, index_t idx)
    {
	switch (prop.getDataType().getPod())
	{
	    case Alembic::Abc::kBooleanPOD:
		return extractScalarProp<bool, uint8, GT_STORE_UINT8>(prop, idx);
	    case Alembic::Abc::kInt8POD:
		return extractScalarProp<int8, int32, GT_STORE_INT32>(prop, idx);
	    case Alembic::Abc::kUint8POD:
		return extractScalarProp<uint8, uint8, GT_STORE_UINT8>(prop, idx);
	    case Alembic::Abc::kUint16POD:
		return extractScalarProp<uint16, int32, GT_STORE_INT32>(prop, idx);
	    case Alembic::Abc::kInt16POD:
		return extractScalarProp<int16, int32, GT_STORE_INT32>(prop, idx);
	    case Alembic::Abc::kUint32POD:
		return extractScalarProp<uint32, int64, GT_STORE_INT64>(prop, idx);
	    case Alembic::Abc::kInt32POD:
		return extractScalarProp<int32, int32, GT_STORE_INT32>(prop, idx);
	    case Alembic::Abc::kInt64POD:
		return extractScalarProp<int64, int64, GT_STORE_INT64>(prop, idx);
	    case Alembic::Abc::kUint64POD:	// Store uint64 in int64 too
		return extractScalarProp<uint64, int64, GT_STORE_INT64>(prop, idx);
	    case Alembic::Abc::kFloat16POD:
		return extractScalarProp<fpreal16, fpreal16, GT_STORE_REAL16>(prop, idx);
	    case Alembic::Abc::kFloat32POD:
		return extractScalarProp<fpreal32, fpreal32, GT_STORE_REAL32>(prop, idx);
	    case Alembic::Abc::kFloat64POD:
		return extractScalarProp<fpreal64, fpreal64, GT_STORE_REAL64>(prop, idx);
	    case Alembic::Abc::kStringPOD:
		return extractScalarString(prop, idx);

	    case Alembic::Abc::kWstringPOD:
	    case Alembic::Abc::kNumPlainOldDataTypes:
	    case Alembic::Abc::kUnknownPOD:
		break;
	}
	return GT_DataArrayHandle();
    }

    void decomposeXForm(
            const M44d &m,
            V3d &scale,
            V3d &shear,
            Quatd &q,
            V3d &t
    )
    {
        M44d mtmp(m);

        // Extract Scale, Shear
	Imath::extractAndRemoveScalingAndShear(mtmp, scale, shear);

        // Extract translation
        t.x = mtmp[3][0];
        t.y = mtmp[3][1];
        t.z = mtmp[3][2];

        // Extract rotation
        q = extractQuat(mtmp);
    }

    M44d recomposeXForm(
            const V3d &scale,
            const V3d &shear,
            const Quatd &rotation,
            const V3d &translation
    )
    {
	M44d	scale_mtx, shear_mtx, rotation_mtx, translation_mtx;

        scale_mtx.setScale(scale);
        shear_mtx.setShear(shear);
        rotation_mtx = rotation.toMatrix44();
        translation_mtx.setTranslation(translation);

        return scale_mtx * shear_mtx * rotation_mtx * translation_mtx;
    }

    static V3d
    lerp(const Imath::V3d &a, const Imath::V3d &b, double bias)
    {
        return V3d(SYSlerp(a[0], b[0], bias), SYSlerp(a[1], b[1], bias),
			SYSlerp(a[2], b[2], bias));
    }

    static M44d
    blendMatrix(const M44d &m0, const M44d &m1, fpreal bias)
    {
	V3d	s0, s1;	// Scales
	V3d	h0, h1;	// Shears
	V3d	t0, t1;	// Translates
	Quatd	q0, q1;	// Rotations

	decomposeXForm(m0, s0, h0, q0, t0);
	decomposeXForm(m1, s1, h1, q1, t1);
	if ((q0 ^ q1) < 0)
	    q1 = -q1;
	return recomposeXForm(lerp(s0, s1, bias),
			    lerp(h0, h1, bias),
			    Imath::slerp(q0, q1, (double)bias),
			    lerp(t0, t1, bias));
    }

    template <typename T, GT_Storage T_STORAGE>
    static GT_DataArrayHandle
    blendArrays(const GT_DataArrayHandle &s0, const T *f0,
		const GT_DataArrayHandle &s1, const T *f1,
		fpreal bias)
    {
	GT_DANumeric<T, T_STORAGE>	*gtarray;
	gtarray = new GT_DANumeric<T, T_STORAGE>(s0->entries(),
				s0->getTupleSize(), s0->getTypeInfo());
	T	*dest = gtarray->data();
	GT_Size	 fullsize = s0->entries() * s0->getTupleSize();
	for (exint i = 0; i < fullsize; ++i)
	{
	    dest[i] = SYSlerp(f0[i], f1[i], bias);
	}
	return GT_DataArrayHandle(gtarray);
    }

    static bool
    areArraysDifferent(const GT_DataArrayHandle &s0,
		const GT_DataArrayHandle &s1)
    {
	return s0->entries() != s1->entries()
		|| s0->getTupleSize() != s1->getTupleSize()
		|| s0->getStorage() != s1->getStorage();
    }

    static GT_DataArrayHandle
    blendArrays(const GT_DataArrayHandle &s0, const GT_DataArrayHandle &s1,
	    fpreal bias)
    {
	if (areArraysDifferent(s0, s1))
	    return s0;
	GT_DataArrayHandle	buf0, buf1;
	switch (s0->getStorage())
	{
	    case GT_STORE_REAL16:
		return blendArrays<fpreal16, GT_STORE_REAL16>(
			s0, s0->getF16Array(buf0),
			s1, s1->getF16Array(buf1), bias);
	    case GT_STORE_REAL32:
		return blendArrays<fpreal32, GT_STORE_REAL32>(
			s0, s0->getF32Array(buf0),
			s1, s1->getF32Array(buf1), bias);
	    case GT_STORE_REAL64:
		return blendArrays<fpreal64, GT_STORE_REAL64>(
			s0, s0->getF64Array(buf0),
			s1, s1->getF64Array(buf1), bias);
	    default:
		UT_ASSERT(0);
	}
	return s0;
    }

    static GT_DataArrayHandle
    readArrayProperty(GABC_IArchive &arch, const IArrayProperty &prop,
	    fpreal t, GT_Type tinfo)
    {
	if(prop.getNumSamples() == 0)
	    return GT_DataArrayHandle();

	index_t i0, i1;
	fpreal	bias = getIndex(t, prop.getTimeSampling(),
				prop.getNumSamples(), i0, i1);
	GT_DataArrayHandle	s0 = getArraySample(arch, prop, i0, tinfo);
	if (i0 == i1 || !GTisFloat(s0->getStorage()))
	    return s0;
	GT_DataArrayHandle	s1 = getArraySample(arch, prop, i1, tinfo);
	return blendArrays(s0, s1, bias);
    }

    template <typename T>
    static GT_DataArrayHandle
    readGeomProperty(GABC_IArchive &arch, const T &gparam, fpreal t,
	    GT_Type tinfo)
    {
	if (!gparam || !gparam.valid())
	    return GT_DataArrayHandle();

	index_t i0, i1;
	fpreal	bias = getIndex(t, gparam.getTimeSampling(),
				gparam.getNumSamples(), i0, i1);
	typename T::sample_type	v0;
	gparam.getExpanded(v0, i0);
	if (!v0.valid() || !v0.getVals()->size())
	{
	    UT_ASSERT(0 && "This is likely a corrupt indexed alembic array");
	    gparam.getIndexed(v0, i0);
	    return arrayFromSample(arch, v0.getVals(),
				gparam.getArrayExtent(), tinfo);
	}
	GT_DataArrayHandle	s0 = arrayFromSample(arch, v0.getVals(),
					gparam.getArrayExtent(), tinfo);
	if (i0 == i1 || !GTisFloat(s0->getStorage()))
	    return s0;
	typename T::sample_type	v1;
	gparam.getExpanded(v1, i1);
	GT_DataArrayHandle	s1 = arrayFromSample(arch, v1.getVals(),
					gparam.getArrayExtent(), tinfo);
	return blendArrays(s0, s1, bias);
    }

    static GT_DataArrayHandle
    readUVProperty(GABC_IArchive &arch, const IV2fGeomParam &uvs, fpreal t)
    {
	// Alembic stores uv's as a 2-tuple, but Houdini expects a 3-tuple
	GT_DataArrayHandle	uv2 = readGeomProperty(arch, uvs,
					t, GT_TYPE_NONE);
	UT_ASSERT(uv2 && uv2->getTupleSize() == 2);
	if (uv2->getTupleSize() == 3)
	    return uv2;
	GT_Size			n = uv2->entries();
	GT_Real32Array		*uv3 = new GT_Real32Array(n, 3,
						uv2->getTypeInfo());
	GT_DataArrayHandle	storage;
	const fpreal32	*src = uv2->getF32Array(storage);
	fpreal32	*dest = uv3->data();
	for (exint i = 0; i < n; ++i, src += 2, dest += 3)
	{
	    dest[0] = src[0];
	    dest[1] = src[1];
	    dest[2] = 0;
	}
	return GT_DataArrayHandle(uv3);
    }

    static GT_DataArrayHandle
    readScalarProperty(GABC_IArchive &arch, const IScalarProperty &prop,
	    fpreal t)
    {
	index_t i0, i1;
	fpreal	bias = getIndex(t, prop.getTimeSampling(),
				prop.getNumSamples(), i0, i1);
	GT_DataArrayHandle	s0 = getScalarSample(arch, prop, i0);
	if (i0 == i1 || !GTisFloat(s0->getStorage()))
	    return s0;
	GT_DataArrayHandle	s1 = getScalarSample(arch, prop, i1);
	return blendArrays(s0, s1, bias);
    }

    template <typename PRIM_T, typename SCHEMA_T>
    static GT_DataArrayHandle
    gabcGetPosition(const GABC_IObject &obj, fpreal t, GEO_AnimationType &atype)
    {
	PRIM_T			 prim(obj.object(), gabcWrapExisting);
	SCHEMA_T		&ss = prim.getSchema();
	IP3fArrayProperty	 P = ss.getPositionsProperty();
	UT_ASSERT(P.valid());
	atype = P.isConstant() ? GEO_ANIMATION_CONSTANT
				: GEO_ANIMATION_ATTRIBUTE;
	return readArrayProperty(*obj.archive(), P, t, GT_TYPE_POINT);
    }

    template <typename PRIM_T, typename SCHEMA_T>
    static GT_DataArrayHandle
    gabcGetVelocity(const GABC_IObject &obj, fpreal t)
    {
	PRIM_T			 prim(obj.object(), gabcWrapExisting);
	SCHEMA_T		&ss = prim.getSchema();
	IV3fArrayProperty	 v = ss.getVelocitiesProperty();
	if (!v.valid())
	    return GT_DataArrayHandle();
	return readArrayProperty(*obj.archive(), v, t, GT_TYPE_VECTOR);
    }

    template <typename PRIM_T, typename SCHEMA_T>
    static GT_DataArrayHandle
    gabcGetWidth(const GABC_IObject &obj, fpreal t)
    {
	PRIM_T		 prim(obj.object(), gabcWrapExisting);
	SCHEMA_T	&ss = prim.getSchema();
	IFloatGeomParam	 v = ss.getWidthsParam();
	if (!v.valid())
	    return GT_DataArrayHandle();
	return readGeomProperty(*obj.archive(), v, t, GT_TYPE_NONE);
    }


    static void
    fillHoudiniAttributes(GT_AttributeList &alist,
		    const GEO_Primitive &prim,
		    GA_AttributeOwner owner,
		    bool *filled)
    {
	const GA_Detail		&gdp = prim.getDetail();
	const GA_AttributeDict	&dict = gdp.getAttributes().getDict(owner);
	const GA_IndexMap		&indexmap = gdp.getIndexMap(owner);
	GA_Offset			 offset;
	if (owner == GA_ATTRIB_DETAIL)
	    offset = GA_Offset(0);
	else
	    offset = prim.getMapOffset();
	GA_Range	range(indexmap, offset, GA_Offset(offset+1));
	for (GA_AttributeDict::iterator it = dict.begin(GA_SCOPE_PUBLIC);
		!it.atEnd(); ++it)
	{
	    int			idx = alist.getIndex((*it)->getName());
	    if (idx >= 0 && !filled[idx])
	    {
		GT_DataArrayHandle h = GT_Util::extractAttribute(*(*it), range);
		alist.set(idx, h, 0);
		filled[idx] = true;
	    }
	}
    }

    #define LOOKUP_TYPE(VAR) \
	GABC_GTUtil::getGTTypeInfo(VAR::traits_type::interpretation(), \
				VAR->getDataType().getExtent())

    #define SET_ARRAY(VAR, NAME, TYPEINFO) \
	if (VAR && *VAR) { \
	    if (ONLY_ANIMATING && VAR->isConstant()) \
		markFilled(alist, NAME, filled); \
	    else \
		setAttributeData(alist, NAME, \
		    readArrayProperty(arch, *VAR, t, TYPEINFO), filled); \
	}
    #define SET_GEOM_PARAM(VAR, NAME, TYPEINFO) \
	if (VAR && VAR->valid() && matchScope(VAR->getScope(), \
		    scope, scope_size)) { \
	    if (ONLY_ANIMATING && VAR->isConstant()) \
		markFilled(alist, NAME, filled); \
	    setAttributeData(alist, NAME, \
		    readGeomProperty(arch, *VAR, t, TYPEINFO), filled); \
	}
    #define SET_GEOM_UVS_PARAM(VAR, NAME) \
	if (VAR && VAR->valid() && matchScope(VAR->getScope(), \
		    scope, scope_size)) { \
	    if (ONLY_ANIMATING && VAR->isConstant()) \
		markFilled(alist, NAME, filled); \
	    setAttributeData(alist, NAME, \
		    readUVProperty(arch, *VAR, t), filled); \
	}

    static GT_Storage
    getGTStorage(ICompoundProperty &parent, const PropertyHeader &head)
    {
	if (head.isArray() || head.isScalar())
	    return GABC_GTUtil::getGTStorage(head.getDataType());
	if (head.isCompound())
	{
	    ICompoundProperty	  comp(parent, head.getName());
	    const PropertyHeader *hidx = comp.getPropertyHeader(".indices");
	    const PropertyHeader *hval = comp.getPropertyHeader(".vals");
	    if (hidx && hval)
	    {
		return getGTStorage(parent, *hval);
	    }
	}
	UT_ASSERT(0 && "Unhandled property type");
	return GT_STORE_INVALID;
    }

    static SYS_AtomicCounter	theUIDCount;

    static inline void
    topologyUID(GT_AttributeList &alist, const GABC_IObject &obj)
    {
	int __topologyIdx = alist.getIndex("__topology");
	if (__topologyIdx < 0)
	    return;

	const GT_DataArrayHandle	&__topology = alist.get(__topologyIdx);
	if (obj.getAnimationType(false) != GEO_ANIMATION_TOPOLOGY)
	{
	    if (!__topology)
		alist.set(__topologyIdx, new GT_IntConstant(1, 0));
	}
	else
	{
	    alist.set(__topologyIdx, new GT_IntConstant(1, theUIDCount.add(1)));
	}
    }

    template <bool ONLY_ANIMATING>
    static void
    fillAttributeList(GT_AttributeList &alist,
	    const GEO_PackedNameMapPtr &namemap,
	    const GEO_Primitive *prim,
	    const GABC_IObject &obj,
	    fpreal t,
	    const GeometryScope *scope,
	    int scope_size,
	    ICompoundProperty arb,
	    const IP3fArrayProperty *P,
	    const IV3fArrayProperty *v,
	    const IN3fGeomParam *N,
	    const IV2fGeomParam *uvs,
	    const IUInt64ArrayProperty *ids,
	    const IFloatGeomParam *widths,
	    const IFloatArrayProperty *Pw)
    {
	UT_ASSERT(alist.entries());
	if (!alist.entries())
	    return;

	GABC_IArchive		&arch = *obj.archive();
	ISampleSelector		 sample(t);
	UT_StackBuffer<bool>	 filled(alist.entries());

	memset(filled, 0, sizeof(bool)*alist.entries());
	topologyUID(alist, obj);

	SET_ARRAY(P, "P", GT_TYPE_POINT)
	SET_ARRAY(v, "v", GT_TYPE_VECTOR)
	SET_ARRAY(ids, "id", GT_TYPE_NONE)
	SET_GEOM_PARAM(N, "N", GT_TYPE_NORMAL)
	SET_GEOM_UVS_PARAM(uvs, "uv")
	SET_GEOM_PARAM(widths, "width", GT_TYPE_NONE)
	SET_ARRAY(Pw, "Pw", GT_TYPE_NONE)
	if (prim && matchScope(gabcConstantScope, scope, scope_size))
	{
	    setAttributeData(alist, "__primitive_id",
		    new GT_IntConstant(1, prim->getMapOffset()), filled);

	    GT_DAIndexedString *objp = new GT_DAIndexedString(1,1);
	    objp->setString(0, 0,  obj.objectPath().c_str());
	    setAttributeData(alist, "__object_path", objp, filled);
			     
	}
	if (arb)
	{
	    for (size_t i = 0; i < arb.getNumProperties(); ++i)
	    {
		const PropertyHeader	&header = arb.getPropertyHeader(i);
		if (!matchScope(getArbitraryPropertyScope(header),
			    scope, scope_size))
		{
		    continue;
		}

		const char	*name = header.getName().c_str();
		if (namemap)
		{
		    name = namemap->getName(name);
		    if (!name)
			continue;
		}
		GT_Storage	store = getGTStorage(arb, header);
		if (store == GT_STORE_INVALID)
		    continue;
		GT_DataArrayHandle data = obj.convertIProperty(arb, header, t);
		if(data)
		    setAttributeData(alist, name, data, filled);
	    }
	}
	// We need to fill Houdini attributes last.  Otherwise, when converting
	// two primitives, the Houdini attributes override the first primitive
	// converted.
	if (prim && matchScope(gabcConstantScope, scope, scope_size))
	{
	    fillHoudiniAttributes(alist, *prim, GA_ATTRIB_PRIMITIVE, filled);
	    fillHoudiniAttributes(alist, *prim, GA_ATTRIB_GLOBAL, filled);
	}
    }


    template <typename T>
    static void
    addPropertyToMap(GT_AttributeMap &map, const char *name, const T *prop)
    {
	if (prop && *prop)
	    map.add(name, true);
    }

    template <typename T>
    static void
    addGeomParamToMap(GT_AttributeMap &map, const char *name, T *param,
	    const GeometryScope *scope, int scope_size, bool override=false)
    {
	if (param && param->valid()
		&& matchScope(param->getScope(), scope, scope_size))
	    map.add(name, override);
    }

    static void
    initializeHoudiniAttributes(const GEO_Primitive &prim,
	    GT_AttributeMap &map, GA_AttributeOwner owner)
    {
	const GA_Detail		&gdp = prim.getDetail();
	const GA_AttributeDict	&dict = gdp.getAttributes().getDict(owner);
	for (GA_AttributeDict::iterator it = dict.begin(GA_SCOPE_PUBLIC);
		!it.atEnd(); ++it)
	{
	    const GA_Attribute	*attrib = it.attrib();
	    if (attrib->getAIFTuple() || attrib->getAIFStringTuple())
		map.add(attrib->getName(), false);
	}
    }

    static GT_AttributeListHandle
    initializeAttributeList(const GEO_Primitive *prim,
			const GABC_IObject &obj,
			const GEO_PackedNameMapPtr &namemap,
			fpreal t, 
			const GeometryScope *scope,
			int scope_size,
			ICompoundProperty arb,
			bool create_topology,
			const IP3fArrayProperty *P = NULL,
			const IV3fArrayProperty *v = NULL,
			const IN3fGeomParam *N = NULL,
			const IV2fGeomParam *uvs = NULL,
			const IUInt64ArrayProperty *ids = NULL,
			const IFloatGeomParam *widths = NULL,
			const IFloatArrayProperty *Pw = NULL)
    {
	GT_AttributeMap	*map = new GT_AttributeMap();

	if (create_topology)
	    map->add("__topology", true);
	addPropertyToMap(*map, "P", P);
	addPropertyToMap(*map, "Pw", Pw);
	addPropertyToMap(*map, "v", v);
	addPropertyToMap(*map, "id", ids);
	addGeomParamToMap(*map, "N", N, scope, scope_size);
	addGeomParamToMap(*map, "uv", uvs, scope, scope_size);
	addGeomParamToMap(*map, "width", widths, scope, scope_size);

	if (prim && matchScope(gabcConstantScope, scope, scope_size))
	{
	    map->add("__primitive_id", true);
	    map->add("__object_path", true);
	}
	if (arb)
	{
	    for (exint i = 0; i < arb.getNumProperties(); ++i)
	    {
		const PropertyHeader	&header = arb.getPropertyHeader(i);
		if (!matchScope(getArbitraryPropertyScope(header), scope, scope_size))
		{
		    continue;
		}
		const char	*name = header.getName().c_str();
		if (namemap)
		{
		    name = namemap->getName(name);
		    if (!name)
			continue;
		}
		GT_Storage	store = getGTStorage(arb, header);
		if (store == GT_STORE_INVALID)
		    continue;
		map->add(name, false);
	    }
	}

	if (prim && matchScope(gabcConstantScope, scope, scope_size))
	{
	    initializeHoudiniAttributes(*prim, *map, GA_ATTRIB_PRIMITIVE);
	    initializeHoudiniAttributes(*prim, *map, GA_ATTRIB_GLOBAL);
	}

	GT_AttributeList	*alist = NULL;
	if (!map->entries())
	    delete map;
	else
	{
	    alist = new GT_AttributeList(GT_AttributeMapHandle(map));
	    if (alist->entries())
	    {
		UT_StackBuffer<bool>	filled(alist->entries());
		memset(filled, 0, sizeof(bool)*alist->entries());
		fillAttributeList<false>(*alist, namemap, prim, obj, t,
			scope, scope_size,
			arb, P, v, N, uvs, ids, widths, Pw);
	    }
	}

	return GT_AttributeListHandle(alist);
    }

    static GT_AttributeListHandle
    updateAttributeList(const GT_AttributeListHandle &src,
			const GEO_Primitive *prim,
			const GABC_IObject &obj,
			const GEO_PackedNameMapPtr &namemap,
			fpreal t, 
			const GeometryScope *scope,
			int scope_size,
			ICompoundProperty arb,
			const IP3fArrayProperty *P = NULL,
			const IV3fArrayProperty *v = NULL,
			const IN3fGeomParam *N = NULL,
			const IV2fGeomParam *uvs = NULL,
			const IUInt64ArrayProperty *ids = NULL,
			const IFloatGeomParam *widths = NULL,
			const IFloatArrayProperty *Pw = NULL)
    {
	if (!src || !src->entries())
	    return src;

	// Copy the existing attributes
	GT_AttributeList	*alist = new GT_AttributeList(*src);
	// Only fill animating attributes
	fillAttributeList<true>(*alist, namemap, prim, obj, t,
		    scope, scope_size, arb, P, v, N, uvs, ids, widths, Pw);
	return GT_AttributeListHandle(alist);
    }


    template <typename T>
    static bool
    isEmpty(const T &ptr)
    {
	return !ptr || !ptr->valid() || ptr->size() == 0;
    }

    class CreateAttributeList
    {
    public:
	inline GT_AttributeListHandle	build(
			    GT_Owner owner,
			    const GEO_Primitive *prim,
			    const GABC_IObject &obj,
			    const GEO_PackedNameMapPtr &namemap,
			    fpreal t,
			    const GeometryScope *scope,
			    int scope_size,
			    ICompoundProperty arb,
			    const IP3fArrayProperty *P = NULL,
			    const IV3fArrayProperty *v = NULL,
			    const IN3fGeomParam *N = NULL,
			    const IV2fGeomParam *uvs = NULL,
			    const IUInt64ArrayProperty *ids = NULL,
			    const IFloatGeomParam *widths = NULL,
			    const IFloatArrayProperty *Pw = NULL) const
	{
	    return initializeAttributeList(prim, obj, namemap, t,
		    scope, scope_size, arb, owner == GT_OWNER_DETAIL,
		    P, v, N, uvs, ids, widths, Pw);
	}
	inline GT_AttributeListHandle	build(
			    GT_Owner owner,
			    const GEO_Primitive *prim,
			    const GABC_IObject &obj,
			    const GEO_PackedNameMapPtr &namemap,
			    fpreal t,
			    GeometryScope scope,
			    ICompoundProperty arb,
			    const IP3fArrayProperty *P = NULL,
			    const IV3fArrayProperty *v = NULL,
			    const IN3fGeomParam *N = NULL,
			    const IV2fGeomParam *uvs = NULL,
			    const IUInt64ArrayProperty *ids = NULL,
			    const IFloatGeomParam *widths = NULL,
			    const IFloatArrayProperty *Pw = NULL) const
	{
	    return initializeAttributeList(prim, obj, namemap, t,
		    &scope, 1, arb, owner == GT_OWNER_DETAIL,
		    P, v, N, uvs, ids, widths, Pw);
	}
    };
    class UpdateAttributeList
    {
    public:
	UpdateAttributeList(const GT_PrimitiveHandle &prim)
	    : myPrim(prim)
	{
	}
	inline const GT_AttributeListHandle &getSource(GT_Owner owner) const
	{
	    switch (owner)
	    {
		case GT_OWNER_VERTEX:
		    return myPrim->getVertexAttributes();
		case GT_OWNER_POINT:
		    return myPrim->getPointAttributes();
		case GT_OWNER_UNIFORM:
		    return myPrim->getUniformAttributes();
		case GT_OWNER_DETAIL:
		    return myPrim->getDetailAttributes();
		default:
		    UT_ASSERT(0);
	    }
	    return myPrim->getPointAttributes();
	}
	inline GT_AttributeListHandle	build(
			    GT_Owner owner,
			    const GEO_Primitive *prim,
			    const GABC_IObject &obj,
			    const GEO_PackedNameMapPtr &namemap,
			    fpreal t,
			    const GeometryScope *scope,
			    int scope_size,
			    ICompoundProperty arb,
			    const IP3fArrayProperty *P = NULL,
			    const IV3fArrayProperty *v = NULL,
			    const IN3fGeomParam *N = NULL,
			    const IV2fGeomParam *uvs = NULL,
			    const IUInt64ArrayProperty *ids = NULL,
			    const IFloatGeomParam *widths = NULL,
			    const IFloatArrayProperty *Pw = NULL) const
	{
	    return updateAttributeList(getSource(owner), prim, obj, namemap, t,
		    scope, scope_size, arb, P, v, N, uvs, ids, widths, Pw);
	}
	inline GT_AttributeListHandle	build(
			    GT_Owner owner,
			    const GEO_Primitive *prim,
			    const GABC_IObject &obj,
			    const GEO_PackedNameMapPtr &namemap,
			    fpreal t,
			    GeometryScope scope,
			    ICompoundProperty arb,
			    const IP3fArrayProperty *P = NULL,
			    const IV3fArrayProperty *v = NULL,
			    const IN3fGeomParam *N = NULL,
			    const IV2fGeomParam *uvs = NULL,
			    const IUInt64ArrayProperty *ids = NULL,
			    const IFloatGeomParam *widths = NULL,
			    const IFloatArrayProperty *Pw = NULL) const
	{
	    return updateAttributeList(getSource(owner), prim, obj, namemap, t,
		    &scope, 1, arb, P, v, N, uvs, ids, widths, Pw);
	}
	const GT_PrimitiveHandle	&myPrim;
    };

    static GT_FaceSetPtr
    loadFaceSet(const GABC_IObject &obj, fpreal t)
    {
	IFaceSet		 shape(obj.object(), gabcWrapExisting);
	IFaceSetSchema		&ss = shape.getSchema();
	IFaceSetSchema::Sample	 sample = ss.getValue(ISampleSelector(t));
	GT_FaceSetPtr		 set = new GT_FaceSet();
	Int32ArraySamplePtr	 faces = sample.getFaces();


	if (!isEmpty(faces))
	{
	    const int32		*indices = faces->get();
	    exint		 size = faces->size();
	    for (exint i = 0; i < size; ++i)
		set->addFace(indices[i]);
	}

	return set;
    }

    template <typename GT_PRIMTYPE>
    static void
    loadFaceSets(GT_PRIMTYPE &pmesh, const GABC_IObject &obj, fpreal t)
    {
	exint	nkids = obj.getNumChildren();
	for (exint i = 0; i < nkids; ++i)
	{
	    GABC_IObject	kid = obj.getChild(i);
	    if (kid.valid() && kid.nodeType() == GABC_FACESET)
	    {
		GT_FaceSetPtr	set = loadFaceSet(kid, t);
		if (set)
		{
		    pmesh.addFaceSet(kid.getName().c_str(), set);
		}
	    }
	}
    }


    template <typename ATTRIB_CREATOR>
    static GT_PrimitiveHandle
    buildPointMesh(const ATTRIB_CREATOR &acreate,
	    const GEO_Primitive *prim,
	    const GABC_IObject &obj,
	    fpreal t,
	    const GEO_PackedNameMapPtr &namemap)
    {
	IPoints			 shape(obj.object(), gabcWrapExisting);
	IPointsSchema		&ss = shape.getSchema();
	IPointsSchema::Sample	 sample = ss.getValue(ISampleSelector(t));

	GT_AttributeListHandle	vertex;
	GT_AttributeListHandle	detail;
	IP3fArrayProperty	P = ss.getPositionsProperty();
	IV3fArrayProperty	v = ss.getVelocitiesProperty();
	IUInt64ArrayProperty	ids = ss.getIdsProperty();
	IFloatGeomParam		widths = ss.getWidthsParam();
	GeometryScope	vertex_scope[3] = { gabcVertexScope, gabcVaryingScope, gabcFacevaryingScope };
	GeometryScope	detail_scope[3] = { gabcUniformScope, gabcConstantScope, gabcUnknownScope };

	vertex = acreate.build(GT_OWNER_POINT, prim, obj, namemap, t,
			vertex_scope, 3, ss.getArbGeomParams(),
			&P, &v, NULL, NULL, &ids, &widths);
	detail = acreate.build(GT_OWNER_DETAIL, prim, obj, namemap, t,
			detail_scope, 3, ss.getArbGeomParams());

	GT_Primitive	*gt = new GT_PrimPointMesh(vertex, detail);
	return GT_PrimitiveHandle(gt);
    }

    template <typename ATTRIB_CREATOR>
    static GT_PrimitiveHandle
    buildSubDMesh(const ATTRIB_CREATOR &acreate,
			const GEO_Primitive *prim,
			const GABC_IObject &obj,
			fpreal t,
			const GEO_PackedNameMapPtr &namemap)
    {
	ISubD			 shape(obj.object(), gabcWrapExisting);
	ISubDSchema		&ss = shape.getSchema();
	ISubDSchema::Sample	 sample = ss.getValue(ISampleSelector(t));
	GT_DataArrayHandle	 counts;
	GT_DataArrayHandle	 indices;

	counts = simpleArrayFromSample(*obj.archive(), sample.getFaceCounts());
	indices = simpleArrayFromSample(*obj.archive(), sample.getFaceIndices());

	GT_AttributeListHandle	 point;
	GT_AttributeListHandle	 vertex;
	GT_AttributeListHandle	 uniform;
	GT_AttributeListHandle	 detail;
	IP3fArrayProperty	 P = ss.getPositionsProperty();
	IV3fArrayProperty	 v = ss.getVelocitiesProperty();
	const IV2fGeomParam	&uvs = ss.getUVsParam();
	GeometryScope	point_scope[2] = { gabcVaryingScope, gabcVertexScope };

	point = acreate.build(GT_OWNER_POINT, prim, obj, namemap, t,
				point_scope, 2,
				ss.getArbGeomParams(),
				&P,
				&v,
				NULL,
				&uvs);
	vertex = acreate.build(GT_OWNER_VERTEX, prim, obj, namemap, t,
				gabcFacevaryingScope,
				ss.getArbGeomParams(),
				NULL,
				NULL,
				NULL,
				&uvs);
	uniform = acreate.build(GT_OWNER_UNIFORM, prim, obj, namemap, t,
				gabcUniformScope, ss.getArbGeomParams());
	detail = acreate.build(GT_OWNER_DETAIL, prim, obj, namemap, t,
				theConstantUnknownScope, 2,
				ss.getArbGeomParams());

	GT_PrimSubdivisionMesh	*gt = new GT_PrimSubdivisionMesh(counts,
					indices,
					point,
					vertex,
					uniform,
					detail);

	/// Add tags
	Int32ArraySamplePtr	creaseIndices = sample.getCreaseIndices();
	FloatArraySamplePtr	creaseSharpness = sample.getCreaseSharpnesses();
	Int32ArraySamplePtr	cornerIndices = sample.getCreaseIndices();
	FloatArraySamplePtr	cornerSharpness = sample.getCreaseSharpnesses();
	Int32ArraySamplePtr	holeIndices = sample.getHoles();
	if (!isEmpty(creaseIndices) && !isEmpty(creaseSharpness))
	{
	    GT_Int32Array	*index = new GT_Int32Array(
						creaseIndices->get(),
						creaseIndices->size(), 1);
	    GT_Real32Array	*weight = new GT_Real32Array(
						creaseSharpness->get(),
						creaseSharpness->size(), 1);
	    UT_ASSERT(index->entries() == weight->entries()*2);
	    gt->appendIntTag("crease", GT_DataArrayHandle(index));
	    gt->appendRealTag("crease", GT_DataArrayHandle(weight));
	}
	if (!isEmpty(cornerIndices) && !isEmpty(cornerSharpness))
	{
	    GT_Int32Array	*index = new GT_Int32Array(
						cornerIndices->get(),
						cornerIndices->size(), 1);
	    GT_Real32Array	*weight = new GT_Real32Array(
						cornerSharpness->get(),
						cornerSharpness->size(), 1);
	    UT_ASSERT(index->entries() == weight->entries());
	    gt->appendIntTag("corner", GT_DataArrayHandle(index));
	    gt->appendRealTag("corner", GT_DataArrayHandle(weight));
	}
	if (!isEmpty(holeIndices))
	{
	    GT_Int32Array	*index = new GT_Int32Array(
						holeIndices->get(),
						holeIndices->size(), 1);
	    gt->appendIntTag("hole", GT_DataArrayHandle(index));
	}
	int		ival;
	if (ival = sample.getInterpolateBoundary())
	{
	    GT_IntConstant	*val = new GT_IntConstant(1, ival);
	    gt->appendIntTag("interpolateboundary", GT_DataArrayHandle(val));
	}
	if (ival = sample.getFaceVaryingInterpolateBoundary())
	{
	    GT_IntConstant	*val = new GT_IntConstant(1, ival);
	    gt->appendIntTag("facevaryinginterpolateboundary",
		    GT_DataArrayHandle(val));
	}
	if (ival = sample.getFaceVaryingPropagateCorners())
	{
	    GT_IntConstant	*val = new GT_IntConstant(1, ival);
	    gt->appendIntTag("facevaryingpropagatecorners",
		    GT_DataArrayHandle(val));
	}

	loadFaceSets(*gt, obj, t);

	return GT_PrimitiveHandle(gt);
    }

    template <typename ATTRIB_CREATOR>
    static GT_PrimitiveHandle
    buildPolyMesh(const ATTRIB_CREATOR &acreate,
			const GEO_Primitive *prim,
			const GABC_IObject &obj,
			fpreal t,
			const GEO_PackedNameMapPtr &namemap)
    {
	IPolyMesh		 shape(obj.object(), gabcWrapExisting);
	IPolyMeshSchema		&ss = shape.getSchema();
	IPolyMeshSchema::Sample	 sample = ss.getValue(ISampleSelector(t));
	GT_DataArrayHandle	 counts;
	GT_DataArrayHandle	 indices;

	counts = simpleArrayFromSample(*obj.archive(), sample.getFaceCounts());
	indices = simpleArrayFromSample(*obj.archive(), sample.getFaceIndices());

	GT_AttributeListHandle	 point;
	GT_AttributeListHandle	 vertex;
	GT_AttributeListHandle	 uniform;
	GT_AttributeListHandle	 detail;
	IP3fArrayProperty	 P = ss.getPositionsProperty();
	IV3fArrayProperty	 v = ss.getVelocitiesProperty();
	const IN3fGeomParam	&N = ss.getNormalsParam();
	const IV2fGeomParam	&uvs = ss.getUVsParam();
	GeometryScope	point_scope[2] = { gabcVaryingScope, gabcVertexScope };

	point = acreate.build(GT_OWNER_POINT, prim, obj, namemap, t,
				point_scope, 2,
				ss.getArbGeomParams(),
				&P,
				&v,
				&N,
				&uvs);
	vertex = acreate.build(GT_OWNER_VERTEX, prim, obj, namemap, t,
				gabcFacevaryingScope,
				ss.getArbGeomParams(),
				NULL,
				NULL,
				&N,
				&uvs);
	uniform = acreate.build(GT_OWNER_UNIFORM, prim, obj, namemap, t,
				gabcUniformScope, ss.getArbGeomParams());
	detail = acreate.build(GT_OWNER_DETAIL, prim, obj, namemap, t,
				theConstantUnknownScope, 2,
				ss.getArbGeomParams());

	GT_PrimPolygonMesh	*gt = new GT_PrimPolygonMesh(counts,
					indices,
					point,
					vertex,
					uniform,
					detail);
	loadFaceSets(*gt, obj, t);

	return GT_PrimitiveHandle(gt);
    }

    static bool
    validCounts(const GT_DataArrayHandle &counts, const GT_CurveEval &eval)
    {
	exint	n = counts->entries();
	for (int i = 0; i < n; ++i)
	    if (!eval.validCount(counts->getI32(i)))
		return false;
	return true;
    }

    static GT_Basis
    getGTBasis(Alembic::AbcGeom::BasisType abasis)
    {
	switch (abasis)
	{
	    case Alembic::AbcGeom::kBezierBasis:
		return GT_BASIS_BEZIER;
	    case Alembic::AbcGeom::kBsplineBasis:
		return GT_BASIS_BSPLINE;
	    case Alembic::AbcGeom::kCatmullromBasis:
		return GT_BASIS_CATMULLROM;
	    case Alembic::AbcGeom::kHermiteBasis:
		return GT_BASIS_HERMITE;
	    case Alembic::AbcGeom::kPowerBasis:
		return GT_BASIS_POWER;
	    case Alembic::AbcGeom::kNoBasis:
		return GT_BASIS_LINEAR;
	}
	UT_ASSERT(0 && "Unexpected basis type");
	return GT_BASIS_LINEAR;
    }

    static void
    basisWarning(const GABC_IObject &obj, const char *basis)
    {
	static bool	warned = false;
	if (!warned)
	{
	    UT_ErrorLog::mantraWarning(
		    "Alembic file %s (%s) has invalid cubic %s %s",
		    obj.archive()->filename().c_str(),
		    obj.getFullName().c_str(),
		    basis,
		    "curves - converting to linear");
	    warned = true;
	}
    }

    template <typename ATTRIB_CREATOR>
    static GT_PrimitiveHandle
    buildCurveMesh(const ATTRIB_CREATOR &acreate, const GEO_Primitive *prim,
			const GABC_IObject &obj,
			fpreal t,
			const GEO_PackedNameMapPtr &namemap)
    {
	ICurves			 shape(obj.object(), gabcWrapExisting);
	ICurvesSchema		&ss = shape.getSchema();
	ICurvesSchema::Sample	 sample = ss.getValue(ISampleSelector(t));
	GT_DataArrayHandle	 counts;

	counts = simpleArrayFromSample(*obj.archive(), sample.getCurvesNumVertices());

	GT_AttributeListHandle	 vertex;
	GT_AttributeListHandle	 uniform;
	GT_AttributeListHandle	 detail;
	IP3fArrayProperty	 P = ss.getPositionsProperty();
	IV3fArrayProperty	 v = ss.getVelocitiesProperty();
	const IN3fGeomParam	&N = ss.getNormalsParam();
	const IV2fGeomParam	&uvs = ss.getUVsParam();
	IFloatGeomParam		 widths = ss.getWidthsParam();
	GeometryScope		 vertex_scope[3] = {
					gabcVaryingScope,
					gabcVertexScope,
					gabcFacevaryingScope
				 };

	vertex = acreate.build(GT_OWNER_VERTEX, prim, obj, namemap, t,
				vertex_scope, 3,
				ss.getArbGeomParams(),
				&P,
				&v,
				&N,
				&uvs,
				NULL,
				&widths);
	uniform = acreate.build(GT_OWNER_UNIFORM, prim, obj, namemap, t,
				gabcUniformScope, ss.getArbGeomParams());
	detail = acreate.build(GT_OWNER_DETAIL, prim, obj, namemap, t,
				theConstantUnknownScope, 2,
				ss.getArbGeomParams());

	GT_Basis	basis = getGTBasis(sample.getBasis());
	bool		periodic = (sample.getWrap() == Alembic::AbcGeom::kPeriodic);
	int		order = (sample.getType() == Alembic::AbcGeom::kCubic) ? 4 : 2;

	if (!validCounts(counts, GT_CurveEval(basis, order, periodic)))
	{
	    basisWarning(obj, GTbasis(basis));
	    basis = GT_BASIS_LINEAR;
	}

	GT_PrimCurveMesh	*gt = new GT_PrimCurveMesh(basis,
					counts,
					vertex,
					uniform,
					detail,
					periodic);

	loadFaceSets(*gt, obj, t);

	return GT_PrimitiveHandle(gt);
    }

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
    buildLocator(const GABC_IObject &obj, fpreal t)
    {
	IXform		xform(obj.object(), gabcWrapExisting);
	IScalarProperty	loc(xform.getProperties(), "locator");
	GABC_IObject	parent = obj.getParent();
	UT_Matrix4D	utmat;
	bool		isconst, inherits;

	if (!parent.worldTransform(t, utmat, isconst, inherits))
	{
	    utmat.identity();
	    isconst = true;
	}
	M44d		pmat = GABC_Util::getM(utmat);
	V3d		psval, phval, prval, pxval;
	double		ldata[6];
	loc.get(ldata, ISampleSelector(t));
	if (!Imath::extractSHRT(pmat, psval, phval, prval, pxval))
	{
	    psval = V3d(1,1,1);
	    prval = V3d(0,0,0);
	    phval = V3d(0,0,0);
	    pxval = V3d(0,0,0);
	}

	GT_AttributeMapHandle	pmap(new GT_AttributeMap());
	GT_AttributeMapHandle	umap(new GT_AttributeMap());
	GT_AttributeListHandle	point;
	GT_AttributeListHandle	detail;

	int	Pidx = pmap->add("P", true);
	int	lPidx = umap->add("localPosition", true);
	int	lSidx = umap->add("localScale", true);
	int	pXidx = umap->add("parentTrans", true);
	int	pRidx = umap->add("parentRot", true);
	int	pSidx = umap->add("parentScale", true);
	int	pHidx = umap->add("parentShear", true);

	point = GT_AttributeListHandle(new GT_AttributeList(pmap));
	detail = GT_AttributeListHandle(new GT_AttributeList(umap));
	point->set( Pidx, new GT_RealConstant(1, ldata, 3), 0);
	detail->set(lPidx, new GT_RealConstant(1, ldata, 3), 0);
	detail->set(lSidx, new GT_RealConstant(1, ldata+3, 3), 0);
	detail->set(pXidx, new GT_RealConstant(1, &pxval.x, 3), 0);
	detail->set(pRidx, new GT_RealConstant(1, &prval.x, 3), 0);
	detail->set(pSidx, new GT_RealConstant(1, &psval.x, 3), 0);
	detail->set(pHidx, new GT_RealConstant(1, &phval.x, 3), 0);

	return new GT_PrimPointMesh(point, detail);
    }

    static GT_PrimCurveMesh	*theReticle = NULL;	// Geometry for reticle
    static GT_PrimitiveHandle	 theReticleCleanup;	// To free on close
    static UT_Lock		 theReticleLock;

    static GT_PrimitiveHandle
    buildTransform(const GABC_IObject &obj)
    {
	UT_ASSERT(obj.valid());
	UT_DoubleLock<GT_PrimCurveMesh *> lock(theReticleLock, theReticle);
	if (!lock.getValue())
	{
	    GT_Real32Array	*P = new GT_Real32Array(6, 3, GT_TYPE_POINT);
	    GT_Real32Array	*Cd = new GT_Real32Array(3, 3, GT_TYPE_COLOR);
	    memset(P->data(), 0, sizeof(fpreal32)*18);
	    memset(Cd->data(), 0, sizeof(fpreal32)*9);
	    P->data()[3]  = 1;
	    P->data()[10] = 1;
	    P->data()[17] = 1;
	    Cd->data()[0] = 1;
	    Cd->data()[4] = 1;
	    Cd->data()[8] = 1;

	    GT_AttributeListHandle	vertex, uniform;
	    GT_DataArrayHandle		counts(new GT_IntConstant(3, 2));
	    vertex = GT_AttributeList::createAttributeList(
			    "P", P,
			    NULL);
	    uniform = GT_AttributeList::createAttributeList(
			    "Cd", Cd,
			    NULL);
	    GT_PrimCurveMesh	*mesh;
	    mesh = new GT_PrimCurveMesh(GT_BASIS_LINEAR,
		    counts, vertex, uniform, GT_AttributeListHandle(), false);
	    theReticleCleanup.reset(mesh);
	    lock.setValue(mesh);
	}
	// Since we'll be setting the transform on the reticle, we need a
	// unique primitive.
	return GT_PrimitiveHandle(new GT_PrimCurveMesh(*lock.getValue()));
    }


    template <typename ATTRIB_CREATOR>
    static GT_PrimitiveHandle
    buildNuPatch(const ATTRIB_CREATOR &acreate, const GEO_Primitive *prim,
			const GABC_IObject &obj,
			fpreal t,
			const GEO_PackedNameMapPtr &namemap)
    {
	INuPatch		 shape(obj.object(), gabcWrapExisting);
	INuPatchSchema		&ss = shape.getSchema();
	INuPatchSchema::Sample	 sample = ss.getValue(ISampleSelector(t));
	int			 uorder = sample.getUOrder();
	int			 vorder = sample.getVOrder();
	GT_DataArrayHandle	 uknots;
	GT_DataArrayHandle	 vknots;

	uknots = simpleArrayFromSample(*obj.archive(), sample.getUKnot());
	vknots = simpleArrayFromSample(*obj.archive(), sample.getVKnot());

	GT_AttributeListHandle	 vertex;
	GT_AttributeListHandle	 uniform;
	GT_AttributeListHandle	 detail;
	IP3fArrayProperty	 P = ss.getPositionsProperty();
	IFloatArrayProperty	 Pw = ss.getPositionWeightsProperty();
	IV3fArrayProperty	 v = ss.getVelocitiesProperty();
	const IN3fGeomParam	&N = ss.getNormalsParam();
	const IV2fGeomParam	&uvs = ss.getUVsParam();
	GeometryScope		 vertex_scope[3] = {
					gabcVaryingScope,
					gabcVertexScope,
					gabcFacevaryingScope
				 };
	GeometryScope		 detail_scope[3] = {
					gabcUniformScope,
					gabcConstantScope,
					gabcUnknownScope
				 };

	vertex = acreate.build(GT_OWNER_VERTEX, prim, obj, namemap, t,
				vertex_scope, 3,
				ss.getArbGeomParams(),
				&P,
				&v,
				&N,
				&uvs,
				NULL,
				NULL,
				&Pw);
	detail = acreate.build(GT_OWNER_DETAIL, prim, obj, namemap, t,
				detail_scope, 3,
				ss.getArbGeomParams());

	GT_PrimNuPatch	*gt = new GT_PrimNuPatch(uorder, uknots,
				    vorder, vknots, vertex, detail);

	if (ss.hasTrimCurve())
	{
	    GT_DataArrayHandle	loopCount;
	    GT_DataArrayHandle	curveCount;
	    GT_DataArrayHandle	curveOrders;
	    GT_DataArrayHandle	curveKnots;
	    GT_DataArrayHandle	curveMin;
	    GT_DataArrayHandle	curveMax;
	    GT_DataArrayHandle	curveU, curveV, curveW, curveUVW;
	    loopCount = simpleArrayFromSample(*obj.archive(), sample.getTrimNumCurves());
	    curveCount = simpleArrayFromSample(*obj.archive(), sample.getTrimNumVertices());
	    curveOrders = simpleArrayFromSample(*obj.archive(), sample.getTrimOrders());
	    curveKnots = simpleArrayFromSample(*obj.archive(), sample.getTrimKnots());
	    curveMin = simpleArrayFromSample(*obj.archive(), sample.getTrimMins());
	    curveMax = simpleArrayFromSample(*obj.archive(), sample.getTrimMaxes());
	    curveU = simpleArrayFromSample(*obj.archive(), sample.getTrimU());
	    curveV = simpleArrayFromSample(*obj.archive(), sample.getTrimV());
	    curveW = simpleArrayFromSample(*obj.archive(), sample.getTrimW());
	    curveUVW = joinVector3Array(curveU, curveV, curveW);

	    GT_TrimNuCurves	*trims = new GT_TrimNuCurves(loopCount,
				    curveCount, curveOrders,
				    curveKnots, curveMin, curveMax, curveUVW);
	    if (!trims->isValid())
	    {
		delete trims;
		trims = NULL;
	    }
	    gt->adoptTrimCurves(trims);
	}

	return GT_PrimitiveHandle(gt);
    }

    template <typename T, typename SCHEMA_T>
    static ICompoundProperty
    geometryProperties(const GABC_IObject &obj)
    {
	T		 shape(obj.object(), gabcWrapExisting);
	SCHEMA_T	&ss = shape.getSchema();
	return ss.getArbGeomParams();
    }

    template <typename T, typename SCHEMA_T>
    static ICompoundProperty
    userProperties(const GABC_IObject &obj)
    {
	T		 shape(obj.object(), gabcWrapExisting);
	SCHEMA_T	&ss = shape.getSchema();
	return ss.getUserProperties();
    }

    static fpreal
    getMaxWidth(IFloatGeomParam param, fpreal frame)
    {
	if (!param.valid())
	    return theDefaultWidth;

	ISampleSelector			iss(frame);
	IFloatGeomParam::sample_type	psample;

	param.getExpanded(psample, iss);
	Alembic::Abc::FloatArraySamplePtr	vals = psample.getVals();
	exint				len = vals->size();
	float				maxwidth = 0;

	const float	*widths = (const float *)vals->get();
	for (exint i = 0; i < len; ++i)
	{
	    maxwidth = SYSmax(maxwidth, widths[i]);
	}
	return maxwidth;
    }

    static UT_BoundingBox
    blendBox(const UT_BoundingBox &b0, UT_BoundingBox &b1, fpreal t)
    {
	return UT_BoundingBox(
		    SYSlerp(b0.xmin(), b1.xmin(), t),
		    SYSlerp(b0.ymin(), b1.ymin(), t),
		    SYSlerp(b0.zmin(), b1.zmin(), t),
		    SYSlerp(b0.xmax(), b1.xmax(), t),
		    SYSlerp(b0.ymax(), b1.ymax(), t),
		    SYSlerp(b0.zmax(), b1.zmax(), t));
    }

    template <typename ABC_T, typename SCHEMA_T>
    static bool
    abcBounds(const IObject &obj, UT_BoundingBox &box, fpreal t, bool &isconst)
    {
	ABC_T		 prim(obj, gabcWrapExisting);
	const SCHEMA_T	&ss = prim.getSchema();
	IBox3dProperty	 bounds = ss.getSelfBoundsProperty();
	index_t		 i0, i1;
	fpreal		 bias = getIndex(t, ss.getTimeSampling(),
					ss.getNumSamples(), i0, i1);
	isconst = bounds.isConstant();
	box = GABC_Util::getBox(bounds.getValue(ISampleSelector(i0)));
	if (box.isValid() && i0 != i1)
	{
	    UT_BoundingBox	b1;
	    b1 = GABC_Util::getBox(bounds.getValue(ISampleSelector(i1)));
	    box = blendBox(box, b1, bias);
	}
	return box.isValid();
    }

    template <typename ABC_T, typename SCHEMA_T>
    static TimeSamplingPtr
    abcTimeSampling(const IObject &obj)
    {
	ABC_T		 prim(obj, gabcWrapExisting);
	const SCHEMA_T	&ss = prim.getSchema();
	return ss.getTimeSampling();
    }

    static bool
    abcArbsAreAnimated(ICompoundProperty arb)
    {
	exint	narb = arb ? arb.getNumProperties() : 0;
	for (exint i = 0; i < narb; ++i)
	{
	    const PropertyHeader	&head = arb.getPropertyHeader(i);
	    if (head.isArray())
	    {
		IArrayProperty		 prop(arb, head.getName());
		if (!prop.isConstant())
		    return true;
	    }
	    else if (head.isScalar())
	    {
		IScalarProperty		 prop(arb, head.getName());
		if (!prop.isConstant())
		    return true;
	    }
	    else if (head.isCompound())
	    {
		ICompoundProperty	prop(arb, head.getName());
		if (prop && abcArbsAreAnimated(prop))
		    return true;
	    }
	    else
	    {
		UT_ASSERT(0 && "Unhandled property storage");
	    }
	}
	return false;
    }

    template <typename ABC_T, typename SCHEMA_T>
    static GEO_AnimationType
    getAnimation(const GABC_IObject &obj)
    {
	ABC_T			 prim(obj.object(), gabcWrapExisting);
	SCHEMA_T		&schema = prim.getSchema();
	GEO_AnimationType	 atype;
	
	switch (schema.getTopologyVariance())
	{
	    case Alembic::AbcGeom::kConstantTopology:
		atype = GEO_ANIMATION_CONSTANT;
		if (abcArbsAreAnimated(schema.getArbGeomParams()))
		    atype = GEO_ANIMATION_ATTRIBUTE;
		else if (abcArbsAreAnimated(schema.getUserProperties()))
		    atype = GEO_ANIMATION_ATTRIBUTE;
		break;
	    case Alembic::AbcGeom::kHomogenousTopology:
		atype = GEO_ANIMATION_ATTRIBUTE;
		break;
	    case Alembic::AbcGeom::kHeterogenousTopology:
		atype = GEO_ANIMATION_TOPOLOGY;
		break;
	}
	return atype;
    }

    template <>
    GEO_AnimationType
    getAnimation<IXform, IXformSchema>(const GABC_IObject &obj)
    {
	IXform		 prim(obj.object(), gabcWrapExisting);
	IXformSchema	&schema = prim.getSchema();
	if (!schema.isConstant())
	    return GEO_ANIMATION_TRANSFORM;
	return GEO_ANIMATION_CONSTANT;
    }

    template <>
    GEO_AnimationType
    getAnimation<IPoints, IPointsSchema>(const GABC_IObject &obj)
    {
	IPoints		 prim(obj.object(), gabcWrapExisting);
	IPointsSchema	&schema = prim.getSchema();
	if (!schema.isConstant())
	    return GEO_ANIMATION_TOPOLOGY;
	if (abcArbsAreAnimated(schema.getArbGeomParams()))
	    return GEO_ANIMATION_ATTRIBUTE;
	return GEO_ANIMATION_CONSTANT;
    }

    static GEO_AnimationType
    getLocatorAnimation(const GABC_IObject &obj)
    {
	IXform				prim(obj.object(), gabcWrapExisting);
	Alembic::Abc::IScalarProperty	loc(prim.getProperties(), "locator");
	return loc.isConstant() ? GEO_ANIMATION_CONSTANT
				: GEO_ANIMATION_ATTRIBUTE;
    }
};

GABC_IObject::GABC_IObject()
    : GABC_IItem()
    , myObjectPath()
    , myObject()
{
}

GABC_IObject::GABC_IObject(const GABC_IObject &obj)
    : GABC_IItem(obj)
    , myObjectPath(obj.myObjectPath)
    , myObject(obj.myObject)
{
}

GABC_IObject::GABC_IObject(const GABC_IArchivePtr &arch,
		const std::string &objectpath)
    : GABC_IItem(arch)
    , myObjectPath(objectpath)
    , myObject()
{
    if (archive())
	archive()->resolveObject(*this);
}

GABC_IObject::GABC_IObject(const GABC_IArchivePtr &arch, const IObject &obj)
    : GABC_IItem(arch)
    , myObjectPath(obj.getFullName())
    , myObject(obj)
{
}

void
GABC_IObject::init()
{
}

GABC_IObject::~GABC_IObject()
{
    GABC_AlembicLock	lock(archive());
    purge();
    setArchive(NULL);
}

void
GABC_IObject::purge()
{
    myObject = IObject();
}

GABC_IObject &
GABC_IObject::operator=(const GABC_IObject &src)
{
    GABC_IItem::operator=(src);
    myObjectPath = src.myObjectPath;
    myObject = src.myObject;

    return *this;
}

exint
GABC_IObject::getNumChildren() const
{
    if (myObject.valid())
    {
	GABC_AlembicLock	lock(archive());
	return myObject.getNumChildren();
    }
    return 0;
}


GABC_IObject
GABC_IObject::getChild(exint index) const
{
    UT_ASSERT(myObject.valid());
    if (myObject.valid())
    {
	GABC_AlembicLock	lock(archive());
	UT_ASSERT(archive());
	return GABC_IObject(archive(), myObject.getChild(index));
    }
    return GABC_IObject();
}

GABC_IObject
GABC_IObject::getChild(const std::string &name) const
{
    if (myObject.valid())
    {
	GABC_AlembicLock	lock(archive());
	UT_ASSERT(archive());
	return GABC_IObject(archive(), myObject.getChild(name));
    }
    return GABC_IObject();
}

GABC_NodeType
GABC_IObject::nodeType() const
{
    if (!myObject.valid())
	return GABC_UNKNOWN;
    const ObjectHeader	&ohead = myObject.getHeader();
    if (IXform::matches(ohead))
	return GABC_XFORM;
    if (IPolyMesh::matches(ohead))
	return GABC_POLYMESH;
    if (ISubD::matches(ohead))
	return GABC_SUBD;
    if (ICamera::matches(ohead))
	return GABC_CAMERA;
    if (IFaceSet::matches(ohead))
	return GABC_FACESET;
    if (ICurves::matches(ohead))
	return GABC_CURVES;
    if (IPoints::matches(ohead))
	return GABC_POINTS;
    if (INuPatch::matches(ohead))
	return GABC_NUPATCH;
    if (ILight::matches(ohead))
	return GABC_LIGHT;
    if (IMaterial::matches(ohead))
	return GABC_MATERIAL;
    UT_ASSERT(!strcmp(myObject.getFullName().c_str(), "/"));
    return GABC_UNKNOWN;
}

bool
GABC_IObject::isMayaLocator() const
{
    if (!myObject.valid())
	return false;
    UT_ASSERT(IXform::matches(myObject.getHeader()));
    return myObject.getProperties().getPropertyHeader("locator") != NULL;
}

GABC_VisibilityType
GABC_IObject::visibility(bool &animated, fpreal t, bool check_parent) const
{
    animated = false;
    if (!myObject.valid())
	return GABC_VISIBLE_DEFER;

    GABC_AlembicLock	lock(archive());
    IVisibilityProperty	vprop = Alembic::AbcGeom::GetVisibilityProperty(
				    const_cast<IObject &>(myObject));
    if (vprop.valid())
    {
	animated = !vprop.isConstant();
	return vprop.getValue(ISampleSelector(t))
		    ? GABC_VISIBLE_VISIBLE
		    : GABC_VISIBLE_HIDDEN;
    }
    if (check_parent)
    {
	GABC_IObject	parent(getParent());
	if (!parent.valid())
	    return GABC_VISIBLE_VISIBLE;
	return parent.visibility(animated, t, true);
    }
    return GABC_VISIBLE_DEFER;
}

GABC_VisibilityCache *
GABC_IObject::visibilityCache() const
{
    // TODO: This reqires lots of traversal of the hierarchy, so it might be a
    // good idea to have a cache.
    if (!myObject.valid())
	return new GABC_VisibilityCache();

    GABC_AlembicLock	lock(archive());
    IVisibilityProperty	vprop = Alembic::AbcGeom::GetVisibilityProperty(
				    const_cast<IObject &>(myObject));
    if (vprop.valid())
    {
	GABC_VisibilityType	vis;
	vis = vprop.getValue(ISampleSelector((index_t)0))
		    ? GABC_VISIBLE_VISIBLE : GABC_VISIBLE_HIDDEN;
	if (vprop.isConstant())
	{
	    return new GABC_VisibilityCache(vis, NULL);
	}

	// Create a data array for the visibility cache
	exint		 nsamples = vprop.getNumSamples();
	exint		 ntrue = 0;
	GT_DABool	*bits = new GT_DABool(nsamples);
	bits->setAllBits(false);
	for (exint i = 0; i < nsamples; ++i)
	{
	    if (vprop.getValue(ISampleSelector((index_t)i)))
	    {
		bits->setBit(i, true);
		ntrue++;
	    }
	}
	if (ntrue == nsamples || ntrue == 0)
	{
	    if (ntrue)
		UT_ASSERT(vis == GABC_VISIBLE_VISIBLE);
	    return new GABC_VisibilityCache(vis, NULL);
	}
	return new GABC_VisibilityCache(vis,
			new GABC_ChannelCache(GT_DataArrayHandle(bits),
						vprop.getTimeSampling()));
    }
    GABC_IObject	parent(getParent());
    if (!parent.valid())
	return new GABC_VisibilityCache(GABC_VISIBLE_VISIBLE, NULL);
    return parent.visibilityCache();
}

GEO_AnimationType
GABC_IObject::getAnimationType(bool include_transform) const
{
    if (!valid())
	return GEO_ANIMATION_INVALID;
    GABC_AlembicLock	lock(archive());
    GEO_AnimationType	atype = GEO_ANIMATION_CONSTANT;

    try
    {
	// Set the topology based on a combination of conditions.  We
	// initialize based on the shape topology, but if the shape topology is
	// constant, there are still various factors which can make the
	// primitive non-constant (i.e. Homogeneous).
	switch (nodeType())
	{
	    case GABC_POLYMESH:
		atype = getAnimation<IPolyMesh, IPolyMeshSchema>(*this);
		break;
	    case GABC_SUBD:
		atype = getAnimation<ISubD, ISubDSchema>(*this);
		break;
	    case GABC_CURVES:
		atype = getAnimation<ICurves, ICurvesSchema>(*this);
		break;
	    case GABC_POINTS:
		atype = getAnimation<IPoints, IPointsSchema>(*this);
		break;
	    case GABC_NUPATCH:
		atype = getAnimation<INuPatch, INuPatchSchema>(*this);
		break;
	    case GABC_XFORM:
		if (isMayaLocator())
		    atype = getLocatorAnimation(*this);
		else
		    atype = getAnimation<IXform, IXformSchema>(*this);
		break;
	    default:
		atype = GEO_ANIMATION_TOPOLOGY;
		break;
	}
	if (atype == GEO_ANIMATION_CONSTANT && include_transform
		&& isTransformAnimated())
	{
	    atype = GEO_ANIMATION_TRANSFORM;
	}
    }
    catch (const std::exception &)
    {
	atype = GEO_ANIMATION_INVALID;
	UT_ASSERT(0 && "Alembic exception");
    }
    return atype;
}

bool
GABC_IObject::getBoundingBox(UT_BoundingBox &box, fpreal t, bool &isconst) const
{
    if (!valid())
	return false;
    GABC_AlembicLock	lock(archive());
    try
    {
	switch (nodeType())
	{
	    case GABC_POLYMESH:
		return abcBounds<IPolyMesh, IPolyMeshSchema>(object(), box, t, isconst);
	    case GABC_SUBD:
		return abcBounds<ISubD, ISubDSchema>(object(), box, t, isconst);
	    case GABC_CURVES:
		return abcBounds<ICurves, ICurvesSchema>(object(), box, t, isconst);
	    case GABC_POINTS:
		return abcBounds<IPoints, IPointsSchema>(object(), box, t, isconst);
	    case GABC_NUPATCH:
		return abcBounds<INuPatch, INuPatchSchema>(object(), box, t, isconst);
	    case GABC_XFORM:
		box.initBounds(0, 0, 0);
		return true;
	    default:
		break;
	}
    }
    catch (const std::exception &)
    {
	UT_ASSERT(0 && "Alembic exception");
    }
    return false;
}

bool
GABC_IObject::getRenderingBoundingBox(UT_BoundingBox &box, fpreal t) const
{
    bool	isconst;
    if (!getBoundingBox(box, t, isconst))
	return false;
    switch (nodeType())
    {
	case GABC_POINTS:
	    {
		GABC_AlembicLock	lock(archive());
		IPoints		points(myObject, gabcWrapExisting);
		IPointsSchema	&ss = points.getSchema();
		box.expandBounds(0, getMaxWidth(ss.getWidthsParam(), t));
	    }
	    break;
	case GABC_CURVES:
	    {
		GABC_AlembicLock	lock(archive());
		ICurves		curves(myObject, gabcWrapExisting);
		ICurvesSchema	&ss = curves.getSchema();
		box.expandBounds(0, getMaxWidth(ss.getWidthsParam(), t));
	    }
	    break;
	default:
	    break;
    }
    return true;
}

TimeSamplingPtr
GABC_IObject::timeSampling()
{
    //GABC_AlembicLock	lock(archive());
    switch (nodeType())
    {
	case GABC_POLYMESH:
	    return abcTimeSampling<IPolyMesh, IPolyMeshSchema>(object());
	case GABC_SUBD:
	    return abcTimeSampling<ISubD, ISubDSchema>(object());
	case GABC_CURVES:
	    return abcTimeSampling<ICurves, ICurvesSchema>(object());
	case GABC_POINTS:
	    return abcTimeSampling<IPoints, IPointsSchema>(object());
	case GABC_NUPATCH:
	    return abcTimeSampling<INuPatch, INuPatchSchema>(object());
	case GABC_XFORM:
	    return abcTimeSampling<IXform, IXformSchema>(object());
	default:
	    break;
    }
    return TimeSamplingPtr();
}

fpreal
GABC_IObject::clampTime(fpreal input_time)
{
    TimeSamplingPtr	time(timeSampling());
    exint		nsamples = time ? time->getNumStoredTimes() : 0;
    if (nsamples < 1)
	return 0;
    return SYSclamp(input_time, time->getSampleTime(0),
			time->getSampleTime(nsamples-1));
}

GT_PrimitiveHandle
GABC_IObject::getPrimitive(const GEO_Primitive *gprim,
	fpreal t, GEO_AnimationType &atype,
	const GEO_PackedNameMapPtr &namemap) const
{
    GABC_AlembicLock	lock(archive());
    GT_PrimitiveHandle	prim;
    atype = GEO_ANIMATION_CONSTANT;
    try
    {
	switch (nodeType())
	{
	    case GABC_POLYMESH:
		prim = buildPolyMesh(CreateAttributeList(), gprim,
			*this, t, namemap);
		break;
	    case GABC_SUBD:
		prim = buildSubDMesh(CreateAttributeList(), gprim,
			*this, t, namemap);
		break;
	    case GABC_POINTS:
		prim = buildPointMesh(CreateAttributeList(), gprim,
			*this, t, namemap);
		break;
	    case GABC_CURVES:
		prim = buildCurveMesh(CreateAttributeList(), gprim,
			*this, t, namemap);
		break;
	    case GABC_NUPATCH:
		prim = buildNuPatch(CreateAttributeList(), gprim,
			*this, t, namemap);
		break;
	    case GABC_XFORM:
		if (isMayaLocator())
		    prim = buildLocator(*this, t);
		else
		    prim = buildTransform(*this);
	    default:
		break;
	}
    }
    catch (const std::exception &)
    {
	UT_ASSERT(0 && "Alembic exception");
	prim = GT_PrimitiveHandle();
    }
    atype = (prim) ? getAnimationType(false) : GEO_ANIMATION_CONSTANT;
    return prim;
}

GT_PrimitiveHandle
GABC_IObject::updatePrimitive(const GT_PrimitiveHandle &src,
				const GEO_Primitive *gprim,
				fpreal new_time,
				const GEO_PackedNameMapPtr &namemap) const
{
    UT_ASSERT(src);
    GT_PrimitiveHandle	prim;
    try
    {
	switch (nodeType())
	{
	    case GABC_POLYMESH:
		prim = buildPolyMesh(UpdateAttributeList(src), gprim,
			    *this, new_time, namemap);
		break;
	    case GABC_SUBD:
		prim = buildSubDMesh(UpdateAttributeList(src), gprim,
			    *this, new_time, namemap);
		break;
	    case GABC_POINTS:
		prim = buildPointMesh(UpdateAttributeList(src), gprim,
			    *this, new_time, namemap);
		break;
	    case GABC_CURVES:
		prim = buildCurveMesh(UpdateAttributeList(src), gprim,
			    *this, new_time, namemap);
		break;
	    case GABC_NUPATCH:
		prim = buildNuPatch(UpdateAttributeList(src), gprim,
			    *this, new_time, namemap);
		break;
	    default:
		break;
	}
    }
    catch (const std::exception &)
    {
	UT_ASSERT(0 && "Alembic exception");
	prim = GT_PrimitiveHandle();
    }
    if (!prim)
	prim = src;
    return prim;
}

GT_PrimitiveHandle
GABC_IObject::getPointCloud(fpreal t,
	GEO_AnimationType &atype) const
{
    GT_DataArrayHandle	P = getPosition(t, atype);
    if (!P && nodeType() == GABC_XFORM)
    {
	P = GT_DataArrayHandle(new GT_RealConstant(1, 0.0, 3, GT_TYPE_POINT));
    }
    if (P)
    {
	GT_AttributeMapHandle	pmap(new GT_AttributeMap());
	pmap->add("P", true);

	GT_AttributeListHandle	point(new GT_AttributeList(pmap));
	GT_AttributeListHandle	uniform;
	point->set(point->getIndex("P"), P);
	GT_PrimPointMesh	*prim = new GT_PrimPointMesh(point, uniform);
	return GT_PrimitiveHandle(prim);
    }
    return GT_PrimitiveHandle();
}

GT_PrimitiveHandle
GABC_IObject::getBoxGeometry(fpreal t, GEO_AnimationType &atype) const
{
    bool		isconst;
    UT_BoundingBox	box;
    if (!getBoundingBox(box, t, isconst))
	return GT_PrimitiveHandle();
    atype = isconst ? GEO_ANIMATION_CONSTANT : GEO_ANIMATION_ATTRIBUTE;
    GT_BuilderStatus	err;
    return GT_PrimitiveBuilder::wireBox(err, box);
}

GT_PrimitiveHandle
GABC_IObject::getCentroidGeometry(fpreal t, GEO_AnimationType &atype) const
{
    bool		isconst;
    UT_BoundingBox	box;
    if (!getBoundingBox(box, t, isconst))
	return GT_PrimitiveHandle();
    atype = isconst ? GEO_ANIMATION_CONSTANT : GEO_ANIMATION_ATTRIBUTE;
    fpreal64	pos[3];
    pos[0] = box.xcenter();
    pos[1] = box.ycenter();
    pos[2] = box.zcenter();
    GT_AttributeListHandle	 pt;
    pt = GT_AttributeList::createAttributeList(
	    "P", new GT_RealConstant(1, pos, 3, GT_TYPE_POINT),
	    NULL);
    return new GT_PrimPointMesh(pt, GT_AttributeListHandle());
}

bool
GABC_IObject::getLocalTransform(UT_Matrix4D &xform, fpreal t,
				GEO_AnimationType &atype,
				bool &inherits) const
{
    bool	isconst = true;
    if (!localTransform(t, xform, isconst, inherits))
	return false;
    atype = isconst ? GEO_ANIMATION_CONSTANT : GEO_ANIMATION_TRANSFORM;
    return true;
}

bool
GABC_IObject::getWorldTransform(UT_Matrix4D &xform, fpreal t,
					GEO_AnimationType &atype) const
{
    if (!valid())
	return false;

    bool	isconst, inherits;
    if (!worldTransform(t, xform, isconst, inherits))
	return false;
    atype = isconst ? GEO_ANIMATION_CONSTANT : GEO_ANIMATION_TRANSFORM;

    return false;
}

bool
GABC_IObject::isTransformAnimated() const
{
    GABC_IObject	xform = *this;
    while (xform.valid() && xform.nodeType() != GABC_XFORM)
	xform = xform.getParent();
    if (!xform.valid())
	return false;

    return GABC_Util::isTransformAnimated(archive()->filename(), xform);
}

GT_DataArrayHandle
GABC_IObject::getPosition(fpreal t, GEO_AnimationType &atype) const
{
    if (valid())
    {
	GABC_AlembicLock	lock(archive());
	try
	{
	    switch (nodeType())
	    {
		case GABC_POLYMESH:
		    return gabcGetPosition<IPolyMesh,
			       IPolyMeshSchema>(*this, t, atype);
		case GABC_SUBD:
		    return gabcGetPosition<ISubD,
			       ISubDSchema>(*this, t, atype);
		case GABC_CURVES:
		    return gabcGetPosition<ICurves,
			       ICurvesSchema>(*this, t, atype);
		case GABC_POINTS:
		    return gabcGetPosition<IPoints,
			       IPointsSchema>(*this, t, atype);
		case GABC_NUPATCH:
		    return gabcGetPosition<INuPatch,
			       INuPatchSchema>(*this, t, atype);
		default:
		    break;
	    }
	}
	catch (const std::exception &)
	{
	    UT_ASSERT(0 && "Alembic exception");
	}
    }
    atype = GEO_ANIMATION_CONSTANT;
    return GT_DataArrayHandle();
}

GT_DataArrayHandle
GABC_IObject::getWidth(fpreal t, GEO_AnimationType &atype) const
{
    if (valid())
    {
	GABC_AlembicLock	lock(archive());
	try
	{
	    switch (nodeType())
	    {
		case GABC_CURVES:
		    return gabcGetWidth<ICurves, ICurvesSchema>(*this, t);
		case GABC_POINTS:
		    return gabcGetWidth<IPoints, IPointsSchema>(*this, t);
		default:
		    break;
	    }
	}
	catch (const std::exception &)
	{
	    UT_ASSERT(0 && "Alembic exception");
	}
    }
    atype = GEO_ANIMATION_CONSTANT;
    return GT_DataArrayHandle();
}

GT_DataArrayHandle
GABC_IObject::getVelocity(fpreal t, GEO_AnimationType &atype) const
{
    if (valid())
    {
	GABC_AlembicLock	lock(archive());
	try
	{
	    switch (nodeType())
	    {
		case GABC_POLYMESH:
		    return gabcGetVelocity<IPolyMesh, IPolyMeshSchema>(*this, t);
		case GABC_SUBD:
		    return gabcGetVelocity<ISubD, ISubDSchema>(*this, t);
		case GABC_CURVES:
		    return gabcGetVelocity<ICurves, ICurvesSchema>(*this, t);
		case GABC_POINTS:
		    return gabcGetVelocity<IPoints, IPointsSchema>(*this, t);
		case GABC_NUPATCH:
		    return gabcGetVelocity<INuPatch, INuPatchSchema>(*this, t);
		default:
		    break;
	    }
	}
	catch (const std::exception &)
	{
	    UT_ASSERT(0 && "Alembic exception");
	}
    }
    return GT_DataArrayHandle();
}

ICompoundProperty
GABC_IObject::getArbGeomParams() const
{
    if (valid())
    {
	switch (nodeType())
	{
	    case GABC_XFORM:
		return geometryProperties<IXform, IXformSchema>(*this);
		break;
	    case GABC_POLYMESH:
		return geometryProperties<IPolyMesh, IPolyMeshSchema>(*this);
		break;
	    case GABC_SUBD:
		return geometryProperties<ISubD, ISubDSchema>(*this);
		break;
	    case GABC_CAMERA:
		return geometryProperties<ICamera, ICameraSchema>(*this);
		break;
	    case GABC_FACESET:
		return geometryProperties<IFaceSet, IFaceSetSchema>(*this);
		break;
	    case GABC_CURVES:
		return geometryProperties<ICurves, ICurvesSchema>(*this);
		break;
	    case GABC_POINTS:
		return geometryProperties<IPoints, IPointsSchema>(*this);
		break;
	    case GABC_NUPATCH:
		return geometryProperties<INuPatch, INuPatchSchema>(*this);
		break;
	    case GABC_LIGHT:
		return geometryProperties<ILight, ILightSchema>(*this);

	    case GABC_MATERIAL:
	    default:
		break;
	}
    }
    return ICompoundProperty();
}

GT_DataArrayHandle
GABC_IObject::convertIProperty(ICompoundProperty parent,
			    const PropertyHeader &header,
			    fpreal t,
			    GEO_AnimationType *atype) const
{
    GABC_IArchive	&arch = *(archive().get());
    if (header.isArray())
    {
	IArrayProperty	prop(parent, header.getName());
	if (atype)
	{
	    *atype = prop.isConstant() ? GEO_ANIMATION_CONSTANT
				    : GEO_ANIMATION_ATTRIBUTE;
	}
	// Pick up the type from the property interpretation
	return readArrayProperty(arch, prop, t, GT_TYPE_NONE);
    }
    else if (header.isScalar())
    {
	IScalarProperty	prop(parent, header.getName());
	if (atype)
	{
	    *atype = prop.isConstant() ? GEO_ANIMATION_CONSTANT
				    : GEO_ANIMATION_ATTRIBUTE;
	}
	return readScalarProperty(arch, prop, t);
    }
    else if (header.isCompound())
    {
	ICompoundProperty	  comp(parent, header.getName());
	const PropertyHeader *hidx = comp.getPropertyHeader(".indices");
	const PropertyHeader *hval = comp.getPropertyHeader(".vals");
	if (hidx && hval)
	{
	    GT_DataArrayHandle	gtidx, gtval;
	    GEO_AnimationType	atidx, atval;
	    gtidx = convertIProperty(comp, *hidx, t, &atidx);
	    gtval = convertIProperty(comp, *hval, t, &atval);
	    if (gtidx && gtval)
	    {
		GT_DataArray	*gt = new GT_DAIndirect(gtidx, gtval);
		return GT_DataArrayHandle(gt);
	    }
	}
    }

    return GT_DataArrayHandle();
}


ICompoundProperty
GABC_IObject::getUserProperties() const
{
    if (valid())
    {
	switch (nodeType())
	{
	    case GABC_XFORM:
		return userProperties<IXform, IXformSchema>(*this);
		break;
	    case GABC_POLYMESH:
		return userProperties<IPolyMesh, IPolyMeshSchema>(*this);
		break;
	    case GABC_SUBD:
		return userProperties<ISubD, ISubDSchema>(*this);
		break;
	    case GABC_CAMERA:
		return userProperties<ICamera, ICameraSchema>(*this);
		break;
	    case GABC_FACESET:
		return userProperties<IFaceSet, IFaceSetSchema>(*this);
		break;
	    case GABC_CURVES:
		return userProperties<ICurves, ICurvesSchema>(*this);
		break;
	    case GABC_POINTS:
		return userProperties<IPoints, IPointsSchema>(*this);
		break;
	    case GABC_NUPATCH:
		return userProperties<INuPatch, INuPatchSchema>(*this);
		break;
	    case GABC_LIGHT:
		return userProperties<ILight, ILightSchema>(*this);

	    case GABC_MATERIAL:
	    default:
		break;
	}
    }
    return ICompoundProperty();
}

exint
GABC_IObject::getNumGeometryProperties() const
{
    GABC_AlembicLock	lock(archive());
    ICompoundProperty	arb = getArbGeomParams();
    return arb ? arb.getNumProperties() : 0;
}

GT_DataArrayHandle
GABC_IObject::getGeometryProperty(exint index, fpreal t,
	std::string &name, GeometryScope &scope,
	GEO_AnimationType &atype) const
{
    GABC_AlembicLock	lock(archive());
    GT_DataArrayHandle	data;
    try
    {
	ICompoundProperty	arb = getArbGeomParams();
	if (arb)
	{
	    const PropertyHeader	&header = arb.getPropertyHeader(index);
	    name = header.getName();
	    data = convertIProperty(arb, header, t, &atype);
	    scope = getArbitraryPropertyScope(header);
	}
    }
    catch (const std::exception &)
    {
	UT_ASSERT(0 && "Alembic exception");
    }
    return data;
}

GT_DataArrayHandle
GABC_IObject::getGeometryProperty(const std::string &name, fpreal t,
	GeometryScope &scope, GEO_AnimationType &atype) const
{
    GABC_AlembicLock	lock(archive());
    GT_DataArrayHandle	data;
    try
    {
	ICompoundProperty	arb = getArbGeomParams();
	if (arb)
	{
	    const PropertyHeader	*header = arb.getPropertyHeader(name);
	    if (header)
	    {
		data = convertIProperty(arb, *header, t, &atype);
		scope = getArbitraryPropertyScope(*header);
	    }
	}
    }
    catch (const std::exception &)
    {
	UT_ASSERT(0 && "Alembic exception");
    }
    return data;
}

exint
GABC_IObject::getNumUserProperties() const
{
    GABC_AlembicLock	lock(archive());
    ICompoundProperty	arb = getUserProperties();
    return arb ? arb.getNumProperties() : 0;
}

GT_DataArrayHandle
GABC_IObject::getUserProperty(exint index, fpreal t,
	std::string &name, GEO_AnimationType &atype) const
{
    GABC_AlembicLock	lock(archive());
    GT_DataArrayHandle	data;
    try
    {
	ICompoundProperty	arb = getUserProperties();
	if (arb)
	{
	    const PropertyHeader	&header = arb.getPropertyHeader(index);
	    name = header.getName();
	    data = convertIProperty(arb, header, t, &atype);
	}
    }
    catch (const std::exception &)
    {
	UT_ASSERT(0 && "Alembic exception");
    }
    return data;
}

GT_DataArrayHandle
GABC_IObject::getUserProperty(const std::string &name, fpreal t,
	GEO_AnimationType &atype) const
{
    GABC_AlembicLock	lock(archive());
    GT_DataArrayHandle	data;
    try
    {
	ICompoundProperty	arb = getUserProperties();
	if (arb)
	{
	    const PropertyHeader	*header = arb.getPropertyHeader(name);
	    if (header)
	    {
		data = convertIProperty(arb, *header, t, &atype);
	    }
	}
    }
    catch (const std::exception &)
    {
	UT_ASSERT(0 && "Alembic exception");
    }
    return data;
}

bool
GABC_IObject::worldTransform(fpreal t, UT_Matrix4D &xform,
	bool &isConstant, bool &inheritsXform) const
{
    if (!valid())
	return false;
    IObject		obj = object();
    GABC_AlembicLock	lock(archive());
    bool		ascended = false;

    while (obj.valid() && !IXform::matches(obj.getHeader()))
    {
	obj = obj.getParent();
	ascended = true;
    }
    if (!obj.valid())
	return false;
    if (ascended)
    {
	return GABC_Util::getWorldTransform(archive()->filename(),
		GABC_IObject(archive(), obj), t, xform,
		isConstant, inheritsXform);
    }
    return GABC_Util::getWorldTransform(archive()->filename(),
			*this, t, xform, isConstant, inheritsXform);
}

bool
GABC_IObject::worldTransform(fpreal t, M44d &m4,
	bool &is_const, bool &inherits) const
{
    UT_Matrix4D	xform;
    if (!worldTransform(t, xform, is_const, inherits))
	return false;
    m4 = GABC_Util::getM(xform);
    return true;
}

bool
GABC_IObject::localTransform(fpreal t, UT_Matrix4D &mat,
	bool &isConstant, bool &inheritsXform) const
{
    if (!valid())
	return false;
    isConstant = true;
    inheritsXform = true;
    if (nodeType() == GABC_XFORM)
    {
	GABC_AlembicLock	lock(archive());
	IXform		xform(myObject, gabcWrapExisting);
	IXformSchema	&ss = xform.getSchema();
	index_t		i0, i1;
	XformSample	sample;
	M44d		m0;
	fpreal		bias = getIndex(t, ss.getTimeSampling(),
					ss.getNumSamples(), i0, i1);

	if (!ss.isConstant())
	    isConstant = false;
	if (!ss.getInheritsXforms())
	    inheritsXform = false;

	ss.get(sample, ISampleSelector(i0));
	m0 = sample.getMatrix();
	if (i0 != i1)
	{
	    M44d	m1;
	    ss.get(sample, ISampleSelector(i1));
	    m1 = sample.getMatrix();
	    m0 = blendMatrix(m0, m1, bias);
	}
	mat = GABC_Util::getM(m0);
    }
    else
    {
	mat.identity();
    }
    return true;
}

bool
GABC_IObject::localTransform(fpreal t, M44d &m4,
	bool &is_const, bool &inherits) const
{
    UT_Matrix4D	xform;
    if (!localTransform(t, xform, is_const, inherits))
	return false;
    m4 = GABC_Util::getM(xform);
    return true;
}
