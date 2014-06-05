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

#include "ROP_AbcGTCompoundShape.h"
#include <UT/UT_WorkBuffer.h>
#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_Primitive.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GABC/GABC_PackedImpl.h>

namespace
{
    typedef UT_Array<GT_PrimitiveHandle>	PrimitiveList;

    static bool
    shouldInstance(const GT_PrimitiveHandle &prim)
    {
	int	ptype = prim->getPrimitiveType();
	if (ptype == GT_GEO_PACKED)
	{
	    const GT_GEOPrimPacked	*gt;
	    gt = UTverify_cast<const GT_GEOPrimPacked *>(prim.get());
	    const GU_PrimPacked	*gu = gt->getPrim();

	    // We don't want to instance a single Alembic shape
	    if (gu->getTypeId() == GABC_NAMESPACE::GABC_PackedImpl::typeId())
	    {
		return false;
	    }
	}
	return ptype == GT_GEO_PACKED || ptype == GT_PRIM_INSTANCE;
    }

    class abc_Refiner : public GT_Refine
    {
    public:
	abc_Refiner(PrimitiveList &list,
		const GT_RefineParms *parms,
		bool use_instancing)
	    : myList(list)
	    , myParms(parms)
	    , myUseInstancing(use_instancing)
	{
	}
	// We need the primitives generated in a consistent order
	virtual bool	allowThreading() const	{ return false; }
	virtual void	addPrimitive(const GT_PrimitiveHandle &prim)
	{
	    if (!prim)
		return;
	    bool	ok = false;
	    if (myUseInstancing && shouldInstance(prim))
	    {
		ok = true;
	    }
	    else
		ok = ROP_AbcGTShape::isPrimitiveSupported(prim);
	    if (ok)
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
	bool			 myUseInstancing;
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
    , myElapsedFrames(0)
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
    myShapes.setCapacity(0);
}

bool
ROP_AbcGTCompoundShape::shapeStart(ROP_AbcGTShape *shape,
        GT_PrimitiveHandle prim,
        GABC_OError &err,
        const ROP_AbcContext &ctx,
        ObjectVisibility vis)
{
    bool    ok = true;

    if (shouldInstance(prim))
    {
        UT_ASSERT(ctx.useInstancing());
        ok = shape->firstInstance(prim, *myShapeParent, err, ctx,
                    myPolysAsSubd, myShowUnusedPoints, vis);
    }
    else
    {
        ok = shape->firstFrame(prim, *myShapeParent, err, ctx, vis);
    }

    return ok;
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
    PrimitiveList	prims;
    GT_RefineParms	rparms;

    initializeRefineParms(rparms, ctx, myPolysAsSubd, myShowUnusedPoints);
    if (ROP_AbcGTShape::isPrimitiveSupported(prim))
	prims.append(prim);
    else
    {
	abc_Refiner refiner(prims, &rparms, ctx.useInstancing());
	prim->refine(refiner, &rparms);
    }

    ++myElapsedFrames;
    myShapeParent = &parent;

    exint       prim_entries = prims.entries();
    if (!prim_entries)
	return true;

    if (prim_entries > 1 && create_container)
    {
	myContainer = new OXform(parent, myName, ctx.timeSampling());
	myShapeParent = myContainer;
    }

    std::string		shape_name = myName;
    UT_WorkBuffer	shape_namebuf;
    for (exint i = 0; i < prim_entries; ++i)
    {
	if (i > 0)
	{
	    shape_namebuf.sprintf("%s_%d", myName.c_str(), (int)i);
	    shape_name = shape_namebuf.buffer();
	}
	myShapes.append(new ROP_AbcGTShape(shape_name));
    }

    for (exint i = 0; i < myShapes.entries(); ++i)
    {
	if (!shapeStart(myShapes(i),
	        prims(i),
	        err,
	        ctx,
	        Alembic::AbcGeom::kVisibilityDeferred))
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
    PrimitiveList	prims;
    GT_RefineParms	rparms;

    initializeRefineParms(rparms, ctx, myPolysAsSubd, myShowUnusedPoints);
    if (ROP_AbcGTShape::isPrimitiveSupported(prim))
	prims.append(prim);
    else
    {
	abc_Refiner	refiner(prims, &rparms, ctx.useInstancing());
	prim->refine(refiner, &rparms);
    }

    exint       shape_entries = myShapes.entries();
    exint       prim_entries = prims.entries();
    exint       s_pos = 0;
    exint       p_pos = 0;

    // Go through the list of existing shapes looking for one with
    // a primitive type matching the one of the current primitive.
    // When one is found, update the sample of the shape using the
    // current primitive then move on to the next one.
    //
    // There may be primitives further along in the list that match
    // the type of the shapes we skip over, but there is no elegant
    // way that I can currently think of to address this.
    while (p_pos < prim_entries && s_pos < shape_entries)
    {
        int         prim_type = prims(p_pos)->getPrimitiveType();
        int         shape_type = myShapes(s_pos)->getPrimitiveType();

        if (prim_type == shape_type)
        {
            // Redundant safety check?
            if (!myShapes(s_pos)->nextFrame(prims(p_pos), err, ctx))
            {
                clear();
                return false;
            }

            ++s_pos;
            ++p_pos;
        }
        else
        {
            myShapes(s_pos)->nextFrameHidden(err);
            ++s_pos;
        }
    }
    // The lists of current primitives and existing shapes might not
    // match up nicely. If we've updated using all of the primitives,
    // mark the remaining shapes as hidden.
    //
    // If we checked all of the existing shapes and still have
    // primitives remaining, create new shapes to house them.
    //
    // At most only one of the two blocks below should be called.
    while (s_pos < shape_entries)
    {
        myShapes(s_pos)->nextFrameHidden(err);
        ++s_pos;
    }
    if (p_pos < prim_entries)
    {
        std::string     shape_name = myName;
        UT_WorkBuffer   shape_namebuf;

        for (exint i = p_pos; i < prim_entries; ++i)
        {
            if (s_pos > 0)
            {
                shape_namebuf.sprintf("%s_%d", myName.c_str(), (int)s_pos);
                shape_name = shape_namebuf.buffer();
            }
            myShapes.append(new ROP_AbcGTShape(shape_name));

            if (!shapeStart(myShapes(s_pos),
                    prims(i),
                    err,
                    ctx,
                    Alembic::AbcGeom::kVisibilityHidden))
            {
                clear();
                return false;
            }

            myShapes(s_pos)->nextFrameHidden(err, (myElapsedFrames - 1));
            myShapes(s_pos)->nextFrame(prims(i), err, ctx);

            ++s_pos;
        }
    }

    ++myElapsedFrames;
    return true;
}

Alembic::Abc::OObject
ROP_AbcGTCompoundShape::getShape() const
{
    if (myContainer)
	return *myContainer;
    UT_ASSERT(myShapes.entries() == 1);
    return myShapes(0)->getOObject();
}
