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

#ifndef __ROP_AbcPackedAbc__
#define __ROP_AbcPackedAbc__

#include "ROP_AbcOutputWalker.h"
#include <GABC/GABC_PackedImpl.h>
#include <GABC/GABC_OError.h>

// ROP_AbcPackedAbc class handles packed Alembic archives for
// ROP_AbcGTCompoundShape.
class ROP_AbcPackedAbc
{
public:
    class AbcInfo;
    class ArchiveInfo;

    typedef Alembic::Abc::OObject	            OObject;

    typedef GABC_NAMESPACE::GABC_OError	            GABC_OError;
    typedef GABC_NAMESPACE::GABC_PackedImpl         GABC_PackedImpl;

    typedef ROP_AbcOutputWalker::RecordsMap         RecordsMap;
    typedef ROP_AbcOutputWalker::RecordsMapInsert   RecordsMapInsert;
    typedef ROP_AbcOutputWalker::StringList         StringList;
    typedef ROP_AbcOutputWalker::StringCollection   StringCollection;

    typedef UT_Array<AbcInfo>                       AbcInfoList;
    typedef UT_Map<std::string, ArchiveInfo *>      ArchiveInfoMap;
    typedef std::pair<std::string, ArchiveInfo *>   ArchiveInfoMapInsert;
    typedef UT_Array<GT_TransformHandle>            TransformList;

    // AbcInfo class is used as a convenient container for packed
    // Alembic information extracted from a GT_Primitive of type
    // GT_GEO_PACKED.
    class AbcInfo
    {
    public:
        AbcInfo(const GABC_PackedImpl *impl,
                const GT_TransformHandle handle)
            : myPackedImpl(impl)
            , myTransformHandle(handle)
        {}
        // Assignment operator is meaningless for this class, but one
        // needs to exist for use with UT_Array.
        AbcInfo& operator=(const AbcInfo &src) { return *this; }
        virtual ~AbcInfo() {}

        const GABC_PackedImpl *     getPackedImpl() const
                                            { return myPackedImpl; }
        const GT_TransformHandle    getTransformHandle() const
                                            { return myTransformHandle; }

    private:
        const GABC_PackedImpl      *myPackedImpl;
        const GT_TransformHandle    myTransformHandle;
    };

    // ArchiveInfo class stores the RecordsMap and a collection of object
    // paths encountered for an associated Alembic archive.
    class ArchiveInfo
    {
    public:
        ArchiveInfo(RecordsMap *records, StringCollection *obj_paths)
            : myRecordsMap(records)
            , myObjectPaths(obj_paths)
        {}
        virtual ~ArchiveInfo()
        {
            if (myRecordsMap)
                delete myRecordsMap;
            if (myObjectPaths)
                delete myObjectPaths;
        }

        RecordsMap *        getRecordsMap() { return myRecordsMap; }
        StringCollection *  getObjectPaths() { return myObjectPaths; }

    private:
        RecordsMap         *myRecordsMap;
        StringCollection   *myObjectPaths;
    };

    ROP_AbcPackedAbc();
    virtual ~ROP_AbcPackedAbc();

    // Clear all stored information on packed Alembics
    void    clear();

    // Write out the first frame to the Alembic archive
    bool    startPackedAbc(AbcInfoList &list,
            const ROP_AbcContext &ctx);
    // Write out subsequent frames to the Alembic archive
    bool    updatePackedAbc(AbcInfoList &list,
            const ROP_AbcContext &ctx,
            exint elapsed_frames);
    // Store a time at which a frame was written out
    void    addTime(fpreal time) { myTimes.append(time); }
    // Set the parent object of the output packed Alembics
    void    setParent(const OObject *parent)
            {
                if (!myShapeParent)
                {
                    myShapeParent = parent;
                }
            }

private:
    // Map to read/write ArchiveInfo objects to/from. Key is filename
    ArchiveInfoMap      myArchiveInfoMap;
    // Parent OObject of packed Alembic hierarchies
    const OObject      *myShapeParent;
    // List of times at which frames were written out. The ith entry
    // corresponds to frame i
    UT_Array<fpreal>    myTimes;
};

#endif