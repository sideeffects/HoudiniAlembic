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
 * NAME:	ROP_AbcSOP.h
 *
 * COMMENTS:
 */

#include "ROP_AbcSOP.h"
#include "ROP_AbcGTShape.h"
#include <SOP/SOP_Node.h>
#include <GT/GT_Refine.h>
#include <GT/GT_GEODetail.h>
#include <GT/GT_Primitive.h>
#include <GT/GT_RefineParms.h>
#include <GABC/GABC_OError.h>

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
	/// (i.e. Tetra, VDB, etc. we build a GT primitive for the detail.  We
	/// can refine this into simpler primitives until we *do* understand
	/// them.
	GT_PrimitiveHandle	detail = GT_GEODetail::makeDetail(gdp, range);
	if (detail)
	{
	    abc_Refiner	refine(primitives, &rparms);
	    detail->refine(refine, &rparms);
	}
    }

    static bool
    isToggleEnabled(OP_Node *node, const char *name, fpreal now, bool def)
    {
	int	value;
	if (node->evalParameterOrProperty(name, 0, now, value))
	    return value != 0;
	return def;
    }

    static void
    initializeRefineParms(GT_RefineParms &rparms, const SOP_Node *sop,
		const ROP_AbcContext &ctx)
    {
	OP_Network	*obj = sop->getCreator();
	if (isToggleEnabled(obj, "vm_rendersubd", ctx.cookTime(), false) ||
	    isToggleEnabled(obj, "ri_rendersubd", ctx.cookTime(), false))
	{
	    rparms.setPolysAsSubdivision(true);
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

	GA_ROAttributeRef	 attrib;
	const char		*aname = ctx.partitionAttribute();
	GT_RefineParms		 rparms;

	initializeRefineParms(rparms, sop, ctx);
	if (UTisstring(aname))
	    attrib = gdp->findStringTuple(GA_ATTRIB_PRIMITIVE, aname);
	GA_ROHandleS		 str(attrib);
	if (!str.isValid() || !attrib.getAttribute()->getAIFSharedStringTuple())
	{
	    buildGeometry(primitives, gdp, rparms, NULL);
	    return;
	}

	UT_Array<GA_OffsetList> partitions;
	NameList		partnames;
	UT_WorkBuffer		namebuf;
	partitions.append(GA_OffsetList());
	partnames.push_back(basename);
	for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
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

    std::string		 name = sop->getName().toStdString();
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
    std::string		name = sop->getName().toStdString();

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
	for (int i = 0; i < prims.entries(); ++i)
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
