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

#include "ROP_AbcPackedAbc.h"
#include <GABC/GABC_IObject.h>
#include <GABC/GABC_Util.h>

namespace {
    typedef GABC_NAMESPACE::GABC_IObject        GABC_IObject;
    typedef GABC_NAMESPACE::GABC_Util           GABC_Util;

    typedef ROP_AbcPackedAbc::AbcInfo           AbcInfo;
    typedef ROP_AbcPackedAbc::AbcInfoList       AbcInfoList;
    typedef ROP_AbcPackedAbc::StringList        StringList;
    typedef ROP_AbcPackedAbc::StringCollection  StringCollection;
    typedef ROP_AbcPackedAbc::TransformList     TransformList;

    // Convenience function to handle repeated node lookups and calls
    // to walker.process() with varying arguments.
    static bool
    process(const StringList &paths,
            const std::string &file,
            const ROP_AbcOutputWalker &walker,
            const ROP_AbcContext &ctx,
            const TransformList &transforms,
            bool create_node,
            bool visible_node,
            int samples_limit = 0)
    {
        GABC_IObject    obj;

        for (exint i = 0; i < paths.entries(); ++i)
        {
            // Get copy of object being processed
            obj = GABC_Util::findObject(file, paths(i));
            // Process object
            if (!walker.process(obj,
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

    // Uses the collection of previously encountered packed Alembics
    // and list of currently encountered Alembics to determine which
    // are existing, which are new, and which existing Alembics are
    // not visible this frame.
    static void
    splitObjects(StringCollection &stored_paths,
            StringList &observed_paths,
            StringCollection &observed_counts,
            TransformList &observed_xforms,
            StringList &visible_paths,
            TransformList &visible_xforms,
            StringList &new_paths,
            TransformList &new_xforms,
            StringList &hidden_paths)
    {
        std::string     path;
        exint           entries = observed_paths.entries();
        exint           i = 0;
        int             stored_count, observed_count;
        int             min, diff;

        // If the count of any previously encountered Alembic is less
        // than the current count, it is not visible this frame.
        for (auto it = stored_paths.begin(); it != stored_paths.end(); ++it)
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
            path = observed_paths(i);
            stored_count = stored_paths.count(path);
            observed_count = observed_counts.count(path);
            // This is the number of previously encountered Alembics
            // visible this frame
            min = std::min(stored_count, observed_count);
            // This is the number of new copies of the Alembic
            diff = observed_count - stored_count;

            for (int j = 0; j < min; ++j)
            {
                visible_paths.append(path);
                visible_xforms.append(observed_xforms(i + j));
            }

            for (int j = 0; j < diff; ++j)
            {
                stored_paths.insert(path);
                new_paths.append(path);
                new_xforms.append(observed_xforms(i + min + j));

            }

            i += observed_count;
        }
    }
}

ROP_AbcPackedAbc::ROP_AbcPackedAbc()
    : myArchiveInfoMap()
    , myShapeParent(NULL)
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
        ArchiveInfo    *archive = it_1->second;
        RecordsMap     *records = archive->getRecordsMap();

        // Free records
        for (auto it_2 = records->begin();
                it_2 != records->end();
                ++it_2)
        {
            delete it_2->second;
        }

        delete archive;
    }

    myArchiveInfoMap.clear();
    myTimes.clear();
}

bool
ROP_AbcPackedAbc::startPackedAbc(AbcInfoList &list,
        const ROP_AbcContext &ctx)
{
    StringList          object_paths;
    StringCollection    object_counts;
    TransformList       object_xforms;
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
            ArchiveInfo            *archive;
            RecordsMap             *my_records = new RecordsMap();
            StringCollection       *my_paths = new StringCollection(
                                            object_counts);
            ROP_AbcOutputWalker     walker(myShapeParent,
                                            my_records,
                                            my_paths,
                                            additional_time);

            if (!process(object_paths,
                    file,
                    walker,
                    ctx,
                    object_xforms,
                    true,
                    true))
            {
                delete my_records;
                delete my_paths;
                return false;
            }
            archive = new ArchiveInfo(my_records, my_paths);
            myArchiveInfoMap.insert(ArchiveInfoMapInsert(file, archive));

            // Reset if we haven't reached the end of list.
            if (impl)
            {
                file = curr_file;
                object_paths.clear();
                object_counts.clear();
                object_xforms.clear();
            }
            else
            {
                break;
            }
        }
        // If the filenames match, add the object path and
        // associated transform to the current collection.
        else
        {
            std::string     path = impl->objectPath();

            object_paths.append(path);
            object_counts.insert(path);
            object_xforms.append(list(pos).getTransformHandle());

            ++pos;
        }
    }

    return true;
}

bool
ROP_AbcPackedAbc::updatePackedAbc(AbcInfoList &list,
        const ROP_AbcContext &ctx,
        exint elapsed_frames)
{
    StringList          object_paths;
    StringCollection    object_counts;
    TransformList       object_xforms;
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
            StringCollection           *my_paths;

            // If no ArchiveInfo object can be found for this filename,
            // these objects are from a new Alembic archive.
            if (it == myArchiveInfoMap.end())
            {
                ROP_AbcContext          ctx_copy(ctx);
                my_records = new RecordsMap();
                my_paths = new StringCollection(object_counts);

                ROP_AbcOutputWalker     walker(myShapeParent,
                                                my_records,
                                                my_paths,
                                                additional_time);

                // The first frame is processed as in startPackedAbc(),
                // except the Alembics are not visible yet
                ctx_copy.cookContext().setTime(myTimes(0));
                if (!process(object_paths,
                        file,
                        walker,
                        ctx_copy,
                        object_xforms,
                        true,
                        false))
                {
                    delete my_records;
                    delete my_paths;
                    return false;
                }
                archive = new ArchiveInfo(my_records, my_paths);
                myArchiveInfoMap.insert(ArchiveInfoMapInsert(file, archive));

                // Update the frame samples for the Alembics up to the
                // current frame. These samples are still not visible
                for (exint i = 1; i < myTimes.entries(); ++i)
                {
                    ctx_copy.cookContext().setTime(myTimes(i));
                    walker.resetCounts();
                    if (!process(object_paths,
                            file,
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
                if (!process(object_paths,
                        file,
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
                StringList          visible_objects, new_objects, hidden_objects;
                TransformList       visible_xforms, new_xforms, hidden_xforms;
                GT_TransformHandle  empty_xform;

                archive = it->second;
                my_records = archive->getRecordsMap();
                my_paths = archive->getObjectPaths();

                // Determine the status of each previously and currently
                // encountered Alembic
                splitObjects(*my_paths,
                        object_paths,
                        object_counts,
                        object_xforms,
                        visible_objects,
                        visible_xforms,
                        new_objects,
                        new_xforms,
                        hidden_objects);

                for (exint i = 0; i < hidden_objects.entries(); ++i)
                {
                    hidden_xforms.append(empty_xform);
                }

                ROP_AbcOutputWalker walker = ROP_AbcOutputWalker(myShapeParent,
                                                            my_records,
                                                            my_paths,
                                                            additional_time);

                // Update previously encountered objects that are currently
                // visible
                if (visible_objects.entries())
                {
                    if (!process(visible_objects,
                            file,
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
                if (hidden_objects.entries())
                {
                    if (!process(hidden_objects,
                            file,
                            walker,
                            ctx,
                            hidden_xforms,
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
                            file,
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
                                file,
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
                            file,
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
                object_paths.clear();
                object_counts.clear();
                object_xforms.clear();
            }
            else
            {
                break;
            }
        }
        else
        {
            std::string     path = impl->objectPath();

            object_paths.append(path);
            object_counts.insert(path);
            object_xforms.append(list(pos).getTransformHandle());

            ++pos;
        }
    }

    return true;
}
