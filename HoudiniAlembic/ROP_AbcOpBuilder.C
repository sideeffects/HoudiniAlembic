/*
 * Copyright (c) 2015
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

#include "ROP_AbcOpBuilder.h"
#include "ROP_AbcObject.h"
#include <GABC/GABC_OError.h>
#include <OP/OP_Network.h>
#include <OBJ/OBJ_Node.h>
#include "ROP_AbcOpXform.h"
#include "ROP_AbcArchive.h"

using namespace GABC_NAMESPACE;

namespace
{
    typedef ROP_AbcOpBuilder::InternalNode	InternalNode;

    static bool
    validObjectType(fpreal t, OBJ_Node *obj)
    {
	if (!obj)
	    return false;

	if (obj->getObjectType() == OBJ_CAMERA
		|| obj->getObjectType() == OBJ_SUBNET
		|| obj->castToOBJGeometry())
	    return true;
	return false;
    }

    static void
    addNodesToTree(const ROP_AbcContext &ctx,
	    ROP_AbcObject *node, const InternalNode &inode)
    {
	typedef ROP_AbcOpBuilder::InternalNode	InternalNode;
	for (InternalNode::const_iterator it = inode.begin();
		it != inode.end(); ++it)
	{
	    const InternalNode	&ikid = it->second;
	    OBJ_Node		*obj = CAST_OBJNODE(ikid.node());
	    if (validObjectType(ctx.cookTime(), obj))
	    {
		ROP_AbcObject		*kid = new ROP_AbcOpXform(obj, ctx);
		node->addChild(obj->getName(), kid);
		addNodesToTree(ctx, kid, ikid);
	    }
	}
    }

    static void
    dumpTree(const InternalNode &node, const char *root, bool full)
    {
	OP_Node		*op = node.node();
	UT_WorkBuffer	 path;
	path.sprintf("%s/%s", root, op ? op->getName().buffer() : "<unknown>");
	if (full || node.childCount() == 0)
	{
	    printf("  %s (%d)\n", path.buffer(), (int)node.childCount());
	}
	for (InternalNode::const_iterator it = node.begin();
		it != node.end(); ++it)
	{
	    dumpTree(it->second, path.buffer(), full);
	}
    }
}

bool
ROP_AbcOpBuilder::addChild(GABC_OError &err, OP_Node *child)
{
    if (!child->isInputAncestor(myRootNode) &&
	    !child->isParentAncestor(myRootNode))
    {
	UT_WorkBuffer	rpath;
	UT_WorkBuffer	cpath;
	myRootNode->getFullPath(rpath);
	child->getFullPath(cpath);
	err.warning("%s is not in the same object network as %s",
		cpath.buffer(), rpath.buffer());
	return false;
    }

    UT_Array<int>	path;
    while (child != myRootNode)
    {
	path.append(child->getUniqueId());
	if (child->getInput(0)
		&& child->getInput(0)->getParent() == child->getParent())
	{
	    child = child->getInput(0);
	}
	else
	{
	    child = child->getParent();
	}
	UT_ASSERT(child);
    }
    InternalNode	*curr = &myTree;
    for (int i = path.entries(); i-- > 0; )
    {
	curr = curr->addChild(path(i));
    }
    return true;
}

void
ROP_AbcOpBuilder::buildTree(ROP_AbcArchive &arch,
	const ROP_AbcContext &ctx) const
{
    addNodesToTree(ctx, &arch, myTree);
}

void
ROP_AbcOpBuilder::dump(bool full) const
{
    dumpTree(myTree, "", full);
    fflush(stdout);
}
