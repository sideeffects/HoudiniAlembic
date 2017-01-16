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

#include <GABC/GABC_Util.h>
#include <UT/UT_WorkBuffer.h>

typedef Alembic::AbcGeom::XformSample XformSample;
typedef GABC_NAMESPACE::GABC_Util GABC_Util;

void
ROP_AbcNode::makeCollisionFreeName(std::string &name) const
{
    if(myChildren.find(name) == myChildren.end())
	return;

    auto it = myMaxUsedId.find(name);
    exint val = (it != myMaxUsedId.end()) ? it->second + 1 : 1;

    UT_WorkBuffer buf;
    buf.append(name.c_str());
    buf.appendSprintf("_%" SYS_PRId64, val);
    name = buf.buffer();

    UT_Array<const ROP_AbcNode *> ancestors;
    for(const ROP_AbcNode *node = this; node; node = node->getParent())
	ancestors.append(node);

    buf.clear();
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

    exint n = name.length();

    const char *s = name.c_str();
    for(exint i = n; i > 0; --i)
    {
	char c = s[i - 1];
	if(c < '0' || c > '9')
	{
	    if(c == '_' && i < n)
	    {
		UT_WorkBuffer buf;
		buf.strncpy(s, i - 1);
		std::string base_name(buf.buffer());
		exint val = 0;
		const char *si = &s[i];
		while(*si)
		    val = 10 * val + *(si++) - '0';

		auto it = myMaxUsedId.find(base_name);
		if(it != myMaxUsedId.end())
		{
		    if(val > it->second)
			it->second = val;
		}
		else
		    myMaxUsedId.emplace(base_name, val);
	    }
	    break;
	}
    }
}
