/*
 * Copyright (c) 2017
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

#include "GABC_OOptions.h"

using namespace GABC_NAMESPACE;

GABC_OOptions::GABC_OOptions()
    : mySubdGroup()
    , myPrimToDetailPattern()
    , myFullBounds(false)
    , myForcePrimToDetail(false)
{
    for(int i = 0; i < GA_ATTRIB_OWNER_N; ++i)
	myAttributePatterns[i] = "*";
}

// Check if the given attribute matches the mask for it's level.
bool
GABC_OOptions::matchAttribute(GA_AttributeOwner own, const char *name) const
{
    if(myPathAttribute == name)
	return false;

    UT_String	str(name);
    return str.multiMatch(myAttributePatterns[own]) != 0;
}

// Determine the attribute owner from the Alembic scope and compare the
// attribute against the mask for that owner.
bool
GABC_OOptions::matchAttribute(Alembic::AbcGeom::GeometryScope scope,
			const char *name) const
{
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
