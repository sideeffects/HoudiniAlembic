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

#ifndef __GABC_ChannelCache__
#define __GABC_ChannelCache__

#include "GABC_API.h"
#include "GABC_Include.h"
#include <GT/GT_DataArray.h>
#include <Alembic/Abc/IObject.h>

namespace GABC_NAMESPACE
{
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
}

#endif
