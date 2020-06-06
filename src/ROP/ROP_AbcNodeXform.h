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

#ifndef __ROP_AbcNodeXform__
#define __ROP_AbcNodeXform__

#include "ROP_AbcNode.h"

#include <Alembic/AbcGeom/All.h>
#include <GABC/GABC_OGTGeometry.h>
#include <UT/UT_Matrix4.h>

#include "ROP_AbcUserProperties.h"

typedef Alembic::AbcGeom::OBox3dProperty OBox3dProperty;
typedef Alembic::AbcGeom::OVisibilityProperty OVisibilityProperty;
typedef Alembic::AbcGeom::OXform OXform;
typedef Alembic::AbcGeom::OXformSchema OXformSchema;
typedef Alembic::AbcGeom::XformSample XformSample;

/// Class describing a transform exported to an Alembic archive.
class ROP_AbcNodeXform : public ROP_AbcNode
{
public:
    ROP_AbcNodeXform(const std::string &name)
	: ROP_AbcNode(name), myMatrix(1), myIsValid(false), myIsSparse(false),
	  myVizValid(false), mySampleCount(0) {}

    OObject getOObject(ROP_AbcArchive &archive, GABC_OError &err) override;
    NodeType getNodeType() const override { return NodeType::XFORM; }
    void purgeObjects() override;

    void getUserPropNames(UT_SortedStringSet &names,
	GABC_OError &err) const override;

    void clearData(bool locked) override;
    void update(ROP_AbcArchive &archive,
	bool displayed, UT_BoundingBox &box,
	const GABC_LayerOptions &layerOptions, GABC_OError &err) override;

    /// Sets the current user properties.
    void setUserProperties(const std::string &vals, const std::string &meta)
	    { myUserPropVals = vals; myUserPropMeta = meta; }
    /// Sets the current transform.
    void setData(const UT_Matrix4D &m) { myMatrix = m; }
    /// Gets the current transform.
    const UT_Matrix4D &getMatrix() const { return myMatrix; }

private:
    void makeValid(ROP_AbcArchive &archive, GABC_OError &err);

    OXform myOXform;
    OObject myOObject;
    OCompoundProperty mySchema;
    OVisibilityProperty myVisibility;
    OBox3dProperty myChildBounds;
    OCompoundProperty myUserProps;

    std::string myUserPropVals;
    std::string myUserPropMeta;
    ROP_AbcUserProperties myUserProperties;

    UT_Matrix4D myMatrix;
    UT_Matrix4D myCachedMatrix;
    exint mySampleCount;
    bool myIsValid;
    bool myIsSparse;
    bool myVizValid;
};

#endif
