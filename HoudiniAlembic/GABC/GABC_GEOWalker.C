/*
 * Copyright (c) 2014
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

#include "GABC_GEOWalker.h"
#include "GABC_PackedImpl.h"
#include <UT/UT_Interrupt.h>
#include <UT/UT_StackBuffer.h>
#include <Alembic/AbcGeom/All.h>
#include <GEO/GEO_PackedNameMap.h>
#include <GEO/GEO_PrimNURBCurve.h>
#include <GEO/GEO_PrimRBezCurve.h>
#include <GT/GT_DataArray.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PrimPolySoup.h>
#include <GU/GU_PrimPart.h>
#include <GU/GU_PrimNURBCurve.h>
#include <GU/GU_PrimNURBSurf.h>
#include <GU/GU_PrimRBezCurve.h>
#include <GA/GA_Handle.h>

namespace Alembic {
    namespace Abc {
        namespace ALEMBIC_VERSION_NS {
            ALEMBIC_ABC_DECLARE_TYPE_TRAITS(Imath::V4f,
                    kFloat32POD,
                    4,
                    "point",
                    P4fTPTraits);
        }
    }
}

using namespace GABC_NAMESPACE;

namespace {
    typedef Imath::M44d				M44d;
    typedef Imath::V3f			        V3f;
    typedef Imath::V3d				V3d;
    typedef Imath::V4f                          V4f;
    typedef Alembic::Abc::DataType		DataType;
    typedef Alembic::Abc::ISampleSelector	ISampleSelector;
    typedef Alembic::Abc::ObjectHeader		ObjectHeader;
    typedef Alembic::Abc::P3fArraySample	P3fArraySample;
    typedef Alembic::Abc::UcharArraySamplePtr	UcharArraySamplePtr;
    typedef Alembic::Abc::Int32ArraySamplePtr	Int32ArraySamplePtr;
    typedef Alembic::Abc::FloatArraySamplePtr	FloatArraySamplePtr;
    typedef Alembic::Abc::P3fArraySamplePtr	P3fArraySamplePtr;
    typedef Alembic::Abc::ICompoundProperty	ICompoundProperty;
    typedef Alembic::Abc::IArrayProperty	IArrayProperty;
    typedef Alembic::Abc::PropertyHeader	PropertyHeader;
    typedef Alembic::Abc::ArraySamplePtr	ArraySamplePtr;
    typedef Alembic::Abc::WrapExistingFlag	WrapExistingFlag;

    typedef Alembic::AbcGeom::IXform		IXform;
    typedef Alembic::AbcGeom::IXformSchema	IXformSchema;
    typedef Alembic::AbcGeom::XformSample	XformSample;

    typedef Alembic::AbcGeom::BasisType         BasisType;
    typedef Alembic::AbcGeom::GeometryScope	GeometryScope;
    typedef Alembic::AbcGeom::ISubD		ISubD;
    typedef Alembic::AbcGeom::ISubDSchema	ISubDSchema;
    typedef Alembic::AbcGeom::IPolyMesh		IPolyMesh;
    typedef Alembic::AbcGeom::IPolyMeshSchema	IPolyMeshSchema;
    typedef Alembic::AbcGeom::ICurves		ICurves;
    typedef Alembic::AbcGeom::ICurvesSchema	ICurvesSchema;
    typedef Alembic::AbcGeom::IPoints		IPoints;
    typedef Alembic::AbcGeom::IPointsSchema	IPointsSchema;
    typedef Alembic::AbcGeom::INuPatch		INuPatch;
    typedef Alembic::AbcGeom::INuPatchSchema	INuPatchSchema;
    typedef Alembic::AbcGeom::IFaceSet		IFaceSet;
    typedef Alembic::AbcGeom::IFaceSetSchema	IFaceSetSchema;
    typedef INuPatchSchema::Sample		INuPatchSample;
    typedef ICurvesSchema::Sample               ICurvesSample;

    // Types used for NURBS rationalization, but undefined by Alembic
    typedef Alembic::Abc::P4fTPTraits                   P4fTPTraits;
    typedef Alembic::Abc::TypedArraySample<P4fTPTraits> P4fArraySample;
    typedef Alembic::Util::shared_ptr<P4fArraySample>   P4fArraySamplePtr;

    const WrapExistingFlag gabcWrapExisting = Alembic::Abc::kWrapExisting;
    const M44d	identity44d(1, 0, 0, 0,
			    0, 1, 0, 0,
			    0, 0, 1, 0,
			    0, 0, 0, 1);

    class PushTransform
    {
    public:
	PushTransform(GABC_GEOWalker &walk, IXform &xform)
	    : myWalk(walk)
	{
	    IXformSchema	&xs = xform.getSchema();
	    XformSample		 sample = xs.getValue(walk.timeSample());
	    if (walk.isConstant() && !xs.isConstant())
	    {
		walk.setNonConstant();
	    }
	    walk.pushTransform(sample.getMatrix(), xs.isConstant(), myPop,
		    xs.getInheritsXforms());
	}
	~PushTransform()
	{
	    myWalk.popTransform(myPop);
	}
    private:
	GABC_GEOWalker			&myWalk;
	GABC_GEOWalker::TransformState	 myPop;
    };

    static GA_AttributeOwner
    getGAOwner(GeometryScope scope)
    {
	switch (scope)
	{
	    case Alembic::AbcGeom::kConstantScope:
		return GA_ATTRIB_DETAIL;
	    case Alembic::AbcGeom::kUnknownScope:
	    case Alembic::AbcGeom::kUniformScope:
		return GA_ATTRIB_PRIMITIVE;
	    case Alembic::AbcGeom::kFacevaryingScope:
		return GA_ATTRIB_VERTEX;
	    case Alembic::AbcGeom::kVertexScope:
	    case Alembic::AbcGeom::kVaryingScope:
		return GA_ATTRIB_POINT;
	}
	UT_ASSERT(0);
	return GA_ATTRIB_OWNER_N;
    }
    
    static GA_Storage
    getGAStorage(const GT_Storage store)
    {
	switch (store)
	{
	    case GT_STORE_UINT8:
		return GA_STORE_INT8;
	    case GT_STORE_INT32:
		return GA_STORE_INT32;
	    case GT_STORE_INT64:
		return GA_STORE_INT64;
	    case GT_STORE_REAL16:
		return GA_STORE_REAL16;
	    case GT_STORE_REAL32:
		return GA_STORE_REAL32;
	    case GT_STORE_REAL64:
		return GA_STORE_REAL64;
	    case GT_STORE_STRING:
		return GA_STORE_STRING;
	    default:
		break;
	}
	return GA_STORE_INVALID;
    }

    static GA_Storage
    getGAStorage(const Alembic::AbcGeom::DataType &dtype)
    {
	switch (dtype.getPod())
	{
	    case Alembic::AbcGeom::kBooleanPOD:
	    case Alembic::AbcGeom::kUint8POD:
		return GA_STORE_UINT8;
	    case Alembic::AbcGeom::kInt8POD:
		return GA_STORE_INT8;
	    case Alembic::AbcGeom::kUint16POD:
	    case Alembic::AbcGeom::kInt16POD:
		return GA_STORE_INT16;
	    case Alembic::AbcGeom::kUint32POD:
	    case Alembic::AbcGeom::kInt32POD:
		return GA_STORE_INT32;
	    case Alembic::AbcGeom::kUint64POD:
	    case Alembic::AbcGeom::kInt64POD:
		return GA_STORE_INT64;
	    case Alembic::AbcGeom::kFloat16POD:
		return GA_STORE_REAL16;
	    case Alembic::AbcGeom::kFloat32POD:
		return GA_STORE_REAL32;
	    case Alembic::AbcGeom::kFloat64POD:
		return GA_STORE_REAL64;
	    case Alembic::AbcGeom::kStringPOD:
		return GA_STORE_STRING;

	    case Alembic::AbcGeom::kWstringPOD:
	    case Alembic::AbcGeom::kNumPlainOldDataTypes:
	    case Alembic::AbcGeom::kUnknownPOD:
		break;
	}
	return GA_STORE_INVALID;
    }

    static inline int
    getGATupleSize(const Alembic::AbcGeom::DataType &dtype)
    {
	return dtype.getExtent();
    }

    static inline GA_TypeInfo
    getGATypeInfo(const char *interp, int tsize)
    {
	if (!strcmp(interp, "point"))
	    return tsize == 4 ? GA_TYPE_HPOINT : GA_TYPE_POINT;
	if (!strcmp(interp, "vector"))
	    return GA_TYPE_VECTOR;
	if (!strcmp(interp, "matrix"))
	    return GA_TYPE_TRANSFORM;
	if (!strcmp(interp, "normal"))
	    return GA_TYPE_NORMAL;
	if (!strcmp(interp, "quat"))
	    return GA_TYPE_QUATERNION;
	if (!strcmp(interp, "rgb") || !strcmp(interp, "rgba"))
	    return GA_TYPE_COLOR;
	return GA_TYPE_VOID;
    }

    static inline GA_TypeInfo
    getGATypeInfo(GT_Type tinfo, int tsize)
    {
	switch (tinfo)
	{
	    case GT_TYPE_POINT:
	    case GT_TYPE_HPOINT:
		return tsize == 4 ? GA_TYPE_HPOINT : GA_TYPE_POINT;
	    case GT_TYPE_VECTOR:
		return GA_TYPE_VECTOR;
	    case GT_TYPE_NORMAL:
		return GA_TYPE_NORMAL;
	    case GT_TYPE_COLOR:
		return GA_TYPE_COLOR;
	    case GT_TYPE_QUATERNION:
		return GA_TYPE_QUATERNION;
	    case GT_TYPE_MATRIX3:
	    case GT_TYPE_MATRIX:
		return GA_TYPE_TRANSFORM;
	    default:
		break;
	}
	return GA_TYPE_VOID;
    }

    static GA_Defaults	theZeroDefaults(0);
    static GA_Defaults	theWidthDefaults(0.1);
    static GA_Defaults	theColorDefaults(1.0);

    static const GA_Defaults &
    getDefaults(const char *name, int tsize, const char *interp)
    {
	if (!strcmp(name, "width") && tsize == 1)
	{
	    return theWidthDefaults;
	}
	if (!strcmp(interp, "color")
		|| !strcmp(interp, "rgb")
		|| !strcmp(interp, "rgba"))
	{
	    return theColorDefaults;
	}
	return theZeroDefaults;
    }

    static GA_RWAttributeRef
    findAttribute(GU_Detail &gdp, GA_AttributeOwner owner,
	    const char *name, const char *abcname,
	    int tsize, GA_Storage storage, const char *interp)
    {
	GA_RWAttributeRef	attrib;
	switch (GAstorageClass(storage))
	{
	    case GA_STORECLASS_REAL:
		attrib = gdp.findFloatTuple(owner, name, tsize);
		break;
	    case GA_STORECLASS_INT:
		attrib = gdp.findIntTuple(owner, name, tsize);
		break;
	    case GA_STORECLASS_STRING:
		attrib = gdp.findStringTuple(owner, name, tsize);
		break;
	    default:
		break;
	}
	if (!attrib.isValid())
	{
	    if (!strcmp(name, "uv") && tsize == 2)
		tsize = 3;	// Adjust for "Houdini" uv coordinates

	    attrib = gdp.addTuple(storage, owner, name, tsize,
				getDefaults(name, tsize, interp));
	    if (attrib.isValid() && abcname)
		attrib.getAttribute()->setExportName(abcname);
	    // Do *not* mark this as a "vector" since we don't want to
	    // transform as a vector.
	}
	return attrib;
    }

    template <typename T>
    static bool
    isEmpty(const T &ptr)
    {
	return !ptr || !ptr->valid() || ptr->size() == 0;
    }

    template <typename T>
    static void
    setStringAttribute(GU_Detail &gdp, GA_RWAttributeRef &attrib,
		    const T &array, int extent, exint start, exint end,
		    bool extend_array)
    {
	GA_RWHandleS		 h(attrib.getAttribute());
	int			 tsize = SYSmin(extent, attrib.getTupleSize());
	const std::string	*data = (const std::string *)array->getData();
	UT_ASSERT(h.isValid());
	for (exint i = start; i < end; ++i)
	{
	    for (int j = 0; j < tsize; ++j)
		h.set(GA_Offset(i), j, data[j].c_str());
	    if (!extend_array)
		data += extent;
	}
    }

    template <typename T, typename GA_T, typename ABC_T>
    static void
    setNumericAttribute(GU_Detail &gdp, GA_RWAttributeRef &attrib,
		    const T &array, int extent, exint start, exint end,
		    bool extend_array, bool set_v_from_p)
    {
	GA_RWHandleT<GA_T>	 h(attrib.getAttribute());
	int			 tsize = SYSmin(extent, attrib.getTupleSize());
	const GA_AIFTuple	*tuple = attrib.getAIFTuple();
	const ABC_T		*master_data = (const ABC_T *)array->getData();
	if (!master_data)
	    return;

	UT_ASSERT(h.isValid());

	if (set_v_from_p)
	{
	    const ABC_T    *data;

	    for (exint i = start; i < end; ++i)
            {
                if (extend_array)
                {
                    GA_Offset   pos = gdp.vertexPoint(GA_Offset(i - start));
                    data = (master_data + (pos * extent));
                }
                else
                {
                    data = master_data;
                }

                for (int j = 0; j < tsize; ++j)
                {
                    if (tuple)
                    {
                        tuple->set(
                                attrib.getAttribute(),
                                GA_Offset(i),
                                (double)data[j],
                                j);
                    }
                    else
                    {
                        h.set(GA_Offset(i), j, data[j]);
                    }
                }
            }
	}
	else
	{
            for (exint i = start; i < end; ++i)
            {
                for (int j = 0; j < tsize; ++j)
                {
                    if (tuple)
                    {
                        tuple->set(
                                attrib.getAttribute(),
                                GA_Offset(i),
                                (double)master_data[j],
                                j);
                    }
                    else
                    {
                        h.set(GA_Offset(i), j, master_data[j]);
                    }
                }
                if (!extend_array)
                {
                    master_data += extent;
                }
            }
	}
    }

    static GA_TypeInfo
    isReallyVector(const char *name, int tsize)
    {
	// Alembic stores all sorts of things as vectors.  For example "uv"
	// coordinates.  We don't actually want "uv" coordinates transformed as
	// vectors, so we strip out the type information here.
	if (!strcmp(name, "uv") && tsize > 1 && tsize < 4)
	    return GA_TYPE_VOID;
	return GA_TYPE_VECTOR;
    }

    template <typename T>
    static void
    fillNumeric(GA_RWAttributeRef &attrib, const GT_DataArrayHandle &array,
	    const T *data, exint start, exint end, bool extend)
    {
	GA_RWHandleT<T> h(attrib);
	if (!h.isValid())
	    return;

	if (extend)
	{
	    // Copy the entire data array to each element
	    exint	atsize = array->getTupleSize() * array->entries();
	    exint	tsize = SYSmin(atsize, attrib.getTupleSize());
	    for (exint i = start; i < end; ++i)
	    {
		for (int j = 0; j < tsize; ++j)
		    h.set(GA_Offset(i), j, data[j]);
	    }
	}
	else
	{
	    exint	atsize = array->getTupleSize();
	    exint	aend = start + array->entries() - 1;
	    exint	tsize = SYSmin(atsize, attrib.getTupleSize());
	    for (exint i = start; i < end; ++i)
	    {
		for (int j = 0; j < tsize; ++j)
		    h.set(GA_Offset(i), j, data[j]);
		if (i < aend)
		    data += array->getTupleSize();
	    }
	}
    }

    static void
    copyStrings(GA_RWAttributeRef &attrib, const GT_DataArrayHandle &array,
	    exint start, exint end, bool extend)
    {
	GA_RWHandleS	h(attrib);
	if (!h.isValid())
	    return;

	if (extend)
	{
	    exint	atsize = array->getTupleSize() * array->entries();
	    exint	tsize = SYSmin(atsize, attrib.getTupleSize());
	    UT_StackBuffer<const char *>	strings(tsize);
	    int		tidx = 0;
	    for (exint i = 0; i < array->entries(); ++i)
	    {
		for (exint j = 0; tidx < tsize && j < array->getTupleSize();
			++j, ++tidx)
		{
		    strings[tidx] = array->getS(i, j);
		}
	    }
	    for (exint off = start; off < end; ++off)
	    {
		for (exint i = 0; i < tsize; ++i)
		    h.set(GA_Offset(off), i, strings[i]);
	    }
	}
	else
	{
	    exint		aend = array->entries()-1;
	    exint		atsize = array->getTupleSize();
	    exint		tsize = SYSmin(atsize, attrib.getTupleSize());
	    exint		aidx = 0;
	    for (exint i = start; i < end; ++i)
	    {
		for (exint j = 0; j < tsize; ++j)
		    h.set(GA_Offset(i), j, array->getS(aidx, j));
		aidx = SYSmin(aidx+1, aend);
	    }
	}
    }

    static void
    copyNumeric(GA_RWAttributeRef &attrib, const GT_DataArrayHandle &array,
	    exint start, exint end, bool extend)
    {
	GT_DataArrayHandle	storage;
	UT_ASSERT(attrib.getAIFTuple());
	switch (attrib.getAIFTuple()->getStorage(attrib.getAttribute()))
	{
	    case GA_STORE_BOOL:
	    case GA_STORE_UINT8:
	    case GA_STORE_INT8:
	    case GA_STORE_INT16:
	    case GA_STORE_INT32:
	    {
		const int32	*data = array->getI32Array(storage);
		fillNumeric<int32>(attrib, array, data, start, end, extend);
		break;
	    }
	    case GA_STORE_INT64:
	    {
		const int64	*data = array->getI64Array(storage);
		fillNumeric<int64>(attrib, array, data, start, end, extend);
		break;
	    }
	    case GA_STORE_REAL16:
	    {
		const fpreal16	*data = array->getF16Array(storage);
		fillNumeric<fpreal16>(attrib, array, data, start, end, extend);
		break;
	    }
	    case GA_STORE_REAL32:
	    {
		const fpreal32	*data = array->getF32Array(storage);
		fillNumeric<fpreal32>(attrib, array, data, start, end, extend);
		break;
	    }
	    case GA_STORE_REAL64:
	    {
		const fpreal64	*data = array->getF64Array(storage);
		fillNumeric<fpreal64>(attrib, array, data, start, end, extend);
		break;
	    }
	    default:
		break;
	}
    }

    template <typename GT_T, typename ABC_T>
    static void
    copyNumericAttribute(GU_Detail &gdp, GA_RWAttributeRef &attrib,
		    const ArraySamplePtr &array,
		    int extent, exint start, exint end, bool extend_array)
    {
	int			 tsize = SYSmin(extent, attrib.getTupleSize());
	const GA_AIFTuple	*tuple = attrib.getAIFTuple();
	const ABC_T		*data = (const ABC_T *)array->getData();
	UT_StackBuffer<GT_T>	 buf(tsize);
	if (!data || !tuple)
	    return;
	for (exint i = start; i < end; ++i)
	{
	    if (SYSisSame<GT_T, ABC_T>())
	    {
		tuple->set(attrib.getAttribute(), GA_Offset(i),
			(GT_T *)data, tsize);
	    }
	    else
	    {
		for (int j = 0; j < tsize; ++j)
		    buf[j] = data[j];
		tuple->set(attrib.getAttribute(), GA_Offset(i), buf, tsize);
	    }
	    if (!extend_array)
		data += extent;
	}
    }

    /// Template argument @T is expected to be a
    ///  boost::shared_ptr<TypedArraySample<TRAITS>>
    template <typename T>
    static void
    setAttribute(GU_Detail &gdp,
            GA_AttributeOwner owner,
            const char *name,
            const char *abcname,
            const T &array,
            exint start,
            exint len,
            bool set_v_from_p = false)
    {
	if (!*array)
	    return;
	GA_RWAttributeRef	attrib;
	GA_Storage	 store = getGAStorage(array->getDataType());
	int		 tsize = getGATupleSize(array->getDataType());
	const char	*interp= T::element_type::traits_type::interpretation();
	bool		 extend_array;
	extend_array = array->size() < len;
	attrib = findAttribute(gdp, owner, name, abcname, tsize, store, interp);
	if (attrib.isValid())
	{
	    if (attrib.getAttribute() != gdp.getP())
	    {
		GA_TypeInfo	tinfo = getGATypeInfo(interp, tsize);
		if (tinfo == GA_TYPE_VECTOR)
		    tinfo = isReallyVector(name, tsize);
		attrib.getAttribute()->setTypeInfo(tinfo);
	    }
	    switch (attrib.getStorageClass())
	    {
		case GA_STORECLASS_REAL:
		    switch (array->getDataType().getPod())
		    {
			case Alembic::AbcGeom::kFloat16POD:
			    setNumericAttribute<T, fpreal32, fpreal16>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array, set_v_from_p);
			    break;
			case Alembic::AbcGeom::kFloat32POD:
			    setNumericAttribute<T, fpreal32, fpreal32>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array, set_v_from_p);
			    break;
			case Alembic::AbcGeom::kFloat64POD:
			    setNumericAttribute<T, fpreal32, fpreal64>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array, set_v_from_p);
			    break;
			default:
			    UT_ASSERT(0 && "Bad alembic type");
			    break;
		    }
		    break;
		case GA_STORECLASS_INT:
		    switch (array->getDataType().getPod())
		    {
			case Alembic::AbcGeom::kUint32POD:
			    setNumericAttribute<T, int32, uint32>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array, set_v_from_p);
			    break;
			case Alembic::AbcGeom::kInt32POD:
			    setNumericAttribute<T, int32, int32>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array, set_v_from_p);
			    break;
			case Alembic::AbcGeom::kUint64POD:
			    setNumericAttribute<T, int32, uint64>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array, set_v_from_p);
			    break;
			case Alembic::AbcGeom::kInt64POD:
			    setNumericAttribute<T, int32, int64>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array, set_v_from_p);
			    break;
			case Alembic::AbcGeom::kUint8POD:
			    setNumericAttribute<T, int32, uint8>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array, set_v_from_p);
			    break;
			case Alembic::AbcGeom::kInt8POD:
			    setNumericAttribute<T, int32, int8>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array, set_v_from_p);
			    break;
			case Alembic::AbcGeom::kUint16POD:
			    setNumericAttribute<T, int32, uint16>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array, set_v_from_p);
			    break;
			case Alembic::AbcGeom::kInt16POD:
			    setNumericAttribute<T, int32, int16>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array, set_v_from_p);
			    break;
			default:
			    UT_ASSERT(0 && "Bad alembic type");
		    }
		    break;
		case GA_STORECLASS_STRING:
		    setStringAttribute<T>(gdp, attrib,
				array, tsize, start, start+len,
				extend_array);
		    break;
		default:
		    UT_ASSERT(0 && "Bad GA storage");
	    }
	}
    }

    /// Template argument @T is expected to be a
    ///  ITypedGeomParam<TRAITS>
    template <typename T>
    static void
    setGeomAttribute(GABC_GEOWalker &walk, const char *name,
		const char *abcname, const T &param, ISampleSelector &iss,
		exint npoint, exint nvertex, exint nprim)
    {
	if (!param.valid())
	    return;
	GA_AttributeOwner	    owner = getGAOwner(param.getScope());
	GU_Detail                  &gdp = walk.detail();
	typename T::sample_type     psample;
        bool                        set_v_from_p = false;
	param.getExpanded(psample, iss);

        // UVs and Normals may be stored as point or vertex attributes.
        // If both are encountered, upgrade point attribute data to vertex
        // attribute.
        if (owner == GA_ATTRIB_POINT && gdp.findVertexAttribute(name).isValid())
        {
            owner = GA_ATTRIB_VERTEX;
            set_v_from_p = true;
        }

	switch (owner)
	{
	    case GA_ATTRIB_POINT:
		setAttribute(
		        gdp,
		        owner,
		        name,
		        abcname,
			psample.getVals(),
			walk.pointCount(),
			npoint);
		break;
	    case GA_ATTRIB_VERTEX:
		setAttribute(
		        gdp,
		        owner,
		        name,
		        abcname,
			psample.getVals(),
			walk.vertexCount(),
			nvertex,
			set_v_from_p);
		break;
	    case GA_ATTRIB_PRIMITIVE:
		setAttribute(
		        gdp,
		        owner,
		        name,
		        abcname,
			psample.getVals(),
			walk.primitiveCount(),
			nprim);
		break;

	    case GA_ATTRIB_DETAIL:
		// TODO: We map detail attributes to primitive attributes, so
		// we need to extend the array to fill all elements!
		setAttribute(
		        gdp,
		        GA_ATTRIB_PRIMITIVE,
		        name,
		        abcname,
			psample.getVals(),
			walk.primitiveCount(),
			nprim);
		break;
	    default:
		UT_ASSERT(0 && "Bad GA owner");
	}
    }

    static bool
    matchAttributeName(GA_AttributeOwner owner, const char *name,
		    const GEO_PackedNameMapPtr &namemap)
    {
	return namemap ? namemap->matchPattern(owner, name) : true;
    }

    #define MATCH_ATTRIBUTE(param, name) \
	(param.valid() && matchAttributeName(getGAOwner(param.getScope()), name, walk.nameMapPtr()))

    #define MATCH_PROPERTY(prop, iss, name) \
	(prop.valid() && matchAttributeName(GA_ATTRIB_POINT, name, walk.nameMapPtr()))

    static exint
    appendPoints(GABC_GEOWalker &walk, exint npoint)
    {
	GU_Detail	&gdp = walk.detail();
	exint		 startpoint = walk.pointCount();
	UT_VERIFY(gdp.appendPointBlock(npoint) == GA_Offset(startpoint));
	return startpoint;
    }

    void
    appendParticles(GABC_GEOWalker &walk, exint npoint)
    {
	GU_Detail	&gdp = walk.detail();
	// Build the particle, appending points as we go
	GU_PrimParticle::build(&gdp, npoint, 1);
    }

    static exint
    setKnotVector(GA_Basis &basis, FloatArraySamplePtr &knots, int offset = 0)
    {
	GA_KnotVector   &kvec = basis.getKnotVector();
        exint           basis_size = kvec.entries();

	UT_ASSERT(basis_size <= (knots->size() - offset));

	for (int i = 0; i < basis_size; ++i)
	    kvec.setValue(i, knots->get()[offset + i]);

	// Adjust flags based on knot values
	basis.validate(GA_Basis::GA_BASIS_ADAPT_FLAGS);

	return basis_size;
    }

    void
    appendNURBS(GABC_GEOWalker &walk,
	    int uorder, FloatArraySamplePtr &uknots,
	    int vorder, FloatArraySamplePtr &vknots)
    {
	GU_Detail	&gdp = walk.detail();
	int		 cols = uknots->size() - uorder;
	int		 rows = vknots->size() - vorder;

	// Build the surface
	GU_PrimNURBSurf::build(&gdp, rows, cols,
				    uorder, vorder,
				    0,	// periodic in U
				    0,	// periodic in V
				    0,	// basis interpolates ends in U
				    0,	// basis interpolates ends in V
				    GEO_PATCH_QUADS);
    }

    void
    appendCurves(GABC_GEOWalker &walk,
            BasisType type,
            Int32ArraySamplePtr counts,
            exint npoint,
            int order)
    {
	GU_Detail	&gdp = walk.detail();
	exint		 startpoint = appendPoints(walk, npoint);
	exint		 ncurves = counts->size();
        GEO_PolyCounts	 pcounts;
        exint		 nvtx = 0;
        for (exint i = 0; i < ncurves; ++i)
        {
            nvtx += (*counts)[i];
            pcounts.append((*counts)[i]);
        }
        UT_StackBuffer<int>	indices(nvtx);
        for (int i = 0; i < nvtx; ++i)
            indices[i] = i;

	switch (type)
	{
	    case Alembic::AbcGeom::kBezierBasis:
	        GEO_PrimRBezCurve::buildBlock(&gdp, GA_Offset(startpoint),
                        npoint, pcounts, indices, order);
	        break;
	    case Alembic::AbcGeom::kBsplineBasis:
	        GEO_PrimNURBCurve::buildBlock(&gdp, GA_Offset(startpoint),
                        npoint, pcounts, indices, order);
	        break;
            case Alembic::AbcGeom::kNoBasis:
            default:
                GU_PrimPoly::buildBlock(&gdp, GA_Offset(startpoint),
                        npoint, pcounts, indices, false);
	}
    }

    void
    appendFaces(GABC_GEOWalker &walk, exint npoint,
		    Int32ArraySamplePtr counts,
		    Int32ArraySamplePtr indices,
		    bool polysoup)
    {
	// First, append points
	GU_Detail	&gdp = walk.detail();
	exint		 startpoint = appendPoints(walk, npoint);
	exint		 nfaces = counts->size();
	GEO_PolyCounts	 pcounts;
	for (exint i = 0; i < nfaces; ++i)
	    pcounts.append((*counts)[i]);
	if (!polysoup)
	{
	    GU_PrimPoly::buildBlock(&gdp, GA_Offset(startpoint), npoint,
			    pcounts, indices->get());
	}
	else
	{
	    GU_PrimPolySoup::build(&gdp, GA_Offset(startpoint), npoint,
			    pcounts, indices->get(), false);
	}
    }

    static void
    copyArrayToAttribute(GABC_GEOWalker &walk,
	    const GT_DataArrayHandle &array,
	    const char *name,
	    const char *abcname,
	    GA_AttributeOwner owner,
	    exint npoint,
	    exint nvertex,
	    exint nprim)
    {
	exint		len;
	exint		start;
	bool		extend = false;
	int		tsize = array->getTupleSize();
	switch (owner)
	{
	    case GA_ATTRIB_POINT:
		start = walk.pointCount();
		len = npoint;
		break;

	    case GA_ATTRIB_VERTEX:
		start = walk.vertexCount();
		len = nvertex;
		break;

	    case GA_ATTRIB_GLOBAL:
		// At the current time, we map global attributes to primitive
		// attributes.
		tsize = array->getTupleSize()*array->entries();
		owner = GA_ATTRIB_PRIMITIVE;
		extend = true;	// Copy data over
		// Fall Through to primitive case
	    case GA_ATTRIB_PRIMITIVE:
		start = walk.primitiveCount();
		len = nprim;
		break;

	    default:
		UT_ASSERT(0);
		return;
	}
	GU_Detail	&gdp = walk.detail();
	GA_Storage	 store = getGAStorage(array->getStorage());
	GA_TypeInfo	 tinfo = getGATypeInfo(array->getTypeInfo(), tsize);
	GA_RWAttributeRef aref = findAttribute(gdp, owner, name, abcname,
					    tsize, store,
					    GTtype(array->getTypeInfo()));
	if (!aref.isValid())
	    return;

	if (aref.getAttribute() != gdp.getP())
	{
	    if (tinfo == GA_TYPE_VECTOR)
		tinfo = isReallyVector(name, tsize);
	    aref.getAttribute()->setTypeInfo(tinfo);
	}
	if (aref.isString())
	    copyStrings(aref, array, start, start+len, extend);
	else if (aref.getAIFTuple() && GAisNumericStorage(store))
	    copyNumeric(aref, array, start, start+len, extend);
    }

    GA_AttributeOwner
    arbitraryGAOwner(const PropertyHeader &ph)
    {
	return getGAOwner(Alembic::AbcGeom::GetGeometryScope(ph.getMetaData()));
    }
    
    void
    fillArb(GABC_GEOWalker &walk, const GABC_IObject &obj,
		ICompoundProperty arb,
		exint npoint, exint nvertex, exint nprim)
    {
	exint	narb = arb ? arb.getNumProperties() : 0;

	for (exint i = 0; i < narb; ++i)
	{
	    const PropertyHeader	&head = arb.getPropertyHeader(i);
	    UT_String			 name(head.getName());
	    GA_AttributeOwner		 owner = arbitraryGAOwner(head);
	    if (!walk.translateAttributeName(owner, name))
		continue;
	    GEO_AnimationType	atype;
	    GT_DataArrayHandle	gt = obj.convertIProperty(arb, head,
					    walk.time(), &atype);
	    if (!gt) {
	        walk.errorHandler().warning("The following arbGeomParam could "
	                "not be read and was ignored: %s.",
	                name.buffer());
		continue;
	    }
	    copyArrayToAttribute(walk, gt, name, head.getName().c_str(),
		    owner, npoint, nvertex, nprim);
	}
    }

    void
    makeAbcPrim(GABC_GEOWalker &walk, const GABC_IObject &obj,
	    const ObjectHeader &ohead)
    {
	switch (obj.nodeType())
	{
	    case GABC_POLYMESH:
	    case GABC_SUBD:
	    case GABC_CURVES:
	    case GABC_POINTS:
	    case GABC_NUPATCH:
	    case GABC_XFORM:
		break;
	    default:
		return;	// Invalid primitive type
	}
	GU_PrimPacked	*packed = GABC_PackedImpl::build(walk.detail(),
				walk.filename(),
				obj,
				walk.time(),
				walk.includeXform(),
				walk.useVisibility());
	GABC_PackedImpl	*abc;
	abc = UTverify_cast<GABC_PackedImpl *>(packed->implementation());
	GA_Offset	pt = walk.getPointForAbcPrim();
	UT_ASSERT(GAisValid(pt));
	packed->setVertexPoint(pt);
	packed->setAttributeNameMap(walk.nameMapPtr());
	packed->setViewportLOD(walk.viewportLOD());
	abc->setUseTransform(walk.includeXform());
	if (!abc->isConstant())
	{
	    walk.setNonConstant();
	}
	walk.setPointLocation(packed, pt);
	walk.trackPtVtxPrim(obj, 0, 0, 1, false);
    }

    GEO_AnimationType
    getAnimationType(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	GEO_AnimationType	atype;
	atype = obj.getAnimationType(false);
	if (atype == GEO_ANIMATION_TOPOLOGY)
	{
	    walk.setNonConstantTopology();
	}
	return atype;
    }

    bool
    hasUniformAttributes(const ICompoundProperty &arb)
    {
	exint	narb = arb ? arb.getNumProperties() : 0;
	for (exint i = 0; i < narb; ++i)
	{
	    const PropertyHeader	&head = arb.getPropertyHeader(i);
	    GeometryScope		 scope;
	    scope = Alembic::AbcGeom::GetGeometryScope(head.getMetaData());
	    if (scope == Alembic::AbcGeom::kUnknownScope ||
		scope == Alembic::AbcGeom::kUniformScope)
		return true;
	}
	return false;
    }

    bool
    hasFaceSets(const GABC_IObject &obj)
    {
	// We assume that if there are any child objects, they are face sets
	exint	nkids = obj.getNumChildren();
	for (exint i = 0; i < nkids; ++i)
	{
	    GABC_IObject	kid(obj.getChild(i));
	    if (kid.nodeType() == GABC_FACESET)
		return true;
	}
	return false;
    }

    P4fArraySamplePtr
    rationalize(const P3fArraySamplePtr &points_ptr, const FloatArraySamplePtr &weights_ptr)
    {
        if (points_ptr->size() != weights_ptr->size())
        {
            UT_ASSERT(0);
            return P4fArraySamplePtr();
        }

        size_t              size = points_ptr->size();
        const V3f	    *points = points_ptr->get();
        const fpreal32	    *weights = weights_ptr->get();
        V4f                 *rationalized_vals = new V4f[size];

        for(exint i = 0; i < size; ++i)
        {
            rationalized_vals[i] = V4f(points[i] / weights[i]);
            rationalized_vals[i].w = weights[i];
        }

        // Use custom destructor to free rationized_vals memory
        return P4fArraySamplePtr(new P4fArraySample(rationalized_vals, size),
                Alembic::AbcCoreAbstract::TArrayDeleter<V4f>());
    }

    bool
    reusePolySoup(GABC_GEOWalker &walk)
    {
	// We're reusing primitives
	const GU_Detail		&gdp = walk.detail();
	exint			 nprim = walk.primitiveCount();
	if (gdp.getNumPrimitives() <= nprim)
	{
	    UT_ASSERT(0 && "Big problems here!");
	    return false;	// Not enough primitives - much bigger problem
	}
	const GEO_Primitive	*prim = gdp.getGEOPrimitive(GA_Offset(nprim));
	return prim->getTypeId() == GA_PRIMPOLYSOUP;
    }

    void
    makeSubD(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	ISubD			subd(obj.object(), gabcWrapExisting);
	ISubDSchema		&ps = subd.getSchema();
	P3fArraySamplePtr	P = ps.getPositionsProperty().getValue(iss);
	Int32ArraySamplePtr	counts=ps.getFaceCountsProperty().getValue(iss);
	Int32ArraySamplePtr	indices = ps.getFaceIndicesProperty().getValue(iss);
	exint			npoint = P->size();
	exint			nvertex = indices->size();
	exint			nprim = counts->size();

	//fprintf(stderr, "SubD: %d %d %d\n", int(npoint), int(nvertex), int(nprim));

	GEO_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GEO_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	else if (walk.reusePrimitives())
	{
	    if (reusePolySoup(walk))
		nprim = 1;
	    if (!walk.includeXform() || walk.transformConstant())
	    {
		walk.trackLastFace(nprim);
		walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, false);
		return;
	    }
	}
	if (!walk.reusePrimitives())
	{
	    bool	soup;
	    soup = (walk.polySoup() == GABC_GEOWalker::ABC_POLYSOUP_SUBD);
	    // If there are uniform attributes
	    if (soup && hasUniformAttributes(ps.getArbGeomParams()))
		soup = false;
	    if (soup && hasFaceSets(obj))
		soup = false;
	    // Assert that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() ==walk.primitiveCount());
	    if (soup)
		nprim = 1;
	    appendFaces(walk, npoint, counts, indices, soup);
	}
	else
	{
	    if (reusePolySoup(walk))
		nprim = 1;
	}

	// Set properties
	setAttribute(walk.detail(), GA_ATTRIB_POINT, "P", NULL,
		ps.getPositionsProperty().getValue(iss),
		walk.pointCount(), npoint);
	if (MATCH_PROPERTY(ps.getVelocitiesProperty(), iss, "v"))
	{
	    setAttribute(walk.detail(), GA_ATTRIB_POINT, "v", NULL,
		    ps.getVelocitiesProperty().getValue(iss),
		    walk.pointCount(), npoint);
	}
	if (MATCH_ATTRIBUTE(ps.getUVsParam(), "uv"))
	{
	    setGeomAttribute(walk, "uv", NULL, ps.getUVsParam(), iss,
		    npoint, nvertex, nprim);
	}
	fillArb(walk, obj, ps.getArbGeomParams(), npoint, nvertex, nprim);

	walk.trackLastFace(nprim);
	walk.trackSubd(nprim);
	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    static void
    makeFaceSet(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	IFaceSet		 faceset(obj.object(), gabcWrapExisting);
	IFaceSetSchema		&ss = faceset.getSchema();
	IFaceSetSchema::Sample	 sample = ss.getValue(walk.timeSample());
	Int32ArraySamplePtr	 faces = sample.getFaces();

	if (faces && faces->valid())
	{
	    GU_Detail		&gdp = walk.detail();
	    GA_PrimitiveGroup	*grp = NULL;
	    const int32		*indices = faces->get();
	    exint		 size = faces->size();
	    UT_String		 name(faceset.getName().c_str());
	    name.forceValidVariableName();
	    grp = gdp.findPrimitiveGroup(name);
	    if (!grp)
		grp = gdp.newPrimitiveGroup(name);
	    if (grp)
	    {
		for (exint i = 0; i < size; ++i)
		{
		    int	off = indices[i];
		    if (off >= 0 && off < walk.lastFaceCount())
			grp->addOffset(off + walk.lastFaceStart());
		}
	    }
	}
    }

    void
    makePolyMesh(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	IPolyMesh		polymesh(obj.object(), gabcWrapExisting);
	IPolyMeshSchema		&ps = polymesh.getSchema();
	P3fArraySamplePtr	P = ps.getPositionsProperty().getValue(iss);
	Int32ArraySamplePtr	counts=ps.getFaceCountsProperty().getValue(iss);
	Int32ArraySamplePtr	indices = ps.getFaceIndicesProperty().getValue(iss);
	exint			npoint = P->size();
	exint			nvertex = indices->size();
	exint			nprim = counts->size();

	//fprintf(stderr, "PolyMesh %s: %d %d %d\n", obj.getFullName().c_str(), int(npoint), int(nvertex), int(nprim));

	GEO_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GEO_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	else if (walk.reusePrimitives())
	{
	    if (reusePolySoup(walk))
		nprim = 1;
	    if (!walk.includeXform() || walk.transformConstant())
	    {
		walk.trackLastFace(nprim);
		walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, false);
		return;
	    }
	}
	if (!walk.reusePrimitives())
	{
	    bool	soup;
	    soup = (walk.polySoup() != GABC_GEOWalker::ABC_POLYSOUP_NONE);
	    // If there are uniform attributes
	    if (soup && hasUniformAttributes(ps.getArbGeomParams()))
		soup = false;
	    if (soup && hasFaceSets(obj))
		soup = false;
	    // Assert that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() ==walk.primitiveCount());
	    if (soup)
		nprim = 1;
	    appendFaces(walk, npoint, counts, indices, soup);
	}
	else
	{
	    if (reusePolySoup(walk))
		nprim = 1;
	}

	// Set properties
	setAttribute(walk.detail(), GA_ATTRIB_POINT, "P", NULL,
		ps.getPositionsProperty().getValue(iss),
		walk.pointCount(), npoint);
	if (MATCH_PROPERTY(ps.getVelocitiesProperty(), iss, "v"))
	{
	    setAttribute(walk.detail(), GA_ATTRIB_POINT, "v", NULL,
		    ps.getVelocitiesProperty().getValue(iss),
		    walk.pointCount(), npoint);
	}
	if (MATCH_ATTRIBUTE(ps.getUVsParam(), "uv"))
	{
	    setGeomAttribute(walk, "uv", NULL, ps.getUVsParam(), iss,
		    npoint, nvertex, nprim);
	}
	if (MATCH_ATTRIBUTE(ps.getNormalsParam(), "N"))
	{
	    setGeomAttribute(walk, "N", NULL, ps.getNormalsParam(), iss,
		    npoint, nvertex, nprim);
	}
	fillArb(walk, obj, ps.getArbGeomParams(), npoint, nvertex, nprim);

	walk.trackLastFace(nprim);
	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    void
    makeCurves(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	ICurves			curves(obj.object(), gabcWrapExisting);
	ICurvesSchema		&cs = curves.getSchema();
	ICurvesSample		c_sample = cs.getValue(iss);
	P3fArraySamplePtr	points = cs.getPositionsProperty()
	                                .getValue(iss);
	Int32ArraySamplePtr	nvtx = cs.getNumVerticesProperty()
	                                .getValue(iss);
	FloatArraySamplePtr     knots = c_sample.getKnots();
        BasisType               type = c_sample.getBasis();
        exint			npoint = points->size();
	exint			nvertex = npoint;
	exint			nprim = nvtx->size();
	int                     uorder;

	switch (c_sample.getType())
        {
            case Alembic::AbcGeom::kCubic:
                uorder = 4;
                break;
            case Alembic::AbcGeom::kLinear:
                uorder = 2;
                break;
            case Alembic::AbcGeom::kVariableOrder:
            {
                UcharArraySamplePtr     abcOrders = c_sample.getOrders();
                if (!isEmpty(abcOrders))
                {
                    uorder = abcOrders->get()[0];
                    break;
                }
            }
            default:
                uorder = 2;
                type = Alembic::AbcGeom::kNoBasis;
        }

	GEO_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GEO_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	else if (walk.reusePrimitives())
	{
	    if (!walk.includeXform() || walk.transformConstant())
	    {
		walk.trackLastFace(nprim);
		walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, false);
		return;
	    }
	}
	if (!walk.reusePrimitives())
	{
	    // Assert that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() == walk.primitiveCount());
	    appendCurves(walk, type, nvtx, npoint, uorder);
	}

	if (type == Alembic::AbcGeom::kBsplineBasis && !isEmpty(knots))
	{
	    exint       offset = 0;
	    exint       prims_added = nvtx->size();
	    exint       prims_prior = walk.primitiveCount();

	    for (exint i = 0; i < prims_added; ++i)
	    {
                GA_Offset	        primoff = GA_Offset(prims_prior + i);
                GU_PrimNURBCurve	*curve = UTverify_cast<GU_PrimNURBCurve *>(
                                            walk.detail().getGEOPrimitive(primoff));
                offset += setKnotVector(*curve->getBasis(), knots, offset);
            }
	}

	// Set properties
	if (MATCH_PROPERTY(cs.getPositionWeightsProperty(), iss, "Pw"))
        {
            setAttribute(walk.detail(), GA_ATTRIB_POINT, "P", NULL,
                        rationalize(points, cs.getPositionWeightsProperty().getValue(iss)),
                        walk.pointCount(), npoint);
        }
        else
        {
            setAttribute(walk.detail(), GA_ATTRIB_POINT, "P", NULL,
                        points,
                        walk.pointCount(), npoint);
        }

	if (cs.getVelocitiesProperty().valid())
	if (MATCH_PROPERTY(cs.getVelocitiesProperty(), iss, "v"))
	{
	    setAttribute(walk.detail(), GA_ATTRIB_POINT, "v", NULL,
		    cs.getVelocitiesProperty().getValue(iss),
		    walk.pointCount(), npoint);
	}
	if (MATCH_ATTRIBUTE(cs.getUVsParam(), "uv"))
	{
	    setGeomAttribute(walk, "uv", NULL, cs.getUVsParam(), iss,
		    npoint, nvertex, nprim);
	}
	if (MATCH_ATTRIBUTE(cs.getWidthsParam(), "width"))
	{
	    setGeomAttribute(walk, "width", NULL, cs.getWidthsParam(), iss,
		    npoint, nvertex, nprim);
	}
	if (MATCH_ATTRIBUTE(cs.getNormalsParam(), "N"))
	{
	    setGeomAttribute(walk, "N", NULL, cs.getNormalsParam(), iss,
		    npoint, nvertex, nprim);
	}
	fillArb(walk, obj, cs.getArbGeomParams(), npoint, nvertex, nprim);

	walk.trackLastFace(nprim);
	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    static void
    locatorAttribute(GABC_GEOWalker &walk, const char *name,
		    fpreal x, fpreal y, fpreal z)
    {
	GA_RWAttributeRef	href = findAttribute(walk.detail(),
						GA_ATTRIB_PRIMITIVE,
						name, NULL, 3, GA_STORE_REAL64,
						"float");
	GA_RWHandleV3		h(href.getAttribute());
	if (h.isValid())
	{
	    UT_Vector3	v(x, y, z);
	    h.set(GA_Offset(walk.primitiveCount()), v);
	}
    }

    void
    makeLocator(GABC_GEOWalker &walk, IXform &xform, const GABC_IObject &obj)
    {
	Alembic::Abc::IScalarProperty	loc(xform.getProperties(), "locator");
	ISampleSelector			iss = walk.timeSample();
	const int			npoint = 1;
	const int			nvertex = 1;
	const int			nprim = 1;

	GEO_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GEO_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	else if (walk.reusePrimitives())
	{
	    if (!walk.includeXform() || walk.transformConstant())
	    {
		walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, false);
		return;
	    }
	}
	if (!walk.reusePrimitives())
	{
	    // Assert that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() ==walk.primitiveCount());
	    appendParticles(walk, npoint);
	}

	double	ldata[6];	// Local translate/scale
	V3d	ls, lh, lr, lt;

	loc.get(ldata, iss);
	if (!Imath::extractSHRT(walk.getTransform(), ls, lh, lr, lt))
	{
	    ls = V3d(1,1,1);
	    lr = V3d(0,0,0);
	    lt = V3d(0,0,0);
	}

	walk.detail().setPos3(GA_Offset(walk.pointCount()),
			ldata[0], ldata[1], ldata[2]);
	locatorAttribute(walk, "localPosition", ldata[0], ldata[1], ldata[2]);
	locatorAttribute(walk, "localScale", ldata[3], ldata[4], ldata[5]);
	locatorAttribute(walk, "parentTrans", lt.x, lt.y, lt.z);
	locatorAttribute(walk, "parentRot", lr.x, lr.y, lr.z);
	locatorAttribute(walk, "parentScale", ls.x, ls.y, ls.z);

	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    void
    makePoints(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	IPoints			points(obj.object(), gabcWrapExisting);
	IPointsSchema		&ps = points.getSchema();
	P3fArraySamplePtr	P = ps.getPositionsProperty().getValue(iss);

	exint			npoint = P->size();
	exint			nvertex = npoint;
	exint			nprim = 1;

	//fprintf(stderr, "Points: %d %d %d\n", int(npoint), int(nvertex), int(nprim));

	GEO_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GEO_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	else if (walk.reusePrimitives())
	{
	    if (!walk.includeXform() || walk.transformConstant())
	    {
		walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, false);
		return;
	    }
	}
	if (!walk.reusePrimitives())
	{
	    // Assert that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() ==walk.primitiveCount());
	    appendParticles(walk, npoint);
	}

	// Set properties
	setAttribute(walk.detail(), GA_ATTRIB_POINT, "P", NULL,
		ps.getPositionsProperty().getValue(iss),
		walk.pointCount(), npoint);
	if (MATCH_PROPERTY(ps.getVelocitiesProperty(), iss, "v"))
	{
	    setAttribute(walk.detail(), GA_ATTRIB_POINT, "v", NULL,
		    ps.getVelocitiesProperty().getValue(iss),
		    walk.pointCount(), npoint);
	}
	if (MATCH_PROPERTY(ps.getIdsProperty(), iss, "id"))
	{
	    setAttribute(walk.detail(), GA_ATTRIB_POINT, "id", NULL,
		    ps.getIdsProperty().getValue(iss),
		    walk.pointCount(), npoint);
	}
	Alembic::AbcGeom::IFloatGeomParam	widths = ps.getWidthsParam();
	if (MATCH_ATTRIBUTE(widths, "width"))
	{
	    setGeomAttribute(walk, "width", NULL, widths, iss,
		    npoint, nvertex, nprim);
	}
	fillArb(walk, obj, ps.getArbGeomParams(), npoint, nvertex, nprim);

	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    void
    makeNuPatch(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	INuPatch		nupatch(obj.object(), gabcWrapExisting);
	INuPatchSchema		&ps = nupatch.getSchema();
	INuPatchSample		patch = ps.getValue(iss);
	FloatArraySamplePtr	uknots = patch.getUKnot();
	FloatArraySamplePtr	vknots = patch.getVKnot();
	int			uorder = patch.getUOrder();
	int			vorder = patch.getVOrder();
	P3fArraySamplePtr	points = ps.getPositionsProperty().getValue(iss);
	exint			npoint = points->size();
	exint			nvertex = npoint;
	exint			nprim = 1;

	// Verify that we have the expected point count
	UT_ASSERT(npoint == (uknots->size()-uorder)*(vknots->size()-vorder));

	//fprintf(stderr, "NuPatch: %d %d %d\n", int(npoint), int(nvertex), int(nprim));

	GEO_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GEO_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	else if (walk.reusePrimitives())
	{
	    if (!walk.includeXform() || walk.transformConstant())
	    {
		walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, false);
		return;
	    }
	}
	if (!walk.reusePrimitives())
	{
	    // Assert that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() == walk.primitiveCount());
	    appendNURBS(walk, uorder, uknots, vorder, vknots);
	}
	GA_Offset	 primoff = GA_Offset(walk.primitiveCount());
	GU_PrimNURBSurf	*surf = UTverify_cast<GU_PrimNURBSurf *>(
			    walk.detail().getGEOPrimitive(primoff));
	setKnotVector(*surf->getUBasis(), uknots);
	setKnotVector(*surf->getVBasis(), vknots);

	// Set properties
        if (MATCH_PROPERTY(ps.getPositionWeightsProperty(), iss, "Pw"))
        {
            setAttribute(walk.detail(), GA_ATTRIB_POINT, "P", NULL,
        		rationalize(points, ps.getPositionWeightsProperty().getValue(iss)),
        		walk.pointCount(), npoint);
        }
        else
        {
            setAttribute(walk.detail(), GA_ATTRIB_POINT, "P", NULL,
            		points,
            		walk.pointCount(), npoint);
        }

	if (MATCH_PROPERTY(ps.getVelocitiesProperty(), iss, "v"))
	{
	    setAttribute(walk.detail(), GA_ATTRIB_POINT, "v", NULL,
		    ps.getVelocitiesProperty().getValue(iss),
		    walk.pointCount(), npoint);
	}
	if (MATCH_ATTRIBUTE(ps.getUVsParam(), "uv"))
	{
	    setGeomAttribute(walk, "uv", NULL, ps.getUVsParam(), iss,
		    npoint, nvertex, nprim);
	}
	if (MATCH_ATTRIBUTE(ps.getNormalsParam(), "N"))
	{
	    setGeomAttribute(walk, "N", NULL, ps.getNormalsParam(), iss,
		    npoint, nvertex, nprim);
	}
	fillArb(walk, obj, ps.getArbGeomParams(), npoint, nvertex, nprim);

	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    static void
    buildPointCloud(GABC_GEOWalker &walk, const GABC_IObject &obj,
		    const P3fArraySamplePtr &P)
    {
	GU_Detail		&gdp = walk.detail();
	exint			 startpoint = walk.pointCount();
	exint			 npoint = P->size();
	const Imath::V3f	*Pdata = P->get();

	if (!walk.reusePrimitives())
	{
	    UT_VERIFY(gdp.appendPointBlock(npoint) == GA_Offset(startpoint));
	}
	for (exint i = 0; i < npoint; ++i)
	{
	    GA_Offset	pt = GA_Offset(startpoint+i);
	    gdp.setPos3(pt, Pdata[i].x, Pdata[i].y, Pdata[i].z);
	}
	UT_String	groupname;
	if (walk.getGroupName(groupname, obj))
	{
	    GA_PointGroup	*g = gdp.newPointGroup(groupname);
	    for (exint i = 0; i < npoint; ++i)
		g->addOffset(GA_Offset(startpoint+i));
	}
	if (getAnimationType(walk, obj) != GEO_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	walk.trackPtVtxPrim(obj, npoint, 0, 0, true);
    }

    template <typename ABC_T, typename SCHEMA_T>
    static void
    makePointMesh(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	ISampleSelector		 iss = walk.timeSample();
	ABC_T			 prim(obj.object(), gabcWrapExisting);
	SCHEMA_T		&ps = prim.getSchema();
	P3fArraySamplePtr	 P = ps.getPositionsProperty().getValue(iss);
	buildPointCloud(walk, obj, P);
    }

    // Vertex->point mappings for box.  The points are expected to be in the
    // order:
    //   0: (xmin, ymin, zmin)
    //   1: (xmax, ymin, zmin)
    //   2: (xmin, ymax, zmin)
    //   3: (xmax, ymax, zmin)
    //   4: (xmin, ymin, zmax)
    //   5: (xmax, ymin, zmax)
    //   6: (xmin, ymax, zmax)
    //   7: (xmax, ymax, zmax)
    static int	boxVertexMap[] = {
	0, 1, 3, 2,	// Front face
	1, 5, 7, 3,	// Right face
	5, 4, 6, 7,	// Back face
	4, 0, 2, 6,	// Left face
	2, 3, 7, 6,	// Top face
	0, 4, 5, 1,	// Bottom face
    };

    static void
    makeBox(GU_Detail &gdp)
    {
	static GEO_PolyCounts *theCounts = 0;
	if (!theCounts)
	{
	    theCounts = new GEO_PolyCounts();
	    theCounts->append(4, 6);
	}

	GA_Offset firstpoint = gdp.appendPointBlock(8);
	GU_PrimPolySoup::build(&gdp, firstpoint, 8, *theCounts,
		boxVertexMap);
	gdp.getTopology().validate();
    }

    static void
    setBoxPositions(GU_Detail &gdp, const UT_BoundingBox &box,
	    exint start)
    {
	gdp.setPos3(GA_Offset(start+0), box.xmin(), box.ymin(), box.zmin());
	gdp.setPos3(GA_Offset(start+1), box.xmax(), box.ymin(), box.zmin());
	gdp.setPos3(GA_Offset(start+2), box.xmin(), box.ymax(), box.zmin());
	gdp.setPos3(GA_Offset(start+3), box.xmax(), box.ymax(), box.zmin());
	gdp.setPos3(GA_Offset(start+4), box.xmin(), box.ymin(), box.zmax());
	gdp.setPos3(GA_Offset(start+5), box.xmax(), box.ymin(), box.zmax());
	gdp.setPos3(GA_Offset(start+6), box.xmin(), box.ymax(), box.zmax());
	gdp.setPos3(GA_Offset(start+7), box.xmax(), box.ymax(), box.zmax());
    }

    static void
    makeHoudiniBox(GABC_GEOWalker &walk, const GABC_IObject &obj,
	    const UT_BoundingBox &box)
    {
	GU_Detail	&gdp = walk.detail();
	if (!walk.reusePrimitives())
	    makeBox(gdp);
	setBoxPositions(gdp, box, walk.pointCount());
	if (getAnimationType(walk, obj) != GEO_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	walk.trackPtVtxPrim(obj, 8, 8, 1, true);
    }
}

GABC_GEOWalker::GABC_GEOWalker(GU_Detail &gdp, GABC_IError &err)
    : myDetail(gdp)
    , myErrorHandler(err)
    , mySubdGroup(NULL)
    , myObjectPattern("*")
    , myNameMapPtr()
    , myBoss(UTgetInterrupt())
    , myBossId(-1)
    , myMatrix(identity44d)
    , myPathAttribute()
    , myLastFaceCount(0)
    , myLastFaceStart(0)
    , myAbcPrimPointMode(ABCPRIM_CENTROID_POINT)
    , myPolySoup(ABC_POLYSOUP_POLYMESH)
    , myViewportLOD(GEO_VIEWPORT_FULL)
    , myAbcSharedPoint(GA_INVALID_OFFSET)
    , myTime(0)
    , myPointCount(0)
    , myVertexCount(0)
    , myPrimitiveCount(0)
    , myGroupMode(ABC_GROUP_SHAPE_NODE)
    , myBoxCullMode(BOX_CULL_IGNORE)
    , myAnimationFilter(ABC_AFILTER_ALL)
    , myIncludeXform(true)
    , myUseVisibility(true)
    , myReusePrimitives(false)
    , myBuildLocator(true)
    , myLoadMode(LOAD_ABC_PRIMITIVES)
    , myBuildAbcShape(true)
    , myBuildAbcXform(false)
    , myPathAttributeChanged(true)
    , myIsConstant(true)
    , myTopologyConstant(true)
    , myTransformConstant(true)
    , myAllTransformConstant(true)
    , myRebuiltNURBS(false)
{
    if (myBoss)
    {
	myBoss->opStart("Building geometry from Alembic tree",
			0, 0, &myBossId);
    }
    myCullBox.makeInvalid();	// 
}

GABC_GEOWalker::~GABC_GEOWalker()
{
    // In theory, this should be true even if we were interrupted.
    if (myBoss)
	myBoss->opEnd(myBossId);
}

void
GABC_GEOWalker::setReusePrimitives(bool v)
{
    myReusePrimitives = v &&
	(myDetail.getNumPrimitives() > 0 || myLoadMode == LOAD_HOUDINI_POINTS);
}

void
GABC_GEOWalker::setBounds(BoxCullMode mode, const UT_BoundingBox &box)
{
    myBoxCullMode = mode;
    myCullBox = box;
    if (!myCullBox.isValid())
	myBoxCullMode = BOX_CULL_IGNORE;
}

void
GABC_GEOWalker::setPointMode(AbcPrimPointMode mode, GA_Offset shared_point)
{
    myAbcPrimPointMode = mode;
    myAbcSharedPoint = shared_point;
}

void
GABC_GEOWalker::setPathAttribute(const GA_RWAttributeRef &a)
{
    GA_RWHandleS	h(a.getAttribute());
    UT_ASSERT(h.isValid() && "Require a string attribute!");
    if (h.isValid())
	myPathAttribute = h;
}

void
GABC_GEOWalker::updateAbcPrims()
{
    bool	setPath = pathAttributeChanged() && myPathAttribute.isValid();
    for (GA_Iterator it(detail().getPrimitiveRange()); !it.atEnd(); ++it)
    {
	GEO_Primitive	*prim = detail().getGEOPrimitive(*it);
	GU_PrimPacked	*pack = UTverify_cast<GU_PrimPacked *>(prim);
	GABC_PackedImpl	*abc = UTverify_cast<GABC_PackedImpl *>(pack->implementation());
	if (!abc->isConstant())
	    setNonConstant();
	abc->setFrame(time());
	if (setPath)
	    myPathAttribute.set(prim->getMapOffset(), abc->objectPath().c_str());
	setPointLocation(pack, pack->getPointOffset(0));
    }
}

bool
GABC_GEOWalker::preProcess(const GABC_IObject &root)
{
    GABC_IObject	parent = root.getParent();
    if (parent.valid())
    {
	UT_Matrix4D	m;
	bool		c, i;
	if (!GABC_Util::getWorldTransform(filename(), parent, time(), m, c, i))
	{
	    m.identity();
	}
	for (int r = 0; r < 4; ++r)
	    for (int c = 0; c < 4; ++c)
		myMatrix.x[r][c] = m(r,c);

        do
        {
            if (parent.getAnimationType(false) != GEO_ANIMATION_CONSTANT)
            {
                myIsConstant = false;
                myTransformConstant = false;
                myAllTransformConstant = false;
                break;
            }

            parent = parent.getParent();
        } while (parent.valid());
    }
    else
    {
	myMatrix = identity44d;
    }

    return true;
}

bool
GABC_GEOWalker::process(const GABC_IObject &obj)
{
    const ObjectHeader	&ohead = obj.getHeader();

    if (useVisibility())
    {
	bool			animated;
	GABC_VisibilityType	vtype;
	vtype = obj.visibility(animated, time(), false);
	if (animated)
	{
	    // Since visibility is animated, the topology will change
	    setNonConstant();
	    setNonConstantTopology();
	}
	if (vtype == GABC_VISIBLE_HIDDEN)
	    return false;	// Don't process children
    }

    //fprintf(stderr, "Process: %s\n", (const char *)obj.getFullName().c_str());
    if (IXform::matches(ohead))
    {
	IXform	xform(obj.object(), gabcWrapExisting);

	if (buildLocator() && obj.isMayaLocator())
	{
	    if (filterObject(obj))
	    {
		if (buildAbcPrim())
		    makeAbcPrim(*this, obj, ohead);
		else
		    makeLocator(*this, xform, obj);
	    }
	    return true;	// Process locator children
	}
	else if (includeXform())
	{
	    PushTransform	push(*this, xform);
	    if (buildAbcPrim() && buildAbcXform() && filterObject(obj))
		makeAbcPrim(*this, obj, ohead);
	    walkChildren(obj);
	    return false;	// Since we walked manually, return false
	}
	else if (buildAbcPrim() && buildAbcXform())
	{
	    makeAbcPrim(*this, obj, ohead);
	    // Fall through to process children
	}
	// Let the walker just process children naturally
	return true;
    }

    if (buildAbcShape() && filterObject(obj))
    {
	switch (myLoadMode)
	{
	    case LOAD_ABC_PRIMITIVES:
		makeAbcPrim(*this, obj, ohead);
		break;
	    case LOAD_HOUDINI_PRIMITIVES:
		switch (obj.nodeType())
		{
		    case GABC_POLYMESH:
			makePolyMesh(*this, obj);
			break;
		    case GABC_SUBD:
			makeSubD(*this, obj);
			break;
		    case GABC_CURVES:
			makeCurves(*this, obj);
			break;
		    case GABC_POINTS:
			makePoints(*this, obj);
			break;
		    case GABC_NUPATCH:
			makeNuPatch(*this, obj);
			break;
		    case GABC_FACESET:
			makeFaceSet(*this, obj);
			break;

		    case GABC_CAMERA:	// Ignore these leaf nodes
		    case GABC_LIGHT:
		    case GABC_MATERIAL:
			break;

		    default:
		    {
			GABC_IObject	parent = obj.getParent();
			if (parent.valid())
			{
			    fprintf(stderr,
				    "Unknown alembic node type: %s\n",
				    obj.getFullName().c_str());
			}
		    }
		}
		break;

	    case LOAD_HOUDINI_POINTS:
		switch (obj.nodeType())
		{
		    case GABC_POLYMESH:
			makePointMesh<IPolyMesh, IPolyMeshSchema>(*this, obj);
			break;
		    case GABC_SUBD:
			makePointMesh<ISubD, ISubDSchema>(*this, obj);
			break;
		    case GABC_CURVES:
			makePointMesh<ICurves, ICurvesSchema>(*this, obj);
			break;
		    case GABC_POINTS:
			makePointMesh<IPoints, IPointsSchema>(*this, obj);
			break;
		    case GABC_NUPATCH:
			makePointMesh<INuPatch, INuPatchSchema>(*this, obj);
			break;

		    case GABC_CAMERA:	// Ignore these leaf nodes
		    case GABC_FACESET:
		    case GABC_LIGHT:
		    case GABC_MATERIAL:
			break;

		    default:
		    {
			GABC_IObject	parent = obj.getParent();
			if (parent.valid())
			{
			    fprintf(stderr,
				    "Unknown alembic node type: %s\n",
				    obj.getFullName().c_str());
			}
		    }
		}
		break;
	    case LOAD_HOUDINI_BOXES:
		{
		    UT_BoundingBox	box;
		    bool		isConstant;
		    if (obj.getBoundingBox(box, myTime, isConstant))
			makeHoudiniBox(*this, obj, box);
		}
		break;
	    default:
		UT_ASSERT(0);
	}
    }

    return true;
}

bool
GABC_GEOWalker::interrupted() const
{
    return myBoss && myBoss->opInterrupt();
}

bool
GABC_GEOWalker::filterObject(const GABC_IObject &obj) const
{
    return matchObjectName(obj) &&
	    matchAnimationFilter(obj) &&
	    matchBounds(obj);
}

bool
GABC_GEOWalker::matchObjectName(const GABC_IObject &obj) const
{
    UT_String	path(obj.getFullName());
    return path.multiMatch(objectPattern());
}

bool
GABC_GEOWalker::matchAnimationFilter(const GABC_IObject &obj) const
{
    if (myAnimationFilter == ABC_AFILTER_ALL)
    {
	return true;
    }

    bool	animating = !transformConstant();
    if (!animating)
    {
	// If none of the transforms in are animating, maybe the object itself
	// is animating.
	animating = obj.getAnimationType(false);
    }
    switch (myAnimationFilter)
    {
	case ABC_AFILTER_STATIC:
	    return !animating;
	case ABC_AFILTER_ANIMATING:
	    return animating;
	case ABC_AFILTER_ALL:
	    UT_ASSERT(0 && "Impossible code!");
    }
    return true;
}

bool
GABC_GEOWalker::matchBounds(const GABC_IObject &obj) const
{
    if (myBoxCullMode == BOX_CULL_IGNORE)
	return true;

    if (IXform::matches(obj.getHeader()))
	return true;	// Any transform nodes match

    bool		isConstant;
    UT_BoundingBox	box;

    obj.getBoundingBox(box, myTime, isConstant);
    if (!isConstant)
    {
	// If the bounding box changes over time, then we may be culled in the
	// future, meaning that the primitive count is non-constant.
	const_cast<GABC_GEOWalker *>(this)->setNonConstant();
	const_cast<GABC_GEOWalker *>(this)->setNonConstantTopology();
    }
    if (includeXform())
    {
	// The top of our transform stack is the world space transform for the
	// shape.
	box.transform(UT_Matrix4(myMatrix.x));
    }
    switch (myBoxCullMode)
    {
	case BOX_CULL_ANY_INSIDE:
	    return myCullBox.intersects(box) != 0;
	case BOX_CULL_INSIDE:
	    return box.isInside(myCullBox) != 0;
	case BOX_CULL_ANY_OUTSIDE:
	    return box.isInside(myCullBox) == 0;
	case BOX_CULL_OUTSIDE:
	    return myCullBox.intersects(box) == 0;
	case BOX_CULL_IGNORE:
	    UT_ASSERT_P(0);
    }
    UT_ASSERT_P(0 && "Unexpected case");
    return true;
}

bool
GABC_GEOWalker::translateAttributeName(GA_AttributeOwner own, UT_String &name)
{
    if (nameMapPtr())
    {
	if (!nameMapPtr()->matchPattern(own, name))
	    return false;
	name = nameMapPtr()->getName(name);
	if (!name.isstring())
	    return false;
    }
    name.forceValidVariableName();
    return true;
}

void
GABC_GEOWalker::pushTransform(const M44d &xform, bool const_xform,
	GABC_GEOWalker::TransformState &stash,
	bool inheritXforms)
{
    stash.push(myMatrix, myTransformConstant);
    if (!const_xform)
    {
        myIsConstant = false;
	myTransformConstant = false;
	myAllTransformConstant = false;
    }
    if (inheritXforms && includeXform())
	myMatrix = xform * myMatrix;
    else
	myMatrix = xform;
}
void
GABC_GEOWalker::popTransform(const GABC_GEOWalker::TransformState &stash)
{
    stash.pop(myMatrix, myTransformConstant);
}

static GABC_IObject
getParentXform(GABC_IObject kid)
{
    GABC_IObject	parent;
    while (true)
    {
	parent = kid.getParent();
	if (!parent.valid())
	{
	    UT_ASSERT(0 && "There should have been a transform");
	    return kid;
	}
	if (IXform::matches(parent.getHeader()))
	    return parent;
	kid = parent;
    }
}

bool
GABC_GEOWalker::getGroupName(UT_String &name, const GABC_IObject &obj) const
{
    switch (myGroupMode)
    {
	case ABC_GROUP_NONE:
	    return false;	// No group name
	case ABC_GROUP_SHAPE_NODE:
	    name.harden(obj.getFullName().c_str());
	    break;
	case ABC_GROUP_XFORM_NODE:
	    name.harden(getParentXform(obj).getFullName().c_str());
	    break;
    }
    name.forceValidVariableName();
    return true;
}

GA_Offset
GABC_GEOWalker::getPointForAbcPrim()
{
    switch (myAbcPrimPointMode)
    {
	case ABCPRIM_SHARED_POINT:
	    if (!GAisValid(myAbcSharedPoint))
		myAbcSharedPoint = myDetail.appendPointOffset();
	    return myAbcSharedPoint;
	case ABCPRIM_UNIQUE_POINT:
	case ABCPRIM_CENTROID_POINT:
	    return myDetail.appendPointOffset();
    }
    return GA_INVALID_OFFSET;
}

void
GABC_GEOWalker::setPointLocation(GU_PrimPacked *prim, GA_Offset pt) const
{

    switch (myAbcPrimPointMode)
    {
	case ABCPRIM_UNIQUE_POINT:
	    myDetail.setPos3(pt, 0, 0, 0);
	    break;
	case ABCPRIM_CENTROID_POINT:
	{
	    prim->movePivotToCentroid();
	    break;
	}
	default:
	    break;
    }
}

void
GABC_GEOWalker::trackLastFace(GA_Size nfaces)
{
    UT_ASSERT(nfaces >= 0);
    UT_ASSERT(myDetail.getNumPrimitives() >= myPrimitiveCount + nfaces);
    myLastFaceStart = GA_Offset(myPrimitiveCount);
    myLastFaceCount = nfaces;
}

void
GABC_GEOWalker::trackSubd(GA_Size nfaces)
{
    if (mySubdGroup)
    {
	for (exint i = 0; i < nfaces; ++i)
	    mySubdGroup->addOffset(GA_Offset(myPrimitiveCount+i));
    }
}

void
GABC_GEOWalker::trackPtVtxPrim(const GABC_IObject &obj,
				exint npoint, exint nvertex, exint nprim,
				bool do_transform)
{
    UT_ASSERT(myDetail.getNumPoints() >= myPointCount + npoint);
    UT_ASSERT(myDetail.getNumVertices() >= myVertexCount + nvertex);
    UT_ASSERT(myDetail.getNumPrimitives() >= myPrimitiveCount + nprim);
    if (nprim && myPathAttribute.isValid() && pathAttributeChanged())
    {
	std::string	 pathStr = obj.getFullName();
	const char	*path = pathStr.c_str();
	for (exint i = 0; i < nprim; ++i)
	{
	    myPathAttribute.set(GA_Offset(myPrimitiveCount+i), path);
	}
    }
    UT_String		 gname;
    GA_PrimitiveGroup	*g = NULL;
    if (nprim && getGroupName(gname, obj))
    {
	g = myDetail.newPrimitiveGroup(gname);
	for (exint i = 0; i < nprim; ++i)
	{
	    g->addOffset(GA_Offset(myPrimitiveCount+i));
	}
    }
    if (do_transform && !buildAbcPrim() &&
	    includeXform() && myMatrix != identity44d)
    {
	GA_Range	xprims = GA_Range(detail().getPrimitiveMap(),
				GA_Offset(myPrimitiveCount),
				GA_Offset(myPrimitiveCount + nprim));
	GA_Range	xpoints = GA_Range(detail().getPointMap(),
				GA_Offset(myPointCount),
				GA_Offset(myPointCount + npoint));
	UT_Matrix4	m4(myMatrix.x);
	// Transform detail and attributes
	detail().transform(m4, xprims, xpoints, false, false, false);
    }
    myPointCount += npoint;
    myVertexCount += nvertex;
    myPrimitiveCount += nprim;
}

#include <UT/UT_StopWatch.h>
void
GABC_GEOWalker::test()
{
    UT_StopWatch	timer;
    timer.start();
    {
	GU_Detail		gdp;
	GABC_IError             err(UTgetInterrupt());
	GABC_GEOWalker	walk(gdp, err);
	if (GABC_Util::walk("test.abc", walk))
	{
	    fprintf(stderr, "Build: %g\n", timer.lap());
	    gdp.save("test.bgeo", NULL);
	    fprintf(stderr, "Save: %g\n", timer.lap());
	}
	else
	    fprintf(stderr, "Unable to walk test.abc\n");
    }
    fprintf(stderr, "Done: %g\n", timer.lap());
}
