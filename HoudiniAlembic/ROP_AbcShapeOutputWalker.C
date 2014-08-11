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

#include "ROP_AbcShapeOutputWalker.h"

using namespace GABC_NAMESPACE;

ROP_AbcShapeOutputWalker::ROP_AbcShapeOutputWalker(const OObject *parent,
        BoundsMap *bounds,
        RecordsMap *records,
        TypeMap *types,
        OReaderCollection *counts,
        GABC_OError &err,
        fpreal time)
    : ROP_AbcOutputWalker(parent, bounds, records, types, err, time)
    , myTotalCounts(counts)
    , myRunningCounts()
    , myStoredMatrix()
    , myCountsFreeze(false)
{}

bool
ROP_AbcShapeOutputWalker::process(const GABC_IObject &node,
         const ROP_AbcContext &ctx,
         const GT_TransformHandle &transform,
         bool create_nodes,
         bool visible_nodes,
         int sample_limit) const
{
    GABC_IObject            parent_node = node.getParent();
    OVisibilityProperty    *vis;
    OXform                 *obj;
    PropertyMap            *arb_map;
    PropertyMap            *p_map;
    Record                 *record;
    RecordsMap::iterator    it;
    UT_Matrix4D             xform_matrix;
    UT_WorkBuffer           name_buffer;
    std::string             node_name;
    exint                   num_kids = parent_node.getNumChildren();
    int                     my_copy;

    myCurrentParent = myOriginalParent;
    myMatrix.identity();
    if (myTotalCounts->count(node.object().getPtr()) > 1)
    {
        if (!myCountsFreeze)
        {
            myRunningCounts.insert(node.object().getPtr());
        }
        my_copy = myRunningCounts.count(node.object().getPtr());
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
        //        /    \                    /      \
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
        //           |                       /     \
        //        [geo1]                 [geo1]  (xform_1)
        //                                          |
        //                                     [geo1_copy]
        //
        // Combined case:
        //
        //          ...                            ...
        //           |                              |
        //        (xform)      ==>               (xform)
        //        /    \                        /      \
        //    [geo1]  [geo2]             (xform_1)    (xform_2)
        //                               /      \         \
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
            myTypeMap->insert(TypeMapInsert(obj->getPtr(), XFORM));

            myErrorHandler.warning(
                    "Multiple copies of shape %s detected. Exported "
                    "hierarchy will not be exactly identical!",
                    node.objectPath().c_str());
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
            myTypeMap->insert(TypeMapInsert(obj->getPtr(), XFORM));

            myErrorHandler.warning(
                    "Multiple children detected under xform %s. "
                    "Exported hierarchy will not be exactly identical!",
                    parent_node.objectPath().c_str());
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

        record = it->second;
        obj = UTverify_cast<OXform *>(record->getObject());
        vis = record->getVisibility();
        arb_map = record->getArbGProperties();
        p_map = record->getUserProperties();

        vis->set(Alembic::AbcGeom::kVisibilityDeferred);

        // Read sample of parent transform
        readXformSample(parent_node, obj, iss, matrix, inherit);
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

            writeXformSample(parent_node,
                    obj,
                    arb_map,
                    p_map,
                    ctx,
                    iss,
                    myMatrix,
                    inherit);
        }
        else
        {
            // Just use parent transform if geometry not visible this frame
            writeXformSample(parent_node,
                    obj,
                    arb_map,
                    p_map,
                    ctx,
                    iss,
                    matrix,
                    inherit);
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
        obj = UTverify_cast<OXform *>(it->second->getObject());

        if (visible_nodes)
        {
            // Store Q' (== Q1') for use by copies
            xform_matrix.invert(inverted);
            myStoredMatrix = myMatrix * inverted;
            // Finish computation of Q
            myMatrix.invert();
            myMatrix = xform_matrix * myMatrix;
            // Apply Q to extra transform layer
            writeSimpleXformSample(obj, myMatrix, true);
        }
        else
        {
            // Just use identity matrix if geometry not visible this frame
            writeSimpleXformSample(obj, xform_matrix, true);
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
        obj = UTverify_cast<OXform *>(it->second->getObject());

        if (visible_nodes)
        {
            // Finish computation of Qn and merge with Q1'
            myMatrix.invert();
            myMatrix = xform_matrix * myMatrix * myStoredMatrix;
            // Apply Qn to extra transform layer
            writeSimpleXformSample(obj, myMatrix, true);
        }
        else
        {
            // Just use identity matrix if geometry not visible this frame
            writeSimpleXformSample(obj, xform_matrix, true);
        }
    }

    return processNode(node, ctx, my_copy, visible_nodes);
}

// Create a new OObject with visibility, user, and arbitrary geometry
// parameters. Store everything in a new Record object.
bool
ROP_AbcShapeOutputWalker::createNode(const GABC_IObject &node,
        const ROP_AbcContext &ctx,
        int copy) const
{
    OObject                *obj;
    OObjectType             obj_type;
    OVisibilityProperty     o_vis;
    PropertyMap            *arb_map;
    PropertyMap            *p_map;
    Record                 *record;
    UT_WorkBuffer           buffer;
    std::string             path;

    switch (node.nodeType())
    {
        case GABC_POLYMESH :
            obj = new OPolyMesh(*myCurrentParent,
                    node.getName(),
                    ctx.timeSampling());
            obj_type = POLYMESH;
            createFaceSetsPolymesh(node,
                    UTverify_cast<OPolyMesh *>(obj),
                    ctx);
            break;

        case GABC_SUBD :
            obj = new OSubD(*myCurrentParent,
                    node.getName(),
                    ctx.timeSampling());
            obj_type = SUBD;
            createFaceSetsSubd(node,
                    UTverify_cast<OSubD *>(obj),
                    ctx);
            break;

        case GABC_POINTS :
            obj = new OPoints(*myCurrentParent,
                    node.getName(),
                    ctx.timeSampling());
            obj_type = POINTS;
            break;

        case GABC_CURVES :
            obj = new OCurves(*myCurrentParent,
                    node.getName(),
                    ctx.timeSampling());
            obj_type = CURVES;
            break;

        case GABC_NUPATCH :
            obj = new ONuPatch(*myCurrentParent,
                    node.getName(),
                    ctx.timeSampling());
            obj_type = NUPATCH;
            break;

        default :
            return false;
    }

    buffer.sprintf("%s%d", node.objectPath().c_str(), copy);
    path = buffer.buffer();
    o_vis = Alembic::AbcGeom::CreateVisibilityProperty(
            *obj,
            ctx.timeSampling());
    arb_map = new PropertyMap();
    p_map = new PropertyMap();

    record = new Record(obj, o_vis, arb_map, p_map);
    myRecordsMap->insert(RecordsMapInsert(path, record));
    myTypeMap->insert(TypeMapInsert(obj->getPtr(), obj_type));

    return true;
}

bool
ROP_AbcShapeOutputWalker::processAncestors(const GABC_IObject &node,
        const ROP_AbcContext &ctx,
        int sample_limit,
        bool visible) const
{
    // Return if we have moved up to the root
    if (!node.valid() || !node.objectPath().compare("/"))
    {
        return true;
    }

    RecordsMap::iterator        it;
    OVisibilityProperty        *vis;
    OXform                     *obj;
    PropertyMap                *arb_map;
    PropertyMap                *p_map;
    Record                     *record;
    ISampleSelector             iss(ctx.cookTime() + myAdditionalSampleTime);
    UT_Matrix4D                 matrix;
    int                         samples;
    bool                        inherit;

    // If we cannot find an OObject for the node, something has gone wrong
    it = myRecordsMap->find(node.objectPath());
    if (it == myRecordsMap->end())
    {
        return false;
    }
    // Process our ancestors first (filial piety!)
    if (!processAncestors(node.getParent(), ctx, sample_limit, visible))
    {
        return false;
    }

    record = it->second;
    obj = UTverify_cast<OXform *>(record->getObject());
    samples = obj->getSchema().getNumSamples();
    readXformSample(node, obj, iss, matrix, inherit);

    // Sanity check: we should never have less samples than the limit
    // or we may end up double-writing the next sample
    UT_ASSERT((unsigned int)samples >= (unsigned int)sample_limit);
    // Only update if we have the expected number of samples
    if ((unsigned int)samples == (unsigned int)sample_limit)
    {
        vis = record->getVisibility();
        arb_map = record->getArbGProperties();
        p_map = record->getUserProperties();

        vis->set(Alembic::AbcGeom::kVisibilityDeferred);

        writeXformSample(node, obj, arb_map, p_map, ctx, iss, matrix, inherit);
    }

    // Add matrix to our inverse transform calculation
    if (visible)
    {
        myMatrix = matrix * myMatrix;
    }

    return true;
}

bool
ROP_AbcShapeOutputWalker::processNode(const GABC_IObject &node,
        const ROP_AbcContext &ctx,
        int copy,
        bool visible) const
{
    OObject                    *obj;
    OVisibilityProperty        *vis;
    PropertyMap                *arb_map;
    PropertyMap                *p_map;
    Record                     *record;
    RecordsMap::iterator        it;
    const ISampleSelector       iss(ctx.cookTime() + myAdditionalSampleTime);
    UT_WorkBuffer               buffer;
    std::string                 path;

    buffer.sprintf("%s%d", node.objectPath().c_str(), copy);
    path = buffer.buffer();
    it = myRecordsMap->find(path);
    // If we can't find an existing OObject, something has gone wrong.
    if (it == myRecordsMap->end())
    {
        return false;
    }

    record = it->second;
    obj = record->getObject();
    vis = record->getVisibility();
    arb_map = record->getArbGProperties();
    p_map = record->getUserProperties();

    if (visible) {
        vis->set(Alembic::AbcGeom::kVisibilityDeferred);
    }
    else
    {
        vis->set(Alembic::AbcGeom::kVisibilityHidden);
    }

    // Call the appropriate sampling function
    switch (node.nodeType())
    {
        case GABC_POLYMESH :
            samplePolyMesh(node,
                    UTverify_cast<OPolyMesh *>(obj),
                    arb_map,
                    p_map,
                    ctx,
                    iss);
            break;

        case GABC_SUBD :
            sampleSubD(node,
                    UTverify_cast<OSubD *>(obj),
                    arb_map,
                    p_map,
                    ctx,
                    iss);
            break;

        case GABC_POINTS :
            samplePoints(node,
                    UTverify_cast<OPoints *>(obj),
                    arb_map,
                    p_map,
                    ctx,
                    iss);
            break;

        case GABC_CURVES :
            sampleCurves(node,
                    UTverify_cast<OCurves *>(obj),
                    arb_map,
                    p_map,
                    ctx,
                    iss);
            break;

        case GABC_NUPATCH :
            sampleNuPatch(node,
                    UTverify_cast<ONuPatch *>(obj),
                    arb_map,
                    p_map,
                    ctx,
                    iss);
            break;

        default :
            break;
    }

    return true;
}
