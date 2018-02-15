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
GABC_LayerOptions::FlagStorage::append(const UT_String &pattern,
    GABC_AbcLayerFlag flag)
{
    myData.append(Item(pattern, flag));
}

GABC_AbcLayerFlag
GABC_LayerOptions::FlagStorage::match(const UT_String &str)
{
    for(auto it = myData.begin(); it != myData.end(); ++it)
	if(str.multiMatch(it->myPat))
	    return it->myFlag;
    return GABC_ALEMBIC_LAYERING_NULL;
}

void
GABC_LayerOptions::MultiFlagStorage::append(const UT_String &pat,
    const UT_String &subPat, GABC_AbcLayerFlag flag)
{
    myData.append(Item(pat, subPat, flag));
}

GABC_AbcLayerFlag
GABC_LayerOptions::MultiFlagStorage::match(const UT_String &str,
    const UT_String &subStr)
{
    FlagStorage tmpStorage;
    for(auto it = myData.begin(); it != myData.end(); ++it)
	if(str.multiMatch(it->myPat))
	    tmpStorage.append(UT_String(it->mySubPat), it->myFlag);
    return tmpStorage.match(subStr);
}

void
GABC_LayerOptions::updateNodePat(const UT_String &nodePat,
    GABC_AbcLayerFlag flag)
{
    myNodeData.append(nodePat, flag);
}

void
GABC_LayerOptions::updateVizPat(const UT_String &nodePat,
    GABC_AbcLayerFlag flag)
{
    myVizData.append(nodePat, flag);
}

void
GABC_LayerOptions::updateAttrPat(const UT_String &nodePat,
    const UT_String &attrPat, GABC_AbcLayerFlag flag)
{
    myAttrData.append(nodePat, attrPat, flag);
}

void
GABC_LayerOptions::updateUserPropPat(const UT_String &nodePat,
    const UT_String &userPropPat, GABC_AbcLayerFlag flag)
{
    myUserPropData.append(nodePat, userPropPat, flag);
}

GABC_AbcLayerFlag
GABC_LayerOptions::matchNode(const UT_String &nodePath)
{
    return myNodeData.match(nodePath);
}

GABC_AbcLayerFlag
GABC_LayerOptions::matchViz(const UT_String &nodePath)
{
    return myVizData.match(nodePath);
}

GABC_AbcLayerFlag
GABC_LayerOptions::matchAttr(const UT_String &nodePath,
    const UT_String &attrName)
{
    return myAttrData.match(nodePath, attrName);
}

GABC_AbcLayerFlag
GABC_LayerOptions::matchUserProp(const UT_String &nodePath,
    const UT_String &userPropName)
{
    return myUserPropData.match(nodePath, userPropName);
}