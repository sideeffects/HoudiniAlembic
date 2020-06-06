/*
 * Copyright (c) 2020
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

#ifndef __ROP_AbcNodeInstance__
#define __ROP_AbcNodeInstance__

#include "ROP_AbcNodeShape.h"

/// Class describing instanced geometry exported to an Alembic archive.
class ROP_AbcNodeInstance : public ROP_AbcNode
{
public:
    ROP_AbcNodeInstance(const std::string &name, ROP_AbcNodeShape *src)
	: ROP_AbcNode(name), mySource(src), myIsValid(false) {}

    OObject getOObject(ROP_AbcArchive &archive, GABC_OError &err) override;
    NodeType getNodeType() const override { return NodeType::INSTANCE; }
    void purgeObjects() override;
    void update(ROP_AbcArchive &archive,
	bool displayed, UT_BoundingBox &box,
	const GABC_LayerOptions &layerOptions, GABC_OError &err) override;

private:
    void makeValid(ROP_AbcArchive &archive,
	const GABC_LayerOptions &layerOptions, GABC_OError &err);

    ROP_AbcNodeShape *mySource;
    bool myIsValid;
};

#endif
