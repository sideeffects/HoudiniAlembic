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
 * NAME:	ROP_AbcGTShape.h
 *
 * COMMENTS:
 */

#include "ROP_AbcGTShape.h"
#include <GABC/GABC_OGTGeometry.h>

ROP_AbcGTShape::ROP_AbcGTShape(const std::string &name)
    : myShape(NULL)
    , myName(name)
{
}

ROP_AbcGTShape::~ROP_AbcGTShape()
{
    delete myShape;
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
	const ROP_AbcContext &ctx)
{
    delete myShape;
    myShape = new GABC_OGTGeometry(myName);
    return myShape->start(prim, parent, err, ctx);
}

bool
ROP_AbcGTShape::nextFrame(const GT_PrimitiveHandle &prim,
	GABC_OError &err,
	const ROP_AbcContext &ctx)
{
    if (!myShape)
	return false;
    return myShape->update(prim, err, ctx);
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
