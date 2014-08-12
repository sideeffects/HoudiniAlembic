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
#include "ROP_AbcXform.h"
#include <GABC/GABC_IObject.h>
#include <GABC/GABC_OError.h>
#include <GABC/GABC_PackedImpl.h>
#include <GABC/GABC_Util.h>

// ROP_AbcPackedAbc class handles packed Alembic archives for
// ROP_AbcGTCompoundShape.
class ROP_AbcPackedAbc
{
public:
    class AbcInfo;
    class ArchiveInfo;

    typedef Alembic::Abc::Box3d                     Box3d;
    typedef Alembic::Abc::ObjectReaderPtr	    ObjectReaderPtr;
    typedef Alembic::Abc::OObject	            OObject;
    typedef ROP_AbcXform                            OXform;

    typedef GABC_NAMESPACE::GABC_IObject            GABC_IObject;
    typedef GABC_NAMESPACE::GABC_OError	            GABC_OError;
    typedef GABC_NAMESPACE::GABC_PackedImpl         GABC_PackedImpl;
    typedef GABC_NAMESPACE::GABC_Util               GABC_Util;

    typedef ROP_AbcOutputWalker::Bounds             Bounds;
    typedef ROP_AbcOutputWalker::BoundsMap          BoundsMap;
    typedef ROP_AbcOutputWalker::OObjectType        OObjectType;
    typedef ROP_AbcOutputWalker::PropertyMap        PropertyMap;
    typedef ROP_AbcOutputWalker::RecordsMap         RecordsMap;
    typedef ROP_AbcOutputWalker::TypeMap            TypeMap;
    typedef ROP_AbcOutputWalker::OReaderCollection  OReaderCollection;

    typedef UT_Array<AbcInfo>                       AbcInfoList;
    typedef UT_Map<std::string, ArchiveInfo *>      ArchiveInfoMap;
    typedef std::pair<std::string, ArchiveInfo *>   ArchiveInfoMapInsert;
    typedef UT_Array<const GABC_IObject *>          GABCIObjectList;
    typedef UT_Array<ObjectReaderPtr>               ObjectReaderList;
    typedef UT_Array<std::string>                   StringList;
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
        ArchiveInfo(RecordsMap *records,
                OReaderCollection *obj_paths)
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
        OReaderCollection * getObjectPaths() { return myObjectPaths; }

    private:
        RecordsMap         *myRecordsMap;
        OReaderCollection  *myObjectPaths;
    };

                    ROP_AbcPackedAbc(GABC_OError &err);
    virtual         ~ROP_AbcPackedAbc();

    // Clear all stored information on packed Alembics
    void            clear();

    // Write out the first frame to the Alembic archive
    virtual bool    startPacked(AbcInfoList &list,
                            const ROP_AbcContext &ctx) = 0;
    // Write out subsequent frames to the Alembic archive
    virtual bool    updatePacked(AbcInfoList &list,
                            const ROP_AbcContext &ctx,
                            exint elapsed_frames) = 0;
    // Write out subsequent frames to the Alembic archive when no packed
    // Alembics are visible
    virtual bool    updateAllHidden(const ROP_AbcContext &ctx,
                            exint elapsed_frames) = 0;
    // Store a time at which a frame was written out
    void            addTime(fpreal time) { myTimes.append(time); }
    // Set the parent object of the packed Alembic root
    void            setParent(const OObject *parent)
                    {
                        if (!myShapeParent)
                        {
                            myShapeParent = parent;
                        }
                    }

protected:
    static bool packedAlembicBounds(BoundsMap *bounds,
                        TypeMap *types,
                        OObject *obj,
                        UT_BoundingBox &box);
    static bool packedAlembicBoundsHelper(BoundsMap *bounds,
                        TypeMap *types,
                        const OObject *obj);
    static bool process(const GABCIObjectList &objects,
                        const ROP_AbcOutputWalker &walker,
                        const ROP_AbcContext &ctx,
                        const TransformList &transforms,
                        bool create_node,
                        bool visible_node,
                        int samples_limit = 0);
    static bool process(const ObjectReaderList &paths,
                        const std::string &file,
                        const ROP_AbcOutputWalker &walker,
                        const ROP_AbcContext &ctx,
                        bool create_node,
                        bool visible_node,
                        int samples_limit = 0);

    // Map to read/write ArchiveInfo objects to/from. Key is filename
    ArchiveInfoMap      myArchiveInfoMap;
    //
    GABC_OError        &myErrorHandler;
    // Parent OObject of packed Alembic hierarchies
    const OObject      *myShapeParent;
    // Map storing the type of all OObjects created while writing
    // the current Alembic archive.
    TypeMap             myTypeMap;
    // List of times at which frames were written out. The ith entry
    // corresponds to frame i
    UT_Array<fpreal>    myTimes;
};

#endif