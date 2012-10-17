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

#ifndef __GABC_Types__
#define __GABC_Types__

#include "GABC_API.h"

enum GABC_NodeType
{
    GABC_UNKNOWN=-1,
    GABC_XFORM,
    GABC_POLYMESH,
    GABC_SUBD,
    GABC_CAMERA,
    GABC_FACESET,
    GABC_CURVES,
    GABC_POINTS,
    GABC_NUPATCH,
    GABC_MAYA_LOCATOR,	// Transform masquerading as a Maya Locator
    GABC_LIGHT,		// Added in Alembic1.1
    GABC_MATERIAL,	// Added in Alembic1.1
};

enum GABC_AnimationType
{
    GABC_ANIMATION_INVALID = -1,
    GABC_ANIMATION_CONSTANT,	// Constant animation (i.e. no animation)
    GABC_ANIMATION_TRANSFORM,	// Only transform is animated
    GABC_ANIMATION_ATTRIBUTE,	// Attribute or transform are animated
    GABC_ANIMATION_TOPOLOGY,	// Topology is animated (changes)
};

GABC_API extern const char	*GABCnodeType(GABC_NodeType type);
GABC_API extern GABC_NodeType	 GABCnodeType(const char *type);
GABC_API extern const char	*GABCanimationType(GABC_AnimationType type);
GABC_API extern GABC_AnimationType	 GABCanimationType(const char *type);

#endif
