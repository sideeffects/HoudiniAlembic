/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Side Effects Software Inc
 *	123 Front Street West, Suite 1401
 *	Toronto, Ontario
 *	Canada   M5J 2M2
 *	416-504-9876
 *
 * NAME:	ROP_AbcOpBuilder.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#include "ROP_AbcOpBuilder.h"
#include "ROP_AbcObject.h"
#include <GABC/GABC_OError.h>
#include <OP/OP_Network.h>
#include <OBJ/OBJ_Node.h>
#include "ROP_AbcOpXform.h"
#include "ROP_AbcArchive.h"

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
	child = child->getParent();
	UT_ASSERT(child);
    }
    InternalNode	*curr = &myTree;
    for (int i = path.entries(); i-- > 0; )
    {
	curr = curr->addChild(path(i));
    }
    return true;
}

static bool
validObjectType(OBJ_Node *obj)
{
    if (!obj)
	return false;

    if (obj->getObjectType() == OBJ_CAMERA || obj->getObjectType() == OBJ_SUBNET)
	return true;
    if (obj->castToOBJGeometry() && obj->isObjectRenderable())
	return true;
    return false;
}

static void
addNodesToTree(const ROP_AbcContext &ctx,
	ROP_AbcObject *node, const ROP_AbcOpBuilder::InternalNode &inode)
{
    typedef ROP_AbcOpBuilder::InternalNode	InternalNode;
    for (InternalNode::const_iterator it = inode.begin();
	    it != inode.end(); ++it)
    {
	const InternalNode	&ikid = it->second;
	OBJ_Node		*obj = CAST_OBJNODE(ikid.node());
	if (validObjectType(obj))
	{
	    ROP_AbcObject		*kid = new ROP_AbcOpXform(obj, ctx);
	    node->addChild(obj->getName(), kid);
	    addNodesToTree(ctx, kid, ikid);
	}
    }
}

void
ROP_AbcOpBuilder::buildTree(ROP_AbcArchive &arch,
	const ROP_AbcContext &ctx) const
{
    addNodesToTree(ctx, &arch, myTree);
}

void
ROP_AbcOpBuilder::ls(bool just_leaves) const
{
    class lsWalk : public WalkFunc
    {
    public:
	lsWalk(bool just_leaves)
	    : myJustLeaves(just_leaves)
	{
	}
	void	process(const InternalNode &node)
		{
		    if (!myJustLeaves || node.childCount() == 0)
		    {
			OP_Node		*op = node.node();
			UT_WorkBuffer	 path;
			if (op)
			    op->getFullPath(path);
			else
			    path.strcpy("<unknown node>");
			fprintf(stderr, "  %s\n", path.buffer());
		    }
		}
    private:
	bool	myJustLeaves;
    };
    lsWalk	w(just_leaves);
    fprintf(stderr, "Tree contents:\n");
    walk(w);
}
