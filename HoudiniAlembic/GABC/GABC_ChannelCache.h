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

#ifndef __GABC_ChannelCache__
#define __GABC_ChannelCache__

#include "GABC_API.h"
#include "GABC_Include.h"
#include <GT/GT_DataArray.h>
#include <Alembic/Abc/IObject.h>

/// Alembic will return arrays or scalars for a given time sample.  This class
/// will "cache" these samples over time.  Each item in the array represents a
/// time sample.  This class requires the size of the array/scalar to remain
/// constant over time.
class GABC_API GABC_ChannelCache
{
public:
    typedef Alembic::Abc::TimeSamplingPtr	TimeSamplingPtr;
    typedef Alembic::Abc::index_t		index_t;
    typedef Alembic::Abc::chrono_t		chrono_t;

    GABC_ChannelCache();
    GABC_ChannelCache(const GABC_ChannelCache &src);
    GABC_ChannelCache(const GT_DataArrayHandle &data,
			    const TimeSamplingPtr &time);
    ~GABC_ChannelCache();

    bool			valid() const		{ return myData; }
    bool			animated() const	{ return myTime; }
    const TimeSamplingPtr	&time() const		{ return myTime; }
    const GT_DataArrayHandle	&data() const		{ return myData; }

    int		tupleSize() const
		    { return myData ? myData->getTupleSize() : 0; }
    exint	sampleCount() const
		    { return myData ? myData->entries() : 0; }
    GT_Storage	storageType() const
		    { return myData ? myData->getStorage() : GT_STORE_INVALID; }

    // Return the sample associated with the given time
    exint	getSample(fpreal t) const
		{
		    exint	sample;
		    getSample(t, sample, t);
		    return sample;
		}
    /// Return the sample associated with the given time, but also return the
    /// fraction which can be used to interpolate to the next sample.  That is,
    /// a value of 0.3 would indicate that the returned sample will contribute
    /// 0.7 to the result while (sample+1) will contribute 0.3
    void	getSample(fpreal t, exint &sample, fpreal &fraction) const
    {
	if (!myTime)
	{
	    sample = 0;
	    fraction = 0;
	    return;
	}
	std::pair<index_t, chrono_t>  t0 = myTime->getFloorIndex(t,
							sampleCount());
	sample = t0.first;
	fraction = t0.second;
	UT_ASSERT(sample >= 0 && sample < sampleCount());
    }

private:
    GT_DataArrayHandle	myData;
    TimeSamplingPtr	myTime;
};

#endif
