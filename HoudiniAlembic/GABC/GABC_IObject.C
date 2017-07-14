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

#include "GABC_IObject.h"
#include "GABC_IArray.h"
#include "GABC_IGTArray.h"
#include "GABC_IArchive.h"
#include "GABC_Util.h"
#include "GABC_GTUtil.h"
#include <SYS/SYS_AtomicInt.h>
#include <GEO/GEO_PackedNameMap.h>
#include <GEO/GEO_PrimPacked.h>
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
    using index_t = Alembic::Abc::index_t;
    using chrono_t = Alembic::Abc::chrono_t;
    using DataType = Alembic::Abc::DataType;
    using M44d = Alembic::Abc::M44d;
    using V3d = Alembic::Abc::V3d;
    using Quatd = Alembic::Abc::Quatd;
    using IObject = Alembic::Abc::IObject;
    using ICompoundProperty = Alembic::Abc::ICompoundProperty;
    using ISampleSelector = Alembic::Abc::ISampleSelector;
    using ObjectHeader = Alembic::Abc::ObjectHeader;
    using TimeSamplingPtr = Alembic::Abc::TimeSamplingPtr;
    using PropertyHeader = Alembic::Abc::PropertyHeader;
    using IP3fArrayProperty = Alembic::Abc::IP3fArrayProperty;
    using IV3fArrayProperty = Alembic::Abc::IV3fArrayProperty;
    using IUInt64ArrayProperty = Alembic::Abc::IUInt64ArrayProperty;
    using IFloatArrayProperty = Alembic::Abc::IFloatArrayProperty;
    using IBox3dProperty = Alembic::Abc::IBox3dProperty;
    using IArrayProperty = Alembic::Abc::IArrayProperty;
    using IScalarProperty = Alembic::Abc::IScalarProperty;
    using ArraySamplePtr = Alembic::Abc::ArraySamplePtr;
    using UcharArraySamplePtr = Alembic::Abc::UcharArraySamplePtr;
    using Int32ArraySamplePtr = Alembic::Abc::Int32ArraySamplePtr;
    using FloatArraySamplePtr = Alembic::Abc::FloatArraySamplePtr;
    using WrapExistingFlag = Alembic::Abc::WrapExistingFlag;
    using IXform = Alembic::AbcGeom::IXform;
    using IXformSchema = Alembic::AbcGeom::IXformSchema;
    using IPolyMesh = Alembic::AbcGeom::IPolyMesh;
    using ISubD = Alembic::AbcGeom::ISubD;
    using ICurves = Alembic::AbcGeom::ICurves;
    using IPoints = Alembic::AbcGeom::IPoints;
    using INuPatch = Alembic::AbcGeom::INuPatch;
    using ILight = Alembic::AbcGeom::ILight;
    using IFaceSet = Alembic::AbcGeom::IFaceSet;
    using IPolyMeshSchema = Alembic::AbcGeom::IPolyMeshSchema;
    using ISubDSchema = Alembic::AbcGeom::ISubDSchema;
    using ICurvesSchema = Alembic::AbcGeom::ICurvesSchema;
    using IPointsSchema = Alembic::AbcGeom::IPointsSchema;
    using INuPatchSchema = Alembic::AbcGeom::INuPatchSchema;
    using IFaceSetSchema = Alembic::AbcGeom::IFaceSetSchema;
    using ICamera = Alembic::AbcGeom::ICamera;
    using XformSample = Alembic::AbcGeom::XformSample;
    using XformOp = Alembic::AbcGeom::XformOp;
    using XformOperationType = Alembic::AbcGeom::XformOperationType;
    using GeometryScope = Alembic::AbcGeom::GeometryScope;
    using IN3fGeomParam = Alembic::AbcGeom::IN3fGeomParam;
    using IV2fGeomParam = Alembic::AbcGeom::IV2fGeomParam;
    using IFloatGeomParam = Alembic::AbcGeom::IFloatGeomParam;
    using IMaterial = Alembic::AbcMaterial::IMaterial;

    const WrapExistingFlag gabcWrapExisting = Alembic::Abc::kWrapExisting;
    const GeometryScope	gabcVaryingScope = Alembic::AbcGeom::kVaryingScope;
    const GeometryScope	gabcVertexScope = Alembic::AbcGeom::kVertexScope;
    const GeometryScope	gabcFacevaryingScope = Alembic::AbcGeom::kFacevaryingScope;
    const GeometryScope	gabcUniformScope = Alembic::AbcGeom::kUniformScope;
    const GeometryScope	gabcConstantScope = Alembic::AbcGeom::kConstantScope;
    const GeometryScope	gabcUnknownScope = Alembic::AbcGeom::kUnknownScope;
    const GeometryScope	theConstantUnknownScope[2] =
					{ gabcConstantScope, gabcUnknownScope };
    const GeometryScope point_scope[2] = 
                                        { gabcVaryingScope, gabcVertexScope };
    const GeometryScope vertex_scope[3] = 
                                        { gabcVertexScope, gabcVaryingScope, gabcFacevaryingScope };
    const GeometryScope detail_scope[3] = 
                                        { gabcUniformScope, gabcConstantScope, gabcUnknownScope };

    static const fpreal	theDefaultWidth = 0.05;


    static GT_DataArrayHandle
    arrayFromSample(GABC_IArchive &arch,
                const ArraySamplePtr &param,
		int array_extent,
		GT_Type gttype,
		bool is_constant,
		exint expected_size)
    {
	if (!param || (expected_size >= 0 && param->size() < expected_size))
	    return GT_DataArrayHandle();

	return GABC_NAMESPACE::GABCarray(
			GABC_IArray::getSample(arch, param, gttype,
					       array_extent, is_constant));
    }
    static GT_DataArrayHandle
    simpleArrayFromSample(GABC_IArchive &arch, const ArraySamplePtr &param,
	    bool is_constant, exint expected_size = -1)
    {
	return arrayFromSample(arch, param, 1, GT_TYPE_NONE, is_constant,
			       expected_size);
    }

    static bool
    matchScope(GeometryScope needle, const GeometryScope *haystack, int size)
    {
	for (int i = 0; i < size; ++i)
	{
	    if (needle == haystack[i])
		return true;
	}
	return false;
    }

    static GeometryScope
    getArbitraryPropertyScope(const PropertyHeader &header)
    {
	return Alembic::AbcGeom::GetGeometryScope(header.getMetaData());
    }

    static void
    setAttributeData(GT_AttributeList &alist,
            const GABC_IObject &obj,
	    const char *name,
	    const GT_DataArrayHandle &data,
	    bool *filled)
    {
	int idx = alist.getIndex(name);
	if (idx >= 0 && data && data->entries() && !filled[idx])
	{
	    alist.set(idx, data, 0);
	    filled[idx] = true;
	}
	else
	{
	    // Remove the attribute if there are problems setting it's data.
	    if (idx >= 0)
	    {
                UT_ErrorLog::mantraWarningOnce(
                        "Error reading %s attribute data for Alembic object "
                            "%s (%s). Ignoring attribute.",
                        name,
                        obj.archive()->filename().c_str(),
                        obj.getFullName().c_str());
            }
	}
    }

    static void
    markFilled(GT_AttributeList &alist, const char *name, bool *filled)
    {
	int idx = alist.getIndex(name);
	if (idx >= 0)
	    filled[idx] = true;
    }

    template <typename T_SCHEMA>
    static bool
    isHeterogeneousTopology(const T_SCHEMA &ss)
    {
	return ss.getTopologyVariance() == Alembic::AbcGeom::kHeterogeneousTopology;
    }

    template <>
    bool
    isHeterogeneousTopology<IPointsSchema>(const IPointsSchema &ss)
    {
	return !ss.getIdsProperty().isConstant();
    }

    static const fpreal	timeBias = 0.0001;

    template <typename T_SCHEMA>
    static void
    adjustDeformingTimeSample(fpreal &t, const GABC_IObject &obj,
	    const T_SCHEMA &ss)
    {
	// If the mesh has varying topology, we need to choose a single time
	// sample rather than trying to blend between samples.
	if (!isHeterogeneousTopology(ss))
	    return;	// We can blend samples with impunity

	// Now, pick a time, any time.
	exint	 nsamp = SYSmax((exint)ss.getNumSamples(), (exint)1);
	if (nsamp < 2)
	    return;	// Only one sample?

	const TimeSamplingPtr	&itime = ss.getTimeSampling();
	std::pair<index_t, chrono_t> t0 = itime->getFloorIndex(t, nsamp);
	std::pair<index_t, chrono_t> t1 = itime->getCeilIndex(t, nsamp);
	if (t0.first == t1.first)
	    return;		// Same time index

	fpreal best = ((t-t0.second) > (t1.second-t)) ? t1.second : t0.second;
	if (!SYSisEqual(best, t, timeBias))
	{
	    UT_ErrorLog::mantraErrorOnce(
		"Alembic sub-frame interpolation error for dynamic topology %s (%s)",
		obj.archive()->filename().c_str(),
		obj.getFullName().c_str());
	}
	t = best;
    }

    static GT_DataArrayHandle
    getArraySample(GABC_IArchive &arch,
            const IArrayProperty &prop,
	    index_t idx,
	    GT_Type tinfo,
	    exint expected_size)
    {
	GABC_IArray	iarray = GABC_IArray::getSample(arch, prop, idx, tinfo);
	if(expected_size >= 0 && iarray.entries() < expected_size)
	{
	    UT_ASSERT("Unexpected array size");
	    return GT_DataArrayHandle();
	}
	return GABC_NAMESPACE::GABCarray(iarray);
    }

    template <typename ABC_POD, typename GT_POD>
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
	return new GT_DAConstantValue<GT_POD>(1, dest, tsize, tinfo);
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
		return extractScalarProp<bool, uint8>(prop, idx);
	    case Alembic::Abc::kInt8POD:
		return extractScalarProp<int8, int32>(prop, idx);
	    case Alembic::Abc::kUint8POD:
		return extractScalarProp<uint8, uint8>(prop, idx);
	    case Alembic::Abc::kUint16POD:
		return extractScalarProp<uint16, int32>(prop, idx);
	    case Alembic::Abc::kInt16POD:
		return extractScalarProp<int16, int32>(prop, idx);
	    case Alembic::Abc::kUint32POD:
		return extractScalarProp<uint32, int64>(prop, idx);
	    case Alembic::Abc::kInt32POD:
		return extractScalarProp<int32, int32>(prop, idx);
	    case Alembic::Abc::kInt64POD:
		return extractScalarProp<int64, int64>(prop, idx);
	    case Alembic::Abc::kUint64POD:	// Store uint64 in int64 too
		return extractScalarProp<uint64, int64>(prop, idx);
	    case Alembic::Abc::kFloat16POD:
		return extractScalarProp<fpreal16, fpreal16>(prop, idx);
	    case Alembic::Abc::kFloat32POD:
		return extractScalarProp<fpreal32, fpreal32>(prop, idx);
	    case Alembic::Abc::kFloat64POD:
		return extractScalarProp<fpreal64, fpreal64>(prop, idx);
	    case Alembic::Abc::kStringPOD:
		return extractScalarString(prop, idx);

	    case Alembic::Abc::kWstringPOD:
	    case Alembic::Abc::kNumPlainOldDataTypes:
	    case Alembic::Abc::kUnknownPOD:
		break;
	}
	return GT_DataArrayHandle();
    }

    // Decompose a transformation matrix into it's basic elements: translation,
    // rotation, scale, and shear.
    static void
    decomposeXForm(
            const M44d &m,
            V3d &scale,
            V3d &shear,
            Quatd &q,
            V3d &t)
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

    // Create a transformation matrix from the base transformations.
    static M44d
    recomposeXForm(
            const V3d &scale,
            const V3d &shear,
            const Quatd &rotation,
            const V3d &translation)
    {
	M44d	scale_mtx, shear_mtx, rotation_mtx, translation_mtx;

        scale_mtx.setScale(scale);
        shear_mtx.setShear(shear);
        rotation_mtx = rotation.toMatrix44();
        translation_mtx.setTranslation(translation);

        return scale_mtx * shear_mtx * rotation_mtx * translation_mtx;
    }

    // Linear interpolation for vectors using SYSlerp.
    static V3d
    lerp(const Imath::V3d &a, const Imath::V3d &b, double bias)
    {
	return V3d(SYSlerp(a[0], b[0], bias),
		   SYSlerp(a[1], b[1], bias),
		   SYSlerp(a[2], b[2], bias));
    }

    // Blend two matrices together by decomposing them into their basic
    // components, interpolating the components, and then recombining them.
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

    // Blend two Alembic XformSamples together by blending their individual
    // operations, then combining them into the final transform.
    static M44d
    blendSamples(XformSample &s0, XformSample &s1, fpreal bias)
    {
        M44d                ret;
        M44d                m;
        XformOp             op0;
        XformOp             op1;
        XformOperationType  type0;
        XformOperationType  type1;
        int                 size0 = s0.getNumOps();
        int                 size1 = s1.getNumOps();
        bool                valid = false;

        ret.makeIdentity();
        // Check that the number of operations matches between samples.
        if (size0 == size1)
        {
            for (int i = 0; i < size0; ++i)
            {
                op0 = s0[i];
                op1 = s1[i];
                type0 = op0.getType();
                type1 = op1.getType();

                // Check that the type of operation is a match.
                if (type0 != type1)
                {
                    UT_ASSERT(0 && "Alembic XformOp types don't match.");
                    break;
                }

                m.makeIdentity();
                if (type0 == Alembic::AbcGeom::kMatrixOperation)
                {
                    M44d    m0 = op0.getMatrix();
                    M44d    m1 = op1.getMatrix();
                    m = blendMatrix(m0, m1, bias);
                }
                else if (type0 == Alembic::AbcGeom::kRotateXOperation)
                {
                    m.setAxisAngle(V3d(1.0, 0.0, 0.0),
                            SYSlerp(SYSdegToRad(op0.getChannelValue(0)),
                                    SYSdegToRad(op1.getChannelValue(0)),
                                    bias));
                }
                else if (type0 == Alembic::AbcGeom::kRotateYOperation)
                {
                    m.setAxisAngle(V3d(0.0, 1.0, 0.0),
                            SYSlerp(SYSdegToRad(op0.getChannelValue(0)),
                                    SYSdegToRad(op1.getChannelValue(0)),
                                    bias));
                }
                else if (type0 == Alembic::AbcGeom::kRotateZOperation)
                {
                    m.setAxisAngle(V3d(0.0, 0.0, 1.0),
                            SYSlerp(SYSdegToRad(op0.getChannelValue(0)),
                                    SYSdegToRad(op1.getChannelValue(0)),
                                    bias));
                }
                else
                {
		    V3d vec0(op0.getChannelValue(0),
			     op0.getChannelValue(1),
			     op0.getChannelValue(2));
		    V3d vec1(op1.getChannelValue(0),
			     op1.getChannelValue(1),
			     op1.getChannelValue(2));

                    if (type0 == Alembic::AbcGeom::kScaleOperation)
                        m.setScale(lerp(vec0, vec1, bias));
                    else if ( type0 == Alembic::AbcGeom::kTranslateOperation )
                        m.setTranslation(lerp(vec0, vec1, bias));
                    else if ( type0 == Alembic::AbcGeom::kRotateOperation )
                    {
                        // Check that the axes match for rotation about an
                        // arbitrary axis.
                        if (vec0 != vec1)
                        {
                            UT_ASSERT(0 && "Alembic XformOp rotation axis don't"
                                    " match.");
                            break;
                        }

                        m.setAxisAngle(vec0,
                                SYSlerp(SYSdegToRad(op0.getChannelValue(3)),
                                        SYSdegToRad(op1.getChannelValue(3)),
                                        bias));
                    }

                }

                ret = m * ret;
            }

            valid = true;
        }
        else
        {
            UT_ASSERT(0 && "Alembic XformSamples have different # XformOps.");
        }

        // If a problem is detected that stops us from blending samples, fall
        // back to getting the final matrices for both samples and blending
        // them. This should work most of the time, though it can be erroneous
        // for complex transformations.
        if (!valid)
        {
            M44d    m0 = s0.getMatrix();
            M44d    m1 = s1.getMatrix();
            ret = blendMatrix(m0, m1, bias);
        }

        return ret;
    }


    template <typename T>
    static GT_DataArrayHandle
    rationalize(const GT_DataArrayHandle &p, const T *p_data,
                const GT_DataArrayHandle &pw, const T *pw_data)
    {
        GT_Size                     p_entries = p->entries();
        GT_Size                     pw_entries = pw->entries();

        if (p_entries != pw_entries)
        {
            UT_ASSERT(0);
            return p;
        }

        GT_Size                     p_tuple = p->getTupleSize();
        // GT_DANumeric holds own copy of data,
        // frees memory during destruction
        GT_DANumeric<T>  *gtarray = new GT_DANumeric<T> (
                p_entries,
                p_tuple,
                p->getTypeInfo());
        T                           *dest = gtarray->data();
        exint                       pos = 0;

        for (exint i = 0; i < p_entries; ++i)
        {
            T val = pw_data[i];

            for (exint j = 0; j < p_tuple; ++j)
            {
                dest[pos] = p_data[pos] / val;
		++pos;
            }
        }

        return GT_DataArrayHandle(gtarray);
    }

    template <typename T>
    static GT_DataArrayHandle
    blendArrays(const GT_DataArrayHandle &s0, const T *f0,
		const GT_DataArrayHandle &s1, const T *f1,
		fpreal bias)
    {
	GT_DANumeric<T>	*gtarray;
	gtarray = new GT_DANumeric<T>(s0->entries(),
				s0->getTupleSize(), s0->getTypeInfo());
	T	*dest = gtarray->data();
	GT_Size	 fullsize = s0->entries() * s0->getTupleSize();
	for (exint i = 0; i < fullsize; ++i)
	    dest[i] = SYSlerp(f0[i], f1[i], bias);
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
		return blendArrays<fpreal16>(
			s0, s0->getF16Array(buf0),
			s1, s1->getF16Array(buf1), bias);
	    case GT_STORE_REAL32:
		return blendArrays<fpreal32>(
			s0, s0->getF32Array(buf0),
			s1, s1->getF32Array(buf1), bias);
	    case GT_STORE_REAL64:
		return blendArrays<fpreal64>(
			s0, s0->getF64Array(buf0),
			s1, s1->getF64Array(buf1), bias);
	    default:
		UT_ASSERT(0);
	}
	return s0;
    }

    static GT_DataArrayHandle
    readScalarProperty(GABC_IArchive &arch,
            const IScalarProperty &prop,
            fpreal t)
    {
        GT_DataArrayHandle  s0, s1;
	index_t             i0, i1;
	fpreal              bias = GABC_Util::getSampleIndex(t,
	                            prop.getTimeSampling(),
                                    prop.getNumSamples(),
                                    i0,
                                    i1);
	bool                is_const = prop.isConstant();

	s0 = getScalarSample(arch, prop, i0);
	if (is_const || i0 == i1 || !s0 || !GTisFloat(s0->getStorage()))
	    return s0;

	s1 = getScalarSample(arch, prop, i1);
	if (!s1)
	    return s0;

	return blendArrays(s0, s1, bias);
    }

    static GT_DataArrayHandle
    readArrayProperty(GABC_IArchive &arch,
            const IArrayProperty &prop,
            fpreal t, GT_Type tinfo, exint expected_size)
    {
	if(prop.getNumSamples() == 0)
	    return GT_DataArrayHandle();

        GT_DataArrayHandle  s0, s1;
	index_t             i0, i1;
        fpreal              bias = GABC_Util::getSampleIndex(t,
                                    prop.getTimeSampling(),
                                    prop.getNumSamples(),
                                    i0,
                                    i1);
	bool                is_const = prop.isConstant();

	s0 = getArraySample(arch, prop, i0, tinfo, expected_size);
	if (is_const || i0 == i1 || !s0 || !GTisFloat(s0->getStorage()))
	    return s0;

	s1 = getArraySample(arch, prop, i1, tinfo, expected_size);
	if (!s1)
	    return s0;

	return blendArrays(s0, s1, bias);
    }

    template <typename T>
    static GT_DataArrayHandle
    readGeomProperty(GABC_IArchive &arch, const T &gparam, fpreal t,
	    GT_Type tinfo, exint expected_size)
    {
	if (!gparam || !gparam.valid())
	    return GT_DataArrayHandle();

	typename T::sample_type v0, v1;
        GT_DataArrayHandle      s0, s1;
	index_t                 i0, i1;
	fpreal                  bias = GABC_Util::getSampleIndex(t,
	                                gparam.getTimeSampling(),
	                                gparam.getNumSamples(),
	                                i0,
	                                i1);
	bool                    is_const = gparam.isConstant();

	gparam.getExpanded(v0, i0);
	if (!v0.getVals() || !v0.getVals()->size())
	{
	    UT_ASSERT(!v0.getVals()->size() && "This is likely a corrupt indexed alembic array");
	    gparam.getIndexed(v0, i0);
	    return arrayFromSample(arch,
	            v0.getVals(),
                    gparam.getArrayExtent(),
                    tinfo,
                    is_const,
		    expected_size);
	}

	s0 = arrayFromSample(arch,
                v0.getVals(),
                gparam.getArrayExtent(),
                tinfo,
                is_const,
		expected_size);
	if (is_const || i0 == i1 || !s0 || !GTisFloat(s0->getStorage()))
	    return s0;

	gparam.getExpanded(v1, i1);
	s1 = arrayFromSample(arch, v1.getVals(),
                gparam.getArrayExtent(), tinfo,
                is_const, expected_size);
	if (!s1)
	    return s0;

	return blendArrays(s0, s1, bias);
    }

    static GT_DataArrayHandle
    readUVProperty(GABC_IArchive &arch, const IV2fGeomParam &uvs,
		fpreal t, exint expected_size)
    {
	// Alembic stores uv's as a 2-tuple, but Houdini expects a 3-tuple
	GT_DataArrayHandle  uv2 = readGeomProperty(arch, uvs, t,
					GT_TYPE_NONE, expected_size);

	UT_ASSERT(uv2 && uv2->getTupleSize() == 2);

	if (!uv2)
	    return GT_DataArrayHandle();

	if (uv2->getTupleSize() == 3)
	    return uv2;

	GT_Size             n = uv2->entries();
	GT_Real32Array     *uv3 = new GT_Real32Array(n, 3, uv2->getTypeInfo());
	GT_DataArrayHandle  storage;
	const fpreal32     *src = uv2->getF32Array(storage);
	fpreal32           *dest = uv3->data();

	if (uvs.isConstant())
	    uv3->copyDataId(*uv2);  // Copy over data id if constant
	for (exint i = 0; i < n; ++i, src += 2, dest += 3)
	{
	    dest[0] = src[0];
	    dest[1] = src[1];
	    dest[2] = 0;
	}

	return GT_DataArrayHandle(uv3);
    }

    template <typename PRIM_T>
    static GT_DataArrayHandle
    gabcGetPosition(const GABC_IObject &obj, fpreal t, GEO_AnimationType &atype)
    {
	PRIM_T				 prim(obj.object(), gabcWrapExisting);
	typename PRIM_T::schema_type	&ss = prim.getSchema();
	IP3fArrayProperty		 P = ss.getPositionsProperty();
	UT_ASSERT(P.valid());
	atype = P.isConstant() ? GEO_ANIMATION_CONSTANT
				: GEO_ANIMATION_ATTRIBUTE;
	return readArrayProperty(*obj.archive(), P, t, GT_TYPE_POINT, -1);
    }

    template <typename PRIM_T>
    static GT_DataArrayHandle
    gabcGetVelocity(const GABC_IObject &obj, fpreal t)
    {
	PRIM_T				 prim(obj.object(), gabcWrapExisting);
	typename PRIM_T::schema_type	&ss = prim.getSchema();
	IV3fArrayProperty		 v = ss.getVelocitiesProperty();
	if (!v.valid())
	    return GT_DataArrayHandle();
	return readArrayProperty(*obj.archive(), v, t, GT_TYPE_VECTOR, -1);
    }

    template <typename PRIM_T>
    static GT_DataArrayHandle
    gabcGetWidth(const GABC_IObject &obj, fpreal t)
    {
	PRIM_T				 prim(obj.object(), gabcWrapExisting);
	typename PRIM_T::schema_type	&ss = prim.getSchema();
	IFloatGeomParam			 v = ss.getWidthsParam();
	if (!v.valid())
	    return GT_DataArrayHandle();
	return readGeomProperty(*obj.archive(), v, t, GT_TYPE_NONE, -1);
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

    #define SET_ARRAY(VAR, NAME, TYPEINFO, NAMEMAP) \
	if (VAR && *VAR && (!NAMEMAP || NAMEMAP->matchPattern(GA_ATTRIB_POINT, NAME))) { \
	    if (ONLY_ANIMATING && VAR->isConstant()) \
		markFilled(alist, NAME, filled); \
	    else \
		setAttributeData(alist, obj, NAME, \
		    readArrayProperty(arch, *VAR, t, TYPEINFO, expected_size), filled); \
	}
    #define SET_GEOM_PARAM(VAR, NAME, OWNER, TYPEINFO) \
	if (VAR && VAR->valid() && matchScope(VAR->getScope(), \
		    scope, scope_size)) { \
	    if (!namemap || namemap->matchPattern(OWNER, NAME)) \
            { \
		if (ONLY_ANIMATING && VAR->isConstant()) \
		    markFilled(alist, NAME, filled); \
                else \
		    setAttributeData(alist, obj, NAME, \
                            readGeomProperty(arch, *VAR, t, TYPEINFO, expected_size), filled); \
	    } \
	}
    #define SET_GEOM_UVS_PARAM(VAR, NAME, OWNER) \
	if (VAR && VAR->valid() && matchScope(VAR->getScope(), \
		    scope, scope_size)) { \
	    if (!namemap || namemap->matchPattern(OWNER, NAME)) { \
		if (ONLY_ANIMATING && VAR->isConstant()) \
		    markFilled(alist, NAME, filled); \
                else \
		    setAttributeData(alist, obj, NAME, \
		        readUVProperty(arch, *VAR, t, expected_size), filled); \
	    } \
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
		return getGTStorage(parent, *hval);
	}
	UT_ASSERT(0 && "Unhandled property type");
	return GT_STORE_INVALID;
    }

    static inline int
    topologyUID(GT_AttributeList &alist, const GABC_IObject &obj)
    {
	int __topologyIdx = alist.getIndex("__topology");
	if (__topologyIdx < 0)
	    return -1;

	const GT_DataArrayHandle	&__topology = alist.get(__topologyIdx);
	if (!__topology)
	{
	    if (obj.getAnimationType(false) != GEO_ANIMATION_TOPOLOGY)
	    {
		int64	 hash;

		hash = UT_String::hash(obj.objectPath().c_str());
		hash = hash << 32;
		if (obj.archive())
		    hash += UT_String::hash(obj.archive()->filename().c_str());
		alist.set(__topologyIdx, new GT_IntConstant(1, hash));
	    }
	    else
		alist.set(__topologyIdx, new GT_IntConstant(1, -1));
	    
	    return __topologyIdx;
	}
	return -1;
    }

    template <bool ONLY_ANIMATING>
    static void
    fillAttributeList(GT_AttributeList &alist,
	    exint expected_size,
	    const GEO_PackedNameMapPtr &namemap,
	    int load_style,
	    const GEO_Primitive *prim,
	    const GABC_IObject &obj,
	    fpreal t,
	    GA_AttributeOwner owner,
	    const GeometryScope *scope,
	    int scope_size,
	    ICompoundProperty arb,
	    const IP3fArrayProperty *P,
	    const IV3fArrayProperty *v,
	    const IN3fGeomParam *N,
	    const IV2fGeomParam *uvs,
	    const IUInt64ArrayProperty *ids,
	    const IFloatGeomParam *widths,
	    const IFloatArrayProperty *Pw,
	    UT_StringArray &removals)
    {
	UT_ASSERT(alist.entries());
	if (!alist.entries())
	    return;

	GABC_IArchive		&arch = *obj.archive();
	ISampleSelector		sample(t);
	UT_StackBuffer<bool>	filled(alist.entries());
	GT_DataArrayHandle      p_data(0);

	memset(filled, 0, sizeof(bool)*alist.entries());
	int idx = topologyUID(alist, obj);
	if(idx >= 0)
	    filled[idx] = 1;

        if (P && *P && (!GEO_PackedNameMapPtr() || GEO_PackedNameMapPtr()->matchPattern(GA_ATTRIB_POINT, "P")))
	{
            if (ONLY_ANIMATING && P->isConstant())
                markFilled(alist, "P", filled);
            else
                p_data = readArrayProperty(arch, *P, t, GT_TYPE_POINT,
				expected_size);
        }
	if (expected_size < 0)
	{
	    UT_ASSERT(p_data);
	    expected_size = p_data->entries();
	}
        if (Pw && *Pw && (!GEO_PackedNameMapPtr() || GEO_PackedNameMapPtr()->matchPattern(GA_ATTRIB_POINT, "Pw")))
	{
            if (ONLY_ANIMATING && Pw->isConstant())
                markFilled(alist, "Pw", filled);
            else
            {
	        GT_DataArrayHandle  buf_p;
	        GT_DataArrayHandle  buf_pw;
                GT_DataArrayHandle  pw_data = readArrayProperty(arch, *Pw, t, GT_TYPE_NONE, expected_size);

                if (p_data && pw_data)
                {
                    switch (p_data->getStorage())
                    {
                        case GT_STORE_REAL16:
                            p_data = rationalize<fpreal16>(p_data,
                                    p_data->getF16Array(buf_p),
                                    pw_data,
                                    pw_data->getF16Array(buf_pw));
                            break;

                	case GT_STORE_REAL32:
                            p_data = rationalize<fpreal32>(p_data,
                                    p_data->getF32Array(buf_p),
                                    pw_data,
                                    pw_data->getF32Array(buf_pw));
                            break;

                        case GT_STORE_REAL64:
                            p_data = rationalize<fpreal64>(p_data,
                                    p_data->getF64Array(buf_p),
                                    pw_data,
                                    pw_data->getF64Array(buf_pw));
                            break;

                        default:
                            UT_ASSERT(0);
                    }
                }

		if(pw_data)
		    setAttributeData(alist, obj, "Pw", pw_data, filled);
            }
        }
        if (p_data)
            setAttributeData(alist, obj, "P", p_data, filled);

	SET_ARRAY(v, "v", GT_TYPE_VECTOR, namemap)
	SET_ARRAY(ids, "id", GT_TYPE_NONE, namemap)
	SET_GEOM_PARAM(N, "N", owner, GT_TYPE_NORMAL)
	SET_GEOM_UVS_PARAM(uvs, "uv", owner)
	SET_GEOM_PARAM(widths, "width", owner, GT_TYPE_NONE)
	if (prim && matchScope(gabcConstantScope, scope, scope_size))
	{
	    setAttributeData(alist, obj, "__primitive_id",
		    new GT_IntConstant(1, prim->getMapOffset()), filled);
	}
	if (arb)
	{
	    for (size_t i = 0; i < arb.getNumProperties(); ++i)
	    {
		const PropertyHeader	&header = arb.getPropertyHeader(i);
		GeometryScope		arbscope = getArbitraryPropertyScope(header);
		if (!matchScope(arbscope, scope, scope_size))
		    continue;

		const char	*name = header.getName().c_str();
		if (namemap)
		{
		    // Verify the attribute matches the pattern
		    if (!namemap->matchPattern(owner, name))
			continue;

		    name = namemap->getName(name);
		    if (!name)
			continue;
		}
		GT_Storage	store = getGTStorage(arb, header);
		if (store == GT_STORE_INVALID)
		    continue;

		GT_DataArrayHandle data = obj.convertIProperty(arb, header, t, namemap, 0, expected_size);
		if(data)
		    setAttributeData(alist, obj, name, data, filled);
	    }
	}
	// We need to fill Houdini attributes last.  Otherwise, when converting
	// two primitives, the Houdini attributes override the first primitive
	// converted.
	if (prim
		&& (load_style & GABC_IObject::GABC_LOAD_HOUDINI)
		&& matchScope(gabcConstantScope, scope, scope_size))
	{
	    fillHoudiniAttributes(alist, *prim, GA_ATTRIB_PRIMITIVE, filled);
	    fillHoudiniAttributes(alist, *prim, GA_ATTRIB_GLOBAL, filled);
	}
	for (exint i = 0; i < alist.entries(); ++i)
	{
	    if (!filled[i])
		removals.append(alist.getName(i));
	}
    }


    template <typename T>
    static void
    addPropertyToMap(GT_AttributeMap &map, const char *name, const T *prop,
		const GEO_PackedNameMapPtr &namemap)
    {
	if (prop && *prop)
	{
	    if (!namemap || namemap->matchPattern(GA_ATTRIB_POINT, name))
		map.add(name, true);
	}
    }

    template <typename T>
    static void
    addGeomParamToMap(GT_AttributeMap &map, const char *name, T *param,
	    GA_AttributeOwner owner, const GeometryScope *scope, int scope_size,
	    const GEO_PackedNameMapPtr &namemap)
    {
	if (param && param->valid()
		&& matchScope(param->getScope(), scope, scope_size))
	{
	    if (!namemap || namemap->matchPattern(owner, name))
		map.add(name, false);
	}
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
			exint expected_size,
			const GABC_IObject &obj,
			const GEO_PackedNameMapPtr &namemap,
			int load_style,
			fpreal t,
			GA_AttributeOwner owner,
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
	addPropertyToMap(*map, "P", P, GEO_PackedNameMapPtr());
	addPropertyToMap(*map, "Pw", Pw, GEO_PackedNameMapPtr());
	addPropertyToMap(*map, "v", v, namemap);
	addPropertyToMap(*map, "id", ids, namemap);
	addGeomParamToMap(*map, "N", N, owner, scope, scope_size, namemap);
	addGeomParamToMap(*map, "uv", uvs, owner, scope, scope_size, namemap);
	addGeomParamToMap(*map, "width", widths, owner, scope, scope_size, namemap);

	if (prim && matchScope(gabcConstantScope, scope, scope_size))
	    map->add("__primitive_id", true);
	if (arb)
	{
	    for (exint i = 0; i < arb.getNumProperties(); ++i)
	    {
		const PropertyHeader	&header = arb.getPropertyHeader(i);
		GeometryScope		 arbscope = getArbitraryPropertyScope(header);
		if (!matchScope(arbscope, scope, scope_size))
		    continue;

		const char	*name = header.getName().c_str();
		if (namemap)
		{
		    if (!namemap->matchPattern(owner, name))
			continue;

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

	if (prim
		&& (load_style & GABC_IObject::GABC_LOAD_HOUDINI)
		&& matchScope(gabcConstantScope, scope, scope_size))
	{
	    initializeHoudiniAttributes(*prim, *map, GA_ATTRIB_PRIMITIVE);
	    initializeHoudiniAttributes(*prim, *map, GA_ATTRIB_GLOBAL);
	}

	if (!map->entries())
	{
	    delete map;
	    return GT_AttributeListHandle();
	}

	GT_AttributeListHandle	alist(new GT_AttributeList(map));
	UT_StringArray		removals;
	if (alist->entries())
	{
	    fillAttributeList<false>(*alist, expected_size, namemap, load_style,
			prim, obj, t, owner, scope, scope_size,
			arb, P, v, N, uvs, ids, widths, Pw,
			removals);
	}
	return alist->removeAttributes(removals);
    }

    static UT_StringHolder
    getAttributeNames(const GEO_PackedNameMapPtr &namemap,
			GA_AttributeOwner owner,
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
        UT_String names;
        GT_AttributeMap map;

        addPropertyToMap(map, "P", P, GEO_PackedNameMapPtr());
        addPropertyToMap(map, "Pw", Pw, GEO_PackedNameMapPtr());
        addPropertyToMap(map, "v", v, namemap);
        addPropertyToMap(map, "id", ids, namemap);
        addGeomParamToMap(map, "N", N, owner, scope, scope_size, namemap);
        addGeomParamToMap(map, "uv", uvs, owner, scope, scope_size, namemap);
        addGeomParamToMap(map, "width", widths, owner, scope, scope_size, namemap);

        if (arb)
        {
            for (exint i = 0; i < arb.getNumProperties(); ++i)
            {
                const PropertyHeader    &header = arb.getPropertyHeader(i);
                GeometryScope            arbscope = getArbitraryPropertyScope(header);
                if (!matchScope(arbscope, scope, scope_size))
                    continue;

                const char      *name = header.getName().c_str();
                if (namemap)
                {
                    if (!namemap->matchPattern(owner, name))
                        continue;

                    name = namemap->getName(name);
                    if (!name)
                        continue;
                }
                GT_Storage      store = getGTStorage(arb, header);
                if (store == GT_STORE_INVALID)
                    continue;

                map.add(name, false);
            }
        }

        UT_StringArray array = map.getNames();
        array.sort();
        array.join(" ", names);

        return names;
    }

    static GT_AttributeListHandle
    updateAttributeList(const GT_AttributeListHandle &src,
			const GEO_Primitive *prim,
			exint expected_size,
			const GABC_IObject &obj,
			const GEO_PackedNameMapPtr &namemap,
			int load_style,
			fpreal t,
			GA_AttributeOwner owner,
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
	GT_AttributeListHandle	alist(new GT_AttributeList(*src));
	UT_StringArray		removals;
	// Only fill animating attributes
	fillAttributeList<true>(*alist, expected_size, namemap, load_style,
			prim, obj, t, owner, scope, scope_size,
			arb, P, v, N, uvs, ids, widths, Pw,
			removals);
	return alist->removeAttributes(removals);
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
			    exint expected_size,
			    const GEO_Primitive *prim,
			    const GABC_IObject &obj,
			    const GEO_PackedNameMapPtr &namemap,
			    int load_style,
			    fpreal t,
			    GA_AttributeOwner owner,
			    const GeometryScope *scope,
			    int scope_size,
			    const ICompoundProperty &arb,
			    const IP3fArrayProperty *P = NULL,
			    const IV3fArrayProperty *v = NULL,
			    const IN3fGeomParam *N = NULL,
			    const IV2fGeomParam *uvs = NULL,
			    const IUInt64ArrayProperty *ids = NULL,
			    const IFloatGeomParam *widths = NULL,
			    const IFloatArrayProperty *Pw = NULL) const
	{
	    return initializeAttributeList(prim, expected_size,
		    obj, namemap, load_style, t,
		    owner, scope, scope_size, arb, owner == GA_ATTRIB_DETAIL,
		    P, v, N, uvs, ids, widths, Pw);
	}
	inline GT_AttributeListHandle	build(
			    exint expected_size,
			    const GEO_Primitive *prim,
			    const GABC_IObject &obj,
			    const GEO_PackedNameMapPtr &namemap,
			    int load_style,
			    fpreal t,
			    GA_AttributeOwner owner,
			    GeometryScope scope,
			    const ICompoundProperty &arb,
			    const IP3fArrayProperty *P = NULL,
			    const IV3fArrayProperty *v = NULL,
			    const IN3fGeomParam *N = NULL,
			    const IV2fGeomParam *uvs = NULL,
			    const IUInt64ArrayProperty *ids = NULL,
			    const IFloatGeomParam *widths = NULL,
			    const IFloatArrayProperty *Pw = NULL) const
	{
	    return initializeAttributeList(prim, expected_size,
		    obj, namemap, load_style, t,
		    owner, &scope, 1, arb, owner == GA_ATTRIB_DETAIL,
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
	inline const GT_AttributeListHandle &getSource(GA_AttributeOwner owner) const
	{
	    switch (owner)
	    {
		case GA_ATTRIB_VERTEX:
		    return myPrim->getVertexAttributes();
		case GA_ATTRIB_POINT:
		    return myPrim->getPointAttributes();
		case GA_ATTRIB_PRIMITIVE:
		    return myPrim->getUniformAttributes();
		case GA_ATTRIB_DETAIL:
		    return myPrim->getDetailAttributes();
		default:
		    UT_ASSERT(0);
	    }
	    return myPrim->getPointAttributes();
	}
	inline GT_AttributeListHandle	build(
			    exint expected_size,
			    const GEO_Primitive *prim,
			    const GABC_IObject &obj,
			    const GEO_PackedNameMapPtr &namemap,
			    int load_style,
			    fpreal t,
			    GA_AttributeOwner owner,
			    const GeometryScope *scope,
			    int scope_size,
			    const ICompoundProperty &arb,
			    const IP3fArrayProperty *P = NULL,
			    const IV3fArrayProperty *v = NULL,
			    const IN3fGeomParam *N = NULL,
			    const IV2fGeomParam *uvs = NULL,
			    const IUInt64ArrayProperty *ids = NULL,
			    const IFloatGeomParam *widths = NULL,
			    const IFloatArrayProperty *Pw = NULL) const
	{
	    return updateAttributeList(getSource(owner), prim, expected_size,
		    obj, namemap, load_style, t, owner, scope, scope_size,
		    arb, P, v, N, uvs, ids, widths, Pw);
	}
	inline GT_AttributeListHandle	build(
			    exint expected_size,
			    const GEO_Primitive *prim,
			    const GABC_IObject &obj,
			    const GEO_PackedNameMapPtr &namemap,
			    int load_style,
			    fpreal t,
			    GA_AttributeOwner owner,
			    GeometryScope scope,
			    const ICompoundProperty &arb,
			    const IP3fArrayProperty *P = NULL,
			    const IV3fArrayProperty *v = NULL,
			    const IN3fGeomParam *N = NULL,
			    const IV2fGeomParam *uvs = NULL,
			    const IUInt64ArrayProperty *ids = NULL,
			    const IFloatGeomParam *widths = NULL,
			    const IFloatArrayProperty *Pw = NULL) const
	{
	    return updateAttributeList(getSource(owner), prim, expected_size,
		    obj, namemap, load_style, t, owner, &scope, 1,
		    arb, P, v, N, uvs, ids, widths, Pw);
	}
	const GT_PrimitiveHandle	&myPrim;
    };

    static GT_AttributeListHandle
    addFileObjectAttribs(const GT_AttributeListHandle &list,
			 const GABC_IObject &obj,
			 fpreal t)
    {
	auto file_da = new GT_DAIndexedString(1);
	auto obj_da = new GT_DAIndexedString(1);
	auto time_da = new GT_DANumeric<fpreal32>(1,1);
	file_da->setString(0,0, obj.archive()->filename().c_str());
	obj_da->setString(0,0, obj.getFullName().c_str());
	time_da->set(t, 0);

	GT_AttributeListHandle rlist;
	if(list)
	{
	    rlist = list->addAttribute("__filename",
					  GT_DataArrayHandle(file_da),  true);
	    rlist = rlist->addAttribute("__object_name",
					  GT_DataArrayHandle(obj_da),  true);
	    rlist = rlist->addAttribute("__time",
					  GT_DataArrayHandle(time_da), true);
	}
	else
	{
	    rlist = GT_AttributeList::createAttributeList(
		"__filename",	 file_da,
		"__object_name", obj_da,
		"__time",	 time_da,
		nullptr);
	}
	return rlist;
    }

    static bool
    loadFaceSet(GT_FaceSetPtr &set, const GABC_IObject &obj,
	    const ISampleSelector &iss)
    {
	IFaceSet		 shape(obj.object(), gabcWrapExisting);
	const IFaceSetSchema	&ss = shape.getSchema();
	IFaceSetSchema::Sample	 sample = ss.getValue(iss);
	Int32ArraySamplePtr	 faces = sample.getFaces();

	if (faces && faces->valid())
	{
	    set = new GT_FaceSet();
	    set->addFaces(faces->get(), faces->size());
	    return true;
	}
	return false;
    }

    template <typename GT_PRIMTYPE>
    static void
    loadFaceSets(GT_PRIMTYPE &pmesh, const GABC_IObject &obj, const UT_StringHolder &attrib, fpreal t)
    {
	exint		nkids = obj.getNumChildren();
	GT_FaceSetPtr	set;
	ISampleSelector iss(t);
	for (exint i = 0; i < nkids; ++i)
	{
	    const GABC_IObject	&kid = obj.getChild(i);
	    if (kid.valid() && kid.nodeType() == GABC_FACESET)
	    {
                UT_String name(kid.getName());
		if (name.multiMatch(attrib) && loadFaceSet(set, kid, iss))
		    pmesh.addFaceSet(name, set);
	    }
	}
    }

    static UT_StringHolder
    getFaceSetNames(const GABC_IObject &obj, const UT_StringHolder &attrib, fpreal t)
    {
        exint nkids = obj.getNumChildren();
        ISampleSelector iss(t);

        UT_StringArray array;
        for (exint i = 0; i < nkids; ++i)
        {
            const GABC_IObject &kid = obj.getChild(i);
            if (kid.valid() && kid.nodeType() == GABC_FACESET)
            {
                UT_String name(kid.getName());
                if (name.multiMatch(attrib))
                {
                    IFaceSet                shape(kid.object(), gabcWrapExisting);
                    const IFaceSetSchema   &ss = shape.getSchema();
                    IFaceSetSchema::Sample  sample = ss.getValue(iss);
                    Int32ArraySamplePtr     faces = sample.getFaces();

                    if (faces && faces->valid())
                        array.append(name);
                }
            }
        }

        UT_String names;
        array.sort();
        array.join(" ", names);

        return names;
    }

    template <typename ATTRIB_CREATOR>
    static GT_PrimitiveHandle
    buildPointMesh(const ATTRIB_CREATOR &acreate,
	    const GEO_Primitive *prim,
	    const GABC_IObject &obj,
	    fpreal t,
	    const GEO_PackedNameMapPtr &namemap,
	    int load_style)
    {
	IPoints			 shape(obj.object(), gabcWrapExisting);
	IPointsSchema		&ss = shape.getSchema();
	adjustDeformingTimeSample(t, obj, ss);
	IPointsSchema::Sample	 sample = ss.getValue(ISampleSelector(t));

	GT_AttributeListHandle	 vertex;
	GT_AttributeListHandle	 detail;
	ICompoundProperty	 arb;
	IP3fArrayProperty	 P = ss.getPositionsProperty();
	IV3fArrayProperty	 v = ss.getVelocitiesProperty();
	IUInt64ArrayProperty	 ids = ss.getIdsProperty();
	IFloatGeomParam		 widths = ss.getWidthsParam();

	if (load_style & GABC_IObject::GABC_LOAD_ARBS)
	    arb = ss.getArbGeomParams();

	vertex = acreate.build(-1, prim, obj, namemap,
		load_style, t, GA_ATTRIB_POINT, vertex_scope, 3, arb,
		&P, &v, NULL, NULL, &ids, &widths);
	detail = acreate.build(1, prim, obj, namemap,
		load_style, t, GA_ATTRIB_DETAIL, detail_scope, 3, arb);

	GT_Primitive	*gt = new GT_PrimPointMesh(vertex, detail);
	return GT_PrimitiveHandle(gt);
    }

    static UT_StringHolder
    getPointMeshAttribs(const GABC_IObject &obj,
                        const GEO_PackedNameMapPtr &namemap,
                        int load_style,
                        GT_Owner owner)
    {
        UT_String emptyString;

        IPoints                  shape(obj.object(), gabcWrapExisting);
        IPointsSchema           &ss = shape.getSchema();
        ICompoundProperty        arb;
        if (load_style & GABC_IObject::GABC_LOAD_ARBS)
            arb = ss.getArbGeomParams();

        IP3fArrayProperty        P = ss.getPositionsProperty();
        IV3fArrayProperty        v = ss.getVelocitiesProperty();
        IUInt64ArrayProperty     ids = ss.getIdsProperty();
        IFloatGeomParam          widths = ss.getWidthsParam();

        switch (owner)
        {
            case GT_OWNER_VERTEX:
                return emptyString;
            case GT_OWNER_POINT:
                return getAttributeNames(namemap, GA_ATTRIB_POINT,
					 vertex_scope, 3, arb, &P, &v, nullptr,
					 nullptr, &ids, &widths);
            case GT_OWNER_PRIMITIVE:
                return emptyString;
            case GT_OWNER_DETAIL:
                return getAttributeNames(namemap, GA_ATTRIB_DETAIL,
					 detail_scope, 3, arb);
            case GT_OWNER_MAX:
            case GT_OWNER_INVALID:
                UT_ASSERT(0 && "Unexpected attribute type");
                break;
        }

        return emptyString;
    }

    static exint
    maxArrayValue(const GT_DataArrayHandle &array)
    {
	if (array->entries() == 0)
	    return 0;

	exint	lo, hi;
	array->getRange(lo, hi);
	return hi;
    }

    template <typename ATTRIB_CREATOR>
    static GT_PrimitiveHandle
    buildSubDMesh(const ATTRIB_CREATOR &acreate,
			const GEO_Primitive *prim,
			const GABC_IObject &obj,
			fpreal t,
			const GEO_PackedNameMapPtr &namemap,
                        const UT_StringHolder &facesetAttrib,
			int load_style)
    {
	ISubD			 shape(obj.object(), gabcWrapExisting);
	ISubDSchema		&ss = shape.getSchema();
	adjustDeformingTimeSample(t, obj, ss);
	ISubDSchema::Sample	 sample = ss.getValue(ISampleSelector(t));
	GT_DataArrayHandle	 counts;
	GT_DataArrayHandle	 indices;
	ICompoundProperty	 arb;
	if (load_style & GABC_IObject::GABC_LOAD_ARBS)
	    arb = ss.getArbGeomParams();

	counts = simpleArrayFromSample(*obj.archive(), sample.getFaceCounts(),
			ss.getFaceCountsProperty().isConstant());
	if (!counts || !counts->entries())
	    return GT_PrimitiveHandle();

	indices = simpleArrayFromSample(*obj.archive(), sample.getFaceIndices(),
			ss.getFaceIndicesProperty().isConstant());
	UT_ASSERT(indices);

	GT_AttributeListHandle	 point;
	GT_AttributeListHandle	 vertex;
	GT_AttributeListHandle	 uniform;
	GT_AttributeListHandle	 detail;
	IP3fArrayProperty	 P = ss.getPositionsProperty();
	IV3fArrayProperty	 v = ss.getVelocitiesProperty();
	const IV2fGeomParam	&uvs = ss.getUVsParam();

	point = acreate.build(maxArrayValue(indices), prim, obj, namemap,
			      load_style, t, GA_ATTRIB_POINT, point_scope, 2,
			      arb, &P, &v, nullptr, &uvs);
	vertex = acreate.build(indices->entries(), prim, obj, namemap,
			       load_style, t, GA_ATTRIB_VERTEX,
			       gabcFacevaryingScope, arb, nullptr, nullptr,
			       nullptr, &uvs);
	uniform = acreate.build(counts->entries(), prim, obj, namemap,
				load_style, t, GA_ATTRIB_PRIMITIVE,
				gabcUniformScope, arb);
	detail = acreate.build(1, prim, obj, namemap, load_style, t,
			       GA_ATTRIB_DETAIL, theConstantUnknownScope, 2,
				arb);

	detail = addFileObjectAttribs(detail, obj, t);

	GT_PrimSubdivisionMesh	*gt = new GT_PrimSubdivisionMesh(counts,
					indices,
					point,
					vertex,
					uniform,
					detail);

	/// Add tags
	Int32ArraySamplePtr	creaseIndices = sample.getCreaseIndices();
	FloatArraySamplePtr	creaseSharpness = sample.getCreaseSharpnesses();
	Int32ArraySamplePtr	cornerIndices = sample.getCornerIndices();
	FloatArraySamplePtr	cornerSharpness = sample.getCornerSharpnesses();
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
	if ((ival = sample.getInterpolateBoundary()))
	{
	    GT_IntConstant	*val = new GT_IntConstant(1, ival);
	    gt->appendIntTag("interpolateboundary", GT_DataArrayHandle(val));
	}
	if ((ival = sample.getFaceVaryingInterpolateBoundary()))
	{
	    GT_IntConstant	*val = new GT_IntConstant(1, ival);
	    gt->appendIntTag("facevaryinginterpolateboundary",
		    GT_DataArrayHandle(val));
	}
	if ((ival = sample.getFaceVaryingPropagateCorners()))
	{
	    GT_IntConstant	*val = new GT_IntConstant(1, ival);
	    gt->appendIntTag("facevaryingpropagatecorners",
		    GT_DataArrayHandle(val));
	}

	if (load_style & GABC_IObject::GABC_LOAD_FACESETS)
	    loadFaceSets(*gt, obj, facesetAttrib, t);

	return GT_PrimitiveHandle(gt);
    }

    static UT_StringHolder
    getSubDMeshAttribs(const GABC_IObject &obj,
                        const GEO_PackedNameMapPtr &namemap,
                        int load_style,
                        GT_Owner owner)
    {
        UT_String emptyString;

        ISubD                    shape(obj.object(), gabcWrapExisting);
        ISubDSchema             &ss = shape.getSchema();
        ICompoundProperty        arb;
        if (load_style & GABC_IObject::GABC_LOAD_ARBS)
            arb = ss.getArbGeomParams();

        IP3fArrayProperty        P = ss.getPositionsProperty();
        IV3fArrayProperty        v = ss.getVelocitiesProperty();
        const IV2fGeomParam     &uvs = ss.getUVsParam();

        switch (owner)
        {
            case GT_OWNER_POINT:
                return getAttributeNames(namemap, GA_ATTRIB_POINT, point_scope,
					 2, arb, &P, &v, nullptr, &uvs);
            case GT_OWNER_VERTEX:
                return getAttributeNames(namemap, GA_ATTRIB_VERTEX,
					 &gabcFacevaryingScope, 1, arb, nullptr,
					 nullptr, nullptr, &uvs);
            case GT_OWNER_PRIMITIVE:
                return getAttributeNames(namemap, GA_ATTRIB_PRIMITIVE,
					 &gabcUniformScope, 1, arb);
            case GT_OWNER_DETAIL:
                return getAttributeNames(namemap, GA_ATTRIB_DETAIL,
					 theConstantUnknownScope, 2, arb);
            case GT_OWNER_MAX:
            case GT_OWNER_INVALID:
                UT_ASSERT(0 && "Unexpected attribute type");
                break;
        }

        return emptyString;
    }

    template <typename ATTRIB_CREATOR>
    static GT_PrimitiveHandle
    buildPolyMesh(const ATTRIB_CREATOR &acreate,
			const GEO_Primitive *prim,
			const GABC_IObject &obj,
			fpreal t,
			const GEO_PackedNameMapPtr &namemap,
                        const UT_StringHolder &facesetAttrib,
			int load_style)
    {
	IPolyMesh		 shape(obj.object(), gabcWrapExisting);
	IPolyMeshSchema		&ss = shape.getSchema();
	adjustDeformingTimeSample(t, obj, ss);
	IPolyMeshSchema::Sample	 sample = ss.getValue(ISampleSelector(t));
	GT_DataArrayHandle	 counts;
	GT_DataArrayHandle	 indices;
	ICompoundProperty	 arb;
	if (load_style & GABC_IObject::GABC_LOAD_ARBS)
	    arb = ss.getArbGeomParams();

	counts = simpleArrayFromSample(*obj.archive(), sample.getFaceCounts(),
			ss.getFaceCountsProperty().isConstant());
	if (!counts || !counts->entries())
	    return GT_PrimitiveHandle();

	indices = simpleArrayFromSample(*obj.archive(), sample.getFaceIndices(),
			ss.getFaceIndicesProperty().isConstant());
	UT_ASSERT(indices);

	GT_AttributeListHandle	 point;
	GT_AttributeListHandle	 vertex;
	GT_AttributeListHandle	 uniform;
	GT_AttributeListHandle	 detail;
	IP3fArrayProperty	 P = ss.getPositionsProperty();
	IV3fArrayProperty	 v = ss.getVelocitiesProperty();
	const IN3fGeomParam	&N = ss.getNormalsParam();
	const IV2fGeomParam	&uvs = ss.getUVsParam();

	point = acreate.build(maxArrayValue(indices), prim, obj, namemap,
			      load_style, t, GA_ATTRIB_POINT, point_scope, 2,
			      arb, &P, &v, &N, &uvs);
	vertex = acreate.build(indices->entries(), prim, obj, namemap,
			       load_style, t, GA_ATTRIB_VERTEX,
			       gabcFacevaryingScope, arb, nullptr, nullptr, &N,
			       &uvs);
	uniform = acreate.build(counts->entries(),
				prim, obj, namemap,
				load_style, t, GA_ATTRIB_PRIMITIVE,
				gabcUniformScope, arb);
	detail = acreate.build(1, prim, obj, namemap,
				load_style, t,
				GA_ATTRIB_DETAIL, theConstantUnknownScope, 2,
				arb);

	detail = addFileObjectAttribs(detail, obj, t);

	GT_PrimPolygonMesh	*gt = new GT_PrimPolygonMesh(counts,
					indices,
					point,
					vertex,
					uniform,
					detail);

	if (load_style & GABC_IObject::GABC_LOAD_FACESETS)
	    loadFaceSets(*gt, obj, facesetAttrib, t);

	return GT_PrimitiveHandle(gt);
    }

    static UT_StringHolder
    getPolyMeshAttribs(const GABC_IObject &obj,
                        const GEO_PackedNameMapPtr &namemap,
                        int load_style,
                        GT_Owner owner)
    {
        UT_String emptyString;

        IPolyMesh                shape(obj.object(), gabcWrapExisting);
        IPolyMeshSchema         &ss = shape.getSchema();
        ICompoundProperty        arb;
        if (load_style & GABC_IObject::GABC_LOAD_ARBS)
            arb = ss.getArbGeomParams();

        IP3fArrayProperty        P = ss.getPositionsProperty();
        IV3fArrayProperty        v = ss.getVelocitiesProperty();
        const IN3fGeomParam     &N = ss.getNormalsParam();
        const IV2fGeomParam     &uvs = ss.getUVsParam();

        switch (owner)
        {
            case GT_OWNER_POINT:
                return getAttributeNames(namemap, GA_ATTRIB_POINT, point_scope,
					 2, arb, &P, &v, &N, &uvs);
            case GT_OWNER_VERTEX:
                return getAttributeNames(namemap, GA_ATTRIB_VERTEX,
					 &gabcFacevaryingScope, 1, arb, nullptr,
					 nullptr, &N, &uvs);
            case GT_OWNER_PRIMITIVE:
                return getAttributeNames(namemap, GA_ATTRIB_PRIMITIVE,
					 &gabcUniformScope, 1, arb);
            case GT_OWNER_DETAIL:
                return getAttributeNames(namemap, GA_ATTRIB_DETAIL,
					 theConstantUnknownScope, 2, arb);
            case GT_OWNER_MAX:
            case GT_OWNER_INVALID:
                UT_ASSERT(0 && "Unexpected attribute type");
                break;
        }

        return emptyString;
    }

    static bool
    validCounts(const GT_DataArrayHandle &counts, int order,
		const GT_CurveEval &eval)
    {
	exint	n = counts->entries();
	for (int i = 0; i < n; ++i)
	{
	    if (!eval.validCount(counts->getI32(i), order))
		return false;
	}
	return true;
    }

    static bool
    validCounts(const GT_DataArrayHandle &counts,
	    const GT_DataArrayHandle &order,
	    const GT_CurveEval &eval)
    {
	exint	n = counts->entries();
	if (n != order->entries())
	    return false;

	for (exint i = 0; i < n; ++i)
	{
	    if (!eval.validCount(counts->getI32(i), order->getI32(i)))
		return false;
	}
	return true;
    }

    static bool
    validCurveCounts(const GT_DataArrayHandle &counts,
	    const GT_AttributeListHandle &vertex)
    {
	if(vertex)
	{
	    exint nvtx = 0;
	    exint n = counts->entries();
	    for(exint i = 0; i < n; ++i)
		nvtx += counts->getI32(i);

	    n = vertex->entries();
	    for(exint i = 0; i < n; ++i)
	    {
		if(vertex->get(i)->entries() < nvtx)
		    return false;
	    }
	}
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
	UT_ErrorLog::mantraWarningOnce(
		    "Alembic file %s (%s) has invalid %s curves - converting "
		        "to linear",
		    obj.archive()->filename().c_str(),
		    obj.getFullName().c_str(),
		    basis);
    }

    static void
    orderWarning(const GABC_IObject &obj, const char *msg)
    {
	UT_ErrorLog::mantraWarningOnce(
		    "Alembic file %s (%s): %s converting to linear",
		    obj.archive()->filename().c_str(),
		    obj.getFullName().c_str(),
		    msg);
    }

    static void
    knotWarning(const GABC_IObject &obj, const char *msg)
    {
	UT_ErrorLog::mantraWarningOnce(
		    "Alembic file %s (%s): %s - Ignoring knot vectors",
		    obj.archive()->filename().c_str(),
		    obj.getFullName().c_str(),
		    msg);
    }

    static void
    curveWarning(const GABC_IObject &obj, const char *msg)
    {
	UT_ErrorLog::mantraWarningOnce(
		    "Alembic file %s (%s): %s - Ignoring corrupt curves",
		    obj.archive()->filename().c_str(),
		    obj.getFullName().c_str(),
		    msg);
    }

    template <typename ATTRIB_CREATOR>
    static GT_PrimitiveHandle
    buildCurveMesh(const ATTRIB_CREATOR &acreate, const GEO_Primitive *prim,
			const GABC_IObject &obj,
			fpreal t,
			const GEO_PackedNameMapPtr &namemap,
                        const UT_StringHolder &facesetAttrib,
			int load_style)
    {
	ICurves			 shape(obj.object(), gabcWrapExisting);
	ICurvesSchema		&ss = shape.getSchema();
	adjustDeformingTimeSample(t, obj, ss);
	ICurvesSchema::Sample	 sample = ss.getValue(ISampleSelector(t));
	GT_DataArrayHandle	 counts;
	ICompoundProperty	 arb;
	if (load_style & GABC_IObject::GABC_LOAD_ARBS)
	    arb = ss.getArbGeomParams();

	counts = simpleArrayFromSample(*obj.archive(),
			sample.getCurvesNumVertices(),
			ss.getNumVerticesProperty().isConstant());
	UT_ASSERT(counts);

	GT_AttributeListHandle	 vertex;
	GT_AttributeListHandle	 uniform;
	GT_AttributeListHandle	 detail;
	IP3fArrayProperty	 P = ss.getPositionsProperty();
	IFloatArrayProperty	 Pw = ss.getPositionWeightsProperty();
	IV3fArrayProperty	 v = ss.getVelocitiesProperty();
	const IN3fGeomParam	&N = ss.getNormalsParam();
	const IV2fGeomParam	&uvs = ss.getUVsParam();
	IFloatGeomParam		 widths = ss.getWidthsParam();

	vertex = acreate.build(-1, prim, obj, namemap, load_style, t,
			       GA_ATTRIB_VERTEX, vertex_scope, 3, arb, &P, &v,
			       &N, &uvs, nullptr, &widths, &Pw);

	// guard against corrupt curves
	if(!validCurveCounts(counts, vertex))
	{
	    curveWarning(obj, "Curves have missing vertices");
	    return GT_PrimitiveHandle();
	}

	uniform = acreate.build(counts->entries(), prim, obj, namemap,
				load_style, t, GA_ATTRIB_PRIMITIVE,
				gabcUniformScope, arb);
	detail = acreate.build(1, prim, obj, namemap, load_style, t,
			       GA_ATTRIB_DETAIL, theConstantUnknownScope, 2,
			       arb);
	
	detail = addFileObjectAttribs(detail, obj, t);

	GT_Basis basis = getGTBasis(sample.getBasis());
	bool	 periodic = (sample.getWrap() == Alembic::AbcGeom::kPeriodic);
	int	 uorder = -1;
	GT_DataArrayHandle	order;
	GT_DataArrayHandle	knots;
	switch (sample.getType())
	{
	    case Alembic::AbcGeom::kCubic:
		uorder = 4;
		break;
	    case Alembic::AbcGeom::kLinear:
		uorder = 2;
		break;
	    case Alembic::AbcGeom::kVariableOrder:
	    {
		UcharArraySamplePtr	abcOrders = sample.getOrders();
		if (isEmpty(abcOrders))
		{
		    orderWarning(obj, "Curves have missing varying orders");
		    uorder = 2;
		    basis = GT_BASIS_LINEAR;
		}
		else
		{
		    order = new GT_UInt8Array(abcOrders->get(),
						abcOrders->size(), 1);
		}
		break;
	    }
	}

	if (uorder > 0)
	{
	    if (!validCounts(counts, uorder, GT_CurveEval(basis, periodic)))
	    {
		basisWarning(obj, GTbasis(basis));
		basis = GT_BASIS_LINEAR;
	    }
	}
	else
	{
	    if (!validCounts(counts, order, GT_CurveEval(basis, periodic)))
	    {
		basisWarning(obj, GTbasis(basis));
		basis = GT_BASIS_LINEAR;
		uorder = 2;
		order = NULL;
	    }
	}

	GT_PrimCurveMesh	*gt = new GT_PrimCurveMesh(basis,
					counts,
					vertex,
					uniform,
					detail,
					periodic);
	if (uorder > 0)
	    gt->setOrder(uorder);
	else
	{
	    UT_ASSERT(order);
	    gt->setOrder(order);
	}
	const FloatArraySamplePtr	&abcknots = sample.getKnots();
	if (!isEmpty(abcknots))
	{
	    if (!gt->setKnots(new GT_Real32Array(abcknots->get(),
						abcknots->size(), 1)))
	    {
		knotWarning(obj, "Invalid knot vector");
	    }
	}

	if (load_style & GABC_IObject::GABC_LOAD_FACESETS)
	    loadFaceSets(*gt, obj, facesetAttrib, t);

	return GT_PrimitiveHandle(gt);
    }

    static UT_StringHolder
    getCurveMeshAttribs(const GABC_IObject &obj,
                        const GEO_PackedNameMapPtr &namemap,
                        int load_style,
                        GT_Owner owner)
    {
        UT_String emptyString;

        ICurves                shape(obj.object(), gabcWrapExisting);
        ICurvesSchema         &ss = shape.getSchema();
        ICompoundProperty      arb;
        if (load_style & GABC_IObject::GABC_LOAD_ARBS)
            arb = ss.getArbGeomParams();

        IP3fArrayProperty        P = ss.getPositionsProperty();
        IFloatArrayProperty      Pw = ss.getPositionWeightsProperty();
        IV3fArrayProperty        v = ss.getVelocitiesProperty();
        const IN3fGeomParam     &N = ss.getNormalsParam();
        const IV2fGeomParam     &uvs = ss.getUVsParam();
        IFloatGeomParam          widths = ss.getWidthsParam();

        switch (owner)
        {
            case GT_OWNER_POINT:
                return emptyString;
            case GT_OWNER_VERTEX:
                return getAttributeNames(namemap, GA_ATTRIB_VERTEX,
					 vertex_scope, 3, arb, &P, &v, &N, &uvs,
					 nullptr, &widths, &Pw);
            case GT_OWNER_PRIMITIVE:
                return getAttributeNames(namemap, GA_ATTRIB_PRIMITIVE,
					 &gabcUniformScope, 1, arb);
            case GT_OWNER_DETAIL:
                return getAttributeNames(namemap, GA_ATTRIB_DETAIL,
					 theConstantUnknownScope, 2, arb);
            case GT_OWNER_MAX:
            case GT_OWNER_INVALID:
                UT_ASSERT(0 && "Unexpected attribute type");
                break;
        }

        return emptyString;
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
	fpreal32	*data = xyz->data();
	xyz->copyDataId(*x);
	for (exint i = 0; i < n; ++i, data += 3)
	{
	    data[0] = x->getF32(i);
	    data[1] = y->getF32(i);
	    data[2] = z->getF32(i);
	}
	return GT_DataArrayHandle(xyz);
    }

    template <typename ATTRIB_CREATOR>
    static GT_PrimitiveHandle
    buildLocator(const ATTRIB_CREATOR &acreate, const GABC_IObject &obj,
		 fpreal t, const GEO_PackedNameMapPtr &namemap, int load_style)
    {
	IXform		xform(obj.object(), gabcWrapExisting);
	IXformSchema	&ss = xform.getSchema();
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

	GT_AttributeMapHandle pmap(new GT_AttributeMap());
	int Pidx = pmap->add("P", true);
	int lSidx = pmap->add("localScale", true);
	int pXidx = pmap->add("parentTrans", true);
	int pRidx = pmap->add("parentRot", true);
	int pSidx = pmap->add("parentScale", true);
	int pHidx = pmap->add("parentShear", true);

	GT_AttributeListHandle point(new GT_AttributeList(pmap));
	point->set( Pidx, new GT_RealConstant(1, ldata, 3), 0);
	point->set(lSidx, new GT_RealConstant(1, ldata+3, 3), 0);
	point->set(pXidx, new GT_RealConstant(1, &pxval.x, 3), 0);
	point->set(pRidx, new GT_RealConstant(1, &prval.x, 3), 0);
	point->set(pSidx, new GT_RealConstant(1, &psval.x, 3), 0);
	point->set(pHidx, new GT_RealConstant(1, &phval.x, 3), 0);

	if (load_style & GABC_IObject::GABC_LOAD_ARBS)
	{
	    point = point->mergeNewAttributes(
			acreate.build(1, NULL, obj, namemap, load_style, t,
				      GA_ATTRIB_POINT, theConstantUnknownScope,
				      2, ss.getArbGeomParams()));
	}

	GT_AttributeListHandle detail;

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
			const GEO_PackedNameMapPtr &namemap,
			int load_style)
    {
	INuPatch		 shape(obj.object(), gabcWrapExisting);
	INuPatchSchema		&ss = shape.getSchema();
	adjustDeformingTimeSample(t, obj, ss);
	INuPatchSchema::Sample	 sample = ss.getValue(ISampleSelector(t));
	int			 uorder = sample.getUOrder();
	int			 vorder = sample.getVOrder();
	GT_DataArrayHandle	 uknots;
	GT_DataArrayHandle	 vknots;
	ICompoundProperty	 arb;
	if (load_style & GABC_IObject::GABC_LOAD_ARBS)
	    arb = ss.getArbGeomParams();

	uknots = simpleArrayFromSample(*obj.archive(), sample.getUKnot(),
			ss.getUKnotsProperty().isConstant());
	vknots = simpleArrayFromSample(*obj.archive(), sample.getVKnot(),
			ss.getVKnotsProperty().isConstant());

	GT_AttributeListHandle	 vertex;
	GT_AttributeListHandle	 uniform;
	GT_AttributeListHandle	 detail;
	IP3fArrayProperty	 P = ss.getPositionsProperty();
	IFloatArrayProperty	 Pw = ss.getPositionWeightsProperty();
	IV3fArrayProperty	 v = ss.getVelocitiesProperty();
	const IN3fGeomParam	&N = ss.getNormalsParam();
	const IV2fGeomParam	&uvs = ss.getUVsParam();

	vertex = acreate.build(-1, prim, obj, namemap, load_style, t,
				GA_ATTRIB_VERTEX, vertex_scope, 3, arb, &P, &v,
				&N, &uvs, nullptr, nullptr, &Pw);
	detail = acreate.build(1, prim, obj, namemap, load_style, t,
			       GA_ATTRIB_DETAIL, detail_scope, 3, arb);

	detail = addFileObjectAttribs(detail, obj, t);
	
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
	    bool	is_const = ss.trimCurveTopologyIsConstant();
	    loopCount = simpleArrayFromSample(*obj.archive(),
			    sample.getTrimNumCurves(), is_const);
	    curveCount = simpleArrayFromSample(*obj.archive(),
			    sample.getTrimNumVertices(), is_const);
	    curveOrders = simpleArrayFromSample(*obj.archive(),
			    sample.getTrimOrders(), is_const);
	    curveKnots = simpleArrayFromSample(*obj.archive(),
			    sample.getTrimKnots(), is_const);
	    curveMin = simpleArrayFromSample(*obj.archive(),
			    sample.getTrimMins(), is_const);
	    curveMax = simpleArrayFromSample(*obj.archive(),
			    sample.getTrimMaxes(), is_const);
	    curveU = simpleArrayFromSample(*obj.archive(),
			    sample.getTrimU(), is_const);
	    curveV = simpleArrayFromSample(*obj.archive(),
			    sample.getTrimV(), is_const);
	    curveW = simpleArrayFromSample(*obj.archive(),
			    sample.getTrimW(), is_const);
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

    static UT_StringHolder
    getNuPatchAttribs(const GABC_IObject &obj,
                        const GEO_PackedNameMapPtr &namemap,
                        int load_style,
                        GT_Owner owner)
    {
        UT_String emptyString;

        INuPatch                shape(obj.object(), gabcWrapExisting);
        INuPatchSchema         &ss = shape.getSchema();
        ICompoundProperty      arb;
        if (load_style & GABC_IObject::GABC_LOAD_ARBS)
            arb = ss.getArbGeomParams();

        IP3fArrayProperty        P = ss.getPositionsProperty();
        IFloatArrayProperty      Pw = ss.getPositionWeightsProperty();
        IV3fArrayProperty        v = ss.getVelocitiesProperty();
        const IN3fGeomParam     &N = ss.getNormalsParam();
        const IV2fGeomParam     &uvs = ss.getUVsParam();

        switch (owner)
        {
            case GT_OWNER_POINT:
                return emptyString;
            case GT_OWNER_VERTEX:
                return getAttributeNames(namemap, GA_ATTRIB_VERTEX,
					 vertex_scope, 3, arb, &P, &v, &N, &uvs,
					 nullptr, nullptr, &Pw);
            case GT_OWNER_PRIMITIVE:
                return emptyString;
            case GT_OWNER_DETAIL:
                return getAttributeNames(namemap, GA_ATTRIB_DETAIL,
					 detail_scope, 3, arb);
            case GT_OWNER_MAX:
            case GT_OWNER_INVALID:
                UT_ASSERT(0 && "Unexpected attribute type");
                break;
        }

        return emptyString;
    }

    template <typename T>
    static ICompoundProperty
    geometryProperties(const GABC_IObject &obj)
    {
	T			 shape(obj.object(), gabcWrapExisting);
	typename T::schema_type	&ss = shape.getSchema();
	return ss.getArbGeomParams();
    }

    template <typename T>
    static ICompoundProperty
    userProperties(const GABC_IObject &obj)
    {
	T			 shape(obj.object(), gabcWrapExisting);
	typename T::schema_type	&ss = shape.getSchema();
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

    template <typename ABC_T>
    static bool
    abcBounds(const IObject &obj, UT_BoundingBox &box, fpreal t, bool &isconst)
    {
	ABC_T		 prim(obj, gabcWrapExisting);
	const typename ABC_T::schema_type	&ss = prim.getSchema();
	IBox3dProperty	 bounds = ss.getSelfBoundsProperty();
	index_t		 i0, i1;
	fpreal		 bias = GABC_Util::getSampleIndex(t,
					ss.getTimeSampling(),
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

    template <typename ABC_T>
    static TimeSamplingPtr
    abcTimeSampling(const IObject &obj)
    {
	ABC_T				    prim(obj, gabcWrapExisting);
	const typename ABC_T::schema_type   &ss = prim.getSchema();
	return ss.getTimeSampling();
    }

    template <typename ABC_T>
    static exint
    abcNumSamples(const IObject &obj)
    {
	ABC_T				    prim(obj, gabcWrapExisting);
	const typename ABC_T::schema_type   &ss = prim.getSchema();
	return ss.getNumSamples();
    }

    static bool
    abcFaceSetsAnimated(const GABC_IObject &obj)
    {
	exint nkids = obj.getNumChildren();
	for (exint i = 0; i < nkids; ++i)
	{
	    const GABC_IObject &kid = obj.getChild(i);
	    if (kid.valid() && kid.nodeType() == GABC_FACESET)
	    {
		IFaceSet shape(kid.object(), gabcWrapExisting);
		const IFaceSetSchema   &ss = shape.getSchema();
		if(!ss.isConstant())
		    return true;
	    }
	}

	return false;
    }

    template <typename ABC_T>
    static GEO_AnimationType
    getAnimation(const GABC_IObject &obj)
    {
	ABC_T				 prim(obj.object(), gabcWrapExisting);
	typename ABC_T::schema_type	&schema = prim.getSchema();
	GEO_AnimationType		 atype;
	
	switch (schema.getTopologyVariance())
	{
	    case Alembic::AbcGeom::kConstantTopology:
		atype = GEO_ANIMATION_CONSTANT;
		if (GABC_Util::isABCPropertyAnimated(schema.getArbGeomParams()) ||
		    GABC_Util::isABCPropertyAnimated(schema.getUserProperties()) ||
		    abcFaceSetsAnimated(obj))
		{
		    atype = GEO_ANIMATION_ATTRIBUTE;
		}
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
    getAnimation<IXform>(const GABC_IObject &obj)
    {
	IXform		 prim(obj.object(), gabcWrapExisting);
	IXformSchema	&schema = prim.getSchema();
	if (!schema.isConstant())
	    return GEO_ANIMATION_TRANSFORM;
	return GEO_ANIMATION_CONSTANT;
    }

    template <>
    GEO_AnimationType
    getAnimation<IPoints>(const GABC_IObject &obj)
    {
	IPoints		 prim(obj.object(), gabcWrapExisting);
	IPointsSchema	&schema = prim.getSchema();
	if (!schema.isConstant())
	    return GEO_ANIMATION_TOPOLOGY;
	if (GABC_Util::isABCPropertyAnimated(schema.getArbGeomParams()))
	    return GEO_ANIMATION_ATTRIBUTE;
	return GEO_ANIMATION_CONSTANT;
    }

    static GEO_AnimationType
    getLocatorAnimation(const GABC_IObject &obj)
    {
	IXform		prim(obj.object(), gabcWrapExisting);
	IXformSchema	&schema = prim.getSchema();
	if (GABC_Util::isABCPropertyAnimated(schema.getArbGeomParams()))
	    return GEO_ANIMATION_ATTRIBUTE;
	Alembic::Abc::IScalarProperty	loc(prim.getProperties(), "locator");
	return loc.isConstant() ? GEO_ANIMATION_CONSTANT
				: GEO_ANIMATION_ATTRIBUTE;
    }

    static GEO_AnimationType
    getFaceSetAnimationType(const GABC_IObject &obj)
    {
	IFaceSet	 shape(obj.object(), gabcWrapExisting);
	IFaceSetSchema	&ss = shape.getSchema();
	return ss.isConstant() ? GEO_ANIMATION_CONSTANT:GEO_ANIMATION_TOPOLOGY;
    }

    template <typename ABCTYPE>
    static GEO_AnimationType
    getGenericAnimationType(const GABC_IObject &obj)
    {
	ABCTYPE				 shape(obj.object(), gabcWrapExisting);
	typename ABCTYPE::schema_type	&ss = shape.getSchema();
	return ss.isConstant() ? GEO_ANIMATION_CONSTANT:GEO_ANIMATION_TRANSFORM;
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

std::string
GABC_IObject::getSourcePath() const
{
    if (myObject.valid() && myObject.isInstanceDescendant())
       return IObject(myObject).instanceSourcePath();

    return myObjectPath;
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
    return GABC_Util::getVisibility(*this, t, animated, check_parent);
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
		atype = getAnimation<IPolyMesh>(*this);
		break;
	    case GABC_SUBD:
		atype = getAnimation<ISubD>(*this);
		break;
	    case GABC_CURVES:
		atype = getAnimation<ICurves>(*this);
		break;
	    case GABC_POINTS:
		atype = getAnimation<IPoints>(*this);
		break;
	    case GABC_NUPATCH:
		atype = getAnimation<INuPatch>(*this);
		break;
	    case GABC_XFORM:
		if (isMayaLocator())
		    atype = getLocatorAnimation(*this);
		else
		    atype = getAnimation<IXform>(*this);
		break;
	    case GABC_FACESET:
		atype = getFaceSetAnimationType(*this);
		break;
	    case GABC_CAMERA:
		atype = getGenericAnimationType<ICamera>(*this);
		break;
	    case GABC_LIGHT:
		atype = getGenericAnimationType<ILight>(*this);
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
		return abcBounds<IPolyMesh>(object(), box, t, isconst);
	    case GABC_SUBD:
		return abcBounds<ISubD>(object(), box, t, isconst);
	    case GABC_CURVES:
		return abcBounds<ICurves>(object(), box, t, isconst);
	    case GABC_POINTS:
		return abcBounds<IPoints>(object(), box, t, isconst);
	    case GABC_NUPATCH:
		return abcBounds<INuPatch>(object(), box, t, isconst);
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
GABC_IObject::timeSampling() const
{
    //GABC_AlembicLock	lock(archive());
    switch (nodeType())
    {
	case GABC_POLYMESH:
	    return abcTimeSampling<IPolyMesh>(object());
	case GABC_SUBD:
	    return abcTimeSampling<ISubD>(object());
	case GABC_CURVES:
	    return abcTimeSampling<ICurves>(object());
	case GABC_POINTS:
	    return abcTimeSampling<IPoints>(object());
	case GABC_NUPATCH:
	    return abcTimeSampling<INuPatch>(object());
	case GABC_XFORM:
	    return abcTimeSampling<IXform>(object());
	default:
	    break;
    }
    return TimeSamplingPtr();
}

exint
GABC_IObject::numSamples() const
{
    switch (nodeType())
    {
	case GABC_POLYMESH:
	    return abcNumSamples<IPolyMesh>(object());
	case GABC_SUBD:
	    return abcNumSamples<ISubD>(object());
	case GABC_CURVES:
	    return abcNumSamples<ICurves>(object());
	case GABC_POINTS:
	    return abcNumSamples<IPoints>(object());
        case GABC_NUPATCH:
	    return abcNumSamples<INuPatch>(object());
        case GABC_XFORM:
            return abcNumSamples<IXform>(object());
	default:
	    break;
    }
    return 0;
}

fpreal
GABC_IObject::clampTime(fpreal input_time) const
{
    TimeSamplingPtr	time(timeSampling());
    exint		nsamples = time ? time->getNumStoredTimes() : 0;
    if (nsamples < 1)
	return 0;
    return SYSclamp(input_time, time->getSampleTime(0),
			time->getSampleTime(nsamples-1));
}

UT_StringHolder
GABC_IObject::getAttributes(const GEO_PackedNameMapPtr &namemap,
                            int load_style,
                            GT_Owner owner) const
{
    GABC_AlembicLock lock(archive());

    switch (nodeType())
    {
        case GABC_POLYMESH:
            return getPolyMeshAttribs(*this, namemap, load_style, owner);
        case GABC_SUBD:
            return getSubDMeshAttribs(*this, namemap, load_style, owner);
        case GABC_POINTS:
            return getPointMeshAttribs(*this, namemap, load_style, owner);
        case GABC_CURVES:
            return getCurveMeshAttribs(*this, namemap, load_style, owner);
        case GABC_NUPATCH:
            return getNuPatchAttribs(*this, namemap, load_style, owner);
        default:
            break;
    }

    UT_String emptyString;
    return emptyString;
}

UT_StringHolder
GABC_IObject::getFaceSets(const UT_StringHolder &attrib, fpreal t, int load_style) const
{
    UT_String emptyString;
    if ( !(load_style & GABC_IObject::GABC_LOAD_FACESETS) )
        return emptyString;

    GABC_AlembicLock lock(archive());

    switch(nodeType())
    {
        case GABC_POLYMESH:
        case GABC_SUBD:
        case GABC_CURVES:
            return getFaceSetNames(*this, attrib, t);
        default:
            break;
    }

    return emptyString;
}

GT_PrimitiveHandle
GABC_IObject::getPrimitive(const GEO_Primitive *gprim,
        fpreal t,
        GEO_AnimationType &atype,
        const GEO_PackedNameMapPtr &namemap,
        const UT_StringHolder &facesetAttrib,
        int load_style) const
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
			*this, t, namemap, facesetAttrib, load_style);
		break;
	    case GABC_SUBD:
		prim = buildSubDMesh(CreateAttributeList(), gprim,
			*this, t, namemap, facesetAttrib, load_style);
		break;
	    case GABC_POINTS:
		prim = buildPointMesh(CreateAttributeList(), gprim,
			*this, t, namemap, load_style);
		break;
	    case GABC_CURVES:
		prim = buildCurveMesh(CreateAttributeList(), gprim,
			*this, t, namemap, facesetAttrib, load_style);
		break;
	    case GABC_NUPATCH:
		prim = buildNuPatch(CreateAttributeList(), gprim,
			*this, t, namemap, load_style);
		break;
	    case GABC_XFORM:
		if (isMayaLocator())
		{
		    prim = buildLocator(CreateAttributeList(),
					*this, t, namemap, load_style);
		}
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
				const GEO_PackedNameMapPtr &namemap,
                                const UT_StringHolder &facesetAttrib,
				int load_style) const
{
    UT_ASSERT(src);
    GT_PrimitiveHandle	prim;
    try
    {
	switch (nodeType())
	{
	    case GABC_POLYMESH:
		prim = buildPolyMesh(UpdateAttributeList(src), gprim,
			    *this, new_time, namemap, facesetAttrib, load_style);
		break;
	    case GABC_SUBD:
		prim = buildSubDMesh(UpdateAttributeList(src), gprim,
			    *this, new_time, namemap, facesetAttrib, load_style);
		break;
	    case GABC_POINTS:
		prim = buildPointMesh(UpdateAttributeList(src), gprim,
			    *this, new_time, namemap, load_style);
		break;
	    case GABC_CURVES:
		prim = buildCurveMesh(UpdateAttributeList(src), gprim,
			    *this, new_time, namemap, facesetAttrib, load_style);
		break;
	    case GABC_NUPATCH:
		prim = buildNuPatch(UpdateAttributeList(src), gprim,
			    *this, new_time, namemap, load_style);
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
GABC_IObject::getPointCloud(fpreal t, GEO_AnimationType &atype) const
{
    GT_DataArrayHandle	P = getPosition(t, atype);
    if (!P && nodeType() == GABC_XFORM)
	P = GT_DataArrayHandle(new GT_RealConstant(1, 0.0, 3, GT_TYPE_POINT));
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

    return GABC_Util::isTransformAnimated(xform);
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
		    return gabcGetPosition<IPolyMesh>(*this, t, atype);
		case GABC_SUBD:
		    return gabcGetPosition<ISubD>(*this, t, atype);
		case GABC_CURVES:
		    return gabcGetPosition<ICurves>(*this, t, atype);
		case GABC_POINTS:
		    return gabcGetPosition<IPoints>(*this, t, atype);
		case GABC_NUPATCH:
		    return gabcGetPosition<INuPatch>(*this, t, atype);
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
		    return gabcGetWidth<ICurves>(*this, t);
		case GABC_POINTS:
		    return gabcGetWidth<IPoints>(*this, t);
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
		    return gabcGetVelocity<IPolyMesh>(*this, t);
		case GABC_SUBD:
		    return gabcGetVelocity<ISubD>(*this, t);
		case GABC_CURVES:
		    return gabcGetVelocity<ICurves>(*this, t);
		case GABC_POINTS:
		    return gabcGetVelocity<IPoints>(*this, t);
		case GABC_NUPATCH:
		    return gabcGetVelocity<INuPatch>(*this, t);
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
		return geometryProperties<IXform>(*this);
		break;
	    case GABC_POLYMESH:
		return geometryProperties<IPolyMesh>(*this);
		break;
	    case GABC_SUBD:
		return geometryProperties<ISubD>(*this);
		break;
	    case GABC_CAMERA:
		return geometryProperties<ICamera>(*this);
		break;
	    case GABC_FACESET:
		return geometryProperties<IFaceSet>(*this);
		break;
	    case GABC_CURVES:
		return geometryProperties<ICurves>(*this);
		break;
	    case GABC_POINTS:
		return geometryProperties<IPoints>(*this);
		break;
	    case GABC_NUPATCH:
		return geometryProperties<INuPatch>(*this);
		break;
	    case GABC_LIGHT:
		return geometryProperties<ILight>(*this);

	    case GABC_MATERIAL:
	    default:
		break;
	}
    }
    return ICompoundProperty();
}

GT_DataArrayHandle
GABC_IObject::convertIProperty(ICompoundProperty &parent,
			    const PropertyHeader &header,
			    fpreal t,
			    const GEO_PackedNameMapPtr &namemap,
			    GEO_AnimationType *atype,
			    exint expected_size) const
{
    GABC_IArchive	&arch = *(archive().get());
    if (header.isArray())
    {
	const std::string &name = header.getName();
	IArrayProperty	prop(parent, name);
	if (atype)
	{
	    *atype = prop.isConstant() ? GEO_ANIMATION_CONSTANT
				    : GEO_ANIMATION_ATTRIBUTE;
	}

	auto &metadata = prop.getMetaData();
	std::string interp = metadata.get("interpretation");
	int size = prop.getDataType().getExtent();

	GT_Type type = GABC_GTUtil::getGTTypeInfo(interp.c_str(), size);
	if(namemap)
	{
	    const char *typeinfo = namemap->getTypeInfo(name);
	    if(typeinfo)
		type = GABC_GTUtil::getGTTypeInfo(typeinfo, size);
	}
	return readArrayProperty(arch, prop, t, type, expected_size);
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
	    gtidx = convertIProperty(comp, *hidx, t, namemap, &atidx, expected_size);
	    gtval = convertIProperty(comp, *hval, t, namemap, &atval);
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
		return userProperties<IXform>(*this);
		break;
	    case GABC_POLYMESH:
		return userProperties<IPolyMesh>(*this);
		break;
	    case GABC_SUBD:
		return userProperties<ISubD>(*this);
		break;
	    case GABC_CAMERA:
		return userProperties<ICamera>(*this);
		break;
	    case GABC_FACESET:
		return userProperties<IFaceSet>(*this);
		break;
	    case GABC_CURVES:
		return userProperties<ICurves>(*this);
		break;
	    case GABC_POINTS:
		return userProperties<IPoints>(*this);
		break;
	    case GABC_NUPATCH:
		return userProperties<INuPatch>(*this);
		break;
	    case GABC_LIGHT:
		return userProperties<ILight>(*this);

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
	    data = convertIProperty(arb, header, t, GEO_PackedNameMapPtr(), &atype);
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
		data = convertIProperty(arb, *header, t, GEO_PackedNameMapPtr(), &atype);
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
	    data = convertIProperty(arb, header, t, GEO_PackedNameMapPtr(), &atype);
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
		data = convertIProperty(arb, *header, t, GEO_PackedNameMapPtr(), &atype);
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
    {
	xform.identity();
	return false;
    }

    IObject		obj = object();
    GABC_AlembicLock	lock(archive());
    bool		ascended = false;

    while (obj.valid() && !IXform::matches(obj.getHeader()))
    {
	obj = obj.getParent();
	ascended = true;
    }
    if (!obj.valid())
    {
	xform.identity();
	return false;
    }

    lock.unlock();
    if (ascended)
    {
	return GABC_Util::getWorldTransform(GABC_IObject(archive(), obj), t,
					    xform, isConstant, inheritsXform);
    }
    return GABC_Util::getWorldTransform(*this, t, xform, isConstant,
					inheritsXform);
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
    {
	mat.identity();
	return false;
    }

    isConstant = true;
    inheritsXform = true;

    if (nodeType() == GABC_XFORM)
    {
	GABC_AlembicLock    lock(archive());
	IXform              xform(myObject, gabcWrapExisting);
	IXformSchema       &ss = xform.getSchema();
	index_t             i0, i1;
	XformSample         s0, s1;
	M44d                matrix;
	fpreal              bias = GABC_Util::getSampleIndex(t,
	                            ss.getTimeSampling(),
                                    ss.getNumSamples(), i0, i1);

        isConstant = ss.isConstant();
        inheritsXform = ss.getInheritsXforms();

	ss.get(s0, ISampleSelector(i0));
	if (i0 == i1)
	    matrix = s0.getMatrix();
	else
	{
	    ss.get(s1, ISampleSelector(i1));
	    matrix = blendSamples(s0, s1, bias);
	}

	mat = GABC_Util::getM(matrix);
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

bool
GABC_IObject::getPropertiesHash(int64 &hash) const
{
    Alembic::Util::Digest prophash;
    if(const_cast<IObject &>(myObject).getPropertiesHash(prophash))
    {
	hash = prophash.words[0] + SYSwang_inthash64(prophash.words[1]);
	return true;
    }

    hash = 0;
    return false;
}
