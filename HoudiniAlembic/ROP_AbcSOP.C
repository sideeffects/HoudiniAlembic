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

#include "ROP_AbcSOP.h"
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
    typedef UT_Array<GT_PrimitiveHandle>	PrimitiveList;
    typedef std::vector<std::string>		NameList;

    class abc_Refiner : public GT_Refine
    {
    public:
	abc_Refiner(PrimitiveList &list,
		    const GT_RefineParms *parms)
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
    buildGeometry(PrimitiveList &primitives,
	    const GU_Detail *gdp,
	    const GT_RefineParms &rparms,
	    const GA_Range *range)
    {
	/// Since there can be all kinds of primitives we don't understand
	/// (i.e. all custom ones, Tetra, etc. we build a GT primitive for the
	/// detail.  We can refine this into simpler primitives until we *do*
	/// understand them.
	GT_PrimitiveHandle	detail = GT_GEODetail::makeDetail(gdp, range);
	if (detail)
	{
	    abc_Refiner	refine(primitives, &rparms);
	    detail->refine(refine, &rparms);
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

    enum
    {
	FORCE_SUBD_OFF,
	FORCE_SUBD_ON,
    };

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

    static void
    initializeRefineParms(GT_RefineParms &rparms, const SOP_Node *sop,
		const ROP_AbcContext &ctx,
		int subdmode,
		bool show_unused_points)
    {
	rparms.setFaceSetMode(ctx.faceSetMode());
	rparms.setFastPolyCompacting(false);
	rparms.setPolysAsSubdivision(subdmode == FORCE_SUBD_ON);
	rparms.setShowUnusedPoints(show_unused_points);
    }

    static void
    partitionGeometryRange(PrimitiveList &primitives,
	    const std::string &basename,
	    NameList &names,
	    const SOP_Node *sop,
	    const GU_Detail *gdp,
	    const GA_Range &range,
	    const ROP_AbcContext &ctx,
	    int subdmode,
	    bool show_unused_points)
    {
	GA_ROAttributeRef	 attrib;
	const char		*aname = ctx.partitionAttribute();
	GT_RefineParms		 rparms;

	initializeRefineParms(rparms, sop, ctx, subdmode, show_unused_points);
	if (UTisstring(aname))
	    attrib = gdp->findStringTuple(GA_ATTRIB_PRIMITIVE, aname);
	GA_ROHandleS		 str(attrib);
	if (!str.isValid() || !attrib.getAttribute()->getAIFSharedStringTuple())
	{
	    buildGeometry(primitives, gdp, rparms, &range);
	    return;
	}

	UT_Array<GA_OffsetList> partitions;
	NameList		partnames;
	UT_WorkBuffer		namebuf;
	partitions.append(GA_OffsetList());
	partnames.push_back(basename);
	for (GA_Iterator it(range); !it.atEnd(); ++it)
	{
	    GA_StringIndexType	idx;
	    idx = str.getIndex(*it);
	    if (idx < 0)
	    {
		partitions(0).append(*it);
	    }
	    else
	    {
		idx++;
		while (partitions.entries() <= idx)
		{
		    const char	*sval = str.get(*it);
		    sval = ctx.partitionModeValue(sval, namebuf);
		    partitions.append(GA_OffsetList());
		    partnames.push_back(sval);;
		}
		partitions(idx).append(*it);
	    }
	}
	for (exint i = 0; i < partitions.entries(); ++i)
	{
	    if (partitions(i).entries())
	    {
		GA_Range	range(gdp->getPrimitiveMap(), partitions(i));
		buildGeometry(primitives, gdp, rparms, &range);
		names.push_back(partnames[i]);
	    }
	}
    }

    static void
    partitionGeometry(PrimitiveList &primitives,
	    const std::string &basename,
	    NameList &names,
	    const SOP_Node *sop,
	    const GU_Detail *gdp,
	    const ROP_AbcContext &ctx)
    {
	names.clear();
	UT_String	subdgroupname;
	if (objectSubd(sop, ctx, subdgroupname))
	{
	    if (subdgroupname.isstring())
	    {
		// If there's a group name, only the primitives in the group
		// should be rendered as subd surfaces.
		const GA_PrimitiveGroup	*subdgroup;
		subdgroup = gdp->findPrimitiveGroup(subdgroupname);
		if (subdgroup)
		{
		    // Build subdivision groups first
		    partitionGeometryRange(primitives, basename, names, sop,
			gdp, GA_Range(*subdgroup), ctx, FORCE_SUBD_ON, false);
		    // Now, build the polygons
		    partitionGeometryRange(primitives, basename, names, sop,
			gdp, GA_Range(*subdgroup, true), ctx, FORCE_SUBD_OFF, true);
		}
		else
		{
		    // If there was no group, then there are no subd surfaces
		    partitionGeometryRange(primitives, basename, names, sop,
			gdp, gdp->getPrimitiveRange(), ctx, FORCE_SUBD_OFF, true);
		}
	    }
	    else
	    {
		// All polygons should be rendered as subd primitives
		partitionGeometryRange(primitives, basename, names, sop,
			gdp, gdp->getPrimitiveRange(), ctx, FORCE_SUBD_ON, true);
	    }
	}
	else
	{
	    // No subdivision primitives
	    partitionGeometryRange(primitives, basename, names, sop,
		    gdp, gdp->getPrimitiveRange(), ctx, FORCE_SUBD_OFF, true);
	}
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
    myShapes.resize(0);
}

static std::string
getUniqueName(UT_Set<std::string>&names, const std::string &orgname)
{
    std::string		result = orgname;
    for (int i = 1;  names.count(result) > 0; ++i)
    {
	UT_WorkBuffer	uname;
	uname.sprintf("%s_%d", orgname.c_str(), i);
	result = uname.toStdString();
    }
    names.insert(result);
    return result;
}

bool
ROP_AbcSOP::start(const OObject &parent,
	GABC_OError &err, const ROP_AbcContext &ctx, UT_BoundingBox &box)
{
    SOP_Node	*sop = getSop(mySopId);
    if (!sop)
	return err.error("Unable to find SOP: %d", mySopId);
    myParent = parent;

    std::string		 name = getName();
    GU_DetailHandle	 gdh = sop->getCookedGeoHandle(ctx.cookContext());
    GU_DetailHandleAutoReadLock	gdl(gdh);
    const GU_Detail	*gdp = gdl.getGdp();
    myTimeDependent = sop->isTimeDependent(ctx.cookContext());

    if (!gdp)
    {
	return err.error("Error cooking SOP: %s", (const char *)sop->getName());
    }

    PrimitiveList	prims;
    NameList		names;
    UT_Set<std::string>	uniquenames;

    if (ctx.fullBounds())
    {
	gdp->computeQuickBounds(myBox);
	box = myBox;
    }
    partitionGeometry(prims, name, names, sop, gdp, ctx);
    if (names.size() == prims.entries())
    {
	for (int i = 0; i < prims.entries(); ++i)
	{
	    size_t	found = names[i].find_last_of("/");
	    name = getUniqueName(uniquenames, names[i].substr(found+1));
	    myShapes.append(new ROP_AbcGTShape(name));
	}
    }
    else
    {
	UT_WorkBuffer	gname;
	std::string	uname;
	gname.strcpy(name.c_str());
	for (int i = 0; i < prims.entries(); ++i)
	{
	    if (i > 0)
		gname.sprintf("%s_%d", name.c_str(), i);
	    uname = getUniqueName(uniquenames, gname.buffer());
	    myShapes.append(new ROP_AbcGTShape(uname.c_str()));
	}
    }
    for (int i = 0; i < prims.entries(); ++i)
    {
	if (!myShapes(i)->firstFrame(prims(i), myParent, err, ctx))
	{
	    clear();
	    UT_WorkBuffer	path;
	    sop->getFullPath(path);
	    return err.error("Error saving first frame: %s", path.buffer());
	}
    }
    return true;
}

bool
ROP_AbcSOP::update(GABC_OError &err,
	const ROP_AbcContext &ctx, UT_BoundingBox &box)
{
    SOP_Node	*sop = getSop(mySopId);
    if (!sop || !myShapes.entries())
    {
	// If there are no shapes, we're ok...
	return sop != NULL;
    }
    UT_WorkBuffer	wbuf;
    sop->getFullPath(wbuf);

    GU_DetailHandle	 gdh = sop->getCookedGeoHandle(ctx.cookContext());
    GU_DetailHandleAutoReadLock	gdl(gdh);
    const GU_Detail	*gdp = gdl.getGdp();
    myTimeDependent = sop->isTimeDependent(ctx.cookContext());

    if (!gdp)
	return err.error("Bad cook for SOP: %s", (const char *)sop->getName());

    PrimitiveList	prims;
    NameList		names;
    std::string		name = getName();

    if (ctx.fullBounds())
    {
	gdp->computeQuickBounds(myBox);
	box = myBox;
    }
    partitionGeometry(prims, name, names, sop, gdp, ctx);
    if (prims.entries() > myShapes.entries())
    {
	// TODO: Add new primitives
    }
    if (myShapes.entries() <= prims.entries())
    {
	int	num = SYSmin(myShapes.entries(), prims.entries());
	for (int i = 0; i < num; ++i)
	{
	    if (!myShapes(i)->nextFrame(prims(i), err, ctx))
	    {
		clear();
		UT_WorkBuffer	path;
		sop->getFullPath(path);
		return err.error("Error saving next frame: %s", path.buffer());
	    }
	}
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
