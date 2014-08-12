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

#include "ROP_AbcPackedShape.h"

namespace {
    typedef Alembic::Abc::ObjectReaderPtr           ObjectReaderPtr;

    typedef GABC_NAMESPACE::GABC_IObject            GABC_IObject;
    typedef GABC_NAMESPACE::GABC_Util               GABC_Util;

    typedef ROP_AbcPackedShape::AbcInfo             AbcInfo;
    typedef ROP_AbcPackedShape::AbcInfoList         AbcInfoList;
    typedef ROP_AbcPackedShape::GABCIObjectList     GABCIObjectList;
    typedef ROP_AbcPackedShape::ObjectReaderList    ObjectReaderList;
    typedef ROP_AbcPackedShape::OReaderCollection   OReaderCollection;
    typedef ROP_AbcPackedShape::StringList          StringList;
    typedef ROP_AbcPackedShape::TransformList       TransformList;

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
        exint                   entries = observed_objs.entries();
        exint                   i = 0;
        int                     stored_count, observed_count;
        int                     min, diff;

        // If the count of any previously encountered Alembic is less
        // than the current count, it is not visible this frame.
        for (auto it = stored_counts.begin(); it != stored_counts.end(); ++it)
        {
            stored_count = it->second;
            observed_count = observed_counts.count(it->first);

            for (int j = 0; j < (stored_count - observed_count); ++j)
            {
                hidden_paths.append(it->first);
            }
        }

        while (i < entries)
        {
            obj = observed_objs(i);
            reader = obj->object().getPtr();
            stored_count = stored_counts.count(reader);
            observed_count = observed_counts.count(reader);
            // This is the number of previously encountered Alembics
            // visible this frame
            min = std::min(stored_count, observed_count);
            // This is the number of new copies of the Alembic
            diff = observed_count - stored_count;

            for (int j = 0; j < min; ++j)
            {
                visible_objs.append(obj);
                visible_xforms.append(observed_xforms(i + j));
            }

            for (int j = 0; j < diff; ++j)
            {
                stored_counts.insert(reader);
                new_objs.append(obj);
                new_xforms.append(observed_xforms(i + min + j));

            }

            i += observed_count;
        }
    }
}

bool
ROP_AbcPackedShape::startPacked(AbcInfoList &list,
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
    // in sorted order: first by filename then by object path.
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
            ROP_AbcShapeOutputWalker    walker(myShapeParent,
                                                &my_bounds,
                                                my_records,
                                                &myTypeMap,
                                                my_readers,
                                                myErrorHandler,
                                                additional_time);

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
            // Ignore packed transforms
            const GABC_IObject &obj = impl->object();
            if (obj.nodeType() == GABC_NAMESPACE::GABC_XFORM)
            {
                continue;
            }

            objects.append(&obj);
            object_counts.insert(obj.object().getPtr());
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
ROP_AbcPackedShape::updatePacked(AbcInfoList &list,
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

                ROP_AbcShapeOutputWalker    walker(myShapeParent,
                                                    &my_bounds,
                                                    my_records,
                                                    &myTypeMap,
                                                    my_readers,
                                                    myErrorHandler,
                                                    additional_time);

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

                // Update the frame samples for the Alembics up to the
                // current frame. These samples are still not visible
                for (exint i = 1; i < myTimes.entries(); ++i)
                {
                    ctx_copy.cookContext().setTime(myTimes(i));
                    walker.resetCounts();
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
                }

                // Write the sample for the current frame. Now the samples
                // are visible
                walker.resetCounts();
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

                ROP_AbcShapeOutputWalker    walker(myShapeParent,
                                                    &my_bounds,
                                                    my_records,
                                                    &myTypeMap,
                                                    my_readers,
                                                    myErrorHandler,
                                                    additional_time);

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

                    walker.setCountsFreeze(true);

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
            // Ignore packed transforms
            const GABC_IObject &obj = impl->object();
            if (obj.nodeType() == GABC_NAMESPACE::GABC_XFORM)
            {
                continue;
            }

            objects.append(&obj);
            object_counts.insert(obj.object().getPtr());
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
ROP_AbcPackedShape::updateAllHidden(const ROP_AbcContext &ctx,
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
        ROP_AbcShapeOutputWalker    walker(myShapeParent,
                                            &my_bounds,
                                            my_records,
                                            &myTypeMap,
                                            my_readers,
                                            myErrorHandler,
                                            additional_time);

        for (auto it2 = my_readers->begin(); it2 != my_readers->end(); ++it2)
        {
            GABC_IObject    obj = GABC_Util::findObject(it->first,
                                    it2->first);

            for (int i = it2->second; i > 0; --i)
            {
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
