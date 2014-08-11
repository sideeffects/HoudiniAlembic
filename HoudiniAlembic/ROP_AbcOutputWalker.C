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

ROP_AbcOutputWalker::ROP_AbcOutputWalker(const OObject *parent,
        BoundsMap *bounds,
        RecordsMap *records,
        TypeMap *types,
        GABC_OError &err,
        fpreal time)
    : myErrorHandler(err)
    , myOriginalParent(parent)
    , myCurrentParent(parent)
    , myBoundsMap(bounds)
    , myRecordsMap(records)
    , myTypeMap(types)
    , myMatrix()
    , myAdditionalSampleTime(time)
{}

// Read Property samples in then write them out to the new archive. Used
// for both arbitrary geometry parameters and user properties.
void
ROP_AbcOutputWalker::sampleCompoundProperties(PropertyMap *p_map,
        ICompoundProperty &in,
        OCompoundProperty &out,
        const ROP_AbcContext &ctx,
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
            OScalarProperty    *out_property = p_map->findScalar(name);
            void               *sample;

            if (!out_property)
            {
                MetaData    metadata(in_property.getMetaData());
                out_property = new OScalarProperty(out,
                        header.getName(),
                        dtype,
                        metadata,
                        ctx.timeSampling());
                p_map->insert(name, out_property);
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
            OArrayProperty     *out_property = p_map->findArray(name);
            ArraySamplePtr      sample;

            if (!out_property)
            {
                MetaData    metadata(in_property.getMetaData());
                out_property = new OArrayProperty(out,
                        header.getName(),
                        dtype,
                        metadata,
                        ctx.timeSampling());
                p_map->insert(name, out_property);
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
            OCompoundProperty  *out_property = p_map->findCompound(name);

            if (!out_property)
            {
                MetaData    metadata(in_property.getMetaData());
                out_property = new OCompoundProperty(out,
                        header.getName(),
                        metadata,
                        ctx.timeSampling());
                p_map->insert(name, out_property);
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

// Read in sample info from an IXform
void
ROP_AbcOutputWalker::readXformSample(const GABC_IObject &node,
        OXform *obj,
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
void
ROP_AbcOutputWalker::writeXformSample(const GABC_IObject &node,
        OXform *obj,
        PropertyMap *arb_map,
        PropertyMap *p_map,
        const ROP_AbcContext &ctx,
        const ISampleSelector &iss,
        UT_Matrix4D &iMatrix,
        bool iInheritXform) const
{
    IXform              xform(node.object(), gabcWrapExisting);
    IXformSchema       &xform_s = xform.getSchema();
    ICompoundProperty   arb_in = xform_s.getArbGeomParams();
    OCompoundProperty   arb_out = obj->getSchema().getArbGeomParams();
    ICompoundProperty   up_in = xform_s.getUserProperties();
    OCompoundProperty   up_out = obj->getSchema().getUserProperties();

    Bounds              bound;
    XformSample         sample;
    sample.setMatrix(GABC_Util::getM(iMatrix));
    sample.setInheritsXforms(iInheritXform);

    obj->getSchema().set(sample);
    bound.xform = obj;
    myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bound));

    if (up_in.valid() && up_out.valid())
    {
        sampleCompoundProperties(p_map, up_in, up_out, ctx, iss, "");
    }
    if (arb_in.valid() && arb_out.valid())
    {
        sampleCompoundProperties(arb_map, arb_in, arb_out, ctx, iss, "");
    }
}

// Write out sample info to an OXform
void
ROP_AbcOutputWalker::writeSimpleXformSample(OXform *obj,
        UT_Matrix4D &iMatrix,
        bool iInheritXform) const
{
    Bounds          bound;
    XformSample     sample;
    sample.setMatrix(GABC_Util::getM(iMatrix));
    sample.setInheritsXforms(iInheritXform);

    obj->getSchema().set(sample);
    bound.xform = obj;
    myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bound));
}

// Create FaceSets for a PolyMesh
void
ROP_AbcOutputWalker::createFaceSetsPolymesh(const GABC_IObject &node,
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
void
ROP_AbcOutputWalker::createFaceSetsSubd(const GABC_IObject &node,
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
void
ROP_AbcOutputWalker::sampleFaceSets(ABC_SCHEMA_IN &input,
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
void
ROP_AbcOutputWalker::samplePolyMesh(const GABC_IObject &node,
        OPolyMesh *obj,
        PropertyMap *arb_map,
        PropertyMap *p_map,
        const ROP_AbcContext &ctx,
        const ISampleSelector &iss) const
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
    Bounds                      bound;
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
    bound.bounds = Alembic::AbcGeom::ComputeBoundsFromPositions(*iPos);
    sample.setSelfBounds(bound.bounds);
    myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bound));

    obj->getSchema().set(sample);
    sampleFaceSets<IPolyMeshSchema, OPolyMeshSchema>(
            polymesh_s,
            obj,
            iss,
            faceset_names);

    if (up_in.valid() && up_out.valid())
    {
        sampleCompoundProperties(p_map, up_in, up_out, ctx, iss, "");
    }
    if (arb_in.valid() && arb_out.valid())
    {
        sampleCompoundProperties(arb_map, arb_in, arb_out, ctx, iss, "");
    }
}

// Read SubD samples in and then write them out to the new archive
void
ROP_AbcOutputWalker::sampleSubD(const GABC_IObject &node,
        OSubD *obj,
        PropertyMap *arb_map,
        PropertyMap *p_map,
        const ROP_AbcContext &ctx,
        const ISampleSelector &iss) const
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
    Bounds                      bound;
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
    bound.bounds = Alembic::AbcGeom::ComputeBoundsFromPositions(*iPos);
    sample.setSelfBounds(bound.bounds);
    myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bound));

    obj->getSchema().set(sample);
    sampleFaceSets<ISubDSchema, OSubDSchema>(
            subd_s,
            obj,
            iss,
            faceset_names);

    if (up_in.valid() && up_out.valid())
    {
        sampleCompoundProperties(p_map, up_in, up_out, ctx, iss, "");
    }
    if (arb_in.valid() && arb_out.valid())
    {
        sampleCompoundProperties(arb_map, arb_in, arb_out, ctx, iss, "");
    }
}

// Read Points samples in and then write them out to the new archive
void
ROP_AbcOutputWalker::samplePoints(const GABC_IObject &node,
        OPoints *obj,
        PropertyMap *arb_map,
        PropertyMap *p_map,
        const ROP_AbcContext &ctx,
        const ISampleSelector &iss) const
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
    ICompoundProperty           arb_in = points_s.getArbGeomParams();
    OCompoundProperty           arb_out = obj->getSchema().getArbGeomParams();
    ICompoundProperty           up_in = points_s.getUserProperties();
    OCompoundProperty           up_out = obj->getSchema().getUserProperties();
    Bounds                      bound;

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
    bound.bounds = Alembic::AbcGeom::ComputeBoundsFromPositions(*iPos);
    sample.setSelfBounds(bound.bounds);
    myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bound));

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
void
ROP_AbcOutputWalker::sampleCurves(const GABC_IObject &node,
        OCurves *obj,
        PropertyMap *arb_map,
        PropertyMap *p_map,
        const ROP_AbcContext &ctx,
        const ISampleSelector &iss) const
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
    Bounds                      bound;

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
    bound.bounds = Alembic::AbcGeom::ComputeBoundsFromPositions(*iPos);
    sample.setSelfBounds(bound.bounds);
    myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bound));

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

// Read NuPatch samples in and then write them out to the new archive
void
ROP_AbcOutputWalker::sampleNuPatch(const GABC_IObject &node,
        ONuPatch *obj,
        PropertyMap *arb_map,
        PropertyMap *p_map,
        const ROP_AbcContext &ctx,
        const ISampleSelector &iss) const
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
    Bounds                  bound;

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
    bound.bounds = Alembic::AbcGeom::ComputeBoundsFromPositions(*iPos);
    sample.setSelfBounds(bound.bounds);
    myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bound));

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

// Read Camera samples in and then write them out to the new archive
void
ROP_AbcOutputWalker::sampleCamera(const GABC_IObject &node,
        OCamera *obj,
        PropertyMap *arb_map,
        PropertyMap *p_map,
        const ROP_AbcContext &ctx,
        const ISampleSelector &iss)
{
    ICamera                 camera(node.object(), gabcWrapExisting);
    ICameraSchema          &camera_s = camera.getSchema();
    CameraSample            camera_ss = camera_s.getValue(iss);
    CameraSample            sample;
    ICompoundProperty       arb_in = camera_s.getArbGeomParams();
    OCompoundProperty       arb_out = obj->getSchema().getArbGeomParams();
    ICompoundProperty       up_in = camera_s.getUserProperties();
    OCompoundProperty       up_out = obj->getSchema().getUserProperties();

    sample.setFocalLength(camera_ss.getFocalLength());
    sample.setHorizontalAperture(camera_ss.getHorizontalAperture());
    sample.setHorizontalFilmOffset(camera_ss.getHorizontalFilmOffset());
    sample.setVerticalAperture(camera_ss.getVerticalAperture());
    sample.setVerticalFilmOffset(camera_ss.getVerticalFilmOffset());
    sample.setLensSqueezeRatio(camera_ss.getLensSqueezeRatio());
    sample.setOverScanLeft(camera_ss.getOverScanLeft());
    sample.setOverScanRight(camera_ss.getOverScanRight());
    sample.setOverScanTop(camera_ss.getOverScanTop());
    sample.setOverScanBottom(camera_ss.getOverScanBottom());
    sample.setFStop(camera_ss.getFStop());
    sample.setFocusDistance(camera_ss.getFocusDistance());
    sample.setShutterOpen(camera_ss.getShutterOpen());
    sample.setShutterClose(camera_ss.getShutterClose());
    sample.setNearClippingPlane(camera_ss.getNearClippingPlane());
    sample.setFarClippingPlane(camera_ss.getFarClippingPlane());

    for (unsigned int i = 0; i < camera_ss.getNumOps(); ++i)
    {
        sample.addOp(camera_ss[i]);
    }

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

// Create OObjects for a parent node and any ancestor nodes that don't
// have existing OObjects yet.
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
    OVisibilityProperty     o_vis;
    OXform                 *obj;
    PropertyMap            *arb_map;
    PropertyMap            *p_map;
    Record                 *record;
    RecordsMap::iterator    it = myRecordsMap->find(node.objectPath());
    Bounds                  bound;
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
        name_buffer.clear();
        name_buffer.sprintf("%s_%d",
                node.getName().c_str(),
                (unsigned int)counter);
        object_name = name_buffer.buffer();
    }

    obj = new OXform(*myCurrentParent,
            object_name,
            ctx.timeSampling());
    myCurrentParent = obj;
    o_vis = Alembic::AbcGeom::CreateVisibilityProperty(
            *obj,
            ctx.timeSampling());
    arb_map = new PropertyMap();
    p_map = new PropertyMap();

    record = new Record(obj, o_vis, arb_map, p_map);
    myRecordsMap->insert(RecordsMapInsert(node.objectPath(), record));
    myTypeMap->insert(TypeMapInsert(obj->getPtr(), XFORM));
    bound.xform = obj;
    myBoundsMap->insert(BoundsMapInsert(obj->getPtr(), bound));

    return true;
}
