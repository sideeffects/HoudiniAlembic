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
 * NAME:	GABC_ChannelCache.h ( GABC Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_ChannelCache.h"

GABC_ChannelCache::GABC_ChannelCache()
    : myData()
    , myTime()
{
}

GABC_ChannelCache::GABC_ChannelCache(const GABC_ChannelCache &src)
    : myData(src.myData)
    , myTime(src.myTime)
{
}

GABC_ChannelCache::GABC_ChannelCache(const GT_DataArrayHandle &data,
	const TimeSamplingPtr &time)
    : myData(data)
    , myTime(time)
{
    if (myTime && myTime->getNumStoredTimes() == 0)
	myTime = TimeSamplingPtr();
}

GABC_ChannelCache::~GABC_ChannelCache()
{
}
