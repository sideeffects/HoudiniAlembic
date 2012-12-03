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
 * NAME:	GABC_GTPrim.h ( GT Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_GTPrim.h"
#include "GABC_GEOPrim.h"
#include "GABC_Util.h"
#include "GABC_NameMap.h"
#include <Alembic/AbcGeom/All.h>
#include <UT/UT_StackBuffer.h>
#include <UT/UT_Lock.h>
#include <UT/UT_DoubleLock.h>
#include <GT/GT_Refine.h>
#include <GT/GT_GEOPrimitive.h>
#include <GT/GT_Util.h>
#include <GT/GT_DANumeric.h>
#include <GT/GT_DAConstantValue.h>
#include <GT/GT_DAIndexedString.h>
#include <GT/GT_PrimitiveBuilder.h>
#include <GT/GT_PrimSubdivisionMesh.h>
#include <GT/GT_PrimPolygonMesh.h>
#include <GT/GT_PrimCurveMesh.h>
#include <GT/GT_PrimPointMesh.h>
#include <GT/GT_PrimNuPatch.h>
#include <GT/GT_TrimNuCurves.h>
#include <GT/GT_AttributeList.h>
#include <GT/GT_Transform.h>

using namespace Alembic::AbcGeom;

namespace
{
    static GT_DataArrayHandle		theXformCounts;
    static GT_AttributeListHandle	theXformVertex;
    static GT_AttributeListHandle	theXformUniform;
}

void
GABC_GTPrimCollect::registerPrimitive(const GA_PrimitiveTypeId &id)
{
    // Just construct.  The constructor registers itself.
    new GABC_GTPrimCollect(id);
}

GABC_GTPrimCollect::GABC_GTPrimCollect(const GA_PrimitiveTypeId &id)
    : myId(id)
{
    // Bind this collector to the given primitive id.  When GT refines
    // primitives and hits the given primitive id, this collector will be
    // invoked.
    bind(myId);
}

GABC_GTPrimCollect::~GABC_GTPrimCollect()
{
}

GT_GEOPrimCollectData *
GABC_GTPrimCollect::beginCollecting(const GT_GEODetailListHandle &geometry,
	const GT_RefineParms *parms) const
{
    return NULL;
}

GT_PrimitiveHandle
GABC_GTPrimCollect::collect(const GT_GEODetailListHandle &,
	const GEO_Primitive *const*plist,
	int,
	GT_GEOPrimCollectData *) const
{
    if (plist && plist[0])
    {
	const GABC_GEOPrim *abc = UTverify_cast<const GABC_GEOPrim *>(plist[0]);
	return abc->gtPrimitive();
    }
    return GT_PrimitiveHandle();
}

GT_PrimitiveHandle
GABC_GTPrimCollect::endCollecting(const GT_GEODetailListHandle &,
				GT_GEOPrimCollectData *) const
{
    return GT_PrimitiveHandle();
}
