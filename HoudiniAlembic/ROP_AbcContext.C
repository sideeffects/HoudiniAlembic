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
 * NAME:	ROP_AbcContext.C
 *
 * COMMENTS:
 */

#include "ROP_AbcContext.h"
#include <CH/CH_Manager.h>
#include <UT/UT_WorkBuffer.h>

namespace
{
    typedef Alembic::Abc::chrono_t		chrono_t;
    typedef Alembic::Abc::TimeSampling		TimeSampling;
    typedef Alembic::Abc::TimeSamplingPtr	TimeSamplingPtr;
    typedef Alembic::Abc::TimeSamplingType	TimeSamplingType;
}

ROP_AbcContext::ROP_AbcContext()
    : GABC_OOptions()
    , myTimeSampling()
    , mySingletonSOP(NULL)
    , myPartitionAttribute()
    , myPartitionMode(ROP_AbcContext::PATHMODE_FULLPATH)
    , myCollapseIdentity(false)
{
}

ROP_AbcContext::~ROP_AbcContext()
{
}

void
ROP_AbcContext::setTimeSampling(fpreal tstart,
	fpreal tstep,
	int mb_samples,
	fpreal shutter_open,
	fpreal shutter_close)
{
    fpreal	spf = CHgetManager()->getSecsPerSample();

    tstart += spf;
    myBlurTimes.resize(0);
    if (mb_samples < 2)
    {
	myTimeSampling.reset(new TimeSampling(tstep, tstart));
	myBlurTimes.append(0);
    }
    else if (shutter_open < shutter_close)
    {
	fpreal	shutter_step = (shutter_close - shutter_open) / mb_samples;
	fpreal	blur_offset = shutter_open;
	if (SYSequalZero(shutter_open) && SYSequalZero(shutter_close-1))
	{
	    // Uniform time sampling
	    myTimeSampling.reset(new TimeSampling(tstep/mb_samples, tstart));
	    for (exint i = 0; i < mb_samples; ++i, blur_offset += shutter_step)
		myBlurTimes.append(blur_offset*spf);
	}
	else
	{
	    std::vector<chrono_t>	abcTimes;
	    for (int i = 0; i < mb_samples; ++i, blur_offset += shutter_step)
	    {
		myBlurTimes.append(blur_offset*spf);
		abcTimes.push_back(tstart + myBlurTimes.last());
	    }
	    TimeSamplingType	tstype(abcTimes.size(), spf);
	    myTimeSampling.reset(new TimeSampling(tstype, abcTimes));
	}
    }
    UT_ASSERT(myBlurTimes.entries() >= 1);
}

void
ROP_AbcContext::setTime(fpreal base_time, exint samp)
{
    UT_ASSERT(samp < myBlurTimes.entries());
    myCookContext.setTime(base_time+myBlurTimes(samp));
}

const char *
ROP_AbcContext::partitionModeValue(int mode, const char *value,
		UT_WorkBuffer &storage, char replace_char)
{
    const char	*slash;
    switch (mode)
    {
	case PATHMODE_SHAPE:
	    slash = strrchr(value, '/');
	    if (UTisstring(slash))
		return slash+1;
	    break;
	case PATHMODE_XFORM:
	    slash = strrchr(value, '/');
	    if (slash && slash != value)
	    {
		const char	*s2;
		for (s2 = slash-1; s2 > value; --s2)
		    if (*s2 == '/')
			break;
		if (*s2 == '/')
		    s2++;
		storage.strncpy(s2, slash-s2);
		return storage.buffer();
	    }
	    break;
	case PATHMODE_XFORM_SHAPE:
	    slash = strrchr(value, '/');
	    if (slash && slash != value)
	    {
		const char	*s2;
		for (s2 = slash-1; s2 > value; --s2)
		    if (*s2 == '/')
			break;
		if (*s2 == '/')
		    s2++;
		// Return a safe version of the object/shape combination
		return partitionModeValue(PATHMODE_FULLPATH, s2, storage);
	    }
	default:
	    break;
    }
    // If we get here, we're in PATHMODE_FULLPATH mode
    if (strchr(value, '/'))
    {
	storage.strcpy(value);
	char	*buf = storage.lock();
	for (int i = 0; buf[i]; ++i)
	    if (buf[i] == '/')
		buf[i] = replace_char;
	storage.release(false);	// Length hasn't changed
	value = storage.buffer();
    }
    return value;
}
