/*
 * Copyright (c) 2013
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
#include <SOP/SOP_Node.h>
#include "ROP_AbcContext.h"
#include "ROP_AbcOpCamera.h"
#include "ROP_AbcSOP.h"
#include <GABC/GABC_Util.h>
#include <GABC/GABC_OError.h>

namespace
{
    typedef Alembic::AbcGeom::OXform		OXform;
    typedef Alembic::AbcGeom::XformSample	XformSample;
    typedef Alembic::Abc::M44d			M44d;
    typedef Alembic::Abc::Box3d			Box3d;

    static const char	*theCameraName = "cameraProperties";

    OBJ_Node *
    getNode(int id)
    {
	OP_Node	*node = OP_Node::lookupNode(id);
	return UTverify_cast<OBJ_Node *>(node);
    }

    void
    nodeFullPath(UT_WorkBuffer &buf, int id)
    {
	OBJ_Node	*o = getNode(id);
	if (!o)
	    buf.sprintf("Node with unique id %d was deleted", id);
	else
	    o->getFullPath(buf);
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
	// Add a camera child
	addChild(theCameraName, new ROP_AbcOpCamera((OBJ_Camera *)node));
    }
    else
    {
	SOP_Node	*sop = ctx.singletonSOP();
	if (!sop || sop->getCreator() != node)
	{
	    if (ctx.useDisplaySOP())
		sop = node->getDisplaySopPtr();
	    else
		sop = node->getRenderSopPtr();
	}
	if (sop)
	{
	    addChild(node->getName(), new ROP_AbcSOP(sop));
	    myGeometryContainer = true;
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
    OBJ_Node	*node = getNode(myNodeId);

    if (!node || !node->getLocalTransform(ctx.cookContext(), myMatrix))
    {
	UT_WorkBuffer	fullpath;
	nodeFullPath(fullpath, myNodeId);
	return err.error("Error cooking transofmr %s at time %g",
		fullpath.buffer(), ctx.cookContext().getTime());
    }
    myTimeDependent = node->isTimeDependent(ctx.cookContext());
    if (!myTimeDependent
	    && myMatrix.isIdentity()
	    && myGeometryContainer
	    && ctx.collapseIdentity()
	    && const_cast<OObject &>(parent).getParent().valid())
    {
	myIdentity = true;
	if (!startChildren(parent, err, ctx, myBox))
	    return false;
    }
    else
    {
	// Process children, computing their bounding box
	myOXform = OXform(parent, name(), ctx.timeSampling());
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

bool
ROP_AbcOpXform::update(GABC_OError &err,
	const ROP_AbcContext &ctx, UT_BoundingBox &box)
{
    if (selfTimeDependent())
    {
	if (!myIdentity)
	{
	    OBJ_Node	*node = getNode(myNodeId);

	    if (!node || !node->getLocalTransform(ctx.cookContext(), myMatrix))
	    {
		UT_WorkBuffer	fullpath;
		nodeFullPath(fullpath, myNodeId);
		return err.error("Error cooking transofmr %s at time %g",
			fullpath.buffer(), ctx.cookContext().getTime());
	    }
	}
    }

    // Process children, computing their bounding box
    UT_BoundingBox	kidbox;
    kidbox.initBounds();
    if (!updateChildren(err, ctx, kidbox))
	return false;

    if (selfTimeDependent())
    {
	XformSample	sample;
	M44d	m = GABC_Util::getM(myMatrix);
	sample.setMatrix(m);
	myOXform.getSchema().set(sample);
    }
    else 
    if (ctx.fullBounds())
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
