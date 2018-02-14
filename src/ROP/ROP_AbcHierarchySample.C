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

#include "ROP_AbcHierarchySample.h"

#include <UT/UT_WorkBuffer.h>

ROP_AbcHierarchySample *
ROP_AbcHierarchySample::getChildXform(const std::string &name)
{
    auto it = myChildren.find(name);
    if(it != myChildren.end())
	return &it->second;

    return &myChildren.emplace(name, ROP_AbcHierarchySample(this, name)).first->second;
}

bool
ROP_AbcHierarchySample::setPreXform(const UT_Matrix4D &m)
{
    if(mySetPreXform)
	return false;

    myPreXform = m;
    mySetPreXform = true;
    return true;
}

void
ROP_AbcHierarchySample::appendShape(
    const std::string &name,
    const GT_PrimitiveHandle &prim,
    bool visible,
    const std::string &userprops,
    const std::string &userpropsmeta)
{
    myShapes[name][prim->getPrimitiveType()].append(std::make_tuple(prim, visible, userprops, userpropsmeta));
}

void
ROP_AbcHierarchySample::appendInstancedShape(
    const std::string &name,
    int type,
    const std::string &key,
    exint idx,
    bool visible,
    const std::string &up_vals,
    const std::string &up_meta)
{
    myInstancedShapes[name][type].append(std::make_tuple(key, idx, visible, up_vals, up_meta));
}

exint
ROP_AbcHierarchySample::getNextInstanceId(const std::string &name)
{
    return ++myInstanceCount[name];
}

exint
ROP_AbcHierarchySample::getNextPackedId(const std::string &name)
{
    return ++myPackedCount[name];
}

void
ROP_AbcHierarchySample::warnRoot(const ROP_AbcArchivePtr &abc)
{
    if(myWarnedRoot)
	return;

    abc->getOError().warning("Cannot push packed primitive transform to root node.");
    myWarnedRoot = true;
}

void
ROP_AbcHierarchySample::warnChildren(const ROP_AbcArchivePtr &abc)
{
    if(myWarnedChildren)
	return;

    UT_Array<const ROP_AbcHierarchySample *> ancestors;
    for(auto node = this; node; node = node->getParent())
	ancestors.append(node);

    UT_WorkBuffer buf;
    for(exint i = ancestors.entries() - 2; i >= 0; --i)
    {
	buf.append('/');
	buf.append(ancestors(i)->myName);
    }

    abc->getOError().warning("Cannot push multiple packed primitive transforms to %s.", buf.buffer());
    myWarnedChildren = true;
}
