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
 * NAME:	GABC_Types.h ( GABC Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_Types.h"
#include <UT/UT_FSA.h>

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
    static UT_FSATable	theAnimationTypeTable(
	GABC_ANIMATION_CONSTANT,	"constant",
	GABC_ANIMATION_TRANSFORM,	"transform",
	GABC_ANIMATION_ATTRIBUTE,	"attribute",
	GABC_ANIMATION_TOPOLOGY,	"topology",
	-1, NULL
    );
}

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

const char *
GABCanimationType(GABC_AnimationType type)
{
    const char	*name = theAnimationTypeTable.getToken(type);
    return name ? name : "<invalid>";
}

GABC_AnimationType
GABCanimationType(const char *type)
{
    return static_cast<GABC_AnimationType>(theAnimationTypeTable.findSymbol(type));
}
