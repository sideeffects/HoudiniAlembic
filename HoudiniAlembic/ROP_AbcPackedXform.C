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

#include "ROP_AbcPackedXform.h"
#include <GABC/GABC_IObject.h>
#include <GABC/GABC_Util.h>

namespace {
    typedef Alembic::Abc::ObjectReaderPtr           ObjectReaderPtr;

    typedef GABC_NAMESPACE::GABC_IObject            GABC_IObject;
    typedef GABC_NAMESPACE::GABC_Util               GABC_Util;

    typedef ROP_AbcPackedXform::AbcInfo             AbcInfo;
    typedef ROP_AbcPackedXform::AbcInfoList         AbcInfoList;
    typedef ROP_AbcPackedXform::GABCIObjectList     GABCIObjectList;
    typedef ROP_AbcPackedXform::ObjectReaderList    ObjectReaderList;
    typedef ROP_AbcPackedXform::OReaderCollection   OReaderCollection;
    typedef ROP_AbcPackedXform::StringList          StringList;
    typedef ROP_AbcPackedXform::TransformList       TransformList;

    // Uses the collection of previously encountered packed Alembics
    // and list of currently encountered Alembics to determine which
    // are existing, which are new, and which existing Alembics are
    // not visible this frame.
    static void
    splitObjects(OReaderCollection &stored_counts,
            GABCIObjectList &observed_objs,
            OReaderCollection &observed_counts,
            TransformList &observed_xforms,
            GABCIObjectList &visible_objs,
            TransformList &visible_xforms,
            GABCIObjectList &new_objs,
            TransformList &new_xforms,
            ObjectReaderList &hidden_paths)
    {
        const GABC_IObject     *obj;
        ObjectReaderPtr         reader;

        // If the count of any previously encountered Alembic is less
        // than the current count, it is not visible this frame.
        for (auto it = stored_counts.begin(); it != stored_counts.end(); ++it)
        {
            if (!observed_counts.count(it->first))
            {
                hidden_paths.append(it->first);
            }
        }

        for (exint i = 0; i < observed_objs.entries(); ++i)
        {
            obj = observed_objs(i);
            reader = obj->object().getPtr();

            if (!stored_counts.count(reader))
            {
                stored_counts.insert(reader);
                new_objs.append(obj);
                new_xforms.append(observed_xforms(i));
            } else {
                visible_objs.append(obj);
                visible_xforms.append(observed_xforms(i));
            }
        }
    }
}

bool
ROP_AbcPackedXform::startPacked(AbcInfoList &list,
        const ROP_AbcContext &ctx)
{
    GABCIObjectList     objects;
    OReaderCollection   object_counts;
    TransformList       object_xforms;
    BoundsMap           my_bounds;
    std::string         file = list(0).getPackedImpl()->filename();
    fpreal              additional_time = ctx.timeSampling()->
                                getTimeSamplingType().getTimePerCycle();
    exint               entries = list.entries();
    exint               pos = 0;

    // Loop through all packed Alembics. The provided list is expected
    // in sorted order, first by filename then by object path.
    while(true)
    {
        // Get the filename of the current packed Alembic, or an
        // empty string if we've reached the end of the list
        const GABC_PackedImpl  *impl = (pos != entries)
                                    ? list(pos).getPackedImpl()
                                    : NULL;
        std::string             curr_file = impl ? impl->filename() : "";

        // If the filename does not match the previous filename,
        // process the collected packed Alembics (all from same file).
        if (file.compare(curr_file))
        {
            ArchiveInfo                *archive;
            RecordsMap                 *my_records = new RecordsMap();
            OReaderCollection          *my_readers = new OReaderCollection(
                                                object_counts);
            ROP_AbcXformOutputWalker    walker(myShapeParent,
                                                &my_bounds,
                                                my_records,
                                                &myTypeMap,
                                                file,
                                                myErrorHandler,
                                                additional_time,
                                                ctx.keepChildren());

            if (!process(objects,
                    walker,
                    ctx,
                    object_xforms,
                    true,
                    true))
            {
                delete my_records;
                delete my_readers;
                return false;
            }
            if (!walker.writeChildren(ctx))
            {
                return false;
            }

            archive = new ArchiveInfo(my_records, my_readers);
            myArchiveInfoMap.insert(ArchiveInfoMapInsert(file, archive));

            // Reset if we haven't reached the end of list.
            if (impl)
            {
                file = curr_file;
                objects.clear();
                object_counts.clear();
                object_xforms.clear();
            }
            else
            {
                break;
            }
        }
        // If the filenames match, add the object to the to-process
        // list, update the copy count, and store the object transform.
        else
        {
            const GABC_IObject &obj = impl->object();
            ObjectReaderPtr     reader = obj.object().getPtr();

            // Ignore packed geometry
            if (obj.nodeType() != GABC_NAMESPACE::GABC_XFORM)
            {
                continue;
            }

            // Ignore multiple copies. Very unclear what is the correct way
            // to handle multiple copies of the same transform.
            if (object_counts.count(reader))
            {
                myErrorHandler.warning(
                        "Multiple copies of transform %s detected."
                        "Only exporting first copy. Multiple copies "
                        "with variable visibility may result in "
                        "unexpected behaviour!",
                        obj.objectPath().c_str());
                ++pos;
                continue;
            }

            objects.append(&obj);
            object_counts.insert(reader);
            object_xforms.append(list(pos).getTransformHandle());

            ++pos;
        }
    }

    if (ctx.fullBounds())
    {
        // Update child bounds
        if (!packedAlembicBoundsHelper(&my_bounds, &myTypeMap, myShapeParent))
        {
            return false;
        }
    }

    return true;
}

bool
ROP_AbcPackedXform::updatePacked(AbcInfoList &list,
        const ROP_AbcContext &ctx,
        exint elapsed_frames)
{
    GABCIObjectList     objects;
    OReaderCollection   object_counts;
    TransformList       object_xforms;
    BoundsMap           my_bounds;
    std::string         file = list(0).getPackedImpl()->filename();
    fpreal              additional_time = ctx.timeSampling()->
                                getTimeSamplingType().getTimePerCycle();
    exint               entries = list.entries();
    exint               pos = 0;

    // Loop through all packed Alembics. The provided list is expected
    // in sorted order, first by filename then by object path.
    while(true)
    {
        // List processing is similar to startPackedAbc().
        const GABC_PackedImpl  *impl = (pos != entries)
                                    ? list(pos).getPackedImpl()
                                    : NULL;
        std::string             curr_file = impl ? impl->filename() : "";

        if (file.compare(curr_file))
        {
            ArchiveInfo                *archive;
            ArchiveInfoMap::iterator    it = myArchiveInfoMap.find(file);
            RecordsMap                 *my_records;
            OReaderCollection          *my_readers;

            // If no ArchiveInfo object can be found for this filename,
            // these objects are from a new Alembic archive.
            if (it == myArchiveInfoMap.end())
            {
                ROP_AbcContext          ctx_copy(ctx);
                my_records = new RecordsMap();
                my_readers = new OReaderCollection(object_counts);

                ROP_AbcXformOutputWalker    walker(myShapeParent,
                                                    &my_bounds,
                                                    my_records,
                                                    &myTypeMap,
                                                    file,
                                                    myErrorHandler,
                                                    additional_time,
                                                    ctx.keepChildren());

                // The first frame is processed as in startPackedAbc(),
                // except the Alembics are not visible yet
                ctx_copy.cookContext().setTime(myTimes(0));
                if (!process(objects,
                        walker,
                        ctx_copy,
                        object_xforms,
                        true,
                        false))
                {
                    delete my_records;
                    delete my_readers;
                    return false;
                }
                archive = new ArchiveInfo(my_records, my_readers);
                myArchiveInfoMap.insert(ArchiveInfoMapInsert(file, archive));

                if (!walker.writeChildren(ctx))
                {
                    return false;
                }
                walker.setStoreChildren(false);

                // Update the frame samples for the Alembics up to the
                // current frame. These samples are still not visible
                for (exint i = 1; i < myTimes.entries(); ++i)
                {
                    ctx_copy.cookContext().setTime(myTimes(i));
                    if (!process(objects,
                            walker,
                            ctx_copy,
                            object_xforms,
                            false,
                            false,
                            (int)i))
                    {
                        return false;
                    }
                    if (!walker.writeChildren(ctx))
                    {
                        return false;
                    }
                }

                // Write the sample for the current frame. Now the samples
                // are visible
                if (!process(objects,
                        walker,
                        ctx,
                        object_xforms,
                        false,
                        true,
                        elapsed_frames))
                {
                    return false;
                }
                if (!walker.writeChildren(ctx))
                {
                    return false;
                }
            }
            else
            {
                GABCIObjectList     visible_objects, new_objects;
                TransformList       visible_xforms, new_xforms;
                ObjectReaderList    hidden_paths;
                GT_TransformHandle  empty_xform;

                archive = it->second;
                my_records = archive->getRecordsMap();
                my_readers = archive->getObjectPaths();

                // Determine the status of each previously and currently
                // encountered Alembic
                splitObjects(*my_readers,
                        objects,
                        object_counts,
                        object_xforms,
                        visible_objects,
                        visible_xforms,
                        new_objects,
                        new_xforms,
                        hidden_paths);

                ROP_AbcXformOutputWalker    walker(myShapeParent,
                                                    &my_bounds,
                                                    my_records,
                                                    &myTypeMap,
                                                    file,
                                                    myErrorHandler,
                                                    additional_time,
                                                    ctx.keepChildren());

                // Update previously encountered objects that are currently
                // visible
                if (visible_objects.entries())
                {
                    if (!process(visible_objects,
                            walker,
                            ctx,
                            visible_xforms,
                            false,
                            true,
                            elapsed_frames))
                    {
                        return false;
                    }
                }

                // Update previously encountered objects that are currently
                // not visible
                if (hidden_paths.entries())
                {
                    if (!process(hidden_paths,
                            file,
                            walker,
                            ctx,
                            false,
                            false,
                            elapsed_frames))
                    {
                        return false;
                    }
                }

                // New Alembics are added similar to those from a new archive
                if (new_objects.entries())
                {
                    ROP_AbcContext      ctx_copy(ctx);

                    if (!walker.writeChildren(ctx))
                    {
                        return false;
                    }
                    walker.clearStorage();

                    ctx_copy.cookContext().setTime(myTimes(0));
                    if (!process(new_objects,
                            walker,
                            ctx_copy,
                            new_xforms,
                            true,
                            false))
                    {
                        return false;
                    }

                    walker.setStoreChildren(false);

                    for (exint i = 1; i < myTimes.entries(); ++i)
                    {
                        ctx_copy.cookContext().setTime(myTimes(i));
                        if (!process(new_objects,
                                walker,
                                ctx_copy,
                                new_xforms,
                                false,
                                false,
                                (int)i))
                        {
                            return false;
                        }
                        if (!walker.writeChildren(ctx))
                        {
                            return false;
                        }
                    }

                    if (!process(new_objects,
                            walker,
                            ctx,
                            new_xforms,
                            false,
                            true,
                            elapsed_frames))
                    {
                        return false;
                    }
                }

                if (!walker.writeChildren(ctx))
                {
                    return false;
                }
            }

            // Reset if we haven't reached the end of list.
            if (impl)
            {
                file = curr_file;
                objects.clear();
                object_counts.clear();
                object_xforms.clear();
            }
            else
            {
                break;
            }
        }
        // If the filenames match, add the object to the to-process
        // list, update the copy count, and store the object transform.
        else
        {
            const GABC_IObject &obj = impl->object();
            ObjectReaderPtr     reader = obj.object().getPtr();

            // Ignore packed geometry
            if (obj.nodeType() != GABC_NAMESPACE::GABC_XFORM)
            {
                continue;
            }

            // Ignore multiple copies. Very unclear what is the correct way
            // to handle multiple copies of the same transform.
            if (object_counts.count(reader))
            {
                myErrorHandler.warning(
                        "Multiple copies of transform %s detected."
                        "Only exporting first copy. Multiple copies "
                        "with variable visibility may result in "
                        "unexpected behaviour!",
                        obj.objectPath().c_str());
                ++pos;
                continue;
            }

            objects.append(&obj);
            object_counts.insert(reader);
            object_xforms.append(list(pos).getTransformHandle());

            ++pos;
        }
    }

    if (ctx.fullBounds())
    {
        // Update child bounds
        if (!packedAlembicBoundsHelper(&my_bounds, &myTypeMap, myShapeParent))
        {
            return false;
        }
    }

    return true;
}

bool
ROP_AbcPackedXform::updateAllHidden(const ROP_AbcContext &ctx,
        exint elapsed_frames)
{
    BoundsMap   my_bounds;
    fpreal      additional_time = ctx.timeSampling()->
                        getTimeSamplingType().getTimePerCycle();

    // For every previously encountered packed Alembic from any archive,
    // update it as hidden for this frame.
    for (auto it = myArchiveInfoMap.begin(); it != myArchiveInfoMap.end(); ++it)
    {
        ArchiveInfo                *archive = it->second;
        RecordsMap                 *my_records = archive->getRecordsMap();
        OReaderCollection          *my_readers = archive->getObjectPaths();
        ROP_AbcXformOutputWalker    walker(myShapeParent,
                                            &my_bounds,
                                            my_records,
                                            &myTypeMap,
                                            it->first,
                                            myErrorHandler,
                                            additional_time,
                                            ctx.keepChildren());

        for (auto it2 = my_readers->begin(); it2 != my_readers->end(); ++it2)
        {
            GABC_IObject    obj = GABC_Util::findObject(it->first,
                                    it2->first);

            if (!walker.process(obj,
                    ctx,
                    GT_TransformHandle(),
                    false,
                    false,
                    elapsed_frames))
            {
                return false;
            }
        }
        if (!walker.writeChildren(ctx))
        {
            return false;
        }
    }

    if (ctx.fullBounds())
    {
        // Update child bounds
        if (!packedAlembicBoundsHelper(&my_bounds, &myTypeMap, myShapeParent))
        {
            return false;
        }
    }

    return true;
}
