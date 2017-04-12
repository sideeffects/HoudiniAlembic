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

#include "GABC_GEOWalker.h"
#include "GABC_PackedImpl.h"
#include <Alembic/AbcGeom/All.h>
#include <GEO/GEO_PrimNURBCurve.h>
#include <GEO/GEO_PrimRBezCurve.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PrimPolySoup.h>
#include <GU/GU_PrimPart.h>
#include <GU/GU_PrimNURBCurve.h>
#include <GU/GU_PrimNURBSurf.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_StackBuffer.h>
#include <UT/UT_WorkArgs.h>
#include <algorithm>

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
    using M44d = Imath::M44d;
    using V3f = Imath::V3f;
    using V3d = Imath::V3d;
    using V4f = Imath::V4f;

    using DataType = Alembic::Abc::DataType;
    using Dimensions = Alembic::Abc::Dimensions;
    using MetaData = Alembic::Abc::MetaData;
    using ISampleSelector = Alembic::Abc::ISampleSelector;
    using ObjectHeader = Alembic::Abc::ObjectHeader;
    using PropertyHeader = Alembic::Abc::PropertyHeader;
    using WrapExistingFlag = Alembic::Abc::WrapExistingFlag;

    using CompoundPropertyReaderPtr = Alembic::Abc::CompoundPropertyReaderPtr;
    using ICompoundProperty = Alembic::Abc::ICompoundProperty;
    using IArrayProperty = Alembic::Abc::IArrayProperty;

    using ArraySample = Alembic::Abc::ArraySample;
    using ArraySamplePtr = Alembic::Abc::ArraySamplePtr;
    using UcharArraySamplePtr = Alembic::Abc::UcharArraySamplePtr;
    using Int32ArraySamplePtr = Alembic::Abc::Int32ArraySamplePtr;
    using UInt64ArraySamplePtr = Alembic::Abc::UInt64ArraySamplePtr;
    using FloatArraySamplePtr = Alembic::Abc::FloatArraySamplePtr;
    using P3fArraySamplePtr = Alembic::Abc::P3fArraySamplePtr;
    using V3fArraySamplePtr = Alembic::Abc::V3fArraySamplePtr;

    using IUInt64ArrayProperty = Alembic::Abc::IUInt64ArrayProperty;
    using IP3fArrayProperty = Alembic::Abc::IP3fArrayProperty;
    using IV3fArrayProperty = Alembic::Abc::IV3fArrayProperty;

    using BasisType = Alembic::AbcGeom::BasisType;
    using CurvePeriodicity = Alembic::AbcGeom::CurvePeriodicity;
    using GeometryScope = Alembic::AbcGeom::GeometryScope;

    using IFloatGeomParam = Alembic::AbcGeom::IFloatGeomParam;
    using IV2fGeomParam = Alembic::AbcGeom::IV2fGeomParam;
    using IN3fGeomParam = Alembic::AbcGeom::IN3fGeomParam;

    using IXform = Alembic::AbcGeom::IXform;
    using IXformSchema = Alembic::AbcGeom::IXformSchema;
    using IFaceSet = Alembic::AbcGeom::IFaceSet;
    using IFaceSetSchema = Alembic::AbcGeom::IFaceSetSchema;
    using IPolyMesh = Alembic::AbcGeom::IPolyMesh;
    using IPolyMeshSchema = Alembic::AbcGeom::IPolyMeshSchema;
    using ISubD = Alembic::AbcGeom::ISubD;
    using ISubDSchema = Alembic::AbcGeom::ISubDSchema;
    using IPoints = Alembic::AbcGeom::IPoints;
    using IPointsSchema = Alembic::AbcGeom::IPointsSchema;
    using ICurves = Alembic::AbcGeom::ICurves;
    using ICurvesSchema = Alembic::AbcGeom::ICurvesSchema;
    using ICurvesSample = ICurvesSchema::Sample;
    using INuPatch = Alembic::AbcGeom::INuPatch;
    using INuPatchSchema = Alembic::AbcGeom::INuPatchSchema;
    using INuPatchSample = INuPatchSchema::Sample;

    // Types used for NURBS rationalization, but undefined by Alembic
    using P4fTPTraits = Alembic::Abc::P4fTPTraits;
    using P4fArraySample = Alembic::Abc::TypedArraySample<P4fTPTraits>;
    using P4fArraySamplePtr = Alembic::Util::shared_ptr<P4fArraySample>;

    static const WrapExistingFlag   gabcWrapExisting = Alembic::Abc::kWrapExisting;
    static const M44d               identity44d(1, 0, 0, 0,
                                            0, 1, 0, 0,
                                            0, 0, 1, 0,
			                    0, 0, 0, 1);
    // Corresponds to the Alembic BasisType enum
    static const std::string        curveNamesArray[6] = {
                                            "NoBasis",      // kNoBasis
                                            "Bezier",       // kBezierBasis
                                            "B-Spline",     // kBsplineBasis
                                            "Catmull-Rom",  // kCatmullromBasis
                                            "Hermite",      // kHermiteBasis
                                            "Power"};       // kPowerBasis

    //
    //  Get GA_AttributeOwner, GA_Storage, GA_TypeInfo info from Alembic
    //

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

    // Alembic stores all sorts of things as vectors. For example "uv"
    // coordinates.
    static GA_TypeInfo
    isReallyVector(const char *name, int tsize)
    {
	// We don't actually want "uv" coordinates transformed as
	// vectors, so we strip out the type information here.
	if (!strcmp(name, "uv") && tsize > 1 && tsize < 4)
	    return GA_TYPE_VOID;
	return GA_TYPE_VECTOR;
    }

    //
    //  Load Attributes from Alembic
    //

    static GA_Defaults	theZeroDefaults(0);
    static GA_Defaults	theWidthDefaults(0.1);
    static GA_Defaults	theColorDefaults(1.0);

    static const GA_Defaults &
    getDefaults(const char *name, int tsize, const char *interp)
    {
	if (!strcmp(name, "width") && tsize == 1)
	    return theWidthDefaults;
	if (!strcmp(interp, "color")
		|| !strcmp(interp, "rgb")
		|| !strcmp(interp, "rgba"))
	{
	    return theColorDefaults;
	}
	return theZeroDefaults;
    }

    static GA_AttributeOwner
    arbitraryGAOwner(const PropertyHeader &ph)
    {
        return getGAOwner(Alembic::AbcGeom::GetGeometryScope(ph.getMetaData()));
    }

    static int
    getArrayExtent(const MetaData &meta)
    {
        std::string extent_s = meta.get("arrayExtent");
        return (extent_s == "") ? 1 : atoi(extent_s.c_str());
    }

    static void
    setupForOwner(GABC_GEOWalker &walk,
            GA_AttributeOwner &owner,
            const char *name,
            exint &start,
            exint &len,
            int &entries,
            int &tsize,
            exint npoint,
            exint nvertex,
            exint nprim,
            bool &promote_points)
    {
	switch (owner)
	{
	    case GA_ATTRIB_POINT:
                // If an attribute is encountered as both a point and vertex
                // attribute, upgrade point attribute data to vertex data.
                if (walk.detail().findVertexAttribute(name))
                {
                    promote_points = true;
                    owner = GA_ATTRIB_VERTEX;
                    // Fall through to vertex case
                }
                else
                {
                    promote_points = false;
                    start = walk.pointCount();
                    len = npoint;
                    break;
                }

	    case GA_ATTRIB_VERTEX:
                start = walk.vertexCount();
                len = nvertex;
		break;

	    case GA_ATTRIB_DETAIL:
	        // Promote detail attributes to primitive attributes, in case
	        // multiple objects in the same Alembic archive have the same
	        // detail attribute with differing values.
	        owner = GA_ATTRIB_PRIMITIVE;
	        tsize *= entries;
	        entries = 1;
	        // Fall through to primitive case

	    case GA_ATTRIB_PRIMITIVE:
                start = walk.primitiveCount();
                len = nprim;
		break;

	    default:
		UT_ASSERT(!"Cannot determine attribute owner");
		return;
	}
    }

    static GA_RWAttributeRef
    findAttribute(GU_Detail &gdp,
            GA_AttributeOwner owner,
            const char *name,
            const char *abcname,
            int tsize,
            GA_Storage storage,
            const char *interp)
    {
	GA_RWAttributeRef   attrib;

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
	    {
	        // Adjust for "Houdini" uv coordinates.
                tsize = 3;
            }

	    attrib = gdp.addTuple(storage,
	            owner,
	            name,
	            tsize,
                    getDefaults(name, tsize, interp));
	    if (attrib.isValid() && abcname)
	    {
                // Do *not* mark this as a "vector" since we don't want to
                // transform as a vector.
                attrib.getAttribute()->setExportName(abcname);
            }
	}

	return attrib;
    }

    template <typename GA_T, typename ABC_T>
    static void
    setNumericAttribute(GU_Detail &gdp,
            GA_RWAttributeRef &attrib,
            const ABC_T *array_data,
            exint start,
            exint end,
            int extent,
            int entries,
            exint npts = 0)
    {
	if (!array_data)
	    return;

	GA_RWHandleT<GA_T>  h(attrib.getAttribute());
	int                 tsize = SYSmin(extent, attrib.getTupleSize());
	const GA_AIFTuple  *tuple = attrib.getAIFTuple();
	--entries;

	UT_ASSERT(h.isValid());

	if (npts)
	{
	    const ABC_T    *data;
	    GA_Offset       pos;

	    for (exint i = start; i < end; ++i)
            {
                pos = gdp.vertexPoint(GA_Offset(i));
                data = (array_data + ((pos - npts) * tsize));

                for (int j = 0; j < tsize; ++j)
                {
                    if (tuple)
                    {
                        tuple->set(
                                attrib.getAttribute(),
                                GA_Offset(i),
                                (GA_T)data[j],
                                j);
                    }
                    else
                        h.set(GA_Offset(i), j, data[j]);
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
                                (GA_T)array_data[j],
                                j);
                    }
                    else
                        h.set(GA_Offset(i), j, array_data[j]);
                }
                if ((i - start) < entries)
                    array_data += tsize;
            }
	}
    }

    static void
    setStringAttribute(GU_Detail &gdp,
            GA_RWAttributeRef &attrib,
            const std::string  *array_data,
            exint start,
            exint end,
            int extent,
            int entries,
            exint npts = 0)
    {
	GA_RWHandleS        h(attrib.getAttribute());
	int                 tsize = SYSmin(extent, attrib.getTupleSize());
	--entries;

	UT_ASSERT(h.isValid());

        if (npts)
        {
	    const std::string  *data;
	    GA_Offset           pos;

            for (exint i = start; i < end; ++i)
            {
                pos = gdp.vertexPoint(GA_Offset(i));
                data = (array_data + ((pos - npts) * tsize));

                for (int j = 0; j < tsize; ++j)
                    h.set(GA_Offset(i), j, data[j].c_str());
            }
        }
        else
        {
            for (exint i = start; i < end; ++i)
            {
                for (int j = 0; j < tsize; ++j)
                    h.set(GA_Offset(i), j, array_data[j].c_str());

                if ((i - start) < entries)
                    array_data += tsize;
            }
        }
    }

    static void
    setAttribute(GABC_GEOWalker &walk,
            const GABC_IObject &obj,
            GA_AttributeOwner owner,
            const char *name,
            const char *abcname,
            const DataType &data_type,
            const Dimensions &dimensions,
            const MetaData &meta_data,
            const void *data,
            exint npoint,
            exint nvertex = 0,
            exint nprim = 0)
    {
	GA_RWAttributeRef   attrib;
	GA_Storage          store = getGAStorage(data_type);
	GU_Detail          &gdp = walk.detail();
	std::string         interp= meta_data.get("interpretation");
	exint               start;
	exint               len;
	exint               npts = 0;
        int                 array_extent = getArrayExtent(meta_data);
        int                 entries = dimensions.numPoints();
	int                 tsize = array_extent * data_type.getExtent();
        bool                promote_points = false;

	UT_ASSERT(array_extent == 1 || (entries % array_extent) == 0);

	if (array_extent > 1)
	    entries = entries / array_extent;
        if (!entries)
	{
	    // Valid attribute but no entries, likely a corrupt Alembic file.
            walk.errorHandler().warning("No entries for attribute %s in "
                    "object %s. Ignoring attribute.",
                    name,
                    obj.getFullName().c_str());
	    return;
        }

        setupForOwner(walk,
                owner,
                name,
                start,
                len,
                entries,
                tsize,
                npoint,
                nvertex,
                nprim,
                promote_points);

	auto &namemap = walk.nameMapPtr();
	if(namemap)
	{
	    const char *typeinfo = namemap->getTypeInfo(abcname);
	    if(typeinfo)
		interp = typeinfo;
	}

	attrib = findAttribute(gdp,
	        owner,
	        name,
	        abcname,
	        tsize,
	        store,
	        interp.c_str());
	if (attrib.isValid())
	{
	    if (attrib.getAttribute() != gdp.getP())
	    {
		GA_TypeInfo tinfo = getGATypeInfo(interp.c_str(), tsize);
		if (tinfo == GA_TYPE_VECTOR)
		    tinfo = isReallyVector(name, tsize);
		attrib.getAttribute()->setTypeInfo(tinfo);
	    }

	    if (promote_points)
	    {
	        walk.errorHandler().warning("Upgrading point attribute "
                        "%s to vertex attribute for object %s.",
                        name,
                        obj.getFullName().c_str());
                npts = walk.pointCount();
            }

	    switch (attrib.getStorageClass())
	    {
		case GA_STORECLASS_REAL:
		    switch (data_type.getPod())
		    {
			case Alembic::AbcGeom::kFloat16POD:
			    setNumericAttribute<fpreal16, fpreal16>(gdp,
			            attrib,
			            (const fpreal16 *)data,
			            start,
			            start+len,
                                    tsize,
                                    entries,
                                    npts);
			    break;
			case Alembic::AbcGeom::kFloat32POD:
			    setNumericAttribute<fpreal32, fpreal32>(gdp,
                                    attrib,
                                    (const fpreal32 *)data,
                                    start,
                                    start+len,
                                    tsize,
                                    entries,
                                    npts);
			    break;
			case Alembic::AbcGeom::kFloat64POD:
			    setNumericAttribute<fpreal64, fpreal64>(gdp,
                                    attrib,
                                   (const fpreal64 *)data,
                                    start,
                                    start+len,
                                    tsize,
                                    entries,
                                    npts);
			    break;
			default:
			    UT_ASSERT(0 && "Bad alembic type");
			    break;
		    }
		    break;
		case GA_STORECLASS_INT:
		    switch (data_type.getPod())
		    {
			case Alembic::AbcGeom::kInt8POD:
			    setNumericAttribute<int32, int8>(gdp,
                                    attrib,
                                    (const int8 *)data,
                                    start,
                                    start+len,
                                    tsize,
                                    entries,
                                    npts);
			    break;
			case Alembic::AbcGeom::kBooleanPOD:
			    setNumericAttribute<uint8, uint8>(gdp,
                                    attrib,
                                    (const uint8 *)data,
                                    start,
                                    start+len,
                                    tsize,
                                    entries,
                                    npts);
			    break;
			case Alembic::AbcGeom::kUint8POD:
			    setNumericAttribute<uint8, uint8>(gdp,
                                    attrib,
                                    (const uint8 *)data,
                                    start,
                                    start+len,
                                    tsize,
                                    entries,
                                    npts);
			    break;
			case Alembic::AbcGeom::kInt16POD:
			    setNumericAttribute<int32, int16>(gdp,
                                    attrib,
                                    (const int16 *)data,
                                    start,
                                    start+len,
                                    tsize,
                                    entries,
                                    npts);
			    break;
			case Alembic::AbcGeom::kUint16POD:
			    setNumericAttribute<int32, uint16>(gdp,
                                    attrib,
                                    (const uint16 *)data,
                                    start,
                                    start+len,
                                    tsize,
                                    entries,
                                    npts);
			    break;
			case Alembic::AbcGeom::kInt32POD:
			    setNumericAttribute<int32, int32>(gdp,
                                    attrib,
                                    (const int32 *)data,
                                    start,
                                    start+len,
                                    tsize,
                                    entries,
                                    npts);
			    break;
			case Alembic::AbcGeom::kUint32POD:
			    setNumericAttribute<int32, uint32>(gdp,
			            attrib,
			            (const uint32 *)data,
			            start,
			            start+len,
                                    tsize,
                                    entries,
                                    npts);
			    break;
			case Alembic::AbcGeom::kInt64POD:
			    setNumericAttribute<int64, int64>(gdp,
                                    attrib,
                                    (const int64 *)data,
                                    start,
                                    start+len,
                                    tsize,
                                    entries,
                                    npts);
			    break;
			case Alembic::AbcGeom::kUint64POD:
			    setNumericAttribute<int64, uint64>(gdp,
                                    attrib,
                                    (const uint64 *)data,
                                    start,
                                    start+len,
                                    tsize,
                                    entries,
                                    npts);
			    break;
			default:
			    UT_ASSERT(0 && "Bad alembic type");
		    }
		    break;
		case GA_STORECLASS_STRING:
		    setStringAttribute(gdp,
		            attrib,
                            (const std::string *)data,
                            start,
                            start+len,
                            tsize,
                            entries,
                            npts);
		    break;
		default:
		    UT_ASSERT(0 && "Bad GA storage");
	    }
	}
    }

    static void
    setIndexedStringAttribute(GABC_GEOWalker &walk,
            const GABC_IObject &obj,
            ICompoundProperty parent,
            ISampleSelector &iss,
            exint npoint,
            exint nvertex,
            exint nprim)
    {
	ArraySamplePtr          isample;
	ArraySamplePtr          vsample;
	IArrayProperty          indices(parent, ".indices");
	IArrayProperty          vals(parent, ".vals");
	const PropertyHeader   &head = parent.getHeader();
	GA_AttributeOwner       owner = arbitraryGAOwner(head);
	GA_RWAttributeRef       attrib;
	GA_RWHandleS            handle;
	GU_Detail              &gdp = walk.detail();
        UT_String               name(head.getName());
        const std::string      *vals_data;
        exint                   start;
        exint                   end;
        exint                   len;
        const int              *indices_data;
        int                     tsize = getArrayExtent(parent.getMetaData());
        int                     entries;
	bool                    promote_points = false;

	if (!indices.valid() || !vals.valid())
            return;

	indices.get(isample, iss);
	vals.get(vsample, iss);
	if (!isample || !vsample
		|| getGAStorage(vsample->getDataType()) != GA_STORE_STRING)
	{
	    return;
	}

        entries = isample->size();
	if (!entries || !vsample->size())
	{
	    // Valid attribute but no entries, likely a corrupt Alembic file.
            walk.errorHandler().warning("No entries for attribute %s in "
                    "object %s. Ignoring attribute.",
                    name.buffer(),
                    obj.getFullName().c_str());
	    return;
	}

        UT_ASSERT(tsize == 1 || (entries % tsize) == 0);
        entries = entries / tsize;
        setupForOwner(walk,
                owner,
                name.buffer(),
                start,
                len,
                entries,
                tsize,
                npoint,
                nvertex,
                nprim,
                promote_points);
        end = start + len;

        if (!walk.translateAttributeName(owner, name))
            return;

        attrib = findAttribute(gdp,
                owner,
                name,
                head.getName().c_str(),
                tsize,
                GA_STORE_STRING,
                "");
        if (!attrib.isValid())
            return;

        handle = GA_RWHandleS(attrib.getAttribute());
        vals_data = (const std::string *)vsample->getData();
        indices_data = (const int *)isample->getData();
        tsize = SYSmin(tsize, attrib.getTupleSize());

	--entries;
        if (promote_points)
        {
            const int  *data;
            GA_Offset   pos;

            walk.errorHandler().warning("Upgrading point attribute "
                    "%s to vertex attribute for object %s.",
                    name.buffer(),
                    obj.getFullName().c_str());

            for (exint i = start; i < end; ++i)
            {
                pos = gdp.vertexPoint(GA_Offset(i));
                data = (indices_data + ((pos - npoint) * tsize));

                for (int j = 0; j < tsize; ++j)
                    handle.set(GA_Offset(i), j, vals_data[data[j]].c_str());
            }
        }
        else
        {
            for (exint i = start; i < end; ++i)
            {
                for (int j = 0; j < tsize; ++j)
                {
                    handle.set(GA_Offset(i),
                            j,
                            vals_data[indices_data[j]].c_str());
                }

                if ((i - start) < entries)
                    indices_data += tsize;
            }
        }
    }

    /// Template argument @T is expected to be a
    ///  ITypedGeomParam<TRAITS>
    template <typename T>
    static void
    setGeomAttribute(GABC_GEOWalker &walk,
            const GABC_IObject &obj,
            const char *name,
            const char *abcname,
            const T &param,
            ISampleSelector &iss,
            exint npoint,
            exint nvertex,
            exint nprim)
    {
	if (!param.valid())
	    return;

	typename T::sample_type     psample;
	ArraySample                *asample;
	GA_AttributeOwner           owner = getGAOwner(param.getScope());

	param.getExpanded(psample, iss);
	asample = psample.getVals().get();
	if (!asample)
	    return;

        setAttribute(walk,
                obj,
                owner,
                name,
                abcname,
                asample->getDataType(),
                asample->getDimensions(),
                param.getMetaData(),
                asample->getData(),
                npoint,
                nvertex,
                nprim);
    }

    static void
    fillArb(GABC_GEOWalker &walk,
            const GABC_IObject &obj,
            ICompoundProperty arb,
            ISampleSelector &iss,
            exint npoint,
            exint nvertex,
            exint nprim)
    {
        if (!arb)
            return;

        ArraySamplePtr              asample;
        CompoundPropertyReaderPtr   cpr_ptr = GetCompoundPropertyReaderPtr(arb);
	exint                       narb = arb.getNumProperties();

	for (exint i = 0; i < narb; ++i)
	{
            const PropertyHeader   &head = arb.getPropertyHeader(i);

            if (head.isCompound())
            {
                setIndexedStringAttribute(walk,
                        obj,
                        ICompoundProperty(cpr_ptr->getCompoundProperty(i), gabcWrapExisting),
                        iss,
                        npoint,
                        nvertex,
                        nprim);
                continue;
            }
            UT_ASSERT(head.isArray());

            IArrayProperty in_property(cpr_ptr->getArrayProperty(i),
				       gabcWrapExisting);
	    if (in_property.getNumSamples() == 0)
		continue;

            in_property.get(asample, iss);
            if (!asample)
		continue;

            GA_AttributeOwner owner = arbitraryGAOwner(head);
            UT_String name(head.getName());
            if (!walk.translateAttributeName(owner, name))
		continue;

            setAttribute(walk,
                    obj,
                    owner,
                    name.buffer(),
                    head.getName().c_str(),
                    asample->getDataType(),
                    asample->getDimensions(),
                    in_property.getMetaData(),
                    asample->getData(),
                    npoint,
                    nvertex,
                    nprim);
	}
    }

    //
    //  Load user property information.
    //

    static void
    fillUserProperties(GABC_GEOWalker &walk,
            const GABC_IObject &obj,
            int userpropsIndex)
    {
        GA_RWAttributeRef   attrib;
        GA_RWHandleS        str_attrib;
        GU_Detail          &gdp = walk.detail();
        UT_JSONWriter      *data_writer;
        UT_JSONWriter      *meta_writer;
        UT_WorkBuffer       data_dictionary;
        UT_WorkBuffer       meta_dictionary;
        bool                load_metadata = (walk.loadUserProps()
                                    == GABC_GEOWalker::UP_LOAD_ALL);
        bool                success = false;

        // One writer for values and one for metadata. If the metadata
        // writer is NULL, metadata will be ignored.
        data_writer = UT_JSONWriter::allocWriter(data_dictionary);
        meta_writer = load_metadata
                ? UT_JSONWriter::allocWriter(meta_dictionary)
                : NULL;

        // Create the dictionaries.
        if (!GABC_Util::importUserPropertyDictionary(data_writer,
                meta_writer,
                obj,
                walk.time()))
        {
            walk.errorHandler().warning("Error reading user properties. "
                    "Ignoring user properties.");
        }
        else
        {
            // Fetch/create the attribute handles and set their values.
            attrib = gdp.findStringTuple(GA_ATTRIB_PRIMITIVE,
                    GABC_Util::theUserPropsValsAttrib,
                    1);
            if (!attrib.isValid())
            {
                attrib = gdp.addTuple(GA_STORE_STRING,
                        GA_ATTRIB_PRIMITIVE,
                        GABC_Util::theUserPropsValsAttrib,
                        1);
            }

            str_attrib = GA_RWHandleS(attrib);
            if (!str_attrib.isValid())
            {
                walk.errorHandler().warning("Error creating user properties "
                        "attribute.");
            }
            else
            {
                str_attrib.set(GA_Offset(userpropsIndex),
                        0,
                        data_dictionary.buffer());
                success = true;
            }

            if (load_metadata)
            {
                attrib = gdp.findStringTuple(GA_ATTRIB_PRIMITIVE,
                        GABC_Util::theUserPropsMetaAttrib,
                        1);
                if (!attrib.isValid())
                {
                    attrib = gdp.addTuple(GA_STORE_STRING,
                            GA_ATTRIB_PRIMITIVE,
                            GABC_Util::theUserPropsMetaAttrib,
                            1);
                }

                str_attrib = GA_RWHandleS(attrib);
                if (!str_attrib.isValid())
                {
                    walk.errorHandler().warning("Error creating user properties"
                            " metadata attribute.");
                }
                else
                {
                    str_attrib.set(GA_Offset(userpropsIndex),
                            0,
                            meta_dictionary.buffer());
                    success = true;
                }
            }
        }

        if (success)
            walk.setNonConstant();

        delete data_writer;
	delete meta_writer;
    }

    static GA_PrimitiveGroup *
    findOrCreatePrimitiveGroup(GU_Detail &gdp, const char *name)
    {
	GA_ElementGroup	*g = gdp.findElementGroup(GA_ATTRIB_PRIMITIVE, name);
	if (!g)
	    g = gdp.createElementGroup(GA_ATTRIB_PRIMITIVE, name);
	return UTverify_cast<GA_PrimitiveGroup *>(g);
    }

    //
    //  Helper functions append geometry to existing details as part of
    //  the process to create Houdini geometry from packed Alembics.
    //
    static void
    appendParticles(GABC_GEOWalker &walk, exint npoint)
    {
	GU_Detail	&gdp = walk.detail();
	// Build the particle, appending points as we go
	GU_PrimParticle::build(&gdp, npoint, 1);
    }

    static exint
    appendPoints(GABC_GEOWalker &walk, exint npoint)
    {
	GU_Detail	&gdp = walk.detail();
	exint		 startpoint = walk.pointCount();
	UT_VERIFY(gdp.appendPointBlock(npoint) == GA_Offset(startpoint));
	return startpoint;
    }

    static void
    appendFaces(GABC_GEOWalker &walk,
            exint npoint,
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
	    GU_PrimPoly::buildBlock(&gdp,
	            GA_Offset(startpoint),
	            npoint,
                    pcounts,
                    indices->get());
	}
	else
	{
	    GEO_PrimPolySoup *polysoup = UTverify_cast<GEO_PrimPolySoup *>(
                    gdp.getGEOPrimitive(GU_PrimPolySoup::build(&gdp,
                            GA_Offset(startpoint),
                            npoint,
                            pcounts,
                            indices->get(),
                            false)));
            polysoup->optimize();
	}
    }

    static bool
    validBasis(BasisType type, int nvertices, int order, bool closed)
    {
        UT_ASSERT(order > 0);

        if ((closed && (nvertices < (order - 1)))
            || (!closed && (nvertices < order)))
        {
            return false;
        }

        if (!closed)
            --nvertices;

        // Currently we only load NURBS and bezier curves, everything else
        // falls through to linear. Thus, only need further basis checks
        // if loading Bezier curves.
        if (type == Alembic::AbcGeom::kBezierBasis)
            return nvertices % (order - 1) == 0;

        return true;
    }

    template <typename T>
    static bool
    isEmpty(const T &ptr)
    {
	return !ptr || !ptr->valid() || ptr->size() == 0;
    }

    static void
    appendCurves(GABC_GEOWalker &walk,
            const GABC_IObject &obj,
            BasisType type,
            Int32ArraySamplePtr counts,
            exint npoint,
            UcharArraySamplePtr orders,
            int uorder,
            bool closed)
    {
        GEO_PolyCounts  pcounts;
	GU_Detail      &gdp = walk.detail();
	UT_Array<int>   porders;
	exint           ncurves = counts->size();
        exint           nvtx = 0;
	exint           startpoint = appendPoints(walk, npoint);
        int             c_val;
        int             o_val;
        bool            err_flag = true;

        // Must have a uniform order or a list of varying orders
        UT_ASSERT(uorder || !isEmpty(orders));

        for (exint i = 0; i < ncurves; ++i)
        {
            c_val = (*counts)[i];

            nvtx += c_val;
            pcounts.append(c_val);
        }

        if (!uorder)
        {
            for (exint i = 0; i < ncurves; ++i)
            {
                o_val = (*orders)[i];

                if (o_val > 2 && o_val < GA_MAXORDER)
                    porders.append(o_val);
                else
                {
                    if (err_flag)
                    {
                        walk.errorHandler().warning("One or more %s curves in "
                                "%s have invalid order - order set to 2",
                                curveNamesArray[type].c_str(),
                                obj.getFullName().c_str());
                        err_flag = false;
                    }
                    porders.append(2);
                }

                if (!validBasis(type, (*counts)[i], o_val, closed))
                {
                    walk.errorHandler().warning("Invalid basis for %s curves "
                            "in %s - converting curves to linear",
                            curveNamesArray[type].c_str(),
                            obj.getFullName().c_str());
                    type = Alembic::AbcGeom::kNoBasis;
                    break;
                }
            }
        }
        else
        {
            for (exint i = 0; i < ncurves; ++i)
            {
                if (!validBasis(type, (*counts)[i], uorder, closed))
                {
                    walk.errorHandler().warning("Invalid basis for %s curves "
                            "in %s - converting curves to linear",
                            curveNamesArray[type].c_str(),
                            obj.getFullName().c_str());
                    type = Alembic::AbcGeom::kNoBasis;
                    break;
                }
            }
        }

        UT_StackBuffer<int>	indices(nvtx);
        for (int i = 0; i < nvtx; ++i)
            indices[i] = i;

	switch (type)
	{
	    case Alembic::AbcGeom::kBezierBasis:
	        GEO_PrimRBezCurve::buildBlock(&gdp,
	                GA_Offset(startpoint),
                        npoint,
                        pcounts,
                        indices,
                        porders,
                        uorder,
                        closed);
	        break;

	    case Alembic::AbcGeom::kBsplineBasis:
	        GEO_PrimNURBCurve::buildBlock(&gdp,
	                GA_Offset(startpoint),
                        npoint,
                        pcounts,
                        indices,
                        porders,
                        uorder,
                        closed,
                        !closed);
	        break;

            default:
                GU_PrimPoly::buildBlock(&gdp,
                        GA_Offset(startpoint),
                        npoint,
                        pcounts,
                        indices,
                        false);
	}
    }

    static void
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

    //
    //  Helper functions for creating Houdini geometry.
    //

    static GEO_AnimationType
    getAnimationType(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	GEO_AnimationType	atype;
	atype = obj.getAnimationType(false);
	if (atype == GEO_ANIMATION_TOPOLOGY)
	    walk.setNonConstantTopology();
	return atype;
    }

    static bool
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

    static bool
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

    static bool
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

    static P4fArraySamplePtr
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

    //
    //  Create Houdini geometry from a packed Alembic.
    //

    static bool
    matchAttributeName(GA_AttributeOwner owner,
            const char *name,
            const GEO_PackedNameMapPtr &namemap)
    {
	return namemap ? namemap->matchPattern(owner, name) : true;
    }

    static void
    makeAbcPrim(GABC_GEOWalker &walk,
            const GABC_IObject &obj,
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
	GABC_PackedImpl		*abc;
	GA_Offset		 pt = walk.getPointForAbcPrim();

	abc = UTverify_cast<GABC_PackedImpl *>(packed->implementation());
	UT_ASSERT(GAisValid(pt));
	packed->setVertexPoint(pt);
	packed->setAttributeNameMap(walk.nameMapPtr());
        packed->setFacesetAttribute(walk.facesetAttribute());
	packed->setViewportLOD(walk.viewportLOD());
	abc->setUseTransform(walk.includeXform());
	if (!abc->isConstant())
	    walk.setNonConstant();
	if (walk.staticTimeZero()
		&& obj.getAnimationType(false) == GEO_ANIMATION_CONSTANT
		&& walk.transformConstant())
	{
	    abc->setFrame(0);
	}
	walk.setPointLocation(packed, pt);

	if (walk.loadUserProps())
	    fillUserProperties(walk, obj, walk.primitiveCount());

	walk.trackPtVtxPrim(obj, 0, 0, 1, false);
    }

    static void
    locatorAttribute(GABC_GEOWalker &walk,
            const char *name,
            fpreal x,
            fpreal y,
            fpreal z)
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

    static void
    makeLocator(GABC_GEOWalker &walk, IXform &xform, const GABC_IObject &obj)
    {
	Alembic::Abc::IScalarProperty	loc(xform.getProperties(), "locator");
	ISampleSelector			iss = walk.timeSample();
	const int			npoint = 1;
	const int			nvertex = 1;
	const int			nprim = 1;

	GEO_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GEO_ANIMATION_CONSTANT)
	    walk.setNonConstant();
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
        fillArb(walk, obj, xform.getSchema().getArbGeomParams(), iss, 1, 1, 1);

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
	    grp = findOrCreatePrimitiveGroup(gdp, name);
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

    static void
    makePolyMesh(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	IPolyMesh		polymesh(obj.object(), gabcWrapExisting);
	IPolyMeshSchema        &ps = polymesh.getSchema();
	IN3fGeomParam           normals = ps.getNormalsParam();
	IV2fGeomParam           uvs = ps.getUVsParam();
	IP3fArrayProperty       positions = ps.getPositionsProperty();
	P3fArraySamplePtr	psample = positions.getValue(iss);
	Int32ArraySamplePtr	counts = ps.getFaceCountsProperty().getValue(iss);
	Int32ArraySamplePtr	indices = ps.getFaceIndicesProperty().getValue(iss);
	exint			npoint = psample->size();
	exint			nvertex = indices->size();
	exint			nprim = counts->size();

	//fprintf(stderr, "PolyMesh %s: %d %d %d\n", obj.getFullName().c_str(), int(npoint), int(nvertex), int(nprim));

	GEO_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GEO_ANIMATION_CONSTANT)
	    walk.setNonConstant();
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
	    bool    soup;
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
	setAttribute(walk,
	        obj,
	        GA_ATTRIB_POINT,
	        "P",
	        NULL,
                psample->getDataType(),
                psample->getDimensions(),
                positions.getMetaData(),
                psample->getData(),
		npoint);
	if (ps.getVelocitiesProperty().valid()
	        && matchAttributeName(GA_ATTRIB_POINT, "v", walk.nameMapPtr()))
	{
            IV3fArrayProperty   velocities = ps.getVelocitiesProperty();
            V3fArraySamplePtr   vsample = velocities.getValue(iss);

	    setAttribute(walk,
	            obj,
	            GA_ATTRIB_POINT,
	            "v",
	            NULL,
                    vsample->getDataType(),
                    vsample->getDimensions(),
                    velocities.getMetaData(),
                    vsample->getData(),
		    npoint);
	}
	if (uvs.valid()
	        && matchAttributeName(getGAOwner(uvs.getScope()), "uv", walk.nameMapPtr()))
	{
	    setGeomAttribute(walk, obj, "uv", NULL, ps.getUVsParam(), iss,
		    npoint, nvertex, nprim);
	}
	if (normals.valid()
	        && matchAttributeName(getGAOwner(normals.getScope()), "N", walk.nameMapPtr()))
	{
	    setGeomAttribute(walk, obj, "N", NULL, ps.getNormalsParam(), iss,
		    npoint, nvertex, nprim);
	}
	fillArb(walk, obj, ps.getArbGeomParams(), iss, npoint, nvertex, nprim);
	if (walk.loadUserProps())
            fillUserProperties(walk, obj, walk.primitiveCount());

	walk.trackLastFace(nprim);
	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    static void
    makeSubD(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	ISubD			subd(obj.object(), gabcWrapExisting);
	ISubDSchema		&ss = subd.getSchema();
	IV2fGeomParam           uvs = ss.getUVsParam();
	IP3fArrayProperty       positions = ss.getPositionsProperty();
	P3fArraySamplePtr	psample = positions.getValue(iss);
	Int32ArraySamplePtr	counts = ss.getFaceCountsProperty().getValue(iss);
	Int32ArraySamplePtr	indices = ss.getFaceIndicesProperty().getValue(iss);
	exint			npoint = psample->size();
	exint			nvertex = indices->size();
	exint			nprim = counts->size();

	//fprintf(stderr, "SubD: %d %d %d\n", int(npoint), int(nvertex), int(nprim));

	GEO_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GEO_ANIMATION_CONSTANT)
	    walk.setNonConstant();
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
	    bool    soup;
	    soup = (walk.polySoup() == GABC_GEOWalker::ABC_POLYSOUP_SUBD);
	    // If there are uniform attributes
	    if (soup && hasUniformAttributes(ss.getArbGeomParams()))
		soup = false;
	    if (soup && hasFaceSets(obj))
		soup = false;

	    // Assert that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() == walk.primitiveCount());
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
	setAttribute(walk,
	        obj,
	        GA_ATTRIB_POINT,
	        "P",
	        NULL,
                psample->getDataType(),
                psample->getDimensions(),
                positions.getMetaData(),
                psample->getData(),
		npoint);
	if (ss.getVelocitiesProperty().valid()
	        && matchAttributeName(GA_ATTRIB_POINT, "v", walk.nameMapPtr()))
	{
            IV3fArrayProperty   velocities = ss.getVelocitiesProperty();
            V3fArraySamplePtr   vsample = velocities.getValue(iss);

	    setAttribute(walk,
	            obj,
	            GA_ATTRIB_POINT,
	            "v",
	            NULL,
                    vsample->getDataType(),
                    vsample->getDimensions(),
                    velocities.getMetaData(),
                    vsample->getData(),
		    npoint);
	}
	if (uvs.valid()
	        && matchAttributeName(getGAOwner(uvs.getScope()), "uv", walk.nameMapPtr()))
	{
	    setGeomAttribute(walk, obj, "uv", NULL, ss.getUVsParam(), iss,
		    npoint, nvertex, nprim);
	}
	fillArb(walk, obj, ss.getArbGeomParams(), iss, npoint, nvertex, nprim);
	if (walk.loadUserProps())
            fillUserProperties(walk, obj, walk.primitiveCount());

	walk.trackLastFace(nprim);
	walk.trackSubd(nprim);
	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    static void
    makePoints(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	ISampleSelector         iss = walk.timeSample();
	IPoints                 points(obj.object(), gabcWrapExisting);
	IPointsSchema          &ps = points.getSchema();
	IFloatGeomParam         widths = ps.getWidthsParam();
	IP3fArrayProperty       positions = ps.getPositionsProperty();
	P3fArraySamplePtr       psample = positions.getValue(iss);
	exint                   npoint = psample->size();
	exint                   nvertex = npoint;
	exint                   nprim = 1;

	//fprintf(stderr, "Points: %d %d %d\n", int(npoint), int(nvertex), int(nprim));

	GEO_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GEO_ANIMATION_CONSTANT)
	    walk.setNonConstant();
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
	setAttribute(walk,
	        obj,
	        GA_ATTRIB_POINT,
	        "P",
	        NULL,
                psample->getDataType(),
                psample->getDimensions(),
                positions.getMetaData(),
                psample->getData(),
		npoint);
	if (ps.getVelocitiesProperty().valid()
	        && matchAttributeName(GA_ATTRIB_POINT, "v", walk.nameMapPtr()))
	{
            IV3fArrayProperty   velocities = ps.getVelocitiesProperty();
            V3fArraySamplePtr   vsample = velocities.getValue(iss);

	    setAttribute(walk,
	            obj,
	            GA_ATTRIB_POINT,
	            "v",
	            NULL,
                    vsample->getDataType(),
                    vsample->getDimensions(),
                    velocities.getMetaData(),
                    vsample->getData(),
		    npoint);
	}
	if (ps.getIdsProperty().valid()
	        && matchAttributeName(GA_ATTRIB_POINT, "id", walk.nameMapPtr()))
	{
            IUInt64ArrayProperty    ids = ps.getIdsProperty();
            UInt64ArraySamplePtr    isample = ids.getValue(iss);

	    setAttribute(walk,
	            obj, GA_ATTRIB_POINT,
	            "id",
	            NULL,
                    isample->getDataType(),
                    isample->getDimensions(),
                    ids.getMetaData(),
                    isample->getData(),
		    npoint);
	}
	if (widths.valid()
	        && matchAttributeName(getGAOwner(widths.getScope()), "width", walk.nameMapPtr()))
	{
	    setGeomAttribute(walk, obj, "width", NULL, widths, iss,
		    npoint, nvertex, nprim);
	}
	fillArb(walk, obj, ps.getArbGeomParams(), iss, npoint, nvertex, nprim);
	if (walk.loadUserProps())
            fillUserProperties(walk, obj, walk.primitiveCount());

	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
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

    static void
    makeCurves(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	ICurves			curves(obj.object(), gabcWrapExisting);
	ICurvesSchema		&cs = curves.getSchema();
	ICurvesSample		c_sample = cs.getValue(iss);
	IFloatGeomParam         widths = cs.getWidthsParam();
	IN3fGeomParam           normals = cs.getNormalsParam();
	IV2fGeomParam           uvs = cs.getUVsParam();
	IP3fArrayProperty       positions = cs.getPositionsProperty();
	P3fArraySamplePtr       psample = positions.getValue(iss);
	Int32ArraySamplePtr	nvtx = cs.getNumVerticesProperty().getValue(iss);
        UcharArraySamplePtr     orders;
	FloatArraySamplePtr     knots = c_sample.getKnots();
        BasisType               type = c_sample.getBasis();
	CurvePeriodicity        period = c_sample.getWrap();
        exint			npoint = psample->size();
	exint			nvertex = npoint;
	exint			nprim = nvtx->size();
	exint                   norders;
	int                     uorder = 0;

	switch (c_sample.getType())
        {
            case Alembic::AbcGeom::kCubic:
                uorder = 4;
                break;

            case Alembic::AbcGeom::kLinear:
                uorder = 2;
                break;

            case Alembic::AbcGeom::kVariableOrder:
                orders = c_sample.getOrders();
                if (!isEmpty(orders))
                {
                    norders = orders->size();
                    if (norders == nprim)
                    {
                        uorder = 0;
                        break;
                    }
                }

            default:
                walk.errorHandler().warning("Error reading order for %s - "
                        "converting curves to linear",
                        obj.getFullName().c_str());
                uorder = 2;
                type = Alembic::AbcGeom::kNoBasis;
        }

	GEO_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GEO_ANIMATION_CONSTANT)
	    walk.setNonConstant();
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
	    appendCurves(walk,
	            obj,
	            type,
	            nvtx,
	            npoint,
	            orders,
	            uorder,
	            (period == Alembic::AbcGeom::kPeriodic));
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
	if (cs.getPositionWeightsProperty().valid()
	        && matchAttributeName(GA_ATTRIB_POINT, "Pw", walk.nameMapPtr()))
        {
            P4fArraySamplePtr   rsample = rationalize(psample, cs.getPositionWeightsProperty().getValue(iss));

            setAttribute(walk,
                    obj,
                    GA_ATTRIB_POINT,
                    "P",
                    NULL,
                    rsample->getDataType(),
                    rsample->getDimensions(),
                    positions.getMetaData(),
                    rsample->getData(),
                    npoint);
        }
        else
        {
            setAttribute(walk,
                    obj,
                    GA_ATTRIB_POINT,
                    "P",
                    NULL,
                    psample->getDataType(),
                    psample->getDimensions(),
                    positions.getMetaData(),
                    psample->getData(),
                    npoint);
        }


	if (cs.getVelocitiesProperty().valid()
	        && matchAttributeName(GA_ATTRIB_POINT, "v", walk.nameMapPtr()))
	{
            IV3fArrayProperty   velocities = cs.getVelocitiesProperty();
            V3fArraySamplePtr   vsample = velocities.getValue(iss);

	    setAttribute(walk,
	            obj,
	            GA_ATTRIB_POINT,
	            "v",
	            NULL,
                    vsample->getDataType(),
                    vsample->getDimensions(),
                    velocities.getMetaData(),
                    vsample->getData(),
		    npoint);
	}
	if (uvs.valid()
	        && matchAttributeName(getGAOwner(uvs.getScope()), "uv", walk.nameMapPtr()))
	{
	    setGeomAttribute(walk, obj, "uv", NULL, cs.getUVsParam(), iss,
		    npoint, nvertex, nprim);
	}
	if (normals.valid()
	        && matchAttributeName(getGAOwner(normals.getScope()), "N", walk.nameMapPtr()))
	{
	    setGeomAttribute(walk, obj, "N", NULL, cs.getNormalsParam(), iss,
		    npoint, nvertex, nprim);
	}
	if (widths.valid()
	        && matchAttributeName(getGAOwner(widths.getScope()), "width", walk.nameMapPtr()))
	{
	    setGeomAttribute(walk, obj, "width", NULL, cs.getWidthsParam(), iss,
		    npoint, nvertex, nprim);
	}
	fillArb(walk, obj, cs.getArbGeomParams(), iss, npoint, nvertex, nprim);
	if (walk.loadUserProps())
            fillUserProperties(walk, obj, walk.primitiveCount());

	walk.trackLastFace(nprim);
	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    static void
    makeNuPatch(GABC_GEOWalker &walk, const GABC_IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	INuPatch		nupatch(obj.object(), gabcWrapExisting);
	INuPatchSchema		&ns = nupatch.getSchema();
	INuPatchSample		patch = ns.getValue(iss);
	IN3fGeomParam           normals = ns.getNormalsParam();
	IV2fGeomParam           uvs = ns.getUVsParam();
	FloatArraySamplePtr	uknots = patch.getUKnot();
	FloatArraySamplePtr	vknots = patch.getVKnot();
	int			uorder = patch.getUOrder();
	int			vorder = patch.getVOrder();
	IP3fArrayProperty       positions = ns.getPositionsProperty();
	P3fArraySamplePtr       psample = positions.getValue(iss);
	exint			npoint = psample->size();
	exint			nvertex = npoint;
	exint			nprim = 1;

	// Verify that we have the expected point count
	UT_ASSERT(npoint == (uknots->size()-uorder)*(vknots->size()-vorder));

	//fprintf(stderr, "NuPatch: %d %d %d\n", int(npoint), int(nvertex), int(nprim));

	GEO_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GEO_ANIMATION_CONSTANT)
	    walk.setNonConstant();
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
	if (ns.getPositionWeightsProperty().valid()
	        && matchAttributeName(GA_ATTRIB_POINT, "Pw", walk.nameMapPtr()))
        {
            P4fArraySamplePtr   rsample = rationalize(psample, ns.getPositionWeightsProperty().getValue(iss));

            setAttribute(walk,
                    obj,
                    GA_ATTRIB_POINT,
                    "P",
                    NULL,
                    rsample->getDataType(),
                    rsample->getDimensions(),
                    positions.getMetaData(),
                    rsample->getData(),
                    npoint);
        }
        else
        {
            setAttribute(walk,
                    obj,
                    GA_ATTRIB_POINT,
                    "P",
                    NULL,
                    psample->getDataType(),
                    psample->getDimensions(),
                    positions.getMetaData(),
                    psample->getData(),
                    npoint);
        }

	if (ns.getVelocitiesProperty().valid()
	        && matchAttributeName(GA_ATTRIB_POINT, "v", walk.nameMapPtr()))
	{
            IV3fArrayProperty   velocities = ns.getVelocitiesProperty();
            V3fArraySamplePtr   vsample = velocities.getValue(iss);

	    setAttribute(walk,
	            obj,
	            GA_ATTRIB_POINT,
	            "v",
	            NULL,
                    vsample->getDataType(),
                    vsample->getDimensions(),
                    velocities.getMetaData(),
                    vsample->getData(),
		    npoint);
	}
	if (uvs.valid()
	        && matchAttributeName(getGAOwner(uvs.getScope()), "uv", walk.nameMapPtr()))
	{
	    setGeomAttribute(walk, obj, "uv", NULL, ns.getUVsParam(), iss,
		    npoint, nvertex, nprim);
	}
	if (normals.valid()
	        && matchAttributeName(getGAOwner(normals.getScope()), "N", walk.nameMapPtr()))
	{
	    setGeomAttribute(walk, obj, "N", NULL, ns.getNormalsParam(), iss,
		    npoint, nvertex, nprim);
	}
	fillArb(walk, obj, ns.getArbGeomParams(), iss, npoint, nvertex, nprim);
	if (walk.loadUserProps())
            fillUserProperties(walk, obj, walk.primitiveCount());

	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    //
    //  Create a point cloud from a packed Alembic.
    //

    static void
    buildPointCloud(GABC_GEOWalker &walk,
            const GABC_IObject &obj,
            const P3fArraySamplePtr &P)
    {
	GU_Detail		&gdp = walk.detail();
	exint			 startpoint = walk.pointCount();
	exint			 npoint = P->size();
	const Imath::V3f	*Pdata = P->get();

	if (!walk.reusePrimitives())
	    UT_VERIFY(gdp.appendPointBlock(npoint) == GA_Offset(startpoint));
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
	    walk.setNonConstant();
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

    //
    //  Create a bounding box from a packed Alembic.
    //

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
    }

    static void
    setBoxPositions(GU_Detail &gdp,
            const UT_BoundingBox &box,
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
    makeHoudiniBox(GABC_GEOWalker &walk,
            const GABC_IObject &obj,
            const UT_BoundingBox &box)
    {
	GU_Detail	&gdp = walk.detail();
	if (!walk.reusePrimitives())
	    makeBox(gdp);
	setBoxPositions(gdp, box, walk.pointCount());
	if (getAnimationType(walk, obj) != GEO_ANIMATION_CONSTANT)
	    walk.setNonConstant();
	walk.trackPtVtxPrim(obj, 8, 8, 1, true);
    }
}

GABC_GEOWalker::GABC_GEOWalker(GU_Detail &gdp, GABC_IError &err)
    : myDetail(gdp)
    , myErrorHandler(err)
    , mySubdGroup(NULL)
    , myObjectPattern("*")
    , myNameMapPtr()
    , myFacesetAttribute("*")
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
    , mySizeCullMode(SIZE_CULL_IGNORE)
    , mySizeCompare(SIZE_COMPARE_LESSTHAN)
    , mySize(0)
    , myAnimationFilter(ABC_AFILTER_ALL)
    , myGeometryFilter(ABC_GFILTER_ALL)
    , myIncludeXform(true)
    , myUseVisibility(true)
    , myStaticTimeZero(true)
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
    myCullBox.makeInvalid();

    myVisibilityStack.push(GABC_VISIBLE_VISIBLE);
}

GABC_GEOWalker::~GABC_GEOWalker()
{
    // In theory, this should be true even if we were interrupted.
    if (myBoss)
	myBoss->opEnd(myBossId);

    while (!myVisibilityStack.empty())
        myVisibilityStack.pop();
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
GABC_GEOWalker::setExcludeObjects(const char *s)
{
    UT_String		 paths(s);
    UT_WorkArgs		 args;

    myExcludeObjects.clear();
    paths.parse(args);
    for (int i = 0; i < args.getArgc(); i++)
	myExcludeObjects.append(args(i));
    myExcludeObjects.sort(true, false);
}

void
GABC_GEOWalker::updateAbcPrims()
{
    bool    setPath = pathAttributeChanged() && myPathAttribute.isValid();
    int     userpropsIndex = 0;
    for (GA_Iterator it(detail().getPrimitiveRange()); !it.atEnd(); ++it)
    {
	GEO_Primitive      *prim = detail().getGEOPrimitive(*it);
	GU_PrimPacked      *pack = UTverify_cast<GU_PrimPacked *>(prim);
	GABC_PackedImpl    *abc = UTverify_cast<GABC_PackedImpl *>(
	                            pack->implementation());
	const GABC_IObject &obj = abc->object();

	if (!abc->isConstant())
	{
	    abc->setFrame(time());
	    setNonConstant();
        }
	else
	    abc->setFrame(staticTimeZero() ? 0 : time());
	if (setPath)
	    myPathAttribute.set(prim->getMapOffset(), abc->objectPath().c_str());
	setPointLocation(pack, pack->getPointOffset(0));

	if (loadUserProps())
            fillUserProperties(*this, obj, userpropsIndex);

        userpropsIndex++;
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
	if (!GABC_Util::getWorldTransform(parent, time(), m, c, i))
	    m.identity();
	for (int r = 0; r < 4; ++r)
	{
	    for (int c = 0; c < 4; ++c)
		myMatrix.x[r][c] = m(r,c);
	}

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
	myMatrix = identity44d;

    return true;
}

bool
GABC_GEOWalker::process(const GABC_IObject &obj)
{
    const ObjectHeader     &ohead = obj.getHeader();
    GABC_VisibilityType     vtype = GABC_VISIBLE_DEFER;
    // Let the walker process children naturally by default
    bool                    process_children = true;
    bool                    vis = useVisibility();

    // Recompute the time range bounds for this object
    computeTimeRange(obj);

    // Packed Alembics handle visibility on their own through the
    // GABC_PackedImpl (see: GABC_PackedImpl::build(), called by makeAbcPrim).
    if (vis && myLoadMode != LOAD_ABC_PRIMITIVES)
    {
	bool    animated;

	vtype = obj.visibility(animated, time(), false);
	if (animated)
	{
	    // Since visibility is animated, the topology will change
	    setNonConstant();
	    setNonConstantTopology();
	}

	if (vtype != GABC_VISIBLE_DEFER)
	    myVisibilityStack.push(vtype);
    }

    if (IXform::matches(ohead))
    {
	IXform  xform(obj.object(), gabcWrapExisting);

	if (buildLocator()
	        && obj.isMayaLocator()
	        && filterObject(obj)
	        && (!vis || (myVisibilityStack.top() == GABC_VISIBLE_VISIBLE)))
	{
            if (buildAbcPrim())
                makeAbcPrim(*this, obj, ohead);
            else
                makeLocator(*this, xform, obj);
	}
	else if (buildAbcPrim()
	        && buildAbcXform()
	        && filterObject(obj)
                && (!vis || (myVisibilityStack.top() == GABC_VISIBLE_VISIBLE)))
	{
	    makeAbcPrim(*this, obj, ohead);
	}

	if (includeXform())
	{
	    IXformSchema       &xs = xform.getSchema();
	    TransformState      state;

	    pushTransform(xs.getValue(timeSample()).getMatrix(),
	            xs.isConstant(),
	            state,
	            xs.getInheritsXforms());

	    walkChildren(obj);

            popTransform(state);

	    // Since we walked manually, return false
	    process_children = false;
	}
    }
    else if (buildAbcShape() && filterObject(obj)
        && (!vis || (myVisibilityStack.top() == GABC_VISIBLE_VISIBLE)))
    {
	switch (myLoadMode)
	{
	    case LOAD_ABC_PRIMITIVES:
	    case LOAD_ABC_UNPACKED:
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

    if (vtype != GABC_VISIBLE_DEFER)
        myVisibilityStack.pop();

    return process_children;
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
            matchGeometryFilter(obj) &&
	    matchBounds(obj) &&
	    matchSize(obj);
}

bool
GABC_GEOWalker::matchObjectName(const GABC_IObject &obj) const
{
    UT_String	path(obj.getFullName());

    if (!path.multiMatch(objectPattern()))
	return false;

    // Test for exclusion based on object name.
    if (excludeObjects().size() > 0)
    {
	const UT_StringHolder	*lb;
	const UT_StringHolder	*start = excludeObjects().array();
	const UT_StringHolder	*end = start + excludeObjects().size();
	const UT_StringRef	 objname = obj.getFullName();

	// Find the first element greater than or equal to the obj name.
	lb = std::lower_bound(start, end, objname);
	// Is there an exact match in our exclude list?
	if (lb >= start && lb < end && objname == *lb)
	    return false;
	// Is the entry before the lower bound (closest entry less than
	// the object) a parent of the object?
	lb = lb - 1;
	if (lb >= start &&
	    objname.length() > lb->length() &&
	    objname.c_str()[lb->length()] == '/' &&
	    objname.startsWith(*lb))
	    return false;
    }

    return true;
}

bool
GABC_GEOWalker::matchAnimationFilter(const GABC_IObject &obj) const
{
    if (myAnimationFilter == ABC_AFILTER_ALL)
	return true;
    if (myAnimationFilter == ABC_AFILTER_TRANSFORMING)
        return !transformConstant();
    if (myAnimationFilter == ABC_AFILTER_DEFORMING)
        return obj.getAnimationType(false); 

    bool animating = !transformConstant() || obj.getAnimationType(false);
    if (myAnimationFilter == ABC_AFILTER_STATIC)
	return !animating;
    if (myAnimationFilter == ABC_AFILTER_ANIMATING)
	return animating;

    UT_ASSERT(0 && "Invalid animation filter type!");
    return true;
}

bool
GABC_GEOWalker::matchGeometryFilter(const GABC_IObject &obj) const
{
    if (myGeometryFilter == ABC_GFILTER_ALL)
        return true;

    GABC_NodeType nType = obj.nodeType();

    if (nType == GABC_POLYMESH)
        return (myGeometryFilter & ABC_GFILTER_POLYMESH) != 0;
    if (nType == GABC_SUBD)
        return (myGeometryFilter & ABC_GFILTER_SUBD) != 0;
    if (nType == GABC_CURVES)
        return (myGeometryFilter & ABC_GFILTER_CURVES) != 0;
    if (nType == GABC_POINTS)
        return (myGeometryFilter & ABC_GFILTER_POINTS) != 0;
    if (nType == GABC_NUPATCH)
        return (myGeometryFilter & ABC_GFILTER_NUPATCH) != 0;

    if (nType < GABC_UNKNOWN || nType >= GABC_NUM_NODE_TYPES)
        UT_ASSERT(0 && "Invalid primitive filter type!");

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
GABC_GEOWalker::matchSize(const GABC_IObject &obj) const
{
    if(mySizeCullMode == SIZE_CULL_IGNORE)
	return true;

    UT_BoundingBox box;
    bool isconst;
    obj.getBoundingBox(box, time(), isconst);
    fpreal x = box.xsize();
    fpreal y = box.ysize();
    fpreal z = box.zsize();

    fpreal val;
    if(mySizeCullMode == SIZE_CULL_AREA)
	val = 2 * (x * y + x * z + y * z);
    else if(mySizeCullMode == SIZE_CULL_RADIUS)
	val = 0.5 * SYSsqrt(x * x + y * y + z * z);
    else // if(mySizeCullMode == SIZE_CULL_VOLUME)
	val = x * y * z;

    if(mySizeCompare == SIZE_COMPARE_LESSTHAN)
	return val < mySize;

    // mySizeCompare == SIZE_COMPARE_GREATERTHAN
    return val > mySize;
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
	case ABC_GROUP_SHAPE_BASENAME:
	    name.harden(obj.getName().c_str());
	    break;
	case ABC_GROUP_XFORM_BASENAME:
	    name.harden(getParentXform(obj).getName().c_str());
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
        exint npoint,
        exint nvertex,
        exint nprim,
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
	    myPathAttribute.set(GA_Offset(myPrimitiveCount+i), path);
    }
    UT_String		 gname;
    GA_PrimitiveGroup	*g = NULL;
    if (nprim && getGroupName(gname, obj))
    {
	g = findOrCreatePrimitiveGroup(myDetail, gname);
	for (exint i = 0; i < nprim; ++i)
	    g->addOffset(GA_Offset(myPrimitiveCount+i));
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

#if 0

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

#endif
