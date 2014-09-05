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

#include "ROP_AbcSOP.h"
#include "ROP_AbcGTCompoundShape.h"
#include "ROP_AbcGTShape.h"
#include <SOP/SOP_Node.h>
#include <GT/GT_Refine.h>
#include <GT/GT_GEODetail.h>
#include <GT/GT_Primitive.h>
#include <GT/GT_RefineParms.h>
#include <GABC/GABC_OError.h>

using namespace GABC_NAMESPACE;

namespace
{
    typedef Alembic::AbcGeom::ObjectVisibility	ObjectVisibility;

    typedef ROP_AbcSOP::abc_PrimContainer       abc_PrimContainer;
    typedef ROP_AbcSOP::IndexMap                IndexMap;
    typedef ROP_AbcSOP::IndexMapInsert          IndexMapInsert;
    typedef ROP_AbcSOP::NameList                NameList;
    typedef ROP_AbcSOP::PartitionMap            PartitionMap;
    typedef ROP_AbcSOP::PartitionMapInsert      PartitionMapInsert;
    typedef ROP_AbcSOP::PrimitiveList           PrimitiveList;

    static void
    buildGeometry(PrimitiveList &primitives,
	    const GU_Detail &gdp,
	    const GA_Range &range,
	    bool force_subd_mode,
	    bool show_pts)
    {
	/// Since there can be all kinds of primitives we don't understand
	/// (i.e. all custom ones, Tetra, etc. we build a GT primitive for the
	/// detail.  We can refine this into simpler primitives until we *do*
	/// understand them.
	GT_PrimitiveHandle	detail = GT_GEODetail::makeDetail(&gdp, &range);
	if (detail)
	{
	    primitives.append(abc_PrimContainer(detail,
	            force_subd_mode,
	            show_pts));
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
        const char *pos2;
        bool        slash;

        flag = false;
        storage.clear();

        while (true)
        {
            slash = false;
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
            {
                break;
            }

            pos2 = strchr(pos1, '/');
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

    SOP_Node *
    getSop(int id)
    {
	OP_Node	*node = OP_Node::lookupNode(id);
	return UTverify_cast<SOP_Node *>(node);
    }
}

ROP_AbcSOP::ROP_AbcSOP(SOP_Node *node)
    : mySopId(node ? node->getUniqueId() : -1)
    , myElapsedFrames(0)
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
    {
        delete it->second;
    }
    myXformMap.clear();

    myIndexMap.clear();
    myPartitionNames.clear();
    myNameMap.clear();
    myPartitionMap.clear();
    myShapeSet.clear();
    myPartitionIndices.clear();

    GABC_OGTGeometry::clearIgnoreList();
}

bool
ROP_AbcSOP::start(const OObject &parent,
	GABC_OError &err, const ROP_AbcContext &ctx, UT_BoundingBox &box)
{
    const GU_Detail	           *gdp;
    GU_DetailHandle	            gdh;
    NameList		            names;
    PrimitiveList	            prims;
    ROP_AbcGTCompoundShape	   *shape;
    SOP_Node                       *sop = getSop(mySopId);
    UT_Set<std::string>             uniquenames;
    std::string		            name = getName();

    if (!sop)
    {
        clear();
        return err.error("Unable to find SOP: %d", mySopId);
    }

    gdh = sop->getCookedGeoHandle(ctx.cookContext());
    gdp = GU_DetailHandleAutoReadLock(gdh).getGdp();

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
    if (ctx.buildFromPath())
    {
        GABC_OGTGeometry::skipAttribute(ctx.pathAttribute());
    }

    myPartitionIndices.append(GA_OffsetList());
    myPartitionNames.push_back(name);
    partitionGeometry(prims,
            names,
            sop,
            *gdp,
            ctx,
            err);
    if (names.size())
    {
        bool    pathless;

        UT_ASSERT(names.size() == prims.entries());

	for (int i = 0; i < prims.entries(); ++i)
	{
	    pathless = (i == 0) && !(names[i].compare(name));
	    shape = new ROP_AbcGTCompoundShape(names[i],
	                                &myShapeSet,
	                                &myXformMap,
	                                !pathless,
					prims(i).mySubdMode,
					prims(i).myShowPts);
	    myShapes.append(shape);
	    myNameMap.insert(NameMapInsert(names[i], (myShapes.entries() - 1)));
	}
    }
    else
    {
	UT_WorkBuffer   name_buffer;

	for (int i = 0; i < prims.entries(); ++i)
	{
	    if (i > 0)
	    {
		name_buffer.sprintf("%s_%d", name.c_str(), i);
	        name = name_buffer.buffer();
            }

	    shape = new ROP_AbcGTCompoundShape(name,
	                                NULL,
	                                NULL,
	                                false,
					prims(i).mySubdMode,
					prims(i).myShowPts);
	    myShapes.append(shape);
	}
    }

    for (int i = 0; i < prims.entries(); ++i)
    {
	if (!myShapes(i)->first(prims(i).myPrim, myParent, err, ctx, false))
	{
	    clear();
	    UT_WorkBuffer	path;
	    sop->getFullPath(path);
	    return err.error("Error saving first frame: %s", path.buffer());
	}
    }

    ++myElapsedFrames;
    return true;
}

bool
ROP_AbcSOP::update(GABC_OError &err,
	const ROP_AbcContext &ctx, UT_BoundingBox &box)
{
    const GU_Detail	           *gdp;
    GU_DetailHandle	            gdh;
    NameList		            names;
    NameMap::iterator               it;
    PrimitiveList	            prims;
    ROP_AbcGTCompoundShape	   *shape;
    SOP_Node                       *sop = getSop(mySopId);
    std::string		            name = getName();

    if (!sop)
    {
        clear();
        return err.error("Unable to find SOP: %d", mySopId);
    }

    gdh = sop->getCookedGeoHandle(ctx.cookContext());
    gdp = GU_DetailHandleAutoReadLock(gdh).getGdp();

    if (!gdp)
    {
        clear();
        UT_WorkBuffer	path;
        sop->getFullPath(path);
        return err.error("Error saving first frame: %s", path.buffer());
    }

    myTimeDependent = sop->isTimeDependent(ctx.cookContext());

    if (ctx.fullBounds())
    {
	gdp->computeQuickBounds(myBox);
	box = myBox;
    }

    partitionGeometry(prims,
            names,
            sop,
            *gdp,
            ctx,
            err);
    if (names.size())
    {
        bool    pathless;

        UT_ASSERT(names.size() == prims.entries());

        for (int i = 0; i < prims.entries(); ++i)
        {
            it = myNameMap.find(names[i]);

            if (it == myNameMap.end())
            {
	        pathless = (i < 0) && names[i].compare(name);
                shape = new ROP_AbcGTCompoundShape(names[i],
                        &myShapeSet,
                        &myXformMap,
                        !pathless,
                        prims(i).mySubdMode,
                        prims(i).myShowPts);

                if (!shape->first(prims(i).myPrim,
                        myParent,
                        err,
                        ctx,
                        false,
                        Alembic::AbcGeom::kVisibilityHidden))
                {
                    clear();
                    UT_WorkBuffer	path;
                    sop->getFullPath(path);
                    return err.error("Error saving next frame: %s", path.buffer());
                }

                shape->updateFromPrevious(err, (myElapsedFrames - 1));
                shape->updateFromPrevious(err,
                        Alembic::AbcGeom::kVisibilityDeferred);

                myShapes.append(shape);
                myNameMap.insert(NameMapInsert(names[i], (myShapes.entries() - 1)));
            }
            else
            {
                if (!myShapes(it->second)->update(prims(i).myPrim, err, ctx))
                {
                    clear();
                    UT_WorkBuffer	path;
                    sop->getFullPath(path);
                    return err.error("Error saving next frame: %s", path.buffer());
                }
            }
        }

        for (int i = 0; i < myShapes.entries(); ++i)
        {
            if (myShapes(i)->getElapsedFrames() == myElapsedFrames)
            {
                myShapes(i)->updateFromPrevious(err);
            }
        }
    }
    else
    {
        UT_ASSERT(myShapes.entries() == prims.entries());

        for (int i = 0; i < prims.entries(); ++i)
        {
            if (!myShapes(i)->update(prims(i).myPrim, err, ctx))
            {
                clear();
                UT_WorkBuffer	path;
                sop->getFullPath(path);
                return err.error("Error saving next frame: %s", path.buffer());
            }
        }
    }

    ++myElapsedFrames;
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

void
ROP_AbcSOP::partitionGeometryRange(PrimitiveList &primitives,
        NameList &names,
        const GU_Detail &gdp,
        const GA_Range &range,
        const ROP_AbcContext &ctx,
        GABC_OError &err,
        bool force_subd_mode,
        bool show_pts)
{
    GA_ROHandleS            str;
    GA_StringIndexType      idx;
    UT_WorkBuffer           namebuf;
    int                     pos;
    const char             *aname;
    const char             *strval;
    bool                    from_path = ctx.buildFromPath();
    bool                    flag = false;

    aname = from_path ? ctx.pathAttribute() : ctx.partitionAttribute();

    str = GA_ROHandleS(gdp.findStringTuple(GA_ATTRIB_PRIMITIVE, aname));
    if (!str.isValid() || !str.getAttribute()->getAIFSharedStringTuple())
    {
        buildGeometry(primitives, gdp, range, force_subd_mode, show_pts);
        return;
    }

    for (GA_Iterator it(range); !it.atEnd(); ++it)
    {
        idx = str.getIndex(*it);
        if (idx < 0)
        {
            myPartitionIndices(0).append(*it);
        }
        else
        {
            IndexMap::iterator  iter = myIndexMap.find(idx);

            if (iter != myIndexMap.end())
            {
                pos = iter->second;
            }
            else
            {
                strval = str.get(*it);
                strval = from_path
                        ? homogenizePath(strval, namebuf, flag)
                        : ctx.partitionModeValue(strval, namebuf);

                if (*strval == 0)
                {
                    err.warning("Invalid %s attribute value for "
                            "primitive %lld was ignored.",
                            aname,
                            ((int64)(*it) - 1));

                    myIndexMap.insert(IndexMapInsert(idx, 0));
                    myPartitionIndices(0).append(*it);
                    continue;
                }
                else if (from_path && flag)
                {
                    err.warning("%s attribute value for primitive %lld has "
                            "odd value. Value interpreted as %s.",
                            aname,
                            ((int64)(*it) - 1),
                            strval);
                }

                PartitionMap::iterator  iter2 = myPartitionMap.find(strval);

                if (iter2 != myPartitionMap.end())
                {
                    pos = iter2->second;
                }
                else
                {
                    pos = myPartitionIndices.entries();
                    myPartitionIndices.append(GA_OffsetList());
                    myPartitionNames.push_back(strval);

                    myPartitionMap.insert(PartitionMapInsert(strval, pos));
                }

                myIndexMap.insert(IndexMapInsert(idx, pos));
            }

            myPartitionIndices(pos).append(*it);
        }
    }

    for (exint i = 0; i < myPartitionIndices.entries(); ++i)
    {
        if (myPartitionIndices(i).entries())
        {
            GA_Range    range(gdp.getPrimitiveMap(), myPartitionIndices(i));

            buildGeometry(primitives, gdp, range, force_subd_mode, show_pts);
            names.push_back(myPartitionNames[i]);

            myPartitionIndices(i).clear();
        }
    }
}

void
ROP_AbcSOP::partitionGeometry(PrimitiveList &primitives,
        NameList &names,
        const SOP_Node *sop,
        const GU_Detail &gdp,
        const ROP_AbcContext &ctx,
        GABC_OError &err)
{
    names.clear();
    UT_String	subdgroupname;

    if (objectSubd(sop, ctx, subdgroupname))
    {
        if (subdgroupname.isstring())
        {
            // If there's a group name, only the primitives in the group
            // should be rendered as subd surfaces.
            const GA_PrimitiveGroup	*subdgroup
                    = gdp.findPrimitiveGroup(subdgroupname);
            if (subdgroup)
            {
                // Build subdivision groups first
                partitionGeometryRange(primitives,
                        names,
                        gdp,
                        GA_Range(*subdgroup),
                        ctx,
                        err,
                        true,
                        false);
                // Now, build the polygons
                partitionGeometryRange(primitives,
                        names,
                        gdp,
                        GA_Range(*subdgroup, true),
                        ctx,
                        err,
                        false,
                        true);
            }
            else
            {
                // If there was no group, then there are no subd surfaces
                partitionGeometryRange(primitives,
                        names,
                        gdp,
                        gdp.getPrimitiveRange(),
                        ctx,
                        err,
                        false,
                        true);
            }
        }
        else
        {
            // All polygons should be rendered as subd primitives
            partitionGeometryRange(primitives,
                    names,
                    gdp,
                    gdp.getPrimitiveRange(),
                    ctx,
                    err,
                    true,
                    true);
        }
    }
    else
    {
        // No subdivision primitives
        partitionGeometryRange(primitives,
                names,
                gdp,
                gdp.getPrimitiveRange(),
                ctx,
                err,
                false,
                true);
    }
}
