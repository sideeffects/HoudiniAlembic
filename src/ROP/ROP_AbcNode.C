/*
 * Copyright (c) 2017
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

#include "ROP_AbcNode.h"

#include <UT/UT_WorkBuffer.h>

void
ROP_AbcNode::makeCollisionFreeName(std::string &name) const
{
    if(myChildren.find(name) == myChildren.end())
	return;

    myResolver.resolve(name);
    UT_Array<const ROP_AbcNode *> ancestors;
    for(const ROP_AbcNode *node = this; node; node = node->getParent())
	ancestors.append(node);

    UT_WorkBuffer buf;
    for(exint i = ancestors.entries() - 2; i >= 0; --i)
    {
	buf.append('/');
	buf.append(ancestors(i)->getName());
    }
    buf.append('/');
    buf.append(name);

    myArchive->getOError().warning("Renaming node to %s to resolve collision.", buf.buffer());
}

void
ROP_AbcNode::addChild(ROP_AbcNode *child)
{
    const std::string &name = child->myName;

    UT_ASSERT(!child->myParent);
    child->myParent = this;
    myChildren.emplace(name, child);
    myResolver.add(name);
}
