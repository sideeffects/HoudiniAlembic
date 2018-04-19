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

#include "GABC_LayerOptions.h"

using namespace GABC_NAMESPACE;

void
GABC_LayerOptions::getMetadata(Alembic::Abc::MetaData &md, LayerType type)
{
    UT_ASSERT(type != LayerType::NONE);
    if(type == LayerType::PRUNE)
	Alembic::AbcCoreLayer::SetPrune(md, true);
    else if(type == LayerType::REPLACE)
	Alembic::AbcCoreLayer::SetReplace(md, true);
}

Alembic::Abc::SparseFlag
GABC_LayerOptions::getSparseFlag(LayerType type)
{
    UT_ASSERT(type != LayerType::NONE);
    if(type == LayerType::PRUNE || type == LayerType::SPARSE)
	return Alembic::Abc::SparseFlag::kSparse;
    // (type == LayerType::FULL || type == LayerType::REPLACE)
    return Alembic::Abc::SparseFlag::kFull;
}

Alembic::AbcGeom::ObjectVisibility
GABC_LayerOptions::getOVisibility(VizType type)
{
    switch(type)
    {
	default:
	    UT_ASSERT(0);
	case VizType::DEFER:
	    return Alembic::AbcGeom::kVisibilityDeferred;
	case VizType::HIDDEN:
	    return Alembic::AbcGeom::kVisibilityHidden;
	case VizType::VISIBLE:
	    return Alembic::AbcGeom::kVisibilityVisible;
    }
}

void
GABC_LayerOptions::appendNodeRule(const UT_StringRef &nodePat,
    LayerType type)
{
    myNodeData.append(nodePat, type);
}

void
GABC_LayerOptions::appendVizRule(const UT_StringRef &nodePat,
    VizType type)
{
    myVizData.append(nodePat, type);
}

void
GABC_LayerOptions::appendAttrRule(const UT_StringRef &nodePat,
    const UT_StringRef &attrPat, LayerType type)
{
    myAttrData.append(nodePat, attrPat, type);
}

void
GABC_LayerOptions::appendFaceSetRule(const UT_StringRef &nodePat,
    const UT_StringRef &attrPat, LayerType type)
{
    myFaceSetData.append(nodePat, attrPat, type);
}

void
GABC_LayerOptions::appendUserPropRule(const UT_StringRef &nodePat,
    const UT_StringRef &userPropPat, LayerType type)
{
    myUserPropData.append(nodePat, userPropPat, type);
}

GABC_LayerOptions::LayerType
GABC_LayerOptions::getNodeType(const UT_StringRef &nodePath) const
{
    LayerType type = myNodeData.getRule(nodePath);

    if(type == LayerType::NONE)
    {
	if(myVizData.matchesNodePattern(nodePath)
	    || myAttrData.matchesNodePattern(nodePath)
	    || myFaceSetData.matchesNodePattern(nodePath)
	    || myUserPropData.matchesNodePattern(nodePath))
	{
	    type = LayerType::SPARSE;
	}
    }

    return type;
}

GABC_LayerOptions::VizType
GABC_LayerOptions::getVizType(const UT_StringRef &nodePath,
    LayerType nodeType) const
{
    if(nodeType == LayerType::NONE || nodeType == LayerType::PRUNE)
	return VizType::NONE;

    auto type = myVizData.getRule(nodePath);
    if(type == VizType::NONE && (nodeType == LayerType::FULL ||
	nodeType == LayerType::REPLACE))
    {
	return VizType::DEFAULT;
    }

    return type;
}

GABC_LayerOptions::LayerType
GABC_LayerOptions::getAttrType(const UT_StringRef &nodePath,
    const UT_StringRef &attrName, LayerType nodeType) const
{
    auto type = myAttrData.getRule(nodePath, attrName);

    if(type != LayerType::PRUNE)
    {
	if(nodeType == LayerType::FULL || nodeType == LayerType::REPLACE)
	    return LayerType::FULL;
	if(nodeType == LayerType::NONE || nodeType == LayerType::PRUNE)
	    return LayerType::NONE;
    }

    return type;
}

GABC_LayerOptions::LayerType
GABC_LayerOptions::getFaceSetType(const UT_StringRef &nodePath,
    const UT_StringRef &faceSetName, LayerType nodeType) const
{
    auto type = myFaceSetData.getRule(nodePath, faceSetName);

    if(type != LayerType::PRUNE)
    {
	if(nodeType == LayerType::FULL || nodeType == LayerType::REPLACE)
	    return LayerType::FULL;
	if(nodeType == LayerType::NONE || nodeType == LayerType::PRUNE)
	    return LayerType::NONE;
    }

    return type;
}

GABC_LayerOptions::LayerType
GABC_LayerOptions::getUserPropType(const UT_StringRef &nodePath,
    const UT_StringRef &userPropName, LayerType nodeType) const
{
    auto type = myUserPropData.getRule(nodePath, userPropName);

    if(type != LayerType::PRUNE)
    {
	if(nodeType == LayerType::FULL || nodeType == LayerType::REPLACE)
	    return LayerType::FULL;
	if(nodeType == LayerType::NONE || nodeType == LayerType::PRUNE)
	    return LayerType::NONE;
    }

    return type;
}