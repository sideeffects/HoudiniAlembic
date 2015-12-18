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

#include "ROP_AbcGTCompoundShape.h"
#include <GABC/GABC_OError.h>
#include <GABC/GABC_OGTGeometry.h>
#include <GABC/GABC_OXform.h>
#include <GABC/GABC_PackedImpl.h>
#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_Primitive.h>
#include <GT/GT_GEOPrimPacked.h>
#include <SOP/SOP_Node.h>
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

    static const GABC_PackedImpl *
    getGABCImpl(const GT_PrimitiveHandle &prim)
    {
        const GU_PrimPacked *gu =
		UTverify_cast<const GT_GEOPrimPacked *>(prim.get())->getPrim();

        return UTverify_cast<const GABC_PackedImpl *>(gu->implementation());
    }

    static void
    initializeRefineParms(GT_RefineParms &rparms,
            const ROP_AbcContext &ctx,
            bool polys_as_subd,
            bool show_unused_points)
    {
	rparms.setCoalesceFragments(false);
	rparms.setFastPolyCompacting(false);
	rparms.setFaceSetMode(ctx.faceSetMode());
	rparms.setPolysAsSubdivision(polys_as_subd);
	rparms.setShowUnusedPoints(show_unused_points);
    }

    static bool
    isPackedAlembic(const GT_PrimitiveHandle &prim)
    {
        int ptype = prim->getPrimitiveType();

        if (ptype == GT_GEO_PACKED)
        {
            const GU_PrimPacked *gu =
		UTverify_cast<const GT_GEOPrimPacked *>(prim.get())->getPrim();

            if (gu->getTypeId() == GABC_PackedImpl::typeId())
                return true;
        }

        return false;
    }

    static bool
    isPacked(const GT_PrimitiveHandle &prim)
    {
	int ptype = prim->getPrimitiveType();
	return ptype == GT_GEO_PACKED || ptype == GT_PRIM_INSTANCE;
    }

    class abc_Refiner : public GT_Refine
    {
    public:
	abc_Refiner(PrimitiveList &dfrm,
	        PrimitiveList &pckd,
		const GT_RefineParms *parms,
		bool use_instancing)
	    : myDeforming(dfrm)
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

            if (isPackedAlembic(prim))
            {
                myPacked.append(prim);
                return;
            }

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
	PrimitiveList          &myDeforming;
	PrimitiveList          &myPacked;
	const GT_RefineParms   *myParms;
	bool                    myUseInstancing;
    };
}

ROP_AbcGTCompoundShape::ROP_AbcGTCompoundShape(const std::string &identifier,
                InverseMap * const inv_map,
                GeoSet * const shape_set,
                XformMap * const xform_map,
		XformUserPropsMap *const user_prop_map,
		bool is_partition,
		bool polys_as_subd,
		bool show_unused_pts,
		bool geo_lock,
		const ROP_AbcContext &ctx)
    : myInverseMap(inv_map)
    , myGeoSet(shape_set)
    , myShapeParent(NULL)
    , myContainer(NULL)
    , myXformMap(xform_map)
    , myXformUserPropsMap(user_prop_map)
    , myElapsedFrames(0)
    , myNumShapes(0)
    , myGeoLock(geo_lock)
    , myPolysAsSubd(polys_as_subd)
    , myShowUnusedPoints(show_unused_pts)
{
    // If the shape has a path, extract the name
    if (is_partition && ctx.buildFromPath())
    {
        myPath = UT_DeepString(identifier);
        int pos = identifier.find_last_of('/');
        // The last '/' will never be in the first position.
        if (pos > 0)
            myName = identifier.substr(pos + 1);
        else
            myName = identifier;
    }
    else
        myName = identifier;
}

ROP_AbcGTCompoundShape::~ROP_AbcGTCompoundShape()
{
    clear();
}

void
ROP_AbcGTCompoundShape::clear()
{
    if (myContainer)
        delete myContainer;

    myShapeParent = NULL;
    myContainer = NULL;

    myDeforming.clear();
    myPacked.clear();

    myElapsedFrames = 0;
    myNumShapes = 0;
}

void
ROP_AbcGTCompoundShape::GTShapeList::dump(int indent) const
{
    printf("%*sShapeList[%d] = [\n", indent, "", (int)myShapes.entries());
    for (int i = 0; i < myShapes.entries(); ++i)
	myShapes(i)->dump(indent+2);
    printf("%*s]\n", indent, "");
}

void
ROP_AbcGTCompoundShape::dump(int indent) const
{
    if (myPacked.size())
    {
	printf("%*sCompound-PackedShapes = [\n", indent, "");
	for (auto it = myPacked.begin(); it != myPacked.end(); ++it)
	    it->second.dump(indent+2);
    }
    if (myDeforming.size())
    {
	printf("%*sCompound-DeformingShapes = [\n", indent, "");
	for (auto it = myDeforming.begin(); it != myDeforming.end(); ++it)
	    it->second.dump(indent+2);
    }
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
    PrimitiveList	deforming;
    PrimitiveList       packed;
    ROP_AbcGTShape     *shape;
    UT_WorkBuffer       shape_namebuf;

    if (ROP_AbcGTShape::isPrimitiveSupported(prim))
	deforming.append(prim);
    else
    {
	GT_RefineParms rparms;
	initializeRefineParms(rparms, ctx, myPolysAsSubd, myShowUnusedPoints);
	abc_Refiner refiner(deforming, packed, &rparms, ctx.useInstancing());
	prim->refine(refiner, &rparms);
    }

    myShapeParent = &parent;

    exint num_dfrm = deforming.entries();
    exint num_pckd = packed.entries();
    // Move on if there's no shapes this frame.
    if (!num_dfrm && !num_pckd)
    {
        ++myElapsedFrames;
	return true;
    }

    UT_WorkBuffer path;
    if(myPath)
	path.append(myPath);
    else if(myXformMap)
    {
	SOP_Node *sop = ctx.singletonSOP();
	if(sop && ctx.buildFromPath())
	{
	    // Primitive does not specify a path.
	    path.sprintf("%s/%s",
			 sop->getCreator()->getName().buffer(),
			 myName.c_str());
	}
    }

    // If we're using a path, the parent for the OObjects should be
    // the root node.
    if (path.length())
    {
        myRoot = findRoot(myShapeParent);
        myShapeParent = &myRoot;
    }
    else if ((num_dfrm > 1) && create_container)
    {
	myContainer = new OXform(parent, myName, ctx.timeSampling());
	myShapeParent = myContainer;
    }

    //
    //  START PACKED
    //
    if (num_pckd)
    {
        bool    calc_inverse = (ctx.packedAlembicPriority()
                        == ROP_AbcContext::PRIORITY_TRANSFORM);

        shape = new ROP_AbcGTShape(myName,
                path.buffer(),
                myInverseMap,
                myGeoSet,
                myXformMap,
		myXformUserPropsMap,
                ROP_AbcGTShape::ALEMBIC,
                myGeoLock);
        if (!shape->firstFrame(packed(0),
                *myShapeParent,
                vis,
                ctx,
                err,
                true,
                myPolysAsSubd,
                myShowUnusedPoints))
        {
            clear();
            return false;
        }

        myPacked.insert(getGABCImpl(packed(0))->getPropertiesHash(), shape);
        ++myNumShapes;

        for (exint i = 1; i < num_pckd; ++i)
        {
            shape_namebuf.sprintf("%s_%d", myName.c_str(), (int)myNumShapes);

            shape = new ROP_AbcGTShape(shape_namebuf.buffer(),
                    path.buffer(),
                    myInverseMap,
                    myGeoSet,
                    myXformMap,
		    myXformUserPropsMap,
                    ROP_AbcGTShape::ALEMBIC,
                    myGeoLock);
            if (!shape->firstFrame(packed(i),
                    *myShapeParent,
                    vis,
                    ctx,
                    err,
                    calc_inverse,
                    myPolysAsSubd,
                    myShowUnusedPoints))
            {
                clear();
                return false;
            }

            myPacked.insert(getGABCImpl(packed(i))->getPropertiesHash(), shape);
            ++myNumShapes;
        }
    }
    //
    //  START DEFORMING
    //
    else
    {
        shape = new ROP_AbcGTShape(myName,
                path.buffer(),
                myInverseMap,
                myGeoSet,
                myXformMap,
		myXformUserPropsMap,
                isPacked(deforming(0)) ? ROP_AbcGTShape::INSTANCE
                        : ROP_AbcGTShape::GEOMETRY,
                myGeoLock);
        if (!shape->firstFrame(deforming(0),
                *myShapeParent,
                vis,
                ctx,
                err,
                false,
                myPolysAsSubd,
                myShowUnusedPoints))
        {
            clear();
            return false;
        }

        myDeforming.insert(deforming(0)->getPrimitiveType(), shape);
        ++myNumShapes;
    }

    for (exint i = (num_pckd ? 0 : 1); i < num_dfrm; ++i)
    {
        shape_namebuf.sprintf("%s_%d", myName.c_str(), (int)myNumShapes);

        shape = new ROP_AbcGTShape(shape_namebuf.buffer(),
                path.buffer(),
                myInverseMap,
                myGeoSet,
                myXformMap,
		myXformUserPropsMap,
                isPacked(deforming(i)) ? ROP_AbcGTShape::INSTANCE
                        : ROP_AbcGTShape::GEOMETRY,
                myGeoLock);
        if (!shape->firstFrame(deforming(i),
                *myShapeParent,
                vis,
                ctx,
                err,
                false,
                myPolysAsSubd,
                myShowUnusedPoints))
        {
            clear();
            return false;
        }

        myDeforming.insert(deforming(i)->getPrimitiveType(), shape);
        ++myNumShapes;
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
    PrimitiveList	deforming;
    PrimitiveList       packed;
    ROP_AbcGTShape     *shape;
    UT_WorkBuffer       shape_namebuf;
    bool                calc_inverse;

    if (ROP_AbcGTShape::isPrimitiveSupported(prim))
        deforming.append(prim);
    else
    {
	GT_RefineParms rparms;
	initializeRefineParms(rparms, ctx, myPolysAsSubd, myShowUnusedPoints);
        abc_Refiner refiner(deforming, packed, &rparms, ctx.useInstancing());
        prim->refine(refiner, &rparms);
    }

    exint num_dfrm = deforming.entries();
    exint num_pckd = packed.entries();

    // Packed Alembics and deforming geometry are updated in the same way.
    // Try to read the next shape in the GTShape list for their object
    // path/primitive type. If one exists, update it for the current frame.
    //
    // If not, create a new GTShape. Write the current info out for the
    // first frame and mark it as hidden. Copy the data as hidden up to the
    // current frame, then copy it again for the current frame but mark it
    // as visible.
    //
    // Lastly, go through and update any shapes we did not encounter this
    // frame as hidden.
    //
    // Because there is no unique info to differentiate between packed Alembic
    // copies, or between different deforming geometry of the same
    // primitive type, if their visibility values are animated, then data
    // for later primitives may fall through to earlier ones. As of now, there
    // is no way to deal with this. Luckily, this edge case shouldn't
    // happen often.

    //
    //  UPDATE PACKED
    //

    myPacked.reset();
    for (exint i = 0; i < num_pckd; ++i)
    {
        calc_inverse = ((i == 0)
                || (ctx.packedAlembicPriority()
                        == ROP_AbcContext::PRIORITY_TRANSFORM));
        shape = myPacked.getNext(getGABCImpl(packed(i))->getPropertiesHash());

        if (shape)
        {
            if (!shape->nextFrame(packed(i), ctx, err, calc_inverse))
            {
                clear();
                return false;
            }
        }
        else
        {
            shape_namebuf.sprintf("%s_%d", myName.c_str(), (int)myNumShapes);
            ++myNumShapes;

            shape = new ROP_AbcGTShape(shape_namebuf.buffer(),
                    myPath,
                    myInverseMap,
                    myGeoSet,
                    myXformMap,
		    myXformUserPropsMap,
                    ROP_AbcGTShape::ALEMBIC,
                    myGeoLock);

            if (!shape->firstFrame(packed(i),
                    *myShapeParent,
                    Alembic::AbcGeom::kVisibilityHidden,
                    ctx,
                    err,
                    calc_inverse,
                    myPolysAsSubd,
                    myShowUnusedPoints))
            {
                clear();
                return false;
            }
            shape->nextFrameFromPrevious(err,
                    Alembic::AbcGeom::kVisibilityHidden,
                    myElapsedFrames - 1);
            shape->nextFrameFromPrevious(err,
                    Alembic::AbcGeom::kVisibilityDeferred);

            myPacked.insert(getGABCImpl(packed(i))->getPropertiesHash(), shape);
        }
    }
    myPacked.updateHidden(err);

    //
    //  UPDATE DEFORMING
    //

    calc_inverse = false;
    myDeforming.reset();
    for (exint i = 0; i < num_dfrm; ++i)
    {
        shape = myDeforming.getNext(deforming(i)->getPrimitiveType());

        if (shape)
        {
            if (!shape->nextFrame(deforming(i), ctx, err, calc_inverse))
            {
                clear();
                return false;
            }
        }
        else
        {
            shape_namebuf.sprintf("%s_%d", myName.c_str(), (int)myNumShapes);
            ++myNumShapes;

            shape = new ROP_AbcGTShape(shape_namebuf.buffer(),
                    myPath,
                    myInverseMap,
                    myGeoSet,
                    myXformMap,
		    myXformUserPropsMap,
                    isPacked(deforming(i)) ? ROP_AbcGTShape::INSTANCE
                            : ROP_AbcGTShape::GEOMETRY,
                    myGeoLock);

            if (!shape->firstFrame(deforming(i),
                    *myShapeParent,
                    Alembic::AbcGeom::kVisibilityHidden,
                    ctx,
                    err,
                    calc_inverse,
                    myPolysAsSubd,
                    myShowUnusedPoints))
            {
                clear();
                return false;
            }
            shape->nextFrameFromPrevious(err,
                    Alembic::AbcGeom::kVisibilityHidden,
                    myElapsedFrames - 1);
            shape->nextFrameFromPrevious(err,
                    Alembic::AbcGeom::kVisibilityDeferred);

            myDeforming.insert(deforming(i)->getPrimitiveType(), shape);
        }
    }
    myDeforming.updateHidden(err);

    //
    //  END
    //

    ++myElapsedFrames;
    return true;
}

bool
ROP_AbcGTCompoundShape::updateFromPrevious(GABC_OError &err,
        ObjectVisibility vis,
        exint frames)
{
    if (frames < 0)
    {
        UT_ASSERT(0 && "Attempted to update less than 0 frames.");
        return false;
    }

    myPacked.reset();
    myPacked.updateFromPrevious(err, vis, frames);
    myDeforming.reset();
    myDeforming.updateFromPrevious(err, vis, frames);

    myElapsedFrames += frames;
    return true;
}

Alembic::Abc::OObject
ROP_AbcGTCompoundShape::getShape()
{
    if (myContainer)
	return *myContainer;

    UT_ASSERT(myNumShapes == 1);
    return myDeforming.getFirst()->getOObject();
}
