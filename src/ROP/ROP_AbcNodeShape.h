/*
 * Copyright (c) 2018
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

#ifndef __ROP_AbcNodeShape__
#define __ROP_AbcNodeShape__

#include "ROP_AbcNode.h"

#include <GABC/GABC_OGTGeometry.h>

#include "ROP_AbcUserProperties.h"

typedef GABC_NAMESPACE::GABC_OGTGeometry GABC_OGTGeometry;

/// Class describing geometry exported to an Alembic archive.
class ROP_AbcNodeShape : public ROP_AbcNode
{
public:
    ROP_AbcNodeShape(const std::string &name)
	: ROP_AbcNode(name), mySampleCount(0), myLocked(false) {}

    virtual OObject getOObject(ROP_AbcArchive &archive, GABC_OError &err);
    virtual NodeType getNodeType() const { return NodeType::SHAPE; }
    virtual void purgeObjects();

    virtual void getAttrNames(UT_SortedStringSet &names) const;
    virtual void getUserPropNames(UT_SortedStringSet &names,
	GABC_OError &err) const;

    virtual void clearData(bool locked);
    virtual void updateLocked(bool locked);
    virtual void update(ROP_AbcArchive &archive,
	bool displayed, UT_BoundingBox &box,
	const GABC_LayerOptions &layerOptions, GABC_OError &err);

    /// Sets the current user properties.
    void setUserProperties(const std::string &vals, const std::string &meta)
	    { myUserPropVals = vals; myUserPropMeta = meta; }
    /// Sets the current geometry.
    void setData(const GT_PrimitiveHandle &prim, bool visible,
	const std::string &subd)
	    { myPrim = prim; setVisibility(visible); mySubdGrp = subd; }

private:
    void clear();

    UT_UniquePtr<GABC_OGTGeometry> myWriter; 
    exint mySampleCount;

    std::string myUserPropVals;
    std::string myUserPropMeta;
    ROP_AbcUserProperties myUserProperties;

    GT_PrimitiveHandle myPrim;
    bool myLocked;
    std::string mySubdGrp;
};

#endif
