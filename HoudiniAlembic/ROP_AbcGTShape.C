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

#include "ROP_AbcGTShape.h"
#include <GABC/GABC_OGTGeometry.h>
#include "ROP_AbcGTInstance.h"

using namespace GABC_NAMESPACE;

ROP_AbcGTShape::ROP_AbcGTShape(const std::string &name)
    : myShape(NULL)
    , myInstance(NULL)
    , myName(name)
    , myPrimType(GT_PRIM_UNDEFINED)
{
}

ROP_AbcGTShape::~ROP_AbcGTShape()
{
    clear();
}

void
ROP_AbcGTShape::clear()
{
    delete myShape;
    delete myInstance;
    myShape = NULL;
    myInstance = NULL;
}

bool
ROP_AbcGTShape::isPrimitiveSupported(const GT_PrimitiveHandle &prim)
{
    return GABC_OGTGeometry::isPrimitiveSupported(prim);
}

bool
ROP_AbcGTShape::firstFrame(const GT_PrimitiveHandle &prim,
	const OObject &parent,
	GABC_OError &err,
	const ROP_AbcContext &ctx,
	ObjectVisibility vis)
{
    clear();
    myPrimType = prim->getPrimitiveType();
    myShape = new GABC_OGTGeometry(myName);
    return myShape->start(prim, parent, err, ctx, vis);
}

bool
ROP_AbcGTShape::nextFrame(const GT_PrimitiveHandle &prim,
	GABC_OError &err,
	const ROP_AbcContext &ctx)
{
    if (myShape)
    {
	return myShape->update(prim, err, ctx);
    }
    if (myInstance)
    {
	return myInstance->update(err, ctx, prim);
    }
    return false;
}

bool
ROP_AbcGTShape::nextFrameHidden(GABC_OError &err, exint frames)
{
    if (frames < 0)
    {
        err.error("Attempted to update less than 0 frames.");
        return false;
    }

    if (myShape)
    {
	return myShape->updateHidden(err, frames);
    }
    if (myInstance)
    {
	return myInstance->updateHidden(err, myPrimType, frames);
    }
    return false;
}

bool
ROP_AbcGTShape::firstInstance(const GT_PrimitiveHandle &prim,
	const OObject &parent,
	GABC_OError &err,
	const ROP_AbcContext &ctx,
	bool subd_mode,
	bool add_unused_pts,
        ObjectVisibility vis)
{
    clear();
    myPrimType = prim->getPrimitiveType();
    myInstance = new ROP_AbcGTInstance(myName);
    return myInstance->first(parent, err, ctx, prim, subd_mode, add_unused_pts, vis);
}

int
ROP_AbcGTShape::getPrimitiveType() const
{
    return myPrimType;
}

bool
ROP_AbcGTShape::start(const OObject &, GABC_OError &,
	const ROP_AbcContext &, UT_BoundingBox &)
{
    UT_ASSERT(0);
    return false;
}

bool
ROP_AbcGTShape::update(GABC_OError &err,
	const ROP_AbcContext &ctx, UT_BoundingBox &box)
{
    UT_ASSERT(0);
    return false;
}

bool
ROP_AbcGTShape::selfTimeDependent() const
{
    return false;	// Only parent knows
}

bool
ROP_AbcGTShape::getLastBounds(UT_BoundingBox &) const
{
    return false;
}

Alembic::Abc::OObject
ROP_AbcGTShape::getOObject() const
{
    if (myInstance)
	return myInstance->getOObject();
    UT_ASSERT(myShape && "Exported geometry will be incorrect");
    return myShape ? myShape->getOObject() : Alembic::Abc::OObject();
}
