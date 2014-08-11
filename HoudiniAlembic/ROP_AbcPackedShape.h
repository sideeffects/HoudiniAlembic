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

#ifndef __ROP_AbcPackedShape__
#define __ROP_AbcPackedShape__

#include "ROP_AbcPackedAbc.h"
#include "ROP_AbcShapeOutputWalker.h"

// ROP_AbcPackedShape class handles packed Alembic shapes for
// ROP_AbcGTCompoundShape.
class ROP_AbcPackedShape : public ROP_AbcPackedAbc
{
public:
    typedef Alembic::Abc::OObject	            OObject;

    typedef GABC_NAMESPACE::GABC_IObject            GABC_IObject;
    typedef GABC_NAMESPACE::GABC_OError	            GABC_OError;
    typedef GABC_NAMESPACE::GABC_PackedImpl         GABC_PackedImpl;
    typedef GABC_NAMESPACE::GABC_Util               GABC_Util;

            ROP_AbcPackedShape(GABC_OError &err)
                : ROP_AbcPackedAbc(err)
            {}
    virtual ~ROP_AbcPackedShape() {}

    // Write out the first frame to the Alembic archive
    bool    startPacked(AbcInfoList &list,
                    const ROP_AbcContext &ctx);
    // Write out subsequent frames to the Alembic archive
    bool    updatePacked(AbcInfoList &list,
                    const ROP_AbcContext &ctx,
                    exint elapsed_frames);
    // Write out subsequent frames to the Alembic archive when no packed
    // Alembics are visible
    bool    updateAllHidden(const ROP_AbcContext &ctx,
                    exint elapsed_frames);
};

#endif