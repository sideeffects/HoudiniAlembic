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

#if 0

#include "ROP_AbcPackedAbc.h"

ROP_AbcPackedAbc::ROP_AbcPackedAbc(GABC_OError &err)
    : myArchiveInfoMap()
    , myErrorHandler(err)
    , myShapeParent(NULL)
    , myTypeMap()
    , myTimes()
{}

ROP_AbcPackedAbc::~ROP_AbcPackedAbc()
{
    clear();
}

void
ROP_AbcPackedAbc::clear()
{
    for (auto it_1 = myArchiveInfoMap.begin();
            it_1 != myArchiveInfoMap.end();
            ++it_1)
    {
        ArchiveInfo        *archive = it_1->second;
        RecordsMap         *records = archive->getRecordsMap();

        // Free records
        for (auto it_2 = records->begin(); it_2 != records->end(); ++it_2)
        {
            delete it_2->second;
        }

        // Free archive
        delete archive;
    }

    myArchiveInfoMap.clear();
    myTypeMap.clear();
    myTimes.clear();
}

// Calculates child bounds for an OObject and all of it's descendants
bool
ROP_AbcPackedAbc::packedAlembicBounds(BoundsMap *bounds_map,
        TypeMap *types,
        OObject *obj,
        UT_BoundingBox &box)
{
    OObjectType         obj_type;
    TypeMap::iterator   it = types->find(obj->getPtr());
    BoundsMap::iterator it2 = bounds_map->find(obj->getPtr());

    // If we cannot detect the type of an OObject, we cannot properly
    // cast it to compute the bounds. In addition, all OObjects created
    // through ROP_AbcPackedAbc should have had their type recorded.
    if (it == types->end())
    {
        return false;
    }

    obj_type = it->second;
    // Skip cameras.
    if (obj_type == ROP_AbcOutputWalker::CAMERA)
    {
        return true;
    }
    // Same as above: something is wrong if we cannot find an entry
    // for the current object.
    else if (it2 == bounds_map->end())
    {
        return false;
    }

    // For transforms:
    if (obj_type == ROP_AbcOutputWalker::XFORM)
    {
        Bounds          bounds = it2->second;
        Box3d           b3d;
        OObject         kid;
        OXform         *xform = bounds.xform;
        UT_BoundingBox  kid_box;
        exint           num_kids = xform->getNumChildren();

        // Compute the bounds of their children
        for (exint i = 0; i < num_kids; ++i)
        {
            kid_box = UT_BoundingBox();
            kid = xform->getChild(i);

            if (!packedAlembicBounds(bounds_map, types, &kid, kid_box))
            {
                return false;
            }

            box.enlargeBounds(kid_box);
        }

        // Store the computed bounds
        b3d = GABC_Util::getBox(box);
        xform->getSchema().getChildBoundsProperty().set(b3d);

        // Modify the bounds by the transform. This is the Xforms bounds.
        box.transform(GABC_Util::getM(xform->getSchema().getRecentXform()));
    }
    // For geometry:
    else
    {
        // Just pass through the recorded bounds.
        Bounds  bounds = it2->second;

        box = GABC_Util::getBox(*(bounds.box));
        delete bounds.box;
    }

    return true;
}

// Helper to the above function. We want to calculate the child bounds for
// all children of myShapeParent but not for myShapeParent itself
// (myShapeParent may or may not be calculating its own child bounds, and
// if it isn't then it's just an identity transform whose parent IS
// calculating its own child bounds).
bool
ROP_AbcPackedAbc::packedAlembicBoundsHelper(BoundsMap *bounds,
        TypeMap *types,
        const OObject *obj)
{
    OObject    *myObj = const_cast<OObject *>(obj);
    exint       num_kids = myObj->getNumChildren();
    for (exint i = 0; i < num_kids; ++i)
    {
        UT_BoundingBox  kid_box;
        OObject         kid = myObj->getChild(i);

        if (!packedAlembicBounds(bounds, types, &kid, kid_box))
        {
            return false;
        }
    }

    return true;
}

// Convenience function to handle repeated node lookups and calls
// to walker.process() with varying arguments.
bool
ROP_AbcPackedAbc::process(const GABCIObjectList &objects,
        const ROP_AbcOutputWalker &walker,
        const ROP_AbcContext &ctx,
        const TransformList &transforms,
        bool create_node,
        bool visible_node,
        int samples_limit)
{
    for (exint i = 0; i < objects.entries(); ++i)
    {
        // Process object
        if (!walker.process(*(objects(i)),
                ctx,
                transforms(i),
                create_node,
                visible_node,
                samples_limit))
        {
            return false;
        }
    }

    return true;
}

// Convenience function to handle repeated node lookups and calls
// to walker.process() with varying arguments.
bool
ROP_AbcPackedAbc::process(const ObjectReaderList &objects,
        const std::string &file,
        const ROP_AbcOutputWalker &walker,
        const ROP_AbcContext &ctx,
        bool create_node,
        bool visible_node,
        int samples_limit)
{
    for (exint i = 0; i < objects.entries(); ++i)
    {
        GABC_IObject    obj = GABC_Util::findObject(file, objects(i));
        // Process object
        if (!walker.process(obj,
                ctx,
                GT_TransformHandle(),
                create_node,
                visible_node,
                samples_limit))
        {
            return false;
        }
    }

    return true;
}

#endif
