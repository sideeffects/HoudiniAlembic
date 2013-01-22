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
    , mySaveAttributes(true)
    , myUseDisplaySOP(false)
    , myFullBounds(false)
{
}

GABC_OOptions::~GABC_OOptions()
{
}
