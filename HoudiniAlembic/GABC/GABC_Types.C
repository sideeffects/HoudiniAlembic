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

#include "GABC_Types.h"
#include <UT/UT_FSA.h>

using namespace GABC_NAMESPACE;

namespace
{
    static UT_FSATable	theNodeTypeTable(
	GABC_XFORM,		"Xform",
	GABC_POLYMESH,		"PolyMesh",
	GABC_SUBD,		"SubD",
	GABC_CAMERA,		"Camera",
	GABC_FACESET,		"FaceSet",
	GABC_CURVES,		"Curves",
	GABC_POINTS,		"Points",
	GABC_NUPATCH,		"NuPatch",
	GABC_LIGHT,		"Light",
	GABC_MATERIAL,		"Material",
	-1, NULL
    );
}

namespace GABC_NAMESPACE
{
const char *
GABCnodeType(GABC_NodeType type)
{
    const char	*name = theNodeTypeTable.getToken(type);
    return name ? name : "<unknown>";
}

GABC_NodeType
GABCnodeType(const char *type)
{
    return static_cast<GABC_NodeType>(theNodeTypeTable.findSymbol(type));
}

}
