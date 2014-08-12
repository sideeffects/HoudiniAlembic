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

#ifndef __ROP_AbcOutputWalker__
#define __ROP_AbcOutputWalker__

#include "ROP_AbcContext.h"
#include <GABC/GABC_API.h>
#include <GABC/GABC_Include.h>
#include <GABC/GABC_IObject.h>
#include <GABC/GABC_OError.h>
#include <GABC/GABC_Util.h>
#include <GT/GT_Transform.h>

// ROP_AbcOutputWalker walks through an Alembic archive, reading
// samples and writing the out to a new archive (with some
// modifications if necessary).
class ROP_AbcOutputWalker
{
public:
    class Record;

    typedef Alembic::AbcGeom::OObject                   OObject;
    typedef Alembic::AbcGeom::OVisibilityProperty       OVisibilityProperty;

    typedef GABC_NAMESPACE::GABC_IObject                GABC_IObject;
    typedef GABC_NAMESPACE::GABC_OError	                GABC_OError;

    typedef UT_Map<std::string, Record *>               RecordsMap;
    typedef std::pair<std::string, Record *>            RecordsMapInsert;
    typedef UT_Array<std::string>                       StringList;

    // Record class used to store OObjects and their visibility
    // properties, if they have one. The Record object must store
    // a copy of the actual visibility property because that is
    // all Alembic will provide us with.
    class Record
    {
    public:
        explicit    Record(OObject *obj)
                        : myObject(obj)
                        , myVisProperty()
                        , myVisPointer(NULL)
                    {}
                    Record(OObject *obj, OVisibilityProperty vis)
                        : myObject(obj)
                        , myVisProperty(vis)
                        , myVisPointer(&myVisProperty)
                    {}
        virtual     ~Record() {
                        if (myObject)
                            delete myObject;
                    }

        OObject *                   getObject() { return myObject; }
        OVisibilityProperty *       getVisibility() { return myVisPointer; }

    private:
        OObject             *myObject;
        OVisibilityProperty  myVisProperty;
        OVisibilityProperty *myVisPointer;
    };

    class StringCollection
    {
    public:
        typedef std::pair<std::string, int>                 InsertType;
        typedef UT_Map<std::string, int>::const_iterator    const_iterator;
        typedef UT_Map<std::string, int>::iterator          iterator;

                        StringCollection() {}
        virtual         ~StringCollection() {}

        const_iterator  begin() const
                        {
                            return myMap.begin();
                        }
        iterator        begin()
                        {
                            return myMap.begin();
                        }
        const_iterator  end() const
                        {
                            return myMap.end();
                        }
        iterator        end()
                        {
                            return myMap.end();
                        }
        int             count(const std::string &str) const
                        {
                            if (myMap.count(str))
                            {
                                return myMap.at(str);
                            }

                            return 0;
                        }
        void            insert(const std::string &str)
                        {
                            if (myMap.count(str))
                            {
                                myMap[str] += 1;
                            }
                            else
                            {
                                myMap.insert(InsertType(str, 1));
                            }
                        }
        void            clear()
                        {
                            myMap.clear();
                        }

    private:
        UT_Map<std::string, int>    myMap;
    };

    ROP_AbcOutputWalker(const OObject *parent,
            RecordsMap *records,
            StringCollection *counts,
            fpreal time);

    virtual ~ROP_AbcOutputWalker() {}

    bool process(const GABC_IObject &node,
            const ROP_AbcContext &ctx,
            const GT_TransformHandle &transform,
            bool create_nodes,
            bool visible_nodes,
            int sample_limit) const;
    // Clear the running count of copies
    void resetCounts()
    {
        myRunningCounts.clear();
    }
    // Lock the running count of copies from being modified
    void setCountsFreeze(bool val)
    {
        myCountsFreeze = val;
    }

private:
    bool            createAncestors(const GABC_IObject &node,
                            const ROP_AbcContext &ctx) const;
    bool            createNode(const GABC_IObject &node,
                            const ROP_AbcContext &ctx,
                            int copy) const;
    bool            processAncestors(const GABC_IObject &node,
                            const ROP_AbcContext &ctx,
                            int sample_limit,
                            bool visible) const;
    bool            processNode(const GABC_IObject &node,
                            const ROP_AbcContext &ctx,
                            int copy,
                            bool visible) const;

    // The OObject that was provided during construction
    OObject const              *const myOriginalParent;
    // The OObject that should be used as the parent for the next OObject
    mutable OObject const      *myCurrentParent;
    // The collection of packed Alembics encountered this frame and
    // the number of copies of each
    StringCollection const     *const myTotalCounts;
    // The running count of copies processed for each Alembic encountered
    // this frame
    mutable StringCollection    myRunningCounts;
    // Map to read/write Records to/from. Key is object path
    RecordsMap                 *myRecordsMap;
    // Matrix used to compute inverse transformation of transforms in hierarchy
    mutable UT_Matrix4D         myMatrix;
    // Matrix cache used by packed Alembics with multiple copies
    mutable UT_Matrix4D         myStoredMatrix;
    // Flat time to add to sample time.
    // This is a consequence of us starting at frame 1
    fpreal const                myAdditionalSampleTime;
    // Allow updates to running counts?
    bool                        myCountsFreeze;
};

#endif