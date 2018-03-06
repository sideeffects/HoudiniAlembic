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
GABC_LayerOptions::Rules::append(const UT_String &pattern,
    LayerType type)
{
    myData.append(Rule(pattern, type));
}

GABC_LayerOptions::LayerType
GABC_LayerOptions::Rules::getLayerType(const UT_String &str) const
{
    for(auto it = myData.begin(); it != myData.end(); ++it)
	if(str.multiMatch(it->myNodePat))
	    return it->myType;
    return LayerType::DEFER;
}

void
GABC_LayerOptions::MultiRules::append(const UT_String &pat,
    const UT_String &subPat, LayerType type)
{
    myData.append(Rule(pat, subPat, type));
}

GABC_LayerOptions::LayerType
GABC_LayerOptions::MultiRules::getLayerType(const UT_String &str,
    const UT_String &subStr) const
{
    for(auto it = myData.begin(); it != myData.end(); ++it)
	if(str.multiMatch(it->myNodePat) && subStr.multiMatch(it->mySubPat))
	    return it->myType;
    return LayerType::DEFER;
}

bool
GABC_LayerOptions::MultiRules::matchesNodePattern(const UT_String &str) const
{
    for(auto it = myData.begin(); it != myData.end(); ++it)
	if(str.multiMatch(it->myNodePat))
	    return true;
    return false;
}

void
GABC_LayerOptions::getMetadata(Alembic::Abc::MetaData &md, LayerType type)
{
    UT_ASSERT(type != LayerType::DEFER);
    if(type == LayerType::PRUNE)
	Alembic::AbcCoreLayer::SetPrune(md, true);
    else if(type == LayerType::REPLACE)
	Alembic::AbcCoreLayer::SetReplace(md, true);
}

Alembic::Abc::SparseFlag
GABC_LayerOptions::getSparseFlag(LayerType type)
{
    UT_ASSERT(type != LayerType::DEFER);
    if(type == LayerType::PRUNE || type == LayerType::SPARSE)
	return Alembic::Abc::SparseFlag::kSparse;
    // (type == LayerType::FULL || type == LayerType::REPLACE)
    return Alembic::Abc::SparseFlag::kFull;
}

void
GABC_LayerOptions::appendNodeRule(const UT_String &nodePat,
    LayerType type)
{
    myNodeData.append(nodePat, type);
}

void
GABC_LayerOptions::appendVizRule(const UT_String &nodePat,
    LayerType type)
{
    myVizData.append(nodePat, type);
}

void
GABC_LayerOptions::appendAttrRule(const UT_String &nodePat,
    const UT_String &attrPat, LayerType type)
{
    myAttrData.append(nodePat, attrPat, type);
}

void
GABC_LayerOptions::appendUserPropRule(const UT_String &nodePat,
    const UT_String &userPropPat, LayerType type)
{
    myUserPropData.append(nodePat, userPropPat, type);
}

GABC_LayerOptions::LayerType
GABC_LayerOptions::getNodeType(const UT_String &nodePath) const
{
    LayerType type = myNodeData.getLayerType(nodePath);

    if(type == LayerType::DEFER)
    {
	if(myVizData.getLayerType(nodePath) != LayerType::DEFER
	    || myAttrData.matchesNodePattern(nodePath)
	    || myUserPropData.matchesNodePattern(nodePath))
	{
	    type = LayerType::SPARSE;
	}
    }

    return type;
}

GABC_LayerOptions::LayerType
GABC_LayerOptions::getVizType(const UT_String &nodePath,
    LayerType nodeType) const
{
    auto type = myVizData.getLayerType(nodePath);

    if(type != LayerType::PRUNE)
    {
	if(nodeType == LayerType::FULL || nodeType == LayerType::REPLACE)
	    return LayerType::FULL;
	if(nodeType == LayerType::DEFER || nodeType == LayerType::PRUNE)
	    return LayerType::DEFER;
    }

    return type;
}

GABC_LayerOptions::LayerType
GABC_LayerOptions::getAttrType(const UT_String &nodePath,
    const UT_String &attrName, LayerType nodeType) const
{
    auto type = myAttrData.getLayerType(nodePath, attrName);

    if(type != LayerType::PRUNE)
    {
	if(nodeType == LayerType::FULL || nodeType == LayerType::REPLACE)
	    return LayerType::FULL;
	if(nodeType == LayerType::DEFER || nodeType == LayerType::PRUNE)
	    return LayerType::DEFER;
    }

    return type;
}

GABC_LayerOptions::LayerType
GABC_LayerOptions::getUserPropType(const UT_String &nodePath,
    const UT_String &userPropName, LayerType nodeType) const
{
    auto type = myUserPropData.getLayerType(nodePath, userPropName);

    if(type != LayerType::PRUNE)
    {
	if(nodeType == LayerType::FULL || nodeType == LayerType::REPLACE)
	    return LayerType::FULL;
	if(nodeType == LayerType::DEFER || nodeType == LayerType::PRUNE)
	    return LayerType::DEFER;
    }

    return type;
}