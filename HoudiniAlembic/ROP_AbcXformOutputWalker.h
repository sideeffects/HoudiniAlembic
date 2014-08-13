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

#ifndef __ROP_AbcXformOutputWalker__
#define __ROP_AbcXformOutputWalker__

#include "ROP_AbcContext.h"
#include "ROP_AbcOutputWalker.h"

class ROP_AbcXformOutputWalker : public ROP_AbcOutputWalker
{
public:
    typedef UT_Map<std::string, UT_Matrix4D>            MatrixMap;
    typedef std::pair<std::string, UT_Matrix4D>         MatrixMapInsert;
    typedef UT_Set<std::string>                         ObjectSet;

            ROP_AbcXformOutputWalker(const OObject *parent,
                    BoundsMap *bounds,
                    RecordsMap *records,
                    TypeMap *types,
                    const std::string &file,
                    GABC_OError &err,
                    fpreal time,
                    int keep_children);

    virtual ~ROP_AbcXformOutputWalker() {}

    bool    process(const GABC_IObject &node,
                    const ROP_AbcContext &ctx,
                    const GT_TransformHandle &transform,
                    bool create_nodes,
                    bool visible_nodes,
                    int sample_limit) const;
    bool    setStoreChildren(bool v) const
            {
                bool temp = myStoreChildren;
                myStoreChildren = v;
                return temp;
            }
    // Write samples for ancestors of sampled transforms
    bool    writeChildren(const ROP_AbcContext &ctx) const;
    // Empty out stored ancestor nodes
    void    clearStorage() const { myNodeStorage.clear(); }

private:
    bool    processAncestors(const GABC_IObject &node,
                    const ROP_AbcContext &ctx,
                    int sample_limit,
                    bool visible) const;
    bool    writeChildrenHelper(const GABC_IObject &node,
                    const OObject *parent,
                    const ROP_AbcContext &ctx) const;
    void    storageHelper(const GABC_IObject &node) const;

    // Some of the transforms in the hierarchy leading to the one currently
    // being processed may have been transformed. Store their new values here.
    mutable MatrixMap   myMatrixCache;
    // Stores child nodes that should be output to the new archive after
    // transforms have been sampled for this frame
    mutable ObjectSet   myNodeStorage;
    // Filename/path of Alembic archive the packed Alembics originate from
    const std::string  &myArchiveFile;
    // Keep transform ancestors? Just transforms or geometry too?
    int                 myKeepChildren;
    // Add children of transforms to storage?
    mutable bool        myStoreChildren;
};

#endif