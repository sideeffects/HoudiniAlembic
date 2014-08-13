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

#include "ROP_AbcXformOutputWalker.h"

using namespace GABC_NAMESPACE;

ROP_AbcXformOutputWalker::ROP_AbcXformOutputWalker(const OObject *parent,
        BoundsMap *bounds,
        RecordsMap *records,
        TypeMap *types,
        const std::string &file,
        GABC_OError &err,
        fpreal time,
        int keep_children)
    : ROP_AbcOutputWalker(parent, bounds, records, types, err, time)
    , myMatrixCache()
    , myNodeStorage()
    , myArchiveFile(file)
    , myKeepChildren(keep_children)
    , myStoreChildren(true)
{}

bool
ROP_AbcXformOutputWalker::process(const GABC_IObject &node,
         const ROP_AbcContext &ctx,
         const GT_TransformHandle &transform,
         bool create_nodes,
         bool visible_nodes,
         int sample_limit) const
{
    GABC_IObject                parent_node = node.getParent();
    ISampleSelector             iss(ctx.cookTime() + myAdditionalSampleTime);
    OVisibilityProperty        *o_vis;
    OXform                     *obj;
    PropertyMap                *arb_map;
    PropertyMap                *p_map;
    Record                     *record;
    RecordsMap::iterator        it;
    UT_Matrix4D                 xform_matrix;
    UT_WorkBuffer               name_buffer;
    int                         samples;

    myCurrentParent = myOriginalParent;
    myMatrix.identity();
    storageHelper(node);

    // Create a new OObject if necessary
    if (create_nodes)
    {
        // Start by creating any ancestors that don't yet exist
        if (!createAncestors(parent_node, ctx))
        {
            return false;
        }

        // Create a new transform, create its visibility, geometry, and user
        // properties. Add everything to a new Record
        obj = new OXform(*myCurrentParent,
                node.getName(),
                ctx.timeSampling());
        OVisibilityProperty vis = Alembic::AbcGeom::CreateVisibilityProperty(
                *obj,
                ctx.timeSampling());
        arb_map = new PropertyMap();
        p_map = new PropertyMap();

        record = new Record(obj, vis, arb_map, p_map);
        myRecordsMap->insert(RecordsMapInsert(node.objectPath(), record));
        myTypeMap->insert(TypeMapInsert(obj->getPtr(), XFORM));

        o_vis = record->getVisibility();
    }
    else
    {
        it = myRecordsMap->find(node.objectPath());
        if (it == myRecordsMap->end())
        {
            return false;
        }

        record = it->second;
        obj = UTverify_cast<OXform *>(record->getObject());
        o_vis = record->getVisibility();
        arb_map = record->getArbGProperties();
        p_map = record->getUserProperties();
    }

    // NOTE:    You may find it helpful to read the ROP_AbcShapeOutputWalker
    //          documentation and its version of the process() method.

    // Read in packed Alembic transform.
    if (transform) {
        transform->getMatrix(xform_matrix);
    }
    else
    {
        xform_matrix.identity();
    }

    samples = obj->getSchema().getNumSamples();
    // Only update if we're at or below the sample limit for this frame
    if ((unsigned int)samples <= (unsigned int)sample_limit)
    {
        if (!processAncestors(parent_node, ctx, sample_limit, visible_nodes))
        {
            return false;
        }

        myMatrix.invert();
        myMatrix = xform_matrix * myMatrix;
        // This transforms matrix has been modified. Use this matrix from now
        // on, instead of the one from the original Alembic archive
        myMatrixCache.insert(MatrixMapInsert(node.objectPath(), myMatrix));

        writeXformSample(node, obj, arb_map, p_map, ctx, iss, myMatrix, true);

        if (visible_nodes) {
            o_vis->set(Alembic::AbcGeom::kVisibilityDeferred);
        }
        else
        {
            // Animated visibility is fine, as long as it's on only a single
            // transform in a branch. All sorts of problems develop if a node
            // and one of its ancestores/descendants both have animated
            // visibility.
            o_vis->set(Alembic::AbcGeom::kVisibilityHidden);
            myErrorHandler.warning(
                "Animated visibility detected on transform %s. "
                "Hidden parent overrides visibility of children. "
                "Be careful when animating visibility of both child "
                "and parent.",
                node.objectPath().c_str());
        }
    }

    return true;
}

bool
ROP_AbcXformOutputWalker::processAncestors(const GABC_IObject &node,
        const ROP_AbcContext &ctx,
        int sample_limit,
        bool visible) const
{
    // Return if we have moved up to the root
    if (!node.valid() || !node.objectPath().compare("/"))
    {
        return true;
    }

    MatrixMap::iterator         m_it = myMatrixCache.find(node.objectPath());

    RecordsMap::iterator        r_it;
    OVisibilityProperty        *o_vis;
    OXform                     *obj;
    PropertyMap                *arb_map;
    PropertyMap                *p_map;
    Record                     *record;
    ISampleSelector             iss(ctx.cookTime() + myAdditionalSampleTime);
    UT_Matrix4D                 matrix;
    int                         samples;
    bool                        inherit;

    if (m_it != myMatrixCache.end() && visible)
    {
        myMatrix = m_it->second * myMatrix;
        return true;
    }

    // If we cannot find an OObject for the node, something has gone wrong
    r_it = myRecordsMap->find(node.objectPath());
    if (r_it == myRecordsMap->end())
    {
        return false;
    }
    // Process our ancestors first (filial piety!)
    if (!processAncestors(node.getParent(), ctx, sample_limit, visible))
    {
        return false;
    }

    record = r_it->second;
    obj = UTverify_cast<OXform *>(record->getObject());
    samples = obj->getSchema().getNumSamples();
    readXformSample(node, obj, iss, matrix, inherit);

    // Sanity check: we should never have less samples than the limit
    // or we may end up double-writing the next sample
    UT_ASSERT((unsigned int)samples >= (unsigned int)sample_limit);
    // Only update if we're at or below the sample limit
    if ((unsigned int)samples == (unsigned int)sample_limit)
    {
        o_vis = record->getVisibility();
        arb_map = record->getArbGProperties();
        p_map = record->getUserProperties();

        o_vis->set(Alembic::AbcGeom::kVisibilityDeferred);

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
ROP_AbcXformOutputWalker::writeChildren(const ROP_AbcContext &ctx) const
{
    const OObject  *parent;

    for (auto it = myNodeStorage.begin(); it != myNodeStorage.end(); ++it)
    {
        // Find parent of child node that should be added to the new archive
        const std::string  &path = *it;
        auto it2 = myRecordsMap->find(path.substr(0, path.find_last_of("/")));

        // If I can't find the parent, how did I get here?
        if (it2 == myRecordsMap->end())
        {
            return false;
        }
        parent = it2->second->getObject();

        // Sample the child node and descendants
        if (!writeChildrenHelper(GABC_Util::findObject(myArchiveFile, *it),
                parent,
                ctx))
        {
            return false;
        }
    }

    return true;
}

bool
ROP_AbcXformOutputWalker::writeChildrenHelper(const GABC_IObject &node,
        const OObject *parent,
        const ROP_AbcContext &ctx) const
{
    if ((node.nodeType() != GABC_XFORM)
            && (myKeepChildren != ROP_AbcContext::KEEP_ALL))
    {
        return true;
    }

    OObject                    *obj;
    OObjectType                 obj_type;
    OVisibilityProperty        *vis;
    Record                     *record;
    PropertyMap                *arb_map;
    PropertyMap                *p_map;
    RecordsMap::iterator        it;
    ICompoundProperty           up_in;
    OCompoundProperty           up_out;
    const ISampleSelector       iss(ctx.cookTime() + myAdditionalSampleTime);
    UT_Matrix4D                 matrix;
    std::string                 path = node.objectPath();
    exint	                nkids = node.getNumChildren();
    bool                        inherit;

    // Create a new OObject if one doesn't exist yet.
    it = myRecordsMap->find(path);
    if (it == myRecordsMap->end())
    {
        switch (node.nodeType())
        {
            case GABC_POLYMESH :
                obj = new OPolyMesh(*parent,
                        node.getName(),
                        ctx.timeSampling());
                obj_type = POLYMESH;
                createFaceSetsPolymesh(node,
                        UTverify_cast<OPolyMesh *>(obj),
                        ctx);
                break;

            case GABC_SUBD :
                obj = new OSubD(*parent,
                        node.getName(),
                        ctx.timeSampling());
                obj_type = SUBD;
                createFaceSetsSubd(node,
                        UTverify_cast<OSubD *>(obj),
                        ctx);
                break;

            case GABC_POINTS :
                obj = new OPoints(*parent,
                        node.getName(),
                        ctx.timeSampling());
                obj_type = POINTS;
                break;

            case GABC_CURVES :
                obj = new OCurves(*parent,
                        node.getName(),
                        ctx.timeSampling());
                obj_type = CURVES;
                break;

            case GABC_NUPATCH :
                obj = new ONuPatch(*parent,
                        node.getName(),
                        ctx.timeSampling());
                obj_type = NUPATCH;
                break;

            case GABC_CAMERA :
                obj = new OCamera(*parent,
                        node.getName(),
                        ctx.timeSampling());
                obj_type = CAMERA;
                break;

            case GABC_XFORM :
                obj = new OXform(*parent,
                        node.getName(),
                        ctx.timeSampling());
                obj_type = XFORM;
                break;

            default :
                return true;
        }

        OVisibilityProperty o_vis = Alembic::AbcGeom::CreateVisibilityProperty(
                                    *obj,
                                    ctx.timeSampling());
        arb_map = new PropertyMap();
        p_map = new PropertyMap();

        record = new Record(obj, o_vis, arb_map, p_map);
        myRecordsMap->insert(RecordsMapInsert(path, record));
        myTypeMap->insert(TypeMapInsert(obj->getPtr(), obj_type));

        vis = record->getVisibility();
    }
    else
    {
        record = it->second;
        obj = record->getObject();
        vis = record->getVisibility();
        arb_map = record->getArbGProperties();
        p_map = record->getUserProperties();
    }

    vis->set(Alembic::AbcGeom::kVisibilityDeferred);

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

        case GABC_CAMERA :
            sampleCamera(node,
                    UTverify_cast<OCamera *>(obj),
                    arb_map,
                    p_map,
                    ctx,
                    iss);
            break;

        case GABC_XFORM :
            readXformSample(node,
                    UTverify_cast<OXform *>(obj),
                    iss,
                    matrix,
                    inherit);
            writeXformSample(node,
                    UTverify_cast<OXform *>(obj),
                    arb_map,
                    p_map,
                    ctx,
                    iss,
                    matrix,
                    inherit);
            break;

        default :
            break;
    }

    for (exint i = 0; i < nkids; ++i)
    {
        if (!writeChildrenHelper(node.getChild(i), obj, ctx))
        {
            return false;
        }
    }

    return true;
}

void
ROP_AbcXformOutputWalker::storageHelper(const GABC_IObject &node) const
{
    if ((myKeepChildren != ROP_AbcContext::KEEP_NONE) && myStoreChildren)
    {
        // Remove the node if it's stored, and instead add its children
        auto    it = myNodeStorage.find(node.objectPath());
        exint	nkids = node.getNumChildren();

        if (it != myNodeStorage.end())
        {
            myNodeStorage.erase(it);
        }

        for (exint i = 0; i < nkids; ++i)
        {
            myNodeStorage.insert(node.getChild(i).objectPath());
        }
    }
}
