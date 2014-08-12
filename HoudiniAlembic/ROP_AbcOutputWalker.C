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

#include "ROP_AbcOutputWalker.h"

using namespace GABC_NAMESPACE;

namespace {
    typedef Alembic::Abc::ISampleSelector           ISampleSelector;
    typedef Alembic::Abc::WrapExistingFlag          WrapExistingFlag;

    // Array Samples
    typedef Alembic::Abc::UcharArraySample          UcharArraySample;
    typedef Alembic::Abc::Int32ArraySample          Int32ArraySample;
    typedef Alembic::Abc::FloatArraySample          FloatArraySample;
    typedef Alembic::Abc::V3fArraySample            V3fArraySample;
    typedef Alembic::Abc::UcharArraySamplePtr       UcharArraySamplePtr;
    typedef Alembic::Abc::Int32ArraySamplePtr       Int32ArraySamplePtr;
    typedef Alembic::Abc::UInt64ArraySamplePtr      UInt64ArraySamplePtr;
    typedef Alembic::Abc::FloatArraySamplePtr       FloatArraySamplePtr;
    typedef Alembic::Abc::N3fArraySamplePtr         N3fArraySamplePtr;
    typedef Alembic::Abc::P3fArraySamplePtr         P3fArraySamplePtr;
    typedef Alembic::Abc::V2fArraySamplePtr         V2fArraySamplePtr;
    typedef Alembic::Abc::V3fArraySamplePtr         V3fArraySamplePtr;

    // General
    typedef Alembic::AbcGeom::IObject               IObject;
    typedef Alembic::AbcGeom::OObject               OObject;
    // Xform
    typedef Alembic::AbcGeom::IXform                IXform;
    typedef Alembic::AbcGeom::IXformSchema          IXformSchema;
    typedef Alembic::AbcGeom::OXform                OXform;
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
    typedef Alembic::AbcGeom::IVisibilityProperty   IVisibilityProperty;
    typedef Alembic::AbcGeom::OVisibilityProperty   OVisibilityProperty;

    const WrapExistingFlag gabcWrapExisting = Alembic::Abc::kWrapExisting;

    // Read in sample info from an IXform
    static void
    readXformSample(const GABC_IObject &node,
            const ISampleSelector &iss,
            UT_Matrix4D &transform,
            bool &inherits)
    {
        IXform          xform(node.object(), gabcWrapExisting);
        IXformSchema   &xform_s = xform.getSchema();
        XformSample     xform_ss = xform_s.getValue(iss);

        transform = GABC_Util::getM(xform_ss.getMatrix());
        inherits = xform_ss.getInheritsXforms();
    }

    // Write out sample info to an OXform
    static void
    writeXformSample(OXform *obj,
            UT_Matrix4D &iMatrix,
            bool iInheritXform)
    {
        XformSample     sample;
        sample.setMatrix(GABC_Util::getM(iMatrix));
        sample.setInheritsXforms(iInheritXform);

        obj->getSchema().set(sample);
    }

    // Create FaceSets for a PolyMesh
    static void
    createFaceSetsPolymesh(const GABC_IObject &node,
            OPolyMesh *obj,
            const ROP_AbcContext &ctx)
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
            const ROP_AbcContext &ctx)
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

    // Read PolyMesh samples in and then write them out to the new archive
    static void
    samplePolyMesh(const GABC_IObject &node,
            const ISampleSelector &iss,
            OPolyMesh *obj)
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
        IPolyMesh                   polymesh(node.object(), gabcWrapExisting);
        IPolyMeshSchema            &polymesh_s = polymesh.getSchema();
        IPolyMeshSchema::Sample     polymesh_ss = polymesh_s.getValue(iss);
        std::vector<std::string>    faceset_names;

        iPos = polymesh_ss.getPositions();
        iInd = polymesh_ss.getFaceIndices();
        iCnt = polymesh_ss.getFaceCounts();
        iVel = polymesh_ss.getVelocities();

        uvs_param = polymesh_s.getUVsParam();
        if (uvs_param.valid())
        {
            uv_samples = uvs_param.getExpandedValue(iss).getVals();
            iUVs = OV2fGeomParam::Sample(
                    *uv_samples,
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

        obj->getSchema().set(sample);
        sampleFaceSets<IPolyMeshSchema, OPolyMeshSchema>(
                polymesh_s,
                obj,
                iss,
                faceset_names);
    }

    // Read SubD samples in and then write them out to the new archive
    static void
    sampleSubD(const GABC_IObject &node,
            const ISampleSelector &iss,
            OSubD *obj)
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
        ISubD                       subd(node.object(), gabcWrapExisting);
        ISubDSchema                &subd_s = subd.getSchema();
        ISubDSchema::Sample         subd_ss = subd_s.getValue(iss);
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
            uv_samples = uvs_param.getExpandedValue(iss).getVals();
            iUVs = OV2fGeomParam::Sample(
                    *uv_samples,
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

        obj->getSchema().set(sample);
        sampleFaceSets<ISubDSchema, OSubDSchema>(
                subd_s,
                obj,
                iss,
                faceset_names);
    }

    // Read Points samples in and then write them out to the new archive
    static void
    samplePoints(const GABC_IObject &node,
            const ISampleSelector &iss,
            OPoints *obj)
    {
        P3fArraySamplePtr           iPos;
        UInt64ArraySamplePtr        iId;
        V3fArraySamplePtr           iVelPtr;
        V3fArraySample              iVel;
        OFloatGeomParam::Sample     iWidths;
        IFloatGeomParam             width_param;
        FloatArraySamplePtr        width_samples;
        IPoints                     points(node.object(),gabcWrapExisting);
        IPointsSchema              &points_s = points.getSchema();
        IPointsSchema::Sample       points_ss = points_s.getValue(iss);

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

        obj->getSchema().set(sample);
    }

    // Read Curves samples in and then write them out to the new archive
    static void
    sampleCurves(const GABC_IObject &node,
            const ISampleSelector &iss,
            OCurves *obj)
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
        FloatArraySamplePtr        width_samples;
        N3fArraySamplePtr           nml_samples;
        V2fArraySamplePtr           uv_samples;
        ICurves                     curves(node.object(), gabcWrapExisting);
        ICurvesSchema              &curves_s = curves.getSchema();
        ICurvesSchema::Sample       curves_ss = curves_s.getValue(iss);

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
            uv_samples = uvs_param.getExpandedValue(iss).getVals();
            iUVs = OV2fGeomParam::Sample(
                    *uv_samples,
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

        obj->getSchema().set(sample);
    }

    // Read NuPatch samples in and then write them out to the new archive
    static void
    sampleNuPatch(const GABC_IObject &node,
            const ISampleSelector &iss,
            ONuPatch *obj)
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
        INuPatch                nupatch(node.object(), gabcWrapExisting);
        INuPatchSchema         &nupatch_s = nupatch.getSchema();
        INuPatchSchema::Sample  nupatch_ss = nupatch_s.getValue(iss);

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
            uv_samples = uvs_param.getExpandedValue(iss).getVals();
            iUVs = OV2fGeomParam::Sample(
                    *uv_samples,
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

        obj->getSchema().set(sample);
    }
}

ROP_AbcOutputWalker::ROP_AbcOutputWalker(const OObject *parent,
        RecordsMap *records,
        StringCollection *counts,
        fpreal time)
    : myOriginalParent(parent)
    , myCurrentParent(parent)
    , myTotalCounts(counts)
    , myRunningCounts()
    , myRecordsMap(records)
    , myMatrix()
    , myStoredMatrix()
    , myAdditionalSampleTime(time)
    , myCountsFreeze(false)
{}

bool
ROP_AbcOutputWalker::process(const GABC_IObject &node,
         const ROP_AbcContext &ctx,
         const GT_TransformHandle &transform,
         bool create_nodes,
         bool visible_nodes,
         int sample_limit) const
{
    GABC_IObject            parent_node = node.getParent();
    OObject                *obj;
    Record                 *record;
    RecordsMap::iterator    it;
    UT_Matrix4D             xform_matrix;
    UT_WorkBuffer           name_buffer, error_buffer;
    std::string             node_name;
    exint                   num_kids = parent_node.getNumChildren();
    int                     my_copy;

    myCurrentParent = myOriginalParent;
    myMatrix.identity();
    if (myTotalCounts->count(node.objectPath()) > 1)
    {
        if (!myCountsFreeze)
        {
            myRunningCounts.insert(node.objectPath());
        }
        my_copy = myRunningCounts.count(node.objectPath());
    }
    else
    {
        my_copy = 1;
    }
    name_buffer.sprintf("%s_%d",
                    node.getName().c_str(),
                    my_copy);
    node_name = name_buffer.buffer();

    // Create a new OObject if necessary
    if (create_nodes)
    {
        // Start by creating any ancestors that don't yet exist
        if (!createAncestors(parent_node, ctx))
        {
            return false;
        }

        // A transform may have multiple pieces of geometry that can be
        // transformed independent of each other. For such nodes, we
        // add another transform layer, each transform containing just
        // one piece of geometry.
        //
        //          ...                        ...
        //           |                          |
        //        (xform)      ==>           (xform)
        //        |    |                     |     |
        //    [geo1]  [geo2]         (xform_1)    (xform_2)
        //                               |            |
        //                            [geo1]       [geo2]
        //
        // In addition, users may have multiple copies of the same packed
        // Alembic. In such cases, the first copy is created as usual.
        // Subsequent copies are created underneath an additional transform
        // layer, each transform containing one copy.
        //
        //          ...                        ...
        //           |                          |
        //        (xform)      ==>           (xform)
        //           |                       |     |
        //        [geo1]                 [geo1]  (xform_1)
        //                                         |
        //                                     [geo1_copy]
        //
        // Combined case:
        //
        //          ...                            ...
        //           |                              |
        //        (xform)      ==>               (xform)
        //        |    |                         |     |
        //    [geo1]  [geo2]             (xform_1)    (xform_2)
        //                               |      |         |
        //                           [geo1]  (xform_1_1)  [geo2]
        //                                        |
        //                                   [geo1_copy]
        //
        // These are necessary precautions to allow users to modify the
        // visibility and transformations of multiple packed Alembics
        // stored within the same file independently of each other.

        if (my_copy > 1)
        {
            if (num_kids > 1)
            {
                it = myRecordsMap->find(node.objectPath() + "/" + node.getName());
                if (it == myRecordsMap->end())
                {
                    return false;
                }
                myCurrentParent = it->second->getObject();
            }

            obj = new OXform(*myCurrentParent,
                    node_name,
                    ctx.timeSampling());
            record = new Record(obj);

            myCurrentParent = obj;
            myRecordsMap->insert(RecordsMapInsert(
                    node.objectPath() + "/" + node_name,
                    record));
        }
        else if (num_kids > 1)
        {
            obj = new OXform(*myCurrentParent,
                    node.getName(),
                    ctx.timeSampling());
            record = new Record(obj);

            myCurrentParent = obj;
            myRecordsMap->insert(RecordsMapInsert(
                    node.objectPath() + "/" + node.getName(),
                    record));
        }

        if (!createNode(node, ctx, my_copy))
        {
            return false;
        }
    }

    // We cannot differentiate between which transforms were applied to
    // the packed geometry by the Alembic hierarchy and which by the
    // user. However, we can figure it out as we walk through the Alembic
    // hierarchy.
    //
    // We read in the combined transform B, composed of the hierarchical
    // transforms (A1, A2, ... , An) and the user transform P:
    //
    //      P An ... A2 A1 == B
    //
    // As we traverse the hierarchy we can figure out the inverse of the
    // hierarchical transforms and extract P:
    //
    //      P == B (A1 ^ -1) (A2 ^ -1) ... (An ^ -1)
    //
    // However, we cannot write out the user transform as the final
    // transform as it truly is while retaining the hierarchy structure.
    // Doing so would also cause problems with xform nodes with multiple
    // geometry nodes or multiple copies of the same packed Alembic. So
    // instead we pretend that the user transform is the first to be applied:
    //
    //      An ... A2 A1 Q == B
    //      Q == (A1 ^ -1) (A2 ^ -1) ... (An ^ -1) B
    //
    // This Q transform is then applied to the first layer above the geometry.
    //
    // For the first copy of a geometry node without siblings Q is merged
    // with the parent transform.
    //
    // For the first copy of a geometry node with siblings Q is applied
    // to the extra transform layer.
    //
    // For copy n of a geometry node Qn is merged with the inverse of Q1 (Q1')
    // and applied to the extra transform layer. This is done to prevent
    // the unique transformation for the first copy from being applied to
    // subsequent copies.
    //
    // NOTE 1:  The inverse transform of the hierarchy is only calculated
    //          and applied if the geometry is visible.
    //
    // NOTE 2:  A reminder that UT_Matrix4D are COLUMN MATRICES !!!

    // Read in the packed Alembic transform (B)
    if (transform) {
        transform->getMatrix(xform_matrix);
    }
    else
    {
        xform_matrix.identity();
    }

    // Case 1:  First copy of geometry without siblings
    if ((num_kids == 1) && (my_copy == 1))
    {
        ISampleSelector     iss(ctx.cookTime() + myAdditionalSampleTime);
        UT_Matrix4D         matrix, inverted;
        bool                inherit;

        if (!processAncestors(parent_node.getParent(),
                ctx,
                sample_limit,
                visible_nodes))
        {
            return false;
        }

        it = myRecordsMap->find(parent_node.objectPath());
        if (it == myRecordsMap->end())
        {
            return false;
        }
        obj = it->second->getObject();

        // Read sample of parent transform
        readXformSample(parent_node, iss, matrix, inherit);
        if (visible_nodes)
        {
            myMatrix = matrix * myMatrix;
            // Store Q' (== Q1') for use by copies
            xform_matrix.invert(inverted);
            myStoredMatrix = myMatrix * inverted;
            // Finish computation of Q
            myMatrix.invert();
            myMatrix = xform_matrix * myMatrix;
            // Merge Q with parent transform
            myMatrix *= matrix;
            writeXformSample(UTverify_cast<OXform *>(obj), myMatrix, inherit);
        }
        else
        {
            // Just use parent transform if geometry not visible this frame
            writeXformSample(UTverify_cast<OXform *>(obj), matrix, inherit);
        }
    }
    // Case 2:  First copy of geometry with siblings
    else if ((num_kids > 1) && (my_copy == 1))
    {
        UT_Matrix4D         inverted;

        if (!processAncestors(parent_node, ctx, sample_limit, visible_nodes))
        {
            return false;
        }

        it = myRecordsMap->find(node.objectPath() + "/" + node.getName());
        if (it == myRecordsMap->end())
        {
            return false;
        }
        obj = it->second->getObject();

        if (visible_nodes)
        {
            // Store Q' (== Q1') for use by copies
            xform_matrix.invert(inverted);
            myStoredMatrix = myMatrix * inverted;
            // Finish computation of Q
            myMatrix.invert();
            myMatrix = xform_matrix * myMatrix;
            // Apply Q to extra transform layer
            writeXformSample(UTverify_cast<OXform *>(obj), myMatrix, true);
        }
        else
        {
            // Just use identity matrix if geometry not visible this frame
            writeXformSample(UTverify_cast<OXform *>(obj), xform_matrix, true);
        }
    }
    // Case 3:  Subsequent copy
    else if (my_copy > 1)
    {
        if (!processAncestors(parent_node, ctx, sample_limit, visible_nodes))
        {
            return false;
        }

        it = myRecordsMap->find(node.objectPath() + "/" + node_name);
        if (it == myRecordsMap->end())
        {
            return false;
        }
        obj = it->second->getObject();

        if (visible_nodes)
        {
            // Finish computation of Qn and merge with Q1'
            myMatrix.invert();
            myMatrix = xform_matrix * myMatrix * myStoredMatrix;
            // Apply Qn to extra transform layer
            writeXformSample(UTverify_cast<OXform *>(obj), myMatrix, true);
        }
        else
        {
            // Just use identity matrix if geometry not visible this frame
            writeXformSample(UTverify_cast<OXform *>(obj), xform_matrix, true);
        }
    }

    return processNode(node, ctx, my_copy, visible_nodes);
}

bool
ROP_AbcOutputWalker::createAncestors(const GABC_IObject &node,
        const ROP_AbcContext &ctx) const
{
    // Return if we have moved up to the root
    if (!node.valid() || !node.objectPath().compare("/"))
    {
        return true;
    }

    ISampleSelector         iss(ctx.cookTime() + myAdditionalSampleTime);
    OXform                 *obj;
    Record                 *record;
    RecordsMap::iterator    it = myRecordsMap->find(node.objectPath());
    UT_WorkBuffer	    name_buffer;
    std::string             object_name = node.getName();
    int                     counter = 0;

    // If this object has an existing xform, reuse it
    if (it != myRecordsMap->end())
    {
        myCurrentParent = it->second->getObject();
        return true;
    }
    // Create any ancestors that don't yet exist first
    if (!createAncestors(node.getParent(), ctx))
    {
        return false;
    }

    // Each node must have a unique name, so calculate one if the
    // original is taken.
    //
    // Since we're reading from an Alembic, this will only happen
    // if we're merging Alembic files with similar root node names
    while(const_cast<OObject *>(myCurrentParent)->getChild(object_name)
            .valid())
    {
        ++counter;
        name_buffer.sprintf("%s_%d",
                node.getName().c_str(),
                (unsigned int)counter);
        object_name = name_buffer.buffer();
    }

    obj = new OXform(*myCurrentParent,
            object_name,
            ctx.timeSampling());
    record = new Record(obj);

    myCurrentParent = obj;
    myRecordsMap->insert(RecordsMapInsert(node.objectPath(), record));

    return true;
}

bool
ROP_AbcOutputWalker::createNode(const GABC_IObject &node,
        const ROP_AbcContext &ctx,
        int copy) const
{
    OObject                *obj;
    OVisibilityProperty     o_vis;
    Record                 *record;
    UT_WorkBuffer           buffer;
    std::string             path;

    switch (node.nodeType())
    {
        case GABC_POLYMESH :
            obj = new OPolyMesh(*myCurrentParent,
                    node.getName(),
                    ctx.timeSampling());
            createFaceSetsPolymesh(node,
                    UTverify_cast<OPolyMesh *>(obj),
                    ctx);
            break;

        case GABC_SUBD :
            obj = new OSubD(*myCurrentParent,
                    node.getName(),
                    ctx.timeSampling());
            createFaceSetsSubd(node,
                    UTverify_cast<OSubD *>(obj),
                    ctx);
            break;

        case GABC_POINTS :
            obj = new OPoints(*myCurrentParent,
                    node.getName(),
                    ctx.timeSampling());
            break;

        case GABC_CURVES :
            obj = new OCurves(*myCurrentParent,
                    node.getName(),
                    ctx.timeSampling());
            break;

        case GABC_NUPATCH :
            obj = new ONuPatch(*myCurrentParent,
                    node.getName(),
                    ctx.timeSampling());
            break;

        default :
            return false;
    }

    buffer.sprintf("%s%d", node.objectPath().c_str(), copy);
    path = buffer.buffer();
    o_vis = Alembic::AbcGeom::CreateVisibilityProperty(
            *obj,
            ctx.timeSampling());
    record = new Record(obj, o_vis);
    myRecordsMap->insert(RecordsMapInsert(path, record));

    return true;
}

bool
ROP_AbcOutputWalker::processAncestors(const GABC_IObject &node,
        const ROP_AbcContext &ctx,
        int sample_limit,
        bool visible) const
{
    // Return if we have moved up to the root
    if (!node.valid() || !node.objectPath().compare("/"))
    {
        return true;
    }

    OXform                 *obj;
    ISampleSelector         iss(ctx.cookTime() + myAdditionalSampleTime);
    RecordsMap::iterator    it = myRecordsMap->find(node.objectPath());
    UT_Matrix4D             matrix;
    int                     samples;
    bool                    inherit;

    // If we cannot find an OObject for the node, something has gone wrong
    if (it == myRecordsMap->end())
    {
        return false;
    }
    // Process our ancestors first (filial piety!)
    if (!processAncestors(node.getParent(), ctx, sample_limit, visible))
    {
        return false;
    }

    readXformSample(node, iss, matrix, inherit);
    obj = UTverify_cast<OXform *>(it->second->getObject());
    samples = obj->getSchema().getNumSamples();

    // Sanity check: we should never have less samples than the limit
    // or we may end up double-writing the next sample
    UT_ASSERT((unsigned int)samples >= (unsigned int)sample_limit);

    // Only update if we have the expected number of samples
    if ((unsigned int)samples == (unsigned int)sample_limit)
    {
        writeXformSample(obj, matrix, inherit);
    }

    // Add matrix to our inverse transform calculation
    if (visible)
    {
        myMatrix = matrix * myMatrix;
    }
    return true;
}

bool
ROP_AbcOutputWalker::processNode(const GABC_IObject &node,
        const ROP_AbcContext &ctx,
        int copy,
        bool visible) const
{
    OObject                *obj;
    OVisibilityProperty    *vis;
    Record                 *record;
    RecordsMap::iterator    it;
    ISampleSelector         iss(ctx.cookTime() + myAdditionalSampleTime);
    UT_WorkBuffer           buffer;
    std::string             path;

    buffer.sprintf("%s%d", node.objectPath().c_str(), copy);
    path = buffer.buffer();
    it = myRecordsMap->find(path);
    if (it == myRecordsMap->end())
    {
        return false;
    }

    record = it->second;
    obj = record->getObject();
    vis = record->getVisibility();

    if (visible) {
        vis->set(Alembic::AbcGeom::kVisibilityDeferred);
    }
    else
    {
        vis->set(Alembic::AbcGeom::kVisibilityHidden);
    }

    switch (node.nodeType())
    {
        case GABC_POLYMESH :
            samplePolyMesh(node, iss, UTverify_cast<OPolyMesh *>(obj));
            break;

        case GABC_SUBD :
            sampleSubD(node, iss, UTverify_cast<OSubD *>(obj));
            break;

        case GABC_POINTS :
            samplePoints(node, iss, UTverify_cast<OPoints *>(obj));
            break;

        case GABC_CURVES :
            sampleCurves(node, iss, UTverify_cast<OCurves *>(obj));
            break;

        case GABC_NUPATCH :
            sampleNuPatch(node, iss, UTverify_cast<ONuPatch *>(obj));
            break;

        default :
            break;
    }

    return true;
}
