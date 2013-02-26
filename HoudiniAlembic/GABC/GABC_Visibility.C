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
 * NAME:	GABC_Visibility.h ( GABC Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_Visibility.h"

void
GABC_VisibilityCache::clear()
{
    myVisible = GABC_VISIBLE_DEFER;
    delete myCache;
    myCache = NULL;
}

void
GABC_VisibilityCache::set(GABC_VisibilityType vtype,
	const GABC_ChannelCache *vcache)
{
    clear();
    myVisible = vtype;
    myCache = vcache ? new GABC_ChannelCache(*vcache) : NULL;
}
