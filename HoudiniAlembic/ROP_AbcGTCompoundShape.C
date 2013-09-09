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

#include "ROP_AbcGTCompoundShape.h"
#include <UT/UT_WorkBuffer.h>
#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_Primitive.h>

namespace
{
    typedef UT_Array<GT_PrimitiveHandle>	PrimitiveList;

    class abc_Refiner : public GT_Refine
    {
    public:
	abc_Refiner(PrimitiveList &list, const GT_RefineParms *parms)
	    : myList(list)
	    , myParms(parms)
	{
	}
	// We need the primitives generated in a consistent order
	virtual bool	allowThreading() const	{ return false; }
	virtual void	addPrimitive(const GT_PrimitiveHandle &prim)
	{
	    if (!prim)
		return;
	    if (ROP_AbcGTShape::isPrimitiveSupported(prim))
	    {
		myList.append(prim);
		return;
	    }
	    // We hit a primitive we don't understand, so refine it
	    prim->refine(*this, myParms);
	}
    private:
	PrimitiveList		&myList;
	const GT_RefineParms	*myParms;
    };

    static void
    initializeRefineParms(GT_RefineParms &rparms, const ROP_AbcContext &ctx,
		bool polys_as_subd,
		bool show_unused_points)
    {
	rparms.setFaceSetMode(ctx.faceSetMode());
	rparms.setFastPolyCompacting(false);
	rparms.setPolysAsSubdivision(polys_as_subd);
	rparms.setShowUnusedPoints(show_unused_points);
	rparms.setCoalesceFragments(false);
    }
}


ROP_AbcGTCompoundShape::ROP_AbcGTCompoundShape(const std::string &name,
		bool polys_as_subd,
		bool show_unused_pts)
    : myShapes()
    , myName(name)
    , myContainer(NULL)
    , myPolysAsSubd(polys_as_subd)
    , myShowUnusedPoints(show_unused_pts)
{
}

ROP_AbcGTCompoundShape::~ROP_AbcGTCompoundShape()
{
    clear();
}

void
ROP_AbcGTCompoundShape::clear()
{
    for (int i = 0; i < myShapes.entries(); ++i)
	delete myShapes(i);
    delete myContainer;
    myContainer = NULL;
    myShapes.resize(0);
}

bool
ROP_AbcGTCompoundShape::first(const GT_PrimitiveHandle &prim,
			const OObject &parent,
			GABC_OError &err,
			const ROP_AbcContext &ctx,
			bool create_container)
{
    UT_ASSERT(prim);

    clear();

    // Refine the primitive into it's atomic shapes
    PrimitiveList	shapes;
    GT_RefineParms	rparms;

    initializeRefineParms(rparms, ctx, myPolysAsSubd, myShowUnusedPoints);
    abc_Refiner	refiner(shapes, &rparms);
    prim->refine(refiner, &rparms);

    if (!shapes.entries())
	return false;

    std::string		shape_name = myName;
    UT_WorkBuffer	shape_namebuf;
    for (exint i = 0; i < shapes.entries(); ++i)
    {
	if (i > 0)
	{
	    shape_namebuf.sprintf("%s_%d", myName.c_str(), (int)i);
	    shape_name = shape_namebuf.buffer();
	}
	myShapes.append(new ROP_AbcGTShape(shape_name));
    }

    OObject		dad(parent);
    if (shapes.entries() > 1 && create_container)
    {
	myContainer = new OXform(parent, myName, ctx.timeSampling());
	dad = *myContainer;
    }
    for (exint i = 0; i < myShapes.entries(); ++i)
    {
	if (!myShapes(i)->firstFrame(shapes(i), dad, err, ctx))
	{
	    clear();
	    return false;
	}
    }
    return true;
}

bool
ROP_AbcGTCompoundShape::update(const GT_PrimitiveHandle &prim,
			GABC_OError &err,
			const ROP_AbcContext &ctx)
{
    // Refine the primitive into it's atomic shapes
    PrimitiveList	shapes;
    GT_RefineParms	rparms;

    initializeRefineParms(rparms, ctx, myPolysAsSubd, myShowUnusedPoints);
    abc_Refiner	refiner(shapes, &rparms);
    prim->refine(refiner, &rparms);

    if (shapes.entries() > myShapes.entries())
    {
	// TODO: Add new primitives
    }
    exint	num = SYSmin(myShapes.entries(), shapes.entries());
    for (exint i = 0; i < num; ++i)
    {
	if (!myShapes(i)->nextFrame(shapes(i), err, ctx))
	{
	    clear();
	    return false;
	}
    }
    return true;
}
