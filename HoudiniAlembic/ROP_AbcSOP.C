/*
 * Copyright (c) 2016
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

#include "ROP_AbcSOP.h"
#include "ROP_AbcGTCompoundShape.h"
#include "ROP_AbcGTShape.h"
#include <GABC/GABC_OError.h>
#include <GABC/GABC_PackedImpl.h>
#include <GT/GT_GEODetail.h>
#include <GT/GT_Primitive.h>
#include <SOP/SOP_Node.h>

using namespace GABC_NAMESPACE;

namespace
{
    typedef Alembic::AbcGeom::ObjectVisibility	ObjectVisibility;

    typedef ROP_AbcSOP::abc_PrimContainer       abc_PrimContainer;
    typedef ROP_AbcSOP::NameList                NameList;
    typedef ROP_AbcSOP::PartitionMap            PartitionMap;
    typedef ROP_AbcSOP::PartitionMapInsert      PartitionMapInsert;
    typedef ROP_AbcSOP::PrimitiveList           PrimitiveList;

    static void
    buildGeometry(PrimitiveList &primitives,
	    const GU_ConstDetailHandle &gdh,
	    const GU_Detail &gdp,
	    const GA_Range &range,
            const std::string &identifier,
            bool is_partition,
            bool check_alembic,
	    bool force_subd_mode,
	    bool show_pts)
    {
	/// Since there can be all kinds of primitives we don't understand
	/// (i.e. all custom ones, Tetra, etc.), we build a GT primitive for the
	/// detail.  We can refine this into simpler primitives until we *do*
	/// understand them.
	GT_PrimitiveHandle	detail = GT_GEODetail::makeDetail(gdh, &range);
	if (detail)
	{
	    const std::string  *id_ptr = &identifier;
	    std::string         subd_id;
            bool                has_alembic = false;

	    if (check_alembic)
	    {
                // Check the range for packed Alembic primitives.
                // This is used for sorting primitives; partitions with
                // packed Alembics must come first.
		if (gdp.containsPrimitiveType(GABC_PackedImpl::typeId()))
		{
		    // We can do a quick check to see if the range is equal to
		    // the full range.
		    if (range.getRTI() == gdp.getPrimitiveRange().getRTI())
		    {
			has_alembic = true;
		    }
		    else
		    {
			// We have 
			for (GA_Iterator it(range); !it.atEnd(); ++it)
			{
			    if (gdp.getPrimitive(*it)->getTypeId()
				    == GABC_PackedImpl::typeId())
			    {
				has_alembic = true;
				break;
			    }
			}
		    }
		}
            }

            if (force_subd_mode && !show_pts)
            {
                subd_id.append(identifier);
                subd_id.append("_subd");
                id_ptr = &subd_id;
            }

	    primitives.append(abc_PrimContainer(detail,
	            *id_ptr,
	            has_alembic,
	            is_partition,
	            force_subd_mode,
	            (show_pts && !is_partition))); // Add free points to the base partition
	}
    }

    static bool
    isToggleEnabled(OP_Node *node, const char *name,
	    const ROP_AbcContext &ctx, bool def)
    {
	int	value;
	if (node->evalParameterOrProperty(name, 0, ctx.cookTime(), value))
	    return value != 0;
	return def;
    }

    static bool
    objectSubd(const SOP_Node *sop, const ROP_AbcContext &ctx,
	    UT_String &groupname)
    {
	// If the user has specified a subd group on the output driver, any
	// polygons in the subd group will be output as OSubD.  All others will
	// be output as OPolyMesh.
	groupname = ctx.subdGroup();
	if (groupname.isstring())
	    return true;

	// If the user didn't specify a subd group on the output driver, use
	// the object settings instead.  If the vm_rendersubd or ri_rendersubd
	// toggles are turned on, we will render the object as subdivision
	// surfaces.
	OP_Network	*obj = sop->getCreator();
	if (!isToggleEnabled(obj, "vm_rendersubd", ctx, false) &&
		!isToggleEnabled(obj, "ri_rendersubd", ctx, false))
	{
	    return false;
	}
	// However, if the user has specified a special group for subd
	// surfaces, we only render primitives in that group as subds, while
	// all others are rendered as polygons.
	if (!obj->evalParameterOrProperty("vm_subdgroup", 0, 
		    ctx.cookTime(), groupname))
	{
	    groupname = "";
	}
	return true;
    }

    static const char *
    homogenizePath(const char *path, UT_WorkBuffer storage, bool &flag)
    {
        const char *pos1 = path;

        flag = false;
        storage.clear();

        while (true)
        {
            bool slash = false;
            while (*pos1 == '/')
            {
                // If flag has been marked true before, it will be true from
                // now on. A single slash will trip slash, then a second
                // will trip flag.
                flag = flag || slash;
                slash = true;

                ++pos1;
            }
            if (*pos1 == 0)
                break;

	    const char *pos2 = strchr(pos1, '/');
            if (!pos2)
            {
                storage.append(pos1);
                break;
            }

            storage.append(pos1, pos2 - pos1 + 1);
            pos1 = pos2;
        }

        return storage.buffer();
    }

    // Sort primitives using strcmp semantics.
    //
    // When placing a packed Alembic directly using a path, the first packed
    // Alembic has its transform matrix merged with its parents' transform
    // matrix. This is because in most cases, we only have one shape underneath
    // a transform. By merging the matrices, we improve the speed and
    // archive file size.
    //
    // However, there may be multiple shapes under a single transform. In
    // addition, these shapes may be a mix of packed Alembics and deforming
    // geometry. In this case, we need to apply an inverse transform to the
    // deforming geometry and the parent transforms of additional packed
    // Alembics.
    //
    // Thus, partitions with shorter paths must be processed first so that
    // the parent transform matrices are available to their children for
    // computing the inverse. In addition, partitions with packed Alembics
    // must come before partitions without any, so that the inverse of the
    // first packed Alembics transform is available when we write deforming
    // geometry.
    static int
    comparePrims(const abc_PrimContainer *a, const abc_PrimContainer *b)
    {
        // Partitions with the shorter path come first.
        UT_String       str_a(a->myIdentifier);
        UT_String       str_b(b->myIdentifier);
        UT_WorkArgs     tokens_a, tokens_b;

        str_a.tokenize(tokens_a, '/');
        str_b.tokenize(tokens_b, '/');

        if (tokens_a.entries() < tokens_b.entries())
            return -1;
        if (tokens_a.entries() > tokens_b.entries())
            return 1;

        // If same partitions have same path length, then partitions with
        // packed Alembics come first
        if (a->myHasAlembic && !b->myHasAlembic)
            return -1;
        if (!a->myHasAlembic && b->myHasAlembic)
            return 1;

        // Same path length, both/neither have Alembics, sort alphabetically.
        // This creates consistent order of shapes on all platforms.
        return strcmp(a->myIdentifier.c_str(), b->myIdentifier.c_str());
    }

    SOP_Node *
    getSop(int id)
    {
	OP_Node	*node = OP_Node::lookupNode(id);
	return UTverify_cast<SOP_Node *>(node);
    }
}

ROP_AbcSOP::ROP_AbcSOP(SOP_Node *node)
    : myElapsedFrames(0)
    , myGeoLock(0)
    , mySopId(node ? node->getUniqueId() : -1)
    , myPathAttribName()
    , myTimeDependent(false)
{
}

ROP_AbcSOP::~ROP_AbcSOP()
{
    clear();
}

void
ROP_AbcSOP::clear()
{
    for (int i = 0; i < myShapes.entries(); ++i)
	delete myShapes(i);
    myShapes.setCapacity(0);

    for (auto it = myXformMap.begin(); it != myXformMap.end(); ++it)
        delete it->second;
    myXformMap.clear();
    for (auto it = myXformUserPropsMap.begin(); 
    	      it != myXformUserPropsMap.end(); 
    	      ++it)
    {
        delete it->second;
    }
    myXformUserPropsMap.clear();

    myInverseMap.clear();
    myGeoSet.clear();
    myNameMap.clear();
    myPartitionIndices.clear();
    myPartitionMap.clear();
    myPartitionNames.clear();

    if (myPathAttribName.isstring())
        GABC_OGTGeometry::getDefaultSkip().deleteSkip(myPathAttribName);
}

bool
ROP_AbcSOP::start(const OObject &parent,
        GABC_OError &err,
        const ROP_AbcContext &ctx,
        UT_BoundingBox &box)
{
    SOP_Node *sop = getSop(mySopId);
    if (!sop)
    {
        clear();
        return err.error("Unable to find SOP: %d", mySopId);
    }

    sop->getParent()->evalParameterOrProperty(
            GABC_Util::theLockGeometryParameter, 0, 0, myGeoLock);

    GU_ConstDetailHandle	gdh(sop->getCookedGeoHandle(ctx.cookContext()));
    GU_DetailHandleAutoReadLock rlock(gdh);
    const GU_Detail		*gdp = rlock.getGdp();

    if (!gdp)
    {
        clear();
        UT_WorkBuffer	path;
        sop->getFullPath(path);
        return err.error("Error saving first frame: %s", path.buffer());
    }

    myParent = parent;
    myTimeDependent = sop->isTimeDependent(ctx.cookContext());

    if (ctx.fullBounds())
    {
        gdp->computeQuickBounds(myBox);
        box = myBox;
    }

    bool part_by_name = ctx.partitionByName();
    bool part_by_path = ctx.buildFromPath();
    if (part_by_path)
    {
        myPathAttribName = ctx.pathAttribute();
        GABC_OGTGeometry::getDefaultSkip().addSkip(myPathAttribName);
    }

    myPartitionIndices.append(GA_OffsetList());
    myPartitionNames.push_back(getName());

    PrimitiveList prims;
    partitionGeometry(prims, sop, gdh, *gdp, ctx, err);
    if (part_by_path)
        prims.sort(comparePrims);

    for (int i = 0; i < prims.entries(); ++i)
    {
        // Create new compound shapes
	ROP_AbcGTCompoundShape *shape = newShape(prims(i), ctx, err);
        if (!shape)
        {
            clear();
            UT_WorkBuffer   path;
            sop->getFullPath(path);
            return err.error("Error saving first frame: %s", path.buffer());
        }
        myShapes.append(shape);

        // Map partition names to specific compound shapes
        if (part_by_path || part_by_name)
            myNameMap.insert(NameMapInsert(prims(i).myIdentifier, i));
    }

    ++myElapsedFrames;

    // Update any OXform objects that were created and set for this
    // frame but not written out.
    if (part_by_path)
    {
        for (auto it = myXformMap.begin(); it != myXformMap.end(); ++it)
        {
            GABC_OXformSchema  &schema = it->second->getSchema();

            if (schema.getNumSamples() < myElapsedFrames)
                schema.finalize();
        }
        myInverseMap.clear();
    }

    return true;
}

bool
ROP_AbcSOP::update(GABC_OError &err,
	const ROP_AbcContext &ctx,
	UT_BoundingBox &box)
{
    SOP_Node *sop = getSop(mySopId);
    if (!sop)
    {
        clear();
        return err.error("Unable to find SOP: %d", mySopId);
    }

    GU_ConstDetailHandle	gdh(sop->getCookedGeoHandle(ctx.cookContext()));
    GU_DetailHandleAutoReadLock rlock(gdh);
    const GU_Detail		*gdp = rlock.getGdp();
    if (!gdp)
    {
        clear();
        UT_WorkBuffer	path;
        sop->getFullPath(path);
        return err.error("Error saving first frame: %s", path.buffer());
    }

    if (ctx.fullBounds())
    {
	gdp->computeQuickBounds(myBox);
	box = myBox;
    }

    PrimitiveList prims;
    partitionGeometry(prims, sop, gdh, *gdp, ctx, err);
    if (ctx.buildFromPath())
        prims.sort(comparePrims);

    bool use_name_map = ctx.partitionByName() || ctx.buildFromPath();

    for (int i = 0; i < prims.entries(); ++i)
    {
        // Use myNameMap if we're partitioning the data.
        if (!myNameMap.empty() || (use_name_map && !myShapes.entries()))
        {
            auto it = myNameMap.find(prims(i).myIdentifier);

            // Create a new compound shape if one does not exist
            // for the current partition.
            if (it == myNameMap.end())
            {
		ROP_AbcGTCompoundShape *shape = newShape(prims(i), ctx, err,
				Alembic::AbcGeom::kVisibilityHidden);
                // Write out the first frame with hidden visibility
                if (!shape)
                {
                    clear();
                    UT_WorkBuffer   path;
                    sop->getFullPath(path);
                    return err.error("Error saving next frame: %s", path.buffer());
                }
                // Copy the data up to the current frame, still hidden
                shape->updateFromPrevious(err,
                        Alembic::AbcGeom::kVisibilityHidden,
                        myElapsedFrames - 1);
                // Copy the data for the current frame, this time visible
                shape->updateFromPrevious(err,
                        Alembic::AbcGeom::kVisibilityDeferred);

                // Add the partition to the list and the map
                myShapes.append(shape);
                myNameMap.insert(NameMapInsert(prims(i).myIdentifier,
                        (myShapes.entries() - 1)));
            }
            // Otherwise, just update the existing shape
            else
            {
                if (!myShapes(it->second)->update(prims(i).myPrim, err, ctx))
                {
                    clear();
                    UT_WorkBuffer   path;
                    sop->getFullPath(path);
                    return err.error("Error saving next frame: %s", path.buffer());
                }
            }
        }
        // If we're not using partitions, just update the existing shapes
        // in the same order they were written in the first frame.
        else
        {
	    // Create primitives for missing shapes
	    for (exint j = myShapes.entries(); j < prims.entries(); ++j)
	    {
		// TODO: It isn't clear whether the primitive will have a
		// unique identifier.
		ROP_AbcGTCompoundShape	*shape = newShape(prims(i), ctx, err,
				Alembic::AbcGeom::kVisibilityHidden);
		if (!shape)
		{
                    clear();
                    UT_WorkBuffer   path;
                    sop->getFullPath(path);
                    return err.error("Error saving next frame: %s", path.buffer());
		}
		// Update the shape as hidden (up to our current frame)
		shape->updateFromPrevious(err,
			Alembic::AbcGeom::kVisibilityHidden,
			myElapsedFrames - 1);
		// Mark the shape as visible this frame
		shape->updateFromPrevious(err,
			Alembic::AbcGeom::kVisibilityDeferred);
		myShapes.append(shape);
	    }
            // The number of existing shapes should match the number of existing
            // primitives always
            UT_ASSERT(myShapes.entries() >= prims.entries());

            for (exint j = 0; j < myShapes.entries(); ++j)
            {
		if (j > prims.entries())
		{
		    // The primitive has disappeared, so we need to mark this
		    // shape as hidden.
		    myShapes(i)->updateFromPrevious(err,
			    Alembic::AbcGeom::kVisibilityHidden);
		}
		else if (!myShapes(i)->update(prims(i).myPrim, err, ctx))
                {
                    clear();
                    UT_WorkBuffer   path;
                    sop->getFullPath(path);
                    return err.error("Error saving next frame: %s", path.buffer());
                }
            }
        }
    }

    // Update all hidden shapes
    for (int i = 0; i < myShapes.entries(); ++i)
    {
        if (myShapes(i)->getElapsedFrames() == myElapsedFrames)
            myShapes(i)->updateFromPrevious(err);
    }

    ++myElapsedFrames;

    // Update any OXform objects that were created and set for this
    // frame but not written out.
    if (ctx.buildFromPath())
    {
        for (auto it = myXformMap.begin(); it != myXformMap.end(); ++it)
        {
            GABC_OXformSchema  &schema = it->second->getSchema();

            if (schema.getNumSamples() < myElapsedFrames)
                schema.finalize();
        }
        myInverseMap.clear();
    }

    return true;
}

bool
ROP_AbcSOP::selfTimeDependent() const
{
    return myTimeDependent;
}

bool
ROP_AbcSOP::getLastBounds(UT_BoundingBox &box) const
{
    box = myBox;
    return true;
}

ROP_AbcGTCompoundShape *
ROP_AbcSOP::newShape(const abc_PrimContainer &prim,
        const ROP_AbcContext &ctx,
        GABC_OError &err,
	Alembic::AbcGeom::ObjectVisibility vis)
{
    ROP_AbcGTCompoundShape *shape =
	new ROP_AbcGTCompoundShape(prim.myIdentifier,
	    &myInverseMap,
	    &myGeoSet,
	    &myXformMap,
	    &myXformUserPropsMap,
	    prim.myIsPartition,
	    prim.mySubdMode,
	    prim.myShowPts,
	    (myGeoLock == 1),
	    ctx);

    // Write out the first frame with hidden visibility
    if (!shape->first(prim.myPrim,
	    myParent,
	    err,
	    ctx,
	    false,
	    vis))
    {
	delete shape;
	return 0;
    }
    return shape;
}

void
ROP_AbcSOP::partitionGeometryRange(PrimitiveList &primitives,
        const GU_ConstDetailHandle &gdh,
	const GU_Detail &gdp,
        const GA_Range &range,
        const ROP_AbcContext &ctx,
        GABC_OError &err,
        bool force_subd_mode,
        bool show_pts)
{
    bool flag = false;

    bool part_by_path = ctx.buildFromPath();
    const char *aname =
		part_by_path ? ctx.pathAttribute() : ctx.partitionAttribute();
    GA_ROHandleS str =
		GA_ROHandleS(gdp.findStringTuple(GA_ATTRIB_PRIMITIVE, aname));
    if (!str.isValid() || !str.getAttribute()->getAIFSharedStringTuple())
    {
        buildGeometry(primitives, gdh, gdp, range, getName(), false, false,
		      force_subd_mode, show_pts);
        return;
    }

    for (GA_Iterator it(range); !it.atEnd(); ++it)
    {
        if (str.getIndex(*it) < 0)
        {
            myPartitionIndices(0).append(*it);
        }
        // Originally mapped string index to PrimitiveList, but the string
        // table is not constant. Thus, need to map processed strings to
        // PrimitiveList.
        else
        {
	    UT_WorkBuffer namebuf;

            // Read and process the path string.
	    const char *strval = str.get(*it);
            strval = part_by_path
                    ? homogenizePath(strval, namebuf, flag)
                    : ctx.partitionModeValue(strval, namebuf);

            // Report warnings if the path string was invalid or odd
            // (multiple consecutive '/'s)
            if (*strval == 0)
            {
                err.warning("Invalid %s attribute value for primitive "
                        "%" SYS_PRId64 " was ignored.",
                        aname,
                        ((int64)(*it) - 1));

                myPartitionIndices(0).append(*it);
                continue;
            }
            else if (part_by_path && flag)
            {
                err.warning("%s attribute value for primitive %" SYS_PRId64
                        " has odd value. Value interpreted as %s.",
                        aname,
                        ((int64)(*it) - 1),
                        strval);
            }

            auto iter2 = myPartitionMap.find(strval);
	    int pos;

            // If we've seen this string before, fetch the position of the
            // corresponding list of primitives.
            if (iter2 != myPartitionMap.end())
            {
                pos = iter2->second;
            }
            // Otherwise: create a new list of primitives, record its
            //            index, and store the partition identifier
            else
            {
                pos = myPartitionIndices.entries();
                myPartitionIndices.append(GA_OffsetList());
                myPartitionNames.push_back(strval);

                myPartitionMap.insert(PartitionMapInsert(strval, pos));
            }

            myPartitionIndices(pos).append(*it);
        }
    }

    // Group the primitives for each partition into a GT_Primitive
    for (exint i = 0; i < myPartitionIndices.entries(); ++i)
    {
        if (myPartitionIndices(i).entries())
        {
            GA_Range    range(gdp.getPrimitiveMap(), myPartitionIndices(i));

            buildGeometry(primitives, gdh, gdp, range, myPartitionNames[i],
			  (i != 0), part_by_path, force_subd_mode, show_pts);

            myPartitionIndices(i).clear();
        }
    }
}

void
ROP_AbcSOP::dump(int indent) const
{
    ROP_AbcObject::dump(indent);
    printf("%*sshapes = {\n", indent, "");
    for (int i = 0; i < myShapes.entries(); ++i)
	myShapes(i)->dump(indent+2);
    printf("%*s}\n", indent, "");
}

void
ROP_AbcSOP::partitionGeometry(PrimitiveList &primitives,
        const SOP_Node *sop,
	const GU_ConstDetailHandle &gdh,
        const GU_Detail &gdp,
        const ROP_AbcContext &ctx,
        GABC_OError &err)
{
    UT_String subdgroupname;
    if (!objectSubd(sop, ctx, subdgroupname))
    {
        // No subdivision primitives
        partitionGeometryRange(primitives, gdh, gdp, gdp.getPrimitiveRange(), ctx,
			       err, false, true);
	return;
    }

    if (!subdgroupname.isstring())
    {
	// All polygons should be rendered as subd primitives
	partitionGeometryRange(primitives, gdh, gdp, gdp.getPrimitiveRange(), ctx,
			       err, true, true);
	return;
    }

    // If there's a group name, only the primitives in the group
    // should be rendered as subd surfaces.
    const GA_PrimitiveGroup *subdgroup =
		gdp.findPrimitiveGroup(subdgroupname);
    if (!subdgroup)
    {
	// If there was no group, then there are no subd surfaces
	partitionGeometryRange(primitives, gdh, gdp, gdp.getPrimitiveRange(), ctx,
			       err, false, true);
	return;
    }

    // Build subdivision groups first
    partitionGeometryRange(primitives, gdh, gdp, GA_Range(*subdgroup), ctx, err,
			   true, false);
    // Now, build the polygons
    partitionGeometryRange(primitives, gdh, gdp, GA_Range(*subdgroup, true), ctx,
			   err, false, true);
}
