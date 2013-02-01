/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Side Effects Software Inc
 *	477 Richmond Street West
 *	Toronto, Ontario
 *	Canada   M5V 3E7
 *	416-504-9876
 *
 * NAME:	GABC_GUPrim.C ( GU Library, C++)
 *
 * COMMENTS:	Definitions for utility functions of tetrahedrons.
 */

#include "GABC_GUPrim.h"
#include "GABC_GTPrim.h"
#include <GA/GA_GBAttributeMath.h>
#include <GEO/GEO_AttributeHandleList.h>
#include <GEO/GEO_WorkVertexBuffer.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_ConvertParms.h>
#include <GU/GU_RayIntersect.h>
#include <GU/GU_MergeUtils.h>
#include <GT/GT_Primitive.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_Util.h>

GA_PrimitiveDefinition *GABC_GUPrim::theDef = 0;

GABC_GUPrim *
GABC_GUPrim::build(GU_Detail *gdp,
		const std::string &filename,
		const std::string &objectpath,
		fpreal frame,
		bool use_transform)
{
    GABC_GUPrim	*abc;

    abc = (GABC_GUPrim *)gdp->appendPrimitive(GABC_GUPrim::theTypeId());

    abc->init(filename, objectpath, frame, use_transform);

    return abc; 
}

GABC_GUPrim *
GABC_GUPrim::build(GU_Detail *gdp,
		const std::string &filename,
		const GABC_IObject &object,
		fpreal frame,
		bool use_transform)
{
    GABC_GUPrim		*abc;
    UT_BoundingBox	 box;

    abc = (GABC_GUPrim *)gdp->appendPrimitive(GABC_GUPrim::theTypeId());
    abc->init(filename, object, frame, use_transform);
    abc->getBBox(&box);		// Look up box while prim is active

    return abc; 
}

GABC_GUPrim::~GABC_GUPrim() 
{
}


// Static callback for our factory.
static GA_Primitive *
gu_newPrimABC(GA_Detail &detail, GA_Offset offset)
{
    return new GABC_GUPrim(UTverify_cast<GU_Detail *>(&detail), offset);
}

void
GABC_GUPrim::registerMyself(GA_PrimitiveFactory *factory)
{
    // Ignore double registration
    if (theDef)
	return;

    theDef = factory->registerDefinition("AlembicRef", 
			gu_newPrimABC,
			GA_FAMILY_NONE);
    
    theDef->setLabel("Alembic Delayed Load");
    theDef->setHasLocalTransform(true);
    registerIntrinsics(*theDef);

    // Register the GT tesselation too (now we know what type id we have)
    GABC_GTPrimCollect::registerPrimitive(theDef->getId());
}

int64
GABC_GUPrim::getMemoryUsage() const
{
    exint       mem = sizeof(*this);
    return mem;
}

static void
addGroupToDetails(UT_PtrArray<GU_Detail *> &details,
	const GA_PrimitiveGroup *group)
{
    const char	*name = group->getName();
    for (int i = 0; i < details.entries(); ++i)
    {
	// If there's an existing group on the detail, that means there was a
	// face set in the Alembic primitive that matched the group that
	// existed on the parent.  In that case, we do *not* want to blindly
	// set the parent group to contain all the converted primitives.
	// Instead we use the result of the face set.
	GA_PrimitiveGroup	*g = details(i)->findPrimitiveGroup(name);
	if (!g)
	{
	    // Add all newly converted primitives to the group
	    g = details(i)->newPrimitiveGroup(name);
	    g->setEntries();
	}
    }
}

bool
GABC_GUPrim::doConvert(const GU_ConvertParms &parms) const
{
    GT_PrimitiveHandle	prim = gtPrimitive();
    if (!prim)
	return false;

    UT_PtrArray<GU_Detail *>	details;
    GT_RefineParms		rparms;

    if (parms.toType == GEO_PrimTypeCompat::GEOPRIMPOLY)
	rparms.setAllowPolySoup(false);

    GT_Util::makeGEO(details, prim, &rparms);
    if (!details.entries())
	return false;

    GA_PrimitiveGroup	*group;
    GA_FOR_ALL_PRIMGROUPS(getParent(), group)
    {
	if (group->containsOffset(getMapOffset()))
	    addGroupToDetails(details, group);
    }

    GU_Detail	*gdp = UTverify_cast<GU_Detail *>(getParent());

    GUmatchAttributesAndMerge(*gdp, details.getRawArray(), details.entries());

    for (int i = 0; i < details.entries(); ++i)
	delete details(i);

    // Return the newly added primitive
    return true;
}

GEO_Primitive *
GABC_GUPrim::convertNew(GU_ConvertParms &parms)
{
    GU_Detail	*gdp = UTverify_cast<GU_Detail *>(getParent());
    GA_Size	 nprims = gdp->getPrimitiveMap().indexSize();

    doConvert(parms);

    return gdp->getPrimitiveMap().indexSize() > nprims
		? gdp->primitives()(nprims) : NULL;
}

GEO_Primitive *
GABC_GUPrim::convert(GU_ConvertParms &parms, GA_PointGroup *delpts)
{
    GU_Detail		*gdp = UTverify_cast<GU_Detail *>(getParent());
    GA_Size		 nprims = gdp->getPrimitiveMap().indexSize();

    if (doConvert(parms))
    {
	GA_PrimitiveGroup	*grp = parms.getDeletePrimitives();

	if (delpts && GAisValid(getVertexOffset(0)))
	    delpts->addOffset(getPointOffset(0));

	if (grp)
	    grp->add(this);
	else
	    getParent()->deletePrimitive(*this, !delpts);
    }
    return gdp->getPrimitiveMap().indexSize() > nprims
		? gdp->primitives()(nprims) : NULL;
}

void
GABC_GUPrim::normal(NormalComp &) const
{
    // No need here.
}

void *
GABC_GUPrim::castTo() const
{
    return (GU_Primitive *)this;
}

const GEO_Primitive *
GABC_GUPrim::castToGeo(void) const
{
    return this;
}

GU_RayIntersect *
GABC_GUPrim::createRayCache(int &persistent)
{
    GU_Detail		*gdp = (GU_Detail *)getParent();
    GU_RayIntersect	*intersect;

    persistent = 0;
    if (gdp->cacheable())
	buildRayCache();

    intersect = getRayCache();
    if (!intersect)
    {
	intersect = new GU_RayIntersect(gdp, this);
	persistent = 1;
    }		    

    return intersect;
}

int
GABC_GUPrim::intersectRay(const UT_Vector3 &org, const UT_Vector3 &dir,
		float tmax, float , float *distance, 
		UT_Vector3 *pos, UT_Vector3 *nml, 
		int, float *, float *, int) const
{
    int			result;
    float		dist;
    UT_BoundingBox	bbox;

    // TODO: Build ray cache and intsrect properly.
    getBBox(&bbox);
    result =  bbox.intersectRay(org, dir, tmax, &dist, nml);
    if (result)
    {
	if (distance)
	    *distance = dist;
	if (pos)
	    *pos = org + dist * dir;
    }
    return result;
}
