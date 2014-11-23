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

#include "GABC_IObject.h"
#include "GABC_OError.h"
#include "GABC_OGTAbc.h"
#include "GABC_OOptions.h"
#include "GABC_PackedImpl.h"
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_Handles.h>
#include <GU/GU_PrimPacked.h>

using namespace GABC_NAMESPACE;

namespace
{
    typedef Alembic::Abc::DataType                  DataType;
    typedef Alembic::Abc::ISampleSelector           ISampleSelector;
    typedef Alembic::Abc::MetaData                  MetaData;
    typedef Alembic::Abc::PropertyHeader            PropertyHeader;
    typedef Alembic::Abc::WrapExistingFlag          WrapExistingFlag;

    typedef Alembic::Abc::CompoundPropertyReaderPtr CompoundPropertyReaderPtr;

    // Geometry
    //typedef Alembic::Abc::Box3d                     Box3d;

    // Array Samples
    typedef Alembic::Abc::UcharArraySample          UcharArraySample;
    typedef Alembic::Abc::UInt32ArraySample         UInt32ArraySample;
    typedef Alembic::Abc::Int32ArraySample          Int32ArraySample;
    typedef Alembic::Abc::FloatArraySample          FloatArraySample;
    typedef Alembic::Abc::V3fArraySample            V3fArraySample;
    typedef Alembic::Abc::ArraySamplePtr            ArraySamplePtr;
    typedef Alembic::Abc::UcharArraySamplePtr       UcharArraySamplePtr;
    typedef Alembic::Abc::Int32ArraySamplePtr       Int32ArraySamplePtr;
    typedef Alembic::Abc::UInt32ArraySamplePtr      UInt32ArraySamplePtr;
    typedef Alembic::Abc::UInt64ArraySamplePtr      UInt64ArraySamplePtr;
    typedef Alembic::Abc::FloatArraySamplePtr       FloatArraySamplePtr;
    typedef Alembic::Abc::N3fArraySamplePtr         N3fArraySamplePtr;
    typedef Alembic::Abc::P3fArraySamplePtr         P3fArraySamplePtr;
    typedef Alembic::Abc::V2fArraySamplePtr         V2fArraySamplePtr;
    typedef Alembic::Abc::V3fArraySamplePtr         V3fArraySamplePtr;

    // Properties
    typedef Alembic::Abc::IScalarProperty           IScalarProperty;
    typedef Alembic::Abc::OScalarProperty           OScalarProperty;
    typedef Alembic::Abc::IArrayProperty            IArrayProperty;
    typedef Alembic::Abc::OArrayProperty            OArrayProperty;
    typedef Alembic::Abc::ICompoundProperty         ICompoundProperty;
    typedef Alembic::Abc::OCompoundProperty         OCompoundProperty;

    // General
    typedef Alembic::AbcGeom::IObject               IObject;
    typedef Alembic::AbcGeom::OObject               OObject;
    // Xform
    typedef Alembic::AbcGeom::IXform                IXform;
    typedef GABC_OXform                             OXform;
    typedef Alembic::AbcGeom::IXformSchema          IXformSchema;
    typedef Alembic::AbcGeom::OXformSchema          OXformSchema;
    typedef Alembic::AbcGeom::XformSample           XformSample;
    // PolyMesh
    typedef Alembic::AbcGeom::IPolyMesh             IPolyMesh;
    typedef Alembic::AbcGeom::IPolyMeshSchema       IPolyMeshSchema;
    typedef Alembic::AbcGeom::OPolyMesh             OPolyMesh;
    typedef Alembic::AbcGeom::OPolyMeshSchema       OPolyMeshSchema;
    // Subdivision
    typedef Alembic::AbcGeom::ISubD                 ISubD;
    typedef Alembic::AbcGeom::ISubDSchema           ISubDSchema;
    typedef Alembic::AbcGeom::OSubD                 OSubD;
    typedef Alembic::AbcGeom::OSubDSchema           OSubDSchema;
    // Points
    typedef Alembic::AbcGeom::IPoints		    IPoints;
    typedef Alembic::AbcGeom::IPointsSchema	    IPointsSchema;
    typedef Alembic::AbcGeom::OPoints		    OPoints;
    typedef Alembic::AbcGeom::OPointsSchema	    OPointsSchema;
    // Curves
    typedef Alembic::AbcGeom::ICurves		    ICurves;
    typedef Alembic::AbcGeom::ICurvesSchema	    ICurvesSchema;
    typedef Alembic::AbcGeom::OCurves		    OCurves;
    typedef Alembic::AbcGeom::OCurvesSchema	    OCurvesSchema;
    // NuPatch
    typedef Alembic::AbcGeom::INuPatch		    INuPatch;
    typedef Alembic::AbcGeom::INuPatchSchema	    INuPatchSchema;
    typedef Alembic::AbcGeom::ONuPatch		    ONuPatch;
    typedef Alembic::AbcGeom::ONuPatchSchema	    ONuPatchSchema;
    // FaceSet
    typedef Alembic::AbcGeom::IFaceSet		    IFaceSet;
    typedef Alembic::AbcGeom::IFaceSetSchema	    IFaceSetSchema;
    typedef Alembic::AbcGeom::OFaceSet		    OFaceSet;
    typedef Alembic::AbcGeom::OFaceSetSchema	    OFaceSetSchema;

    // Curve/NURBS
    typedef Alembic::AbcGeom::BasisType             BasisType;
    typedef Alembic::AbcGeom::CurvePeriodicity      CurvePeriodicity;
    typedef Alembic::AbcGeom::CurveType             CurveType;

    // Parameters
    typedef Alembic::AbcGeom::IFloatGeomParam       IFloatGeomParam;
    typedef Alembic::AbcGeom::OFloatGeomParam       OFloatGeomParam;
    typedef Alembic::AbcGeom::IN3fGeomParam         IN3fGeomParam;
    typedef Alembic::AbcGeom::ON3fGeomParam         ON3fGeomParam;
    typedef Alembic::AbcGeom::IV2fGeomParam         IV2fGeomParam;
    typedef Alembic::AbcGeom::OV2fGeomParam         OV2fGeomParam;

    // Visibility
    typedef Alembic::AbcGeom::ObjectVisibility      ObjectVisibility;
    typedef Alembic::AbcGeom::OVisibilityProperty   OVisibilityProperty;

    typedef GABC_OGTAbc::GABCPropertyMap            GABCPropertyMap;
    typedef GABC_OGTAbc::PropertyMap                PropertyMap;

    const static WrapExistingFlag   gabcWrapExisting
                                            = Alembic::Abc::kWrapExisting;

    static void
    writePropertiesFromPrevious(const GABCPropertyMap &pmap)
    {
        if (!pmap.size())
        {
            return;
        }

        exint   num_samples;

        // Write a new sample using the previous sample (if a previous
        // sample exists)
        for (GABCPropertyMap::const_iterator it = pmap.begin();
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

    static const GABC_IObject &
    getObject(const GT_PrimitiveHandle &prim)
    {
        const GT_GEOPrimPacked *gt = UTverify_cast<const GT_GEOPrimPacked *>(
                                        prim.get());
        const GU_PrimPacked    *gu = gt->getPrim();
        const GABC_PackedImpl  *gabc = UTverify_cast<const GABC_PackedImpl *>(
                                        gu->implementation());

        return gabc->object();
    }

    // Read Property samples in then write them out to the new archive. Used
    // for both arbitrary geometry parameters and user properties.
    static void
    sampleCompoundProperties(PropertyMap &p_map,
            ICompoundProperty &in,
            OCompoundProperty &out,
            const GABC_OOptions &ctx,
            const ISampleSelector &iss,
            const std::string &base_name)
    {
        CompoundPropertyReaderPtr   cpr_ptr = GetCompoundPropertyReaderPtr(in);
        exint                       num_props = in.getNumProperties();

        for (exint i = 0; i < num_props; ++i)
        {
            const PropertyHeader   &header = cpr_ptr->getPropertyHeader(i);
            const std::string       name = base_name + header.getName();

            if (header.isScalar())
            {
                const DataType     &dtype = header.getDataType();
                IScalarProperty     in_property(cpr_ptr->getScalarProperty(i),
                                            gabcWrapExisting);
                OScalarProperty    *out_property = p_map.findScalar(name);
                void               *sample;

                if (!out_property)
                {
                    MetaData    metadata(in_property.getMetaData());
                    out_property = new OScalarProperty(out,
                            header.getName(),
                            dtype,
                            metadata,
                            ctx.timeSampling());
                    p_map.insert(name, out_property);
                }
                else if (in_property.isConstant())
                {
                    out_property->setFromPrevious();
                    continue;
                }

                if (dtype.getPod() == Alembic::Abc::kStringPOD)
                {
                    std::string     str;
                    sample = (void *)(&str);
                    in_property.get(sample, iss);
                    out_property->set(sample);
                }
                else if (dtype.getPod() == Alembic::Abc::kWstringPOD)
                {
                    std::wstring     str;
                    sample = (void *)(&str);
                    in_property.get(sample, iss);
                    out_property->set(sample);
                }
                else
                {
                    sample = malloc(dtype.getNumBytes());
                    in_property.get(sample, iss);
                    out_property->set(sample);
                    free(sample);
                }
            }
            else if (header.isArray())
            {
                const DataType     &dtype = header.getDataType();
                IArrayProperty      in_property(cpr_ptr->getArrayProperty(i),
                                            gabcWrapExisting);
                OArrayProperty     *out_property = p_map.findArray(name);
                ArraySamplePtr      sample;

                if (!out_property)
                {
                    MetaData    metadata(in_property.getMetaData());
                    out_property = new OArrayProperty(out,
                            header.getName(),
                            dtype,
                            metadata,
                            ctx.timeSampling());
                    p_map.insert(name, out_property);
                }
                else if (in_property.isConstant())
                {
                    out_property->setFromPrevious();
                    continue;
                }

                in_property.get(sample, iss);
                out_property->set(*sample);
            }
            else
            {
                ICompoundProperty   in_property(cpr_ptr->getCompoundProperty(i),
                                            gabcWrapExisting);
                OCompoundProperty  *out_property = p_map.findCompound(name);

                if (!out_property)
                {
                    MetaData    metadata(in_property.getMetaData());
                    out_property = new OCompoundProperty(out,
                            header.getName(),
                            metadata,
                            ctx.timeSampling());
                    p_map.insert(name, out_property);
                }

                sampleCompoundProperties(p_map,
                        in_property,
                        *out_property,
                        ctx,
                        iss,
                        name);
            }
        }
    }

    // Create FaceSets for a PolyMesh
    static void
    createFaceSetsPolymesh(const GABC_IObject &node,
            OPolyMesh *obj,
            const GABC_OOptions &ctx)
    {
        IPolyMesh                   polymesh(node.object(), gabcWrapExisting);
        IPolyMeshSchema            &polymesh_s = polymesh.getSchema();
        OPolyMeshSchema            &polymesh_s_o = obj->getSchema();
        std::vector<std::string>    faceset_names;

        polymesh_s.getFaceSetNames(faceset_names);
        for (auto it = faceset_names.begin(); it != faceset_names.end(); ++it)
        {
            OFaceSet       &faceset = polymesh_s_o.createFaceSet(*it);
            OFaceSetSchema &faceset_s = faceset.getSchema();
            faceset_s.setTimeSampling(ctx.timeSampling());
        }
    }

    // Create FaceSets for a subdivision surface
    static void
    createFaceSetsSubd(const GABC_IObject &node,
            OSubD *obj,
            const GABC_OOptions &ctx)
    {
        ISubD                       subd(node.object(), gabcWrapExisting);
        ISubDSchema                &subd_s = subd.getSchema();
        OSubDSchema                &subd_s_o = obj->getSchema();
        std::vector<std::string>    faceset_names;

        subd_s.getFaceSetNames(faceset_names);
        for (auto it = faceset_names.begin(); it != faceset_names.end(); ++it)
        {
            OFaceSet       &faceset = subd_s_o.createFaceSet(*it);
            OFaceSetSchema &faceset_s = faceset.getSchema();
            faceset_s.setTimeSampling(ctx.timeSampling());
        }
    }

    // Read FaceSet samples in and then write them out to the new archive.
    // Used by PolyMesh and SubD
    template <typename ABC_SCHEMA_IN, typename ABC_SCHEMA_OUT>
    static void
    sampleFaceSets(ABC_SCHEMA_IN &input,
            Alembic::AbcGeom::OSchemaObject<ABC_SCHEMA_OUT> *output_o,
            const ISampleSelector &iss,
            std::vector<std::string> &names)
    {
        ABC_SCHEMA_OUT         &output = output_o->getSchema();
        Int32ArraySamplePtr     faces;

        for (auto it = names.begin(); it != names.end(); ++it)
        {
            UT_ASSERT(input.hasFaceSet(*it));
            UT_ASSERT(output.hasFaceSet(*it));

            IFaceSet                faceset_i = input.getFaceSet(*it);
            IFaceSetSchema         &faceset_i_s = faceset_i.getSchema();
            IFaceSetSchema::Sample  faceset_i_ss = faceset_i_s.getValue(iss);
            OFaceSet                faceset_o = output.getFaceSet(*it);

            faces = faceset_i_ss.getFaces();

            OFaceSetSchema::Sample  sample(*faces);
            faceset_o.getSchema().set(sample);
        }
    }

    // TODO: Fix child bounds, this update has messed them up again

    // Read in sample info from an IXform
    static void
    sampleXform(const GABC_IObject &node,
            OXform *obj,
            PropertyMap &arb_map,
            PropertyMap &p_map,
            bool reuse_user_props,
            const GABC_OOptions &ctx,
            const ISampleSelector &iss)
    {
        IXform              xform(node.object(), gabcWrapExisting);
        IXformSchema       &xform_s = xform.getSchema();
        ICompoundProperty   arb_in = xform_s.getArbGeomParams();
        OCompoundProperty   arb_out = obj->getSchema().getArbGeomParams();
        ICompoundProperty   up_in = xform_s.getUserProperties();
        OCompoundProperty   up_out = obj->getSchema().getUserProperties();
        XformSample         xform_ss = xform_s.getValue(iss);

        XformSample         sample;
        sample.setInheritsXforms(xform_ss.getInheritsXforms());

        obj->getSchema().set(sample);

//        if (ctx.fullBounds()) {
//            bound.xform = obj;
//            myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bound));
//        }

        if (reuse_user_props)
        {
            if (up_in.valid() && up_out.valid())
            {
                sampleCompoundProperties(p_map, up_in, up_out, ctx, iss, "");
            }
        }
        if (arb_in.valid() && arb_out.valid())
        {
            sampleCompoundProperties(arb_map, arb_in, arb_out, ctx, iss, "");
        }
    }

    // Read PolyMesh samples in and then write them out to the new archive
    static void
    samplePolyMesh(const GABC_IObject &node,
            OPolyMesh *obj,
            PropertyMap &arb_map,
            PropertyMap &p_map,
            bool reuse_user_props,
            const GABC_OOptions &ctx,
            const ISampleSelector &iss)
    {
        P3fArraySamplePtr           iPos;
        Int32ArraySamplePtr         iInd;
        Int32ArraySamplePtr         iCnt;
        ON3fGeomParam::Sample       iNml;
        OV2fGeomParam::Sample       iUVs;
        V3fArraySamplePtr           iVel;
        IN3fGeomParam               nml_param;
        IV2fGeomParam               uvs_param;
        N3fArraySamplePtr           nml_samples;
        V2fArraySamplePtr           uv_samples;
        UInt32ArraySample           uv_indices;
        IPolyMesh                   polymesh(node.object(), gabcWrapExisting);
        IPolyMeshSchema            &polymesh_s = polymesh.getSchema();
        IPolyMeshSchema::Sample     polymesh_ss = polymesh_s.getValue(iss);
        ICompoundProperty           arb_in = polymesh_s.getArbGeomParams();
        OCompoundProperty           arb_out = obj->getSchema().getArbGeomParams();
        ICompoundProperty           up_in = polymesh_s.getUserProperties();
        OCompoundProperty           up_out = obj->getSchema().getUserProperties();
        //Bounds                      bounds;
        std::vector<std::string>    faceset_names;

        iPos = polymesh_ss.getPositions();
        iInd = polymesh_ss.getFaceIndices();
        iCnt = polymesh_ss.getFaceCounts();
        iVel = polymesh_ss.getVelocities();

        uvs_param = polymesh_s.getUVsParam();
        if (uvs_param.valid())
        {
            IV2fGeomParam::Sample   sample = uvs_param.getExpandedValue(iss);
            UInt32ArraySamplePtr    indices_ptr = sample.getIndices();

            uv_indices = indices_ptr ? *indices_ptr : UInt32ArraySample();
            uv_samples = sample.getVals();
            iUVs = OV2fGeomParam::Sample(
                    *uv_samples,
                    uv_indices,
                    Alembic::AbcGeom::kFacevaryingScope);
        }
        nml_param = polymesh_s.getNormalsParam();
        if (nml_param.valid())
        {
            nml_samples = nml_param.getExpandedValue(iss).getVals();
            iNml = ON3fGeomParam::Sample(
                    *nml_samples,
                    Alembic::AbcGeom::kFacevaryingScope);
        }

        polymesh_s.getFaceSetNames(faceset_names);

        OPolyMeshSchema::Sample    sample(*iPos, *iInd, *iCnt, iUVs, iNml);
        if (iVel && iVel->valid())
        {
            sample.setVelocities(*iVel);
        }
//        if (ctx.fullBounds()) {
//            bounds.box = new Box3d;
//            *(bounds.box) = Alembic::AbcGeom::ComputeBoundsFromPositions(*iPos);
//            sample.setSelfBounds(*(bounds.box));
//            myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bounds));
//        }

        obj->getSchema().set(sample);
        sampleFaceSets<IPolyMeshSchema, OPolyMeshSchema>(
                polymesh_s,
                obj,
                iss,
                faceset_names);

        if (reuse_user_props)
        {
            if (up_in.valid() && up_out.valid())
            {
                sampleCompoundProperties(p_map, up_in, up_out, ctx, iss, "");
            }
        }
        if (arb_in.valid() && arb_out.valid())
        {
            sampleCompoundProperties(arb_map, arb_in, arb_out, ctx, iss, "");
        }
    }

    // Read SubD samples in and then write them out to the new archive
    static void
    sampleSubD(const GABC_IObject &node,
            OSubD *obj,
            PropertyMap &arb_map,
            PropertyMap &p_map,
            bool reuse_user_props,
            const GABC_OOptions &ctx,
            const ISampleSelector &iss)
    {
        P3fArraySamplePtr           iPos;
        Int32ArraySamplePtr         iInd;
        Int32ArraySamplePtr         iCnt;
        Int32ArraySamplePtr         iCreaseIndicesPtr;
        Int32ArraySample            iCreaseIndices;
        Int32ArraySamplePtr         iCreaseLengthsPtr;
        Int32ArraySample            iCreaseLengths;
        FloatArraySamplePtr         iCreaseSharpnessesPtr;
        FloatArraySample            iCreaseSharpnesses;
        Int32ArraySamplePtr         iCornerIndicesPtr;
        Int32ArraySample            iCornerIndices;
        FloatArraySamplePtr         iCornerSharpnessesPtr;
        FloatArraySample            iCornerSharpnesses;
        Int32ArraySamplePtr         iHolesPtr;
        Int32ArraySample            iHoles;
        V3fArraySamplePtr           iVel;
        OV2fGeomParam::Sample       iUVs;
        IV2fGeomParam               uvs_param;
        V2fArraySamplePtr           uv_samples;
        UInt32ArraySample           uv_indices;
        ISubD                       subd(node.object(), gabcWrapExisting);
        ISubDSchema                &subd_s = subd.getSchema();
        ISubDSchema::Sample         subd_ss = subd_s.getValue(iss);
        ICompoundProperty           arb_in = subd_s.getArbGeomParams();
        OCompoundProperty           arb_out = obj->getSchema().getArbGeomParams();
        ICompoundProperty           up_in = subd_s.getUserProperties();
        OCompoundProperty           up_out = obj->getSchema().getUserProperties();
//        Bounds                      bounds;
        std::vector<std::string>    faceset_names;

        iPos = subd_ss.getPositions();
        iInd = subd_ss.getFaceIndices();
        iCnt = subd_ss.getFaceCounts();

        iCreaseIndicesPtr = subd_ss.getCreaseIndices();
        if (iCreaseIndicesPtr)
        {
            iCreaseIndices = *iCreaseIndicesPtr;
        }
        iCreaseLengthsPtr = subd_ss.getCreaseLengths();
        if (iCreaseLengthsPtr)
        {
            iCreaseLengths = *iCreaseLengthsPtr;
        }
        iCreaseSharpnessesPtr = subd_ss.getCreaseSharpnesses();
        if (iCreaseSharpnessesPtr)
        {
            iCreaseSharpnesses = *iCreaseSharpnessesPtr;
        }
        iCornerIndicesPtr = subd_ss.getCornerIndices();
        if (iCornerIndicesPtr)
        {
            iCornerIndices = *iCornerIndicesPtr;
        }
        iCornerSharpnessesPtr = subd_ss.getCornerSharpnesses();
        if (iCornerSharpnessesPtr)
        {
            iCornerSharpnesses = *iCornerSharpnessesPtr;
        }
        iHolesPtr = subd_ss.getHoles();
        if (iHolesPtr)
        {
            iHoles = *iHolesPtr;
        }

        iVel = subd_ss.getVelocities();

        uvs_param = subd_s.getUVsParam();
        if (uvs_param.valid())
        {
            IV2fGeomParam::Sample   sample = uvs_param.getExpandedValue(iss);
            UInt32ArraySamplePtr    indices_ptr = sample.getIndices();

            uv_indices = indices_ptr ? *indices_ptr : UInt32ArraySample();
            uv_samples = sample.getVals();
            iUVs = OV2fGeomParam::Sample(
                    *uv_samples,
                    uv_indices,
                    Alembic::AbcGeom::kFacevaryingScope);
        }

        subd_s.getFaceSetNames(faceset_names);

        OSubDSchema::Sample     sample(
                *iPos,
                *iInd,
                *iCnt,
                iCreaseIndices,
                iCreaseLengths,
                iCreaseSharpnesses,
                iCornerIndices,
                iCornerSharpnesses,
                iHoles);
        if (iVel && iVel->valid())
        {
            sample.setVelocities(*iVel);
        }
        if (iUVs.valid())
        {
            sample.setUVs(iUVs);
        }
        sample.setInterpolateBoundary(
                subd_ss.getInterpolateBoundary());
        sample.setFaceVaryingInterpolateBoundary(
                subd_ss.getFaceVaryingInterpolateBoundary());
        sample.setFaceVaryingPropagateCorners(
                subd_ss.getFaceVaryingPropagateCorners());
//        if (ctx.fullBounds()) {
//            bounds.box = new Box3d;
//            *(bounds.box) = Alembic::AbcGeom::ComputeBoundsFromPositions(*iPos);
//            sample.setSelfBounds(*(bounds.box));
//            myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bounds));
//        }

        obj->getSchema().set(sample);
        sampleFaceSets<ISubDSchema, OSubDSchema>(
                subd_s,
                obj,
                iss,
                faceset_names);

        if (reuse_user_props)
        {
            if (up_in.valid() && up_out.valid())
            {
                sampleCompoundProperties(p_map, up_in, up_out, ctx, iss, "");
            }
        }
        if (arb_in.valid() && arb_out.valid())
        {
            sampleCompoundProperties(arb_map, arb_in, arb_out, ctx, iss, "");
        }
    }

    // Read Points samples in and then write them out to the new archive
    static void
    samplePoints(const GABC_IObject &node,
            OPoints *obj,
            PropertyMap &arb_map,
            PropertyMap &p_map,
            bool reuse_user_props,
            const GABC_OOptions &ctx,
            const ISampleSelector &iss)
    {
        P3fArraySamplePtr           iPos;
        UInt64ArraySamplePtr        iId;
        V3fArraySamplePtr           iVelPtr;
        V3fArraySample              iVel;
        OFloatGeomParam::Sample     iWidths;
        IFloatGeomParam             width_param;
        FloatArraySamplePtr         width_samples;
        IPoints                     points(node.object(),gabcWrapExisting);
        IPointsSchema              &points_s = points.getSchema();
        IPointsSchema::Sample       points_ss = points_s.getValue(iss);
        ICompoundProperty           arb_in = points_s.getArbGeomParams();
        OCompoundProperty           arb_out = obj->getSchema().getArbGeomParams();
        ICompoundProperty           up_in = points_s.getUserProperties();
        OCompoundProperty           up_out = obj->getSchema().getUserProperties();
//        Bounds                      bounds;

        iPos = points_ss.getPositions();
        iId = points_ss.getIds();
        iVelPtr = points_ss.getVelocities();
        if (iVelPtr)
        {
            iVel = *iVelPtr;
        }

        width_param = points_s.getWidthsParam();
        if (width_param.valid())
        {
            width_samples = width_param.getExpandedValue(iss).getVals();
            iWidths = OFloatGeomParam::Sample(
                    *width_samples,
                    Alembic::AbcGeom::kConstantScope);
        }

        OPointsSchema::Sample       sample(*iPos, *iId, iVel, iWidths);
//        if (ctx.fullBounds()) {
//            bounds.box = new Box3d;
//            *(bounds.box) = Alembic::AbcGeom::ComputeBoundsFromPositions(*iPos);
//            sample.setSelfBounds(*(bounds.box));
//            myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bounds));
//        }

        obj->getSchema().set(sample);

        if (up_in.valid() && up_out.valid())
        {
            sampleCompoundProperties(p_map, up_in, up_out, ctx, iss, "");
        }
        if (arb_in.valid() && arb_out.valid())
        {
            sampleCompoundProperties(arb_map, arb_in, arb_out, ctx, iss, "");
        }
    }

    // Read Curves samples in and then write them out to the new archive
    static void
    sampleCurves(const GABC_IObject &node,
            OCurves *obj,
            PropertyMap &arb_map,
            PropertyMap &p_map,
            bool reuse_user_props,
            const GABC_OOptions &ctx,
            const ISampleSelector &iss)
    {
        P3fArraySamplePtr           iPos;
        Int32ArraySamplePtr         iCnt;
        CurveType                   iOrder;
        CurvePeriodicity            iPeriod;
        BasisType                   iBasis;
        FloatArraySamplePtr         iPosWeightPtr;
        FloatArraySample            iPosWeight;
        UcharArraySamplePtr         iOrdersPtr;
        UcharArraySample            iOrders;
        FloatArraySamplePtr         iKnotsPtr;
        FloatArraySample            iKnots;
        V3fArraySamplePtr           iVel;
        OFloatGeomParam::Sample     iWidths;
        OV2fGeomParam::Sample       iUVs;
        ON3fGeomParam::Sample       iNml;
        IFloatGeomParam             width_param;
        IN3fGeomParam               nml_param;
        IV2fGeomParam               uvs_param;
        FloatArraySamplePtr         width_samples;
        N3fArraySamplePtr           nml_samples;
        V2fArraySamplePtr           uv_samples;
        UInt32ArraySample           uv_indices;
        ICurves                     curves(node.object(), gabcWrapExisting);
        ICurvesSchema              &curves_s = curves.getSchema();
        ICurvesSchema::Sample       curves_ss = curves_s.getValue(iss);
        ICompoundProperty           arb_in = curves_s.getArbGeomParams();
        OCompoundProperty           arb_out = obj->getSchema().getArbGeomParams();
        ICompoundProperty           up_in = curves_s.getUserProperties();
        OCompoundProperty           up_out = obj->getSchema().getUserProperties();
//        Bounds                      bounds;

        iPos = curves_ss.getPositions();
        iCnt = curves_ss.getCurvesNumVertices();
        iOrder = curves_ss.getType();
        iPeriod = curves_ss.getWrap();
        iBasis = curves_ss.getBasis();
        iPosWeightPtr = curves_ss.getPositionWeights();
        if (iPosWeightPtr)
        {
            iPosWeight = *iPosWeightPtr;
        }
        iOrdersPtr = curves_ss.getOrders();
        if (iOrdersPtr)
        {
            iOrders = *iOrdersPtr;
        }
        iKnotsPtr = curves_ss.getKnots();
        if (iKnotsPtr)
        {
            iKnots = *iKnotsPtr;
        }
        iVel = curves_ss.getVelocities();

        width_param = curves_s.getWidthsParam();
        if (width_param.valid())
        {
            width_samples = width_param.getExpandedValue(iss).getVals();
            iWidths = OFloatGeomParam::Sample(
                    *width_samples,
                    Alembic::AbcGeom::kConstantScope);
        }
        uvs_param = curves_s.getUVsParam();
        if (uvs_param.valid())
        {
            IV2fGeomParam::Sample   sample = uvs_param.getExpandedValue(iss);
            UInt32ArraySamplePtr    indices_ptr = sample.getIndices();

            uv_indices = indices_ptr ? *indices_ptr : UInt32ArraySample();
            uv_samples = sample.getVals();
            iUVs = OV2fGeomParam::Sample(
                    *uv_samples,
                    uv_indices,
                    Alembic::AbcGeom::kVertexScope);
        }
        nml_param = curves_s.getNormalsParam();
        if (nml_param.valid())
        {
            nml_samples = nml_param.getExpandedValue(iss).getVals();
            iNml = ON3fGeomParam::Sample(
                    *nml_samples,
                    Alembic::AbcGeom::kVertexScope);
        }

        OCurvesSchema::Sample	sample(
                *iPos,
                *iCnt,
                iOrder,
                iPeriod,
                iWidths,
                iUVs,
                iNml,
                iBasis,
                iPosWeight,
                iOrders,
                iKnots);
        if (iVel && iVel->valid())
        {
            sample.setVelocities(*iVel);
        }
//        if (ctx.fullBounds()) {
//            bounds.box = new Box3d;
//            *(bounds.box) = Alembic::AbcGeom::ComputeBoundsFromPositions(*iPos);
//            sample.setSelfBounds(*(bounds.box));
//            myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bounds));
//        }

        obj->getSchema().set(sample);

        if (reuse_user_props)
        {
            if (up_in.valid() && up_out.valid())
            {
                sampleCompoundProperties(p_map, up_in, up_out, ctx, iss, "");
            }
        }
        if (arb_in.valid() && arb_out.valid())
        {
            sampleCompoundProperties(arb_map, arb_in, arb_out, ctx, iss, "");
        }
    }

    // Read NuPatch samples in and then write them out to the new archive
    static void
    sampleNuPatch(const GABC_IObject &node,
            ONuPatch *obj,
            PropertyMap &arb_map,
            PropertyMap &p_map,
            bool reuse_user_props,
            const GABC_OOptions &ctx,
            const ISampleSelector &iss)
    {
        P3fArraySamplePtr       iPos;
        int                     iNumU;
        int                     iNumV;
        int                     iOrderU;
        int                     iOrderV;
        FloatArraySamplePtr     iUKnot;
        FloatArraySamplePtr     iVKnot;
        FloatArraySamplePtr     iPosWeightPtr;
        FloatArraySample        iPosWeight;
        V3fArraySamplePtr       iVel;
        ON3fGeomParam::Sample   iNml;
        OV2fGeomParam::Sample   iUVs;
        IN3fGeomParam           nml_param;
        IV2fGeomParam           uvs_param;
        N3fArraySamplePtr       nml_samples;
        V2fArraySamplePtr       uv_samples;
        UInt32ArraySample       uv_indices;
        INuPatch                nupatch(node.object(), gabcWrapExisting);
        INuPatchSchema         &nupatch_s = nupatch.getSchema();
        INuPatchSchema::Sample  nupatch_ss = nupatch_s.getValue(iss);
        ICompoundProperty       arb_in = nupatch_s.getArbGeomParams();
        OCompoundProperty       arb_out = obj->getSchema().getArbGeomParams();
        ICompoundProperty       up_in = nupatch_s.getUserProperties();
        OCompoundProperty       up_out = obj->getSchema().getUserProperties();
//        Bounds                  bounds;

        iPos = nupatch_ss.getPositions();
        iNumU = nupatch_ss.getNumU();
        iNumV = nupatch_ss.getNumV();
        iOrderU = nupatch_ss.getUOrder();
        iOrderV = nupatch_ss.getVOrder();
        iUKnot = nupatch_ss.getUKnot();
        iVKnot = nupatch_ss.getVKnot();
        iPosWeightPtr = nupatch_ss.getPositionWeights();
        if (iPosWeightPtr)
        {
            iPosWeight = *iPosWeightPtr;
        }
        iVel = nupatch_ss.getVelocities();

        uvs_param = nupatch_s.getUVsParam();
        if (uvs_param.valid())
        {
            IV2fGeomParam::Sample   sample = uvs_param.getExpandedValue(iss);
            UInt32ArraySamplePtr    indices_ptr = sample.getIndices();

            uv_indices = indices_ptr ? *indices_ptr : UInt32ArraySample();
            uv_samples = sample.getVals();
            iUVs = OV2fGeomParam::Sample(
                    *uv_samples,
                    uv_indices,
                    Alembic::AbcGeom::kVertexScope);
        }
        nml_param = nupatch_s.getNormalsParam();
        if (nml_param.valid())
        {
            nml_samples = nml_param.getExpandedValue(iss).getVals();
            iNml = ON3fGeomParam::Sample(
                    *nml_samples,
                    Alembic::AbcGeom::kVertexScope);
        }

        ONuPatchSchema::Sample      sample(
                *iPos,
                iNumU,
                iNumV,
                iOrderU,
                iOrderV,
                *iUKnot,
                *iVKnot,
                iNml,
                iUVs,
                iPosWeight);
        if (iVel && iVel->valid())
        {
            sample.setVelocities(*iVel);
        }
//        if (ctx.fullBounds()) {
//            bounds.box = new Box3d;
//            *(bounds.box) = Alembic::AbcGeom::ComputeBoundsFromPositions(*iPos);
//            sample.setSelfBounds(*(bounds.box));
//            myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bounds));
//        }

        obj->getSchema().set(sample);

        if (reuse_user_props)
        {
            if (up_in.valid() && up_out.valid())
            {
                sampleCompoundProperties(p_map, up_in, up_out, ctx, iss, "");
            }
        }
        if (arb_in.valid() && arb_out.valid())
        {
            sampleCompoundProperties(arb_map, arb_in, arb_out, ctx, iss, "");
        }
    }
}

GABC_OGTAbc::GABC_OGTAbc(const std::string &name)
    : myType(GABC_UNKNOWN)
    , myUserPropState(UNSET)
    , myName(name)
    , myElapsedFrames(0)
{
    myShape.myVoidPtr = NULL;
}

GABC_OGTAbc::~GABC_OGTAbc()
{
    clear();
}

void
GABC_OGTAbc::clear()
{
    clearUserProperties();

    switch (myType)
    {
	case GABC_POLYMESH:
	    delete myShape.myPolyMesh;
	    break;
	case GABC_SUBD:
	    delete myShape.mySubD;
	    break;
	case GABC_POINTS:
	    delete myShape.myPoints;
	    break;
	case GABC_CURVES:
	    delete myShape.myCurves;
	    break;
	case GABC_NUPATCH:
	    delete myShape.myNuPatch;
	    break;
	case GABC_XFORM:
	    // Xform in XformMap, will be deleted from there.
	    break;
	default:
	    break;
    }
    myShape.myVoidPtr = NULL;
}

void
GABC_OGTAbc::clearUserProperties()
{
    for (GABCPropertyMap::const_iterator it = myNewUserProperties.begin();
            it != myNewUserProperties.end();
            ++it)
    {
        delete it->second;
    }
    myNewUserProperties.clear();
    myUserPropState = UNSET;
}

bool
GABC_OGTAbc::makeUserProperties(const GT_PrimitiveHandle &prim,
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

    if (prim->getPrimitiveType() == GT_GEO_PACKED)
    {
        GT_GEOPrimPacked   *packed = UTverify_cast<GT_GEOPrimPacked *>(
                                    prim.get());

        attribs_handle = packed->getInstanceAttributes();
    }
    else
    {
        attribs_handle = prim->getUniformAttributes();
    }
    if (!attribs_handle)
    {
        myUserPropState = REUSE_USER_PROPERTIES;
        return true;
    }

    meta_attrib = attribs_handle->get(GABC_Util::theUserPropsMetaAttrib, 0);
    vals_attrib = attribs_handle->get(GABC_Util::theUserPropsValsAttrib, 0);
    if (!meta_attrib && !vals_attrib)
    {
        myUserPropState = REUSE_USER_PROPERTIES;
    }
    else if (!vals_attrib)
    {
        meta_buffer = meta_attrib->getS(0,0);

        if (!meta_buffer || !(*meta_buffer))
        {
            myUserPropState = REUSE_USER_PROPERTIES;
        }
        else
        {
            myUserPropState = IGNORE_USER_PROPERTIES;
            err.warning("Found user properties metadata attribute, but not "
                    "data attribute. Ignoring user properties.");
        }
    }
    else if (!meta_attrib)
    {
        vals_buffer = vals_attrib->getS(0,0);

        if (!vals_buffer || !(*vals_buffer))
        {
            myUserPropState = REUSE_USER_PROPERTIES;
        }
        else
        {
            myUserPropState = IGNORE_USER_PROPERTIES;
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
            myUserPropState = REUSE_USER_PROPERTIES;
        }
        else if (!meta_buffer || !(*meta_buffer))
        {
            myUserPropState = IGNORE_USER_PROPERTIES;
            err.warning("User properties metadata attribute empty. Ignoring "
                    "user properties.");
        }
        else if (!vals_buffer || !(*vals_buffer))
        {
            myUserPropState = IGNORE_USER_PROPERTIES;
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
                    myNewUserProperties,
                    parent,
                    err,
                    ctx))
            {
                err.warning("Ignoring user properties (%s).",
                        myName.c_str());
                myUserPropState = IGNORE_USER_PROPERTIES;
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
GABC_OGTAbc::writeUserProperties(const GT_PrimitiveHandle &prim,
        GABC_OError &err,
        const GABC_OOptions &ctx)
{
    GT_AttributeListHandle  attribs_handle;
    GT_DataArrayHandle      meta_attrib;
    GT_DataArrayHandle      vals_attrib;

    if (prim->getPrimitiveType() == GT_GEO_PACKED)
    {
        GT_GEOPrimPacked   *packed = UTverify_cast<GT_GEOPrimPacked *>(
                                    prim.get());

        attribs_handle = packed->getInstanceAttributes();
    }
    else
    {
        attribs_handle = prim->getUniformAttributes();
    }
    if (!attribs_handle)
    {
        if (myUserPropState == WRITE_USER_PROPERTIES)
        {
            err.warning("Missing user property information for %s on frame "
                    "%" SYS_PRId64 ". Reusing previous samples.",
                    myName.c_str(),
                    (ctx.firstFrame() + myElapsedFrames + 1));
            writePropertiesFromPrevious(myNewUserProperties);
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
            writePropertiesFromPrevious(myNewUserProperties);
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
            writePropertiesFromPrevious(myNewUserProperties);
        }
        else
        {
            UT_ASSERT(myUserPropState == REUSE_USER_PROPERTIES);

            if (myElapsedFrames)
            {
                err.warning("Partial user property information detected for %s "
                        "on frame %" SYS_PRId64 ", but none detected "
                        "for previous frames. Ignoring it.",
                        myName.c_str(),
                        (ctx.firstFrame() + myElapsedFrames + 1));
                myUserPropState = REUSE_AND_ERROR_OCCURRED;
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
                writePropertiesFromPrevious(myNewUserProperties);
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
                        myNewUserProperties,
                        NULL,
                        err,
                        ctx))
                {
                    err.warning("Reusing previous user property samples "
                            "(%s, frame %" SYS_PRId64 ")",
                            myName.c_str(),
                            (ctx.firstFrame() + myElapsedFrames + 1));
                    writePropertiesFromPrevious(myNewUserProperties);
                }
            }
        }
        else
        {
            UT_ASSERT(myUserPropState == REUSE_USER_PROPERTIES);

            if ((meta_buffer && *meta_buffer) || (vals_buffer && *vals_buffer))
            {
                err.warning("User property information detected for %s on "
                        "frame %" SYS_PRId64 ", but none detected "
                        "for previous frames. Information ignored",
                        myName.c_str(),
                        (ctx.firstFrame() + myElapsedFrames + 1));
                myUserPropState = REUSE_AND_ERROR_OCCURRED;
            }
        }
    }

    return true;
}

void
GABC_OGTAbc::writeUserPropertiesFromPrevious()
{
    writePropertiesFromPrevious(myNewUserProperties);
}

bool
GABC_OGTAbc::start(const GT_PrimitiveHandle &prim,
        const OObject &parent,
        fpreal cook_time,
        const GABC_OOptions &ctx,
        GABC_OError &err,
        ObjectVisibility vis)
{
    UT_ASSERT(prim);

    const GABC_IObject  obj = getObject(prim);
    OCompoundProperty   user_props;

    myElapsedFrames = 0;
    myType = obj.nodeType();
    switch (myType)
    {
        case GABC_POLYMESH:
            myShape.myPolyMesh = new OPolyMesh(parent,
                    myName,
                    ctx.timeSampling());
            createFaceSetsPolymesh(obj, myShape.myPolyMesh, ctx);
            user_props = myShape.myPolyMesh->getSchema().getUserProperties();

            myVisibility = Alembic::AbcGeom::CreateVisibilityProperty(
                    *(myShape.myPolyMesh),
                    ctx.timeSampling());
            break;

        case GABC_SUBD:
            myShape.mySubD = new OSubD(parent, myName, ctx.timeSampling());
            createFaceSetsSubd(obj, myShape.mySubD, ctx);
            user_props = myShape.mySubD->getSchema().getUserProperties();

            myVisibility = Alembic::AbcGeom::CreateVisibilityProperty(
                    *(myShape.mySubD),
                    ctx.timeSampling());
            break;

        case GABC_POINTS:
            myShape.myPoints = new OPoints(parent, myName, ctx.timeSampling());
            user_props = myShape.myPoints->getSchema().getUserProperties();

            myVisibility = Alembic::AbcGeom::CreateVisibilityProperty(
                    *(myShape.myPoints),
                    ctx.timeSampling());
            break;

        case GABC_CURVES:
            myShape.myCurves = new OCurves(parent, myName, ctx.timeSampling());
            user_props = myShape.myCurves->getSchema().getUserProperties();

            myVisibility = Alembic::AbcGeom::CreateVisibilityProperty(
                    *(myShape.myCurves),
                    ctx.timeSampling());
            break;

        case GABC_NUPATCH:
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

    if (!makeUserProperties(prim, &user_props, err, ctx))
    {
        err.error("Error saving user properties: ");
        return false;
    }

    return update(prim, cook_time, ctx, err, vis);
}


bool
GABC_OGTAbc::startXform(const GT_PrimitiveHandle &prim,
                        OXform *xform,
                        fpreal cook_time,
                        const GABC_OOptions &ctx,
                        GABC_OError &err,
                        ObjectVisibility vis)
{
    UT_ASSERT(prim);

    OCompoundProperty   user_props = xform->getSchema().getUserProperties();

    myType = GABC_XFORM;
    myShape.myXform = xform;
    myVisibility = Alembic::AbcGeom::CreateVisibilityProperty(
                        *xform,
                        ctx.timeSampling());

    if (!makeUserProperties(prim,
            &user_props,
            err,
            ctx))
    {
        err.error("Error saving user properties: ");
        return false;
    }

    return update(prim, cook_time, ctx, err, vis);
}

bool
GABC_OGTAbc::update(const GT_PrimitiveHandle &prim,
        fpreal cook_time,
	const GABC_OOptions &ctx,
	GABC_OError &err,
	ObjectVisibility vis)
{
    UT_ASSERT(prim);

    if (myType == GABC_UNKNOWN || myUserPropState == UNSET)
    {
	UT_ASSERT(0 && "Need to save first frame!");
	return false;
    }

    const GABC_IObject  obj = getObject(prim);
    ISampleSelector     iss(cook_time);
    bool                reuse_up = (myUserPropState >= REUSE_USER_PROPERTIES);

    myVisibility.set(vis);

    switch (myType)
    {
	case GABC_POLYMESH:
	    samplePolyMesh(obj,
                    myShape.myPolyMesh,
                    myArbProps,
                    myUserProps,
                    reuse_up,
                    ctx,
                    iss);
            break;

	case GABC_SUBD:
	    sampleSubD(obj,
                    myShape.mySubD,
                    myArbProps,
                    myUserProps,
                    reuse_up,
                    ctx,
                    iss);
            break;

	case GABC_POINTS:
	    samplePoints(obj,
                    myShape.myPoints,
                    myArbProps,
                    myUserProps,
                    reuse_up,
                    ctx,
                    iss);
            break;

	case GABC_CURVES:
	    sampleCurves(obj,
                    myShape.myCurves,
                    myArbProps,
                    myUserProps,
                    reuse_up,
                    ctx,
                    iss);
            break;

	case GABC_NUPATCH:
	    sampleNuPatch(obj,
                    myShape.myNuPatch,
                    myArbProps,
                    myUserProps,
                    reuse_up,
                    ctx,
                    iss);
            break;

        case GABC_XFORM:
            sampleXform(obj,
                    myShape.myXform,
                    myArbProps,
                    myUserProps,
                    reuse_up,
                    ctx,
                    iss);
            break;

	default:
	    UT_ASSERT(0 && "Unhandled primitive");
	    return false;
    }

    // Step in here even if we're reusing existing user properties to check
    // for errors.
    if (myUserPropState == WRITE_USER_PROPERTIES
            || myUserPropState == REUSE_USER_PROPERTIES)
    {
        if (!writeUserProperties(prim, err, ctx))
        {
            err.error("Error saving user properties: ");
            return false;
        }
    }

    ++myElapsedFrames;
    return true;
}

bool
GABC_OGTAbc::updateFromPrevious(GABC_OError &err,
        ObjectVisibility vis,
        exint frames)
{
    if (myType == GABC_UNKNOWN)
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
	case GABC_POLYMESH:
	    for (exint i = 0; i < frames; ++i)
	    {
                myVisibility.set(vis);
                myShape.myPolyMesh->getSchema().setFromPrevious();
                myArbProps.setFromPrevious();
                myUserProps.setFromPrevious();
            }
	    break;

	case GABC_SUBD:
	    for (exint i = 0; i < frames; ++i)
	    {
                myVisibility.set(vis);
                myShape.mySubD->getSchema().setFromPrevious();
                myArbProps.setFromPrevious();
                myUserProps.setFromPrevious();
            }
	    break;

	case GABC_POINTS:
	    for (exint i = 0; i < frames; ++i)
	    {
                myVisibility.set(vis);
                myShape.myPoints->getSchema().setFromPrevious();
                myArbProps.setFromPrevious();
                myUserProps.setFromPrevious();
            }
	    break;

	case GABC_CURVES:
	    for (exint i = 0; i < frames; ++i)
	    {
                myVisibility.set(vis);
                myShape.myCurves->getSchema().setFromPrevious();
                myArbProps.setFromPrevious();
                myUserProps.setFromPrevious();
            }
	    break;

	case GABC_NUPATCH:
	    for (exint i = 0; i < frames; ++i)
	    {
                myVisibility.set(vis);
                myShape.myNuPatch->getSchema().setFromPrevious();
                myArbProps.setFromPrevious();
                myUserProps.setFromPrevious();
            }
	    break;

        case GABC_XFORM:
	    for (exint i = 0; i < frames; ++i)
	    {
                myVisibility.set(vis);
                myShape.myXform->getSchema().setFromPrevious();
                myArbProps.setFromPrevious();
                myUserProps.setFromPrevious();
            }
            break;

	default:
	    UT_ASSERT(0 && "Unhandled primitive");
	    return false;
    }

    myElapsedFrames += frames;
    return true;
}
