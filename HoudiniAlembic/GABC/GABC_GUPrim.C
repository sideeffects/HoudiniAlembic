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

#include "GABC_GUPrim.h"
#if !defined(GABC_PACKED)
#include "GABC_GTPrim.h"
#include "GABC_NameMap.h"
#include <UT/UT_WorkBuffer.h>
#include <UT/UT_JSONParser.h>
#include <GA/GA_GBAttributeMath.h>
#include <GA/GA_SaveMap.h>
#include <GEO/GEO_AttributeHandleList.h>
#include <GEO/GEO_WorkVertexBuffer.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_ConvertParms.h>
#include <GU/GU_RayIntersect.h>
#include <GU/GU_MergeUtils.h>
#include <GT/GT_Primitive.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_Util.h>

#if !defined(GABC_PRIMITIVE_TOKEN)
    #define GABC_PRIMITIVE_TOKEN	"AlembicRef"
    #define GABC_PRIMITIVE_LABEL	"Alembic Delayed Load"
#endif

using namespace GABC_NAMESPACE;

GA_PrimitiveDefinition *GABC_GUPrim::theDef = 0;

GABC_GUPrim *
GABC_GUPrim::build(GU_Detail *gdp,
		const std::string &filename,
		const std::string &objectpath,
		fpreal frame,
		bool use_transform,
		bool check_visibility)
{
    GABC_GUPrim	*abc;

    abc = (GABC_GUPrim *)gdp->appendPrimitive(GABC_GUPrim::theTypeId());

    abc->init(filename, objectpath, frame, use_transform, check_visibility);

    return abc; 
}

GABC_GUPrim *
GABC_GUPrim::build(GU_Detail *gdp,
		const std::string &filename,
		const GABC_IObject &object,
		fpreal frame,
		bool use_transform,
		bool check_visibility)
{
    GABC_GUPrim		*abc;
    UT_BoundingBox	 box;

    abc = (GABC_GUPrim *)gdp->appendPrimitive(GABC_GUPrim::theTypeId());
    abc->init(filename, object, frame, use_transform, check_visibility);
    abc->getBBox(&box);		// Look up box while prim is active

    return abc; 
}

GABC_GUPrim::~GABC_GUPrim() 
{
}


namespace
{
// Static callback for our factory.
static GA_Primitive *
gu_newPrimABC(GA_Detail &detail, GA_Offset offset,
	const GA_PrimitiveDefinition &)
{
    return new GABC_GUPrim(UTverify_cast<GU_Detail *>(&detail), offset);
}

class AbcSharedDataLoader : public GA_PrimitiveDefinition::SharedDataLoader
{
public:
    AbcSharedDataLoader() {}
    virtual ~AbcSharedDataLoader() {}
    virtual bool	load(UT_JSONParser &p, GA_LoadMap &m) const;
};

bool
AbcSharedDataLoader::load(UT_JSONParser &p, GA_LoadMap &load) const
{
    typedef GABC_NameMap::LoadContainer		NameMapContainer;
    UT_WorkBuffer	key;
    UT_WorkBuffer	type;
    for (UT_JSONParser::iterator it = p.beginArray(); !it.atEnd(); ++it)
    {
	if (!p.parseString(type))
	    return false;
	if (!p.parseString(key))
	    return false;

	if (!strcmp(type.buffer(), "namemap"))
	{
	    NameMapContainer	data;
	    if (!GABC_NameMap::load(data.myNameMap, p))
		return false;
	    load.resolveSharedData(key.buffer(), &data);
	}
	else
	{
	    p.addWarning("Unknown Alembic Shared Data: %s", type.buffer());
	    if (!p.skipNextObject())
		return false;
	}
    }
    return true;
}

}	// End namespace

void
GABC_GUPrim::registerMyself(GA_PrimitiveFactory *factory)
{
    // Ignore double registration
    if (theDef)
	return;

    theDef = factory->registerDefinition(GABC_PRIMITIVE_TOKEN, 
			gu_newPrimABC,
			GA_FAMILY_NONE);
    
    theDef->setLabel(GABC_PRIMITIVE_LABEL);
    theDef->setHasLocalTransform(true);
    theDef->setSharedDataLoader(new AbcSharedDataLoader());
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
addGroupToDetails(UT_Array<GU_Detail *> &details,
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

    UT_Array<GU_Detail *>	details;
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
#else
#include "GABC_PackedImpl.h"

namespace GABC_NAMESPACE
{
    namespace GABC_GUPrim
    {
	void
	registerMyself(GA_PrimitiveFactory *pfact)
	{
	    GABC_PackedImpl::install(pfact);
	}
    }
}
#endif
