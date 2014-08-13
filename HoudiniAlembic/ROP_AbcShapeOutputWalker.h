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

#ifndef __ROP_AbcShapeOutputWalker__
#define __ROP_AbcShapeOutputWalker__

#include "ROP_AbcContext.h"
#include "ROP_AbcOutputWalker.h"

class ROP_AbcShapeOutputWalker : public ROP_AbcOutputWalker
{
public:
    typedef GABC_NAMESPACE::GABC_IObject                GABC_IObject;
    typedef GABC_NAMESPACE::GABC_OError	                GABC_OError;

    ROP_AbcShapeOutputWalker(const OObject *parent,
            BoundsMap *bounds,
            RecordsMap *records,
            TypeMap *types,
            OReaderCollection *counts,
            GABC_OError &err,
            fpreal time);

    virtual ~ROP_AbcShapeOutputWalker() {}

    bool process(const GABC_IObject &node,
            const ROP_AbcContext &ctx,
            const GT_TransformHandle &transform,
            bool create_nodes,
            bool visible_nodes,
            int sample_limit) const;
    // Clear the running count of copies
    void            resetCounts()
                    {
                        myRunningCounts.clear();
                    }
    // Lock the running count of copies from being modified
    void            setCountsFreeze(bool val)
                    {
                        myCountsFreeze = val;
                    }

private:
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


    // The collection of packed Alembics encountered this frame and
    // the number of copies of each
    OReaderCollection const    *const myTotalCounts;
    // The running count of copies processed for each Alembic encountered
    // this frame
    mutable OReaderCollection   myRunningCounts;
    // Stores the matrix of the first copy of a packed Alembic. Need the inverse
    // of this matrix for subsequent copies
    mutable UT_Matrix4D         myStoredMatrix;
    // Allow updates to running counts?
    bool                        myCountsFreeze;
};

#endif