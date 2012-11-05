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
 * NAME:	ROP_AbcShape.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#include "ROP_AbcShape.h"
#include "ROP_AbcGeometry.h"
#include "ROP_AbcError.h"
#include "ROP_AbcContext.h"
#include <SOP/SOP_Node.h>
#include <GT/GT_Refine.h>
#include <GT/GT_GEODetail.h>

ROP_AbcShape::ROP_AbcShape(SOP_Node *sop, Alembic::AbcGeom::OXform *parent)
    : mySop(sop)
    , myParent(parent)
    , myTimeDependent(false)
    , myTopologyWarned(false)
{
}

ROP_AbcShape::~ROP_AbcShape()
{
    clear();
}

void
ROP_AbcShape::clear()
{
    for (int i = 0; i < myGeos.entries(); ++i)
	delete myGeos(i);
    myGeos.resize(0);
}

namespace
{
    class abc_Refiner : public GT_Refine
    {
    public:
	abc_Refiner(UT_Array<GT_PrimitiveHandle> &prims,
		    const GT_RefineParms *parms)
	    : myPrimitives(prims)
	    , myParms(parms)
	{
	}
	virtual bool	allowThreading() const	{ return false; }
	virtual void	addPrimitive(const GT_PrimitiveHandle &prim);

    private:
	UT_Array<GT_PrimitiveHandle>	&myPrimitives;
	const GT_RefineParms		*myParms;
    };

    void
    abc_Refiner::addPrimitive(const GT_PrimitiveHandle &prim)
    {
	switch (prim->getPrimitiveType())
	{
	    case GT_PRIM_POINT_MESH:
	    case GT_PRIM_POLYGON_MESH:
	    case GT_PRIM_CURVE_MESH:
	    case GT_PRIM_CURVE:
	    case GT_PRIM_POLYGON:
	    case GT_PRIM_NUPATCH:
	    case GT_GEO_PRIMTPSURF:
		myPrimitives.append(prim);
		return;
	}
	// Unsupported primitive type, so refine further
	prim->refine(*this, myParms);
    }

    static void
    buildGeometry(UT_Array<GT_PrimitiveHandle> &primitives,
		const GU_Detail *gdp,
		const GA_Range *range)
    {
        // Make a primitive for an entire detail
	GT_PrimitiveHandle	detail = GT_GEODetail::makeDetail(gdp, range);
	abc_Refiner		refine(primitives, NULL);
        // Refining this primitive will iterate over the primitives in the detail.
	if (detail)
	    detail->refine(refine, NULL);
    }

    static void
    partitionGeometry(UT_Array<GT_PrimitiveHandle> &primitives,
		const GU_Detail *gdp,
		const char *partition,
		std::vector< std::string > *oShapeNames=NULL)
    {
	if (oShapeNames)
	    oShapeNames->clear();

	GA_ROAttributeRef		 sattrib;
	const GA_AIFSharedStringTuple	*stuple = NULL;
	
	if (partition && *partition)
	    sattrib = gdp->findStringTuple(GA_ATTRIB_PRIMITIVE, partition);
	if (sattrib.isValid())
	    stuple = sattrib.getAttribute()->getAIFSharedStringTuple();
	if (!stuple)
	{
	    buildGeometry(primitives, gdp, NULL);
	    return;
	}

	GA_OffsetList			unpartitioned;
	UT_Array<GA_OffsetList>	partitions;
	GA_ROHandleS			str(sattrib.getAttribute());
	for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
	{
	    GA_StringIndexType	idx;
            // Get the string index at the given offset
            idx = str.getIndex(*it);
            if (idx < 0)
            {
                unpartitioned.append(*it);
            }
            else
            {
                while (partitions.entries() <= idx)
                {
                    partitions.append(GA_OffsetList());
		    if (oShapeNames)
			oShapeNames->push_back(str.get(*it));
                }
                partitions(idx).append(*it);
            }
        }
        if (unpartitioned.entries())
        {
            GA_Range range(gdp->getPrimitiveMap(), unpartitioned);
            buildGeometry(primitives, gdp, &range);
        }
        for (exint i = 0; i < partitions.entries(); ++i)
        {
            if (partitions(i).entries())
            {
                GA_Range range(gdp->getPrimitiveMap(), partitions(i));
                buildGeometry(primitives, gdp, &range);
            }
        }
    }
}

void
ROP_AbcShape::create(ROP_AbcError &err, const char *basename,
	ROP_AbcContext &context,
	const Alembic::AbcGeom::TimeSamplingPtr &timeRange)
{
    clear();

    GU_DetailHandle	 gdh = mySop->getCookedGeoHandle(context.context());
    GU_DetailHandleAutoReadLock	 gdl(gdh);
    const GU_Detail			*gdp = gdl.getGdp();
    if (!gdp)
	return;
    UT_Array<GT_PrimitiveHandle>	 prims;
    UT_WorkBuffer			 gname;
    const char				*name = basename;
    std::vector< std::string >		 names;

    partitionGeometry(prims, gdp, context.partitionAttribute(), &names);
    myTimeDependent = mySop->isTimeDependent(context.context());

    if (names.size() == prims.entries())
    {
        for (int i = 0; i < prims.entries(); ++i)
        {
            size_t found = names[i].find_last_of("/");
            name = names[i].substr(found+1).c_str();
            myGeos.append(new ROP_AbcGeometry(err, mySop, name,
			prims(i), myParent, timeRange, context));
        }
    }
    else
    {
        for (int i = 0; i < prims.entries(); ++i)
        {
            if (i > 0)
            {
                gname.sprintf("%s_%d", basename, i);
                name = gname.buffer();
            }
            myGeos.append(new ROP_AbcGeometry(err, mySop, name,
			prims(i), myParent, timeRange, context));
        }
    }

    UT_WorkBuffer	fullpath;
    mySop->getFullPath(fullpath);
    err.info("%s added %" SYS_PRId64 " geometry nodes", fullpath.buffer(), myGeos.entries());
}

bool
ROP_AbcShape::writeSample(ROP_AbcError &err, ROP_AbcContext &context)
{
    GU_DetailHandle	 gdh = mySop->getCookedGeoHandle(context.context());
    GU_DetailHandleAutoReadLock	 gdl(gdh);
    const GU_Detail		*gdp = gdl.getGdp();

    if (!gdp)
    {
	UT_WorkBuffer	fullpath;
	mySop->getFullPath(fullpath);
	err.error("Error cooking %s at time %g",
		fullpath.buffer(), context.time());
	return false;
    }
    UT_Array<GT_PrimitiveHandle>	prims;

    partitionGeometry(prims, gdp, context.partitionAttribute());
    if (prims.entries() != myGeos.entries())
    {
	UT_WorkBuffer	fullpath;
	mySop->getFullPath(fullpath);
	if (!myTopologyWarned)
	{
	    err.warning("%s - primitive count changed (%" SYS_PRId64 " vs. %" SYS_PRId64 ")",
		    fullpath.buffer(), prims.entries(), myGeos.entries());
	    myTopologyWarned = true;
	}
	return false;
    }
    for (exint i = 0; i < myGeos.entries(); ++i)
    {
	if (!myGeos(i)->writeSample(err, prims(i)))
	{
	    return false;
	}
    }

    return true;
}
