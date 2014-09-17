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
#include "ROP_AbcXform.h"
#include <GABC/GABC_OError.h>
#include <GABC/GABC_OGTGeometry.h>
#include <GABC/GABC_PackedImpl.h>
#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_Primitive.h>
#include <GT/GT_GEOPrimPacked.h>
#include <UT/UT_WorkArgs.h>
#include <UT/UT_WorkBuffer.h>

namespace
{
    typedef Alembic::Abc::OObject	        OObject;

    typedef GABC_NAMESPACE::GABC_OError         GABC_OError;
    typedef GABC_NAMESPACE::GABC_OGTGeometry    GABC_OGTGeometry;
    typedef GABC_NAMESPACE::GABC_PackedImpl     GABC_PackedImpl;

    typedef UT_Array<GT_PrimitiveHandle>        PrimitiveList;

    static OObject
    findRoot(const OObject *start)
    {
        // getParent is bitwise const, not logical const
        OObject current = *(const_cast<OObject *>(start));
        OObject parent = current.getParent();

        while (parent.valid())
        {
            current = parent;
            parent = current.getParent();
        }

        return current;
    }

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

    static bool
    isPackedAlembic(const GT_PrimitiveHandle &prim)
    {
        int ptype = prim->getPrimitiveType();

        if (ptype == GT_GEO_PACKED)
        {
            const GT_GEOPrimPacked     *gt;
            const GU_PrimPacked	       *gu;

            gt = UTverify_cast<const GT_GEOPrimPacked *>(prim.get());
            gu = gt->getPrim();

            if (gu->getTypeId() == GABC_PackedImpl::typeId())
            {
                return true;
            }
        }

        return false;
    }

    static bool
    isPacked(const GT_PrimitiveHandle &prim)
    {
	int ptype = prim->getPrimitiveType();

	if (ptype == GT_GEO_PACKED)
	{
            const GT_GEOPrimPacked     *gt;
            const GU_PrimPacked	       *gu;

            gt = UTverify_cast<const GT_GEOPrimPacked *>(prim.get());
            gu = gt->getPrim();

	    // We don't want to instance packed Alembics
	    if (gu->getTypeId() == GABC_PackedImpl::typeId())
	    {
		return false;
	    }
	}

	return ptype == GT_GEO_PACKED || ptype == GT_PRIM_INSTANCE;
    }

    class abc_Refiner : public GT_Refine
    {
    public:
	abc_Refiner(PrimitiveList &dfrm,
	        PrimitiveList &pckd,
                GABC_OError &err,
		const GT_RefineParms *parms,
		bool use_instancing)
	    : myErrorHandler(err)
	    , myDeforming(dfrm)
	    , myPacked(pckd)
	    , myParms(parms)
	    , myUseInstancing(use_instancing)
	{}

	// We need the primitives generated in a consistent order
	virtual bool	allowThreading() const	{ return false; }

	virtual void	addPrimitive(const GT_PrimitiveHandle &prim)
	{
	    if (!prim)
		return;

//            if (isPackedAlembic(prim))
//            {
//                myPacked.append(prim);
//                return;
//            }

            if ((myUseInstancing && isPacked(prim))
                    || ROP_AbcGTShape::isPrimitiveSupported(prim))
            {
                myDeforming.append(prim);
                return;
            }

	    // We hit a primitive we don't understand, so refine it
	    prim->refine(*this, myParms);
	}

    private:
        GABC_OError            &myErrorHandler;
	PrimitiveList          &myDeforming;
	PrimitiveList          &myPacked;
	const GT_RefineParms   *myParms;
	bool                    myUseInstancing;
    };
}

ROP_AbcGTCompoundShape::ROP_AbcGTCompoundShape(const std::string &name,
                ShapeSet * const shape_set,
                XformMap * const xform_map,
		bool has_path,
		bool polys_as_subd,
		bool show_unused_pts)
    : myShapeParent(NULL)
    , myContainer(NULL)
    , myShapeSet(shape_set)
    , myShapes()
    , myXformMap(xform_map)
    , myElapsedFrames(0)
    , myPolysAsSubd(polys_as_subd)
    , myShowUnusedPoints(show_unused_pts)
{
    if (has_path)
    {
        int pos = name.find_last_of('/');
        // The last '/' will never be in the first position.
        if (pos > 0)
        {
            myName = name.substr(pos + 1);
        }
        else
        {
            myName = name;
        }

        myPath = name;
    }
    else
    {
        myName = name;
    }
}

ROP_AbcGTCompoundShape::~ROP_AbcGTCompoundShape()
{
    clear();
}

void
ROP_AbcGTCompoundShape::clear()
{
    for (int i = 0; i < myShapes.entries(); ++i)
    {
	delete myShapes(i);
    }

    if (myContainer)
        delete myContainer;

    myShapeParent = NULL;
    myContainer = NULL;
    myShapes.setCapacity(0);

    myElapsedFrames = 0;
}

bool
ROP_AbcGTCompoundShape::first(const GT_PrimitiveHandle &prim,
			const OObject &parent,
			GABC_OError &err,
			const ROP_AbcContext &ctx,
			bool create_container,
                        ObjectVisibility vis)
{
    UT_ASSERT(prim);

    clear();

    // Refine the primitive into it's atomic shapes
    GT_RefineParms	rparms;
    PrimitiveList	deforming;
    PrimitiveList       packed;
    UT_WorkBuffer       shape_namebuf;
    std::string         shape_name;
    exint               num_dfrm;
    exint               num_pckd;

    initializeRefineParms(rparms, ctx, myPolysAsSubd, myShowUnusedPoints);

    if (ROP_AbcGTShape::isPrimitiveSupported(prim))
    {
	deforming.append(prim);
    }
    else
    {
	abc_Refiner refiner(deforming,
	        packed,
	        err,
	        &rparms,
	        ctx.useInstancing());
	prim->refine(refiner, &rparms);
    }

    myShapeParent = &parent;

    num_dfrm = deforming.entries();
    num_pckd = packed.entries();
    if (!num_dfrm && !num_pckd)
    {
        ++myElapsedFrames;
	return true;
    }

    if (myPath)
    {
        myRoot = findRoot(myShapeParent);
        myShapeParent = &myRoot;
    }
    else
    {
        if ((num_dfrm > 1) && create_container)
        {
            myContainer = new OXform(parent, myName, ctx.timeSampling());
            myShapeParent = myContainer;
        }
    }

    //
    //  START DEFORMING
    //

    shape_name = myName;
    for (exint i = 0; i < num_dfrm; ++i)
    {
	if (i > 0)
	{
	    shape_namebuf.sprintf("%s_%d", myName.c_str(), (int)i);
	    shape_name = shape_namebuf.buffer();
	}
	myShapes.append(new ROP_AbcGTShape(shape_name, myPath));
    }

    for (exint i = 0; i < num_dfrm; ++i)
    {
	if (!myShapes(i)->firstFrame(deforming(i),
                *myShapeParent,
                myShapeSet,
                myXformMap,
                err,
                ctx,
                vis,
                isPacked(deforming(i)),
                myPolysAsSubd,
                myShowUnusedPoints))
	{
	    clear();
	    return false;
	}
    }

    //
    //  END
    //

    ++myElapsedFrames;
    return true;
}

bool
ROP_AbcGTCompoundShape::update(const GT_PrimitiveHandle &prim,
			GABC_OError &err,
			const ROP_AbcContext &ctx)
{
    // Refine the primitive into it's atomic shapes
    GT_RefineParms	rparms;
    PrimitiveList	deforming;
    PrimitiveList       packed;
    UT_WorkBuffer       shape_namebuf;
    std::string         shape_name;
    exint               num_dfrm;
    exint               num_shapes;
    exint               p_pos;
    exint               s_pos;

    initializeRefineParms(rparms, ctx, myPolysAsSubd, myShowUnusedPoints);

    if (ROP_AbcGTShape::isPrimitiveSupported(prim))
    {
        deforming.append(prim);
    }
    else
    {
        abc_Refiner refiner(deforming,
                packed,
                err,
                &rparms,
                ctx.useInstancing());
        prim->refine(refiner, &rparms);
    }

    num_dfrm = deforming.entries();
    num_shapes = myShapes.entries();

    //
    // UPDATE DEFORMING
    //

    p_pos = 0;
    s_pos = 0;
    // Go through the list of existing shapes looking for one with
    // a primitive type matching the one of the current primitive.
    // When one is found, update the sample of the shape using the
    // current primitive then move on to the next one.
    //
    // There may be primitives further along in the list that match
    // the type of the shapes we skip over, but there is no elegant
    // way that I can currently think of to address this.
    while (p_pos < num_dfrm && s_pos < num_shapes)
    {
        int     prim_type = deforming(p_pos)->getPrimitiveType();
        int     shape_type = myShapes(s_pos)->getPrimitiveType();

        if (prim_type == shape_type)
        {
            // Redundant safety check?
            if (!myShapes(s_pos)->nextFrame(deforming(p_pos), err, ctx))
            {
                clear();
                return false;
            }

            ++s_pos;
            ++p_pos;
        }
        else
        {
            myShapes(s_pos)->nextFrameFromPrevious(err);
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
    while (s_pos < num_shapes)
    {
        myShapes(s_pos)->nextFrameFromPrevious(err);
        ++s_pos;
    }
    if (p_pos < num_dfrm)
    {
        shape_name = myName;
        for (exint i = p_pos; i < num_dfrm; ++i)
        {
            if (s_pos > 0)
            {
                shape_namebuf.sprintf("%s_%d", myName.c_str(), (int)s_pos);
                shape_name = shape_namebuf.buffer();
            }
            myShapes.append(new ROP_AbcGTShape(shape_name, myPath));

            if (!myShapes(s_pos)->firstFrame(deforming(i),
                    *myShapeParent,
                    myShapeSet,
                    myXformMap,
                    err,
                    ctx,
                    Alembic::AbcGeom::kVisibilityHidden,
                    isPacked(deforming(i)),
                    myPolysAsSubd,
                    myShowUnusedPoints))
            {
                clear();
                return false;
            }

            myShapes(s_pos)->nextFrameFromPrevious(err, myElapsedFrames - 1);
            myShapes(s_pos)->nextFrameFromPrevious(err, 1,
                    Alembic::AbcGeom::kVisibilityDeferred);

            ++s_pos;
        }
    }

    //
    //  END
    //

    ++myElapsedFrames;
    return true;
}

bool
ROP_AbcGTCompoundShape::updateFromPrevious(GABC_OError &err,
        exint frames,
        ObjectVisibility vis)
{
    if (frames < 0)
    {
        err.error("Attempted to update less than 0 frames.");
        return false;
    }

    for (int i = 0; i < myShapes.entries(); ++i)
    {
        if (!myShapes(i)->nextFrameFromPrevious(err, frames, vis))
        {
            return false;
        }
    }

    myElapsedFrames += frames;
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
