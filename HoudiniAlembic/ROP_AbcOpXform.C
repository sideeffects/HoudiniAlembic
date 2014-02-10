/*
 * Copyright (c) 2014
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

#include "ROP_AbcOpXform.h"
#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_SubNet.h>
#include <SOP/SOP_Node.h>
#include "ROP_AbcContext.h"
#include "ROP_AbcOpCamera.h"
#include "ROP_AbcSOP.h"
#include <GABC/GABC_Util.h>
#include <GABC/GABC_OError.h>

using namespace GABC_NAMESPACE;

namespace
{
    typedef Alembic::Abc::OObject			OObject;
    typedef Alembic::AbcGeom::OXform			OXform;
    typedef Alembic::AbcGeom::OVisibilityProperty	OVisibilityProperty;
    typedef Alembic::AbcGeom::ObjectVisibility		ObjectVisibility;
    typedef Alembic::AbcGeom::XformSample		XformSample;
    typedef Alembic::Abc::M44d				M44d;
    typedef Alembic::Abc::Box3d				Box3d;

    static const char	*theCameraName = "cameraProperties";

    static OBJ_Node *
    getXformNode(int id)
    {
	OP_Node	*node = OP_Node::lookupNode(id);
	return UTverify_cast<OBJ_Node *>(node);
    }

    static void
    xformNodeFullPath(UT_WorkBuffer &buf, int id)
    {
	OBJ_Node	*o = getXformNode(id);
	if (!o)
	    buf.sprintf("Node with unique id %d was deleted", id);
	else
	    o->getFullPath(buf);
    }

    static bool
    collapseTransform(bool geometry_container,
		bool time_dependent,
		OP_Node *node,
		const UT_Matrix4D &xform,
		const ROP_AbcContext &ctx,
		const OObject &parent)
    {
	// We can't collapse if we're the root node
	if (!const_cast<OObject &>(parent).getParent().valid())
	    return false;

	int	abc_collapse;
	if (node->evalParameterOrProperty("abc_collapse", 0,
			ctx.cookContext().getTime(), abc_collapse))
	{
	    // There's a parameter "abc_collapse".  So, if the user has set
	    // this to "true", we'll collapse the transform.  This will even
	    // allow subnet objects to be collapsed out of the Alembic tree.
	    // Of course, the transform is also collapsed out.
	    if (abc_collapse != 0)
		return true;
	}

	// For the remainder of the options, we only collapse if we're the
	// geometry object containing SOPs (not sub-nets).
	if (!geometry_container)
	    return false;	// Only collapse the SOP container

	switch (ctx.collapseIdentity())
	{
	    case ROP_AbcContext::COLLAPSE_NONE:
		// Don't collapse any geometry nodes
		return false;
	    case ROP_AbcContext::COLLAPSE_GEOMETRY:
		// Collapse all geometry containers, regardless of transforms
		const_cast<OObject &>(parent).getParent().valid();
		return true;
	    case ROP_AbcContext::COLLAPSE_IDENTITY:
		// Collapse geometry containers only if it's a non-animated
		// identity transform.
		return !time_dependent && xform.isIdentity();
	}
	return false;
    }
}

ROP_AbcOpXform::ROP_AbcOpXform(OBJ_Node *node, const ROP_AbcContext &ctx)
    : myNodeId(node ? node->getUniqueId() : -1)
    , myOXform()
    , myTimeDependent(false)
    , myIdentity(false)
    , myGeometryContainer(false)
{
    if (node->getObjectType() == OBJ_CAMERA)
    {
	// Don't add a camera when saving geometry
	if (!ctx.singletonSOP())
	{
	    // Add a camera child
	    addChild(theCameraName, new ROP_AbcOpCamera((OBJ_Camera *)node));
	}
    }
    else if (node->isObjectRenderable(0))
    {
	SOP_Node	*sop = ctx.singletonSOP();
	if (!sop || sop->getCreator() == node)
	{
	    if (!sop)
	    {
		if (ctx.useDisplaySOP())
		    sop = node->getDisplaySopPtr();
		else
		    sop = node->getRenderSopPtr();
	    }
	    if (sop)
	    {
		addChild(sop->getName(), new ROP_AbcSOP(sop));
		myGeometryContainer = true;
	    }
	}
    }
}

ROP_AbcOpXform::~ROP_AbcOpXform()
{
}

bool
ROP_AbcOpXform::start(const OObject &parent,
	GABC_OError &err, const ROP_AbcContext &ctx, UT_BoundingBox &box)
{
    OBJ_Node	*node = getXformNode(myNodeId);
    bool	 evaluated = false;

    if (node)
    {
	if (node->isSubNetwork(false))
	{
	    OBJ_SubNet	*sub = UTverify_cast<OBJ_SubNet *>(node);
	    evaluated = sub->getSubnetTransform(ctx.cookContext(), myMatrix);
	}
	else
	{
	    evaluated = node->getLocalTransform(ctx.cookContext(), myMatrix);
	}
    }
    if (!evaluated)
    {
	UT_WorkBuffer	fullpath;
	xformNodeFullPath(fullpath, myNodeId);
	return err.error("Error cooking transform %s at time %g",
		fullpath.buffer(), ctx.cookContext().getTime());
    }
    myTimeDependent = node->isTimeDependent(ctx.cookContext());
    if (collapseTransform(myGeometryContainer, myTimeDependent,
		node, myMatrix, ctx, parent))
    {
	myIdentity = true;
	// Now, we have to adjust the names of the children so they don't
	// collide with any of the parent's children.
	ROP_AbcObject	*owner = getParent();
	for (ChildContainer::const_iterator it = begin(); it != end(); ++it)
	{
	    // If the names are equal, then we don't have to worry about name
	    // collisions since we're replacing this object with the child in
	    // the hierarchy.
	    if (it->second->getName() != getName())
		owner->fakeParent(it->second);
	}

	if (!startChildren(parent, err, ctx, myBox))
	    return false;
    }
    else
    {
	// Process children, computing their bounding box
	myOXform = OXform(parent, getName(), ctx.timeSampling());
	if (!startChildren(myOXform, err, ctx, myBox))
	    return false;
    }

    if (!myIdentity)
    {
	XformSample	sample;
	M44d	m = GABC_Util::getM(myMatrix);
	sample.setMatrix(m);
	myOXform.getSchema().set(sample);

	if (ctx.fullBounds())
	{
	    Box3d	b3 = GABC_Util::getBox(myBox);
	    myOXform.getSchema().getChildBoundsProperty().set(b3);
	}
	if (node->isDisplayTimeDependent())
	{
	    // Note:  This is only a valid check if the display channels have
	    // been evaluated, so there could be problems in hbatch.  However,
	    // we already performed the evaluation when building the tree, so
	    // we should be ok.
	    myVisibility = CreateVisibilityProperty(myOXform,
				ctx.timeSampling());
	    setVisibility(ctx);
	    myTimeDependent = true;
	}
    }

    // Now, set the bounding box for my parent
    if (ctx.fullBounds())
    {
	box = myBox;
	box.transform(myMatrix);
    }

    updateTimeDependentKids();

    return true;
}

void
ROP_AbcOpXform::setVisibility(const ROP_AbcContext &ctx)
{
    if (myVisibility)
    {
	OBJ_Node		*node = getXformNode(myNodeId);
	ObjectVisibility	 v = Alembic::AbcGeom::kVisibilityDeferred;

	if (node && !node->getObjectDisplay(ctx.cookContext().getTime()))
	    v = Alembic::AbcGeom::kVisibilityHidden;
	myVisibility.set(v);
    }
}

bool
ROP_AbcOpXform::update(GABC_OError &err,
	const ROP_AbcContext &ctx, UT_BoundingBox &box)
{
    if (selfTimeDependent())
    {
	if (!myIdentity)
	{
	    OBJ_Node	*node = getXformNode(myNodeId);

	    if (!node || !node->getLocalTransform(ctx.cookContext(), myMatrix))
	    {
		UT_WorkBuffer	fullpath;
		xformNodeFullPath(fullpath, myNodeId);
		return err.error("Error cooking transofmr %s at time %g",
			fullpath.buffer(), ctx.cookContext().getTime());
	    }
	}
    }
    if (myVisibility)
	setVisibility(ctx);

    // Process children, computing their bounding box
    UT_BoundingBox	kidbox;
    kidbox.initBounds();
    if (!updateChildren(err, ctx, kidbox))
	return false;

    if (selfTimeDependent() && !myIdentity)
    {
	XformSample	sample;
	M44d	m = GABC_Util::getM(myMatrix);
	sample.setMatrix(m);
	myOXform.getSchema().set(sample);
    }
    else if (ctx.fullBounds())
    {
	if (kidbox != myBox)
	{
	    myBox = kidbox;
	    Box3d	b3 = GABC_Util::getBox(myBox);
	    myOXform.getSchema().getChildBoundsProperty().set(b3);
	}

	// Set up bounding box for my parent
	box = myBox;
	if (!myIdentity)
	    box.transform(myMatrix);
    }

    updateTimeDependentKids();

    return true;
}

bool
ROP_AbcOpXform::selfTimeDependent() const
{
    return myTimeDependent;
}

bool
ROP_AbcOpXform::getLastBounds(UT_BoundingBox &box) const
{
    box = myBox;
    return true;
}
