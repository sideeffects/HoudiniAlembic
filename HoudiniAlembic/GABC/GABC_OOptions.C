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
 * NAME:	GABC_OOptions.C
 *
 * COMMENTS:
 */

#include "GABC_OOptions.h"

GABC_OOptions::GABC_OOptions()
    : myOptimizeSpace(OPTIMIZE_DEFAULT)
    , myFaceSetMode(FACESET_DEFAULT)
    , mySubdGroup()
    , mySaveAttributes(true)
    , myUseDisplaySOP(false)
    , myFullBounds(false)
    , myAttributeStars(true)
{
    for (int i = 0; i < GA_ATTRIB_OWNER_N; ++i)
	myAttributePatterns[i] = "*";
}

GABC_OOptions::~GABC_OOptions()
{
}

void
GABC_OOptions::checkAttributeStars()
{
    myAttributeStars = true;
    for (int i = 0; i < GA_ATTRIB_OWNER_N; ++i)
    {
	if (myAttributePatterns[i] != "*")
	{
	    myAttributeStars = false;
	    break;
	}
    }
}

bool
GABC_OOptions::matchAttribute(GA_AttributeOwner own, const char *name) const
{
    if (myAttributeStars)
	return true;
    UT_String	str(name);
    return str.multiMatch(myAttributePatterns[own]) != 0;
}

bool
GABC_OOptions::matchAttribute(Alembic::AbcGeom::GeometryScope scope,
			const char *name) const
{
    if (myAttributeStars)
	return true;
    switch (scope)
    {
	case Alembic::AbcGeom::kConstantScope:
	    return matchAttribute(GA_ATTRIB_DETAIL, name);
	case Alembic::AbcGeom::kUniformScope:
	case Alembic::AbcGeom::kUnknownScope:
	    return matchAttribute(GA_ATTRIB_PRIMITIVE, name);
	case Alembic::AbcGeom::kVaryingScope:
	    return matchAttribute(GA_ATTRIB_POINT, name);
	case Alembic::AbcGeom::kVertexScope:
	case Alembic::AbcGeom::kFacevaryingScope:
	    return matchAttribute(GA_ATTRIB_VERTEX, name);
    }
    return true;
}
