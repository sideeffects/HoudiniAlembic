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

#include "ROP_AbcArchive.h"
#include <GABC/GABC_Util.h>

#include <Alembic/AbcCoreHDF5/All.h>
#if defined(GABC_OGAWA)
    #include <Alembic/AbcCoreOgawa/All.h>
#endif

#include <FS/FS_Writer.h>
#include <OP/OP_Director.h>
#include <SYS/SYS_Version.h>
#include <UT/UT_Date.h>

typedef GABC_NAMESPACE::GABC_Util GABC_Util;

typedef Alembic::Abc::Box3d Box3d;
typedef Alembic::Abc::chrono_t chrono_t;
typedef Alembic::Abc::MetaData MetaData;
typedef Alembic::Abc::OArchive OArchive;
typedef Alembic::Abc::TimeSampling TimeSampling;
typedef Alembic::Abc::TimeSamplingType TimeSamplingType;

ROP_AbcArchive::ROP_AbcArchive(
    const char *filename, bool ogawa, GABC_OError &err)
    : mySampleCount(0)
    , myHasCachedBounds(false)
    , myOOptions(myTimeSampling)
    , myOError(err)
{
    // Since HDF5 doesn't allow writing to a file already opened for reading,
    // clear any IArchives which might be hanging around.
    GABC_Util::clearCache();

    auto md = Alembic::Abc::MetaData();

    // set the application name
    UT_WorkBuffer version;
    version.sprintf("Houdini %s", SYS_Version::full());
    md.set(Alembic::Abc::kApplicationNameKey, version.buffer());

    // set the date
    auto now = time(0);
    char datebuf[128];
#if defined(WIN32)
    ctime_s(datebuf, 128, &now);
#else
    ctime_r(&now, datebuf);
#endif
    if (auto nl = strchr(datebuf, '\n'))
	*nl = 0;
    md.set(Alembic::Abc::kDateWrittenKey, datebuf);

    // set the description
    UT_String hipfile;
    OPgetDirector()->getCommandManager()->getVariable("HIPFILE", hipfile);

    UT_WorkBuffer timestamp;
    UT_Date::dprintf(timestamp, "%Y-%m-%d %H:%M:%S", time(0));

    UT_WorkBuffer userinfo;
    userinfo.sprintf("Exported from %s on %s",
		     hipfile.buffer(), timestamp.buffer());
    md.set(Alembic::Abc::kUserDescriptionKey, userinfo.buffer());

    try
    {
#if defined(GABC_OGAWA)
	if(ogawa)
	{
	    myWriter.reset(new FS_Writer(filename));
	    if(myWriter)
	    {
		Alembic::AbcCoreAbstract::ArchiveWriterPtr writer;
		auto factory = Alembic::AbcCoreOgawa::WriteArchive();
		myArchive.reset(new OArchive(factory(myWriter->getStream(), md), Alembic::Abc::kWrapExisting, Alembic::Abc::ErrorHandler::kThrowPolicy));
		return;
	    }
	}
#endif

	auto factory = Alembic::AbcCoreHDF5::WriteArchive();
	myArchive.reset(new OArchive(factory(filename, md), Alembic::Abc::kWrapExisting, Alembic::Abc::ErrorHandler::kThrowPolicy));
    }
    catch(const std::exception &e)
    {
	myOError.error("Error creating archive: %s", e.what());
    }
}

void
ROP_AbcArchive::setTimeSampling(
    int nframes, fpreal tstart, fpreal tend,
    int mb_samples, fpreal shutter_open, fpreal shutter_close)
{
    fpreal tdelta = tend - tstart;

    fpreal spf = CHgetManager()->getSecsPerSample();
    
    fpreal tstep;
    if(nframes < 2 || SYSequalZero(tdelta))
	tstep = spf;
    else
	tstep = tdelta / (nframes - 1);

    tstart += spf;
    if(mb_samples < 2)
    {
	myTimeSampling.reset(new TimeSampling(tstep, tstart));
	myBlurTimes.append(0);
    }
    else if(shutter_open < shutter_close)
    {
	fpreal shutter_step = shutter_close - shutter_open;
	fpreal blur_offset = shutter_open;
	if(SYSequalZero(shutter_step - 1))
	{
	    shutter_step /= mb_samples;
	    // Uniform time sampling
	    myTimeSampling.reset(new TimeSampling(tstep/mb_samples, tstart));
	    for(exint i = 0; i < mb_samples; ++i, blur_offset += shutter_step)
		myBlurTimes.append(blur_offset * spf);
	}
	else
	{
	    shutter_step /= mb_samples - 1;
	    std::vector<chrono_t> abcTimes;
	    for(int i = 0; i < mb_samples; ++i, blur_offset += shutter_step)
	    {
		myBlurTimes.append(blur_offset * spf);
		abcTimes.push_back(tstart + myBlurTimes.last());
	    }
	    TimeSamplingType tstype(abcTimes.size(), spf);
	    myTimeSampling.reset(new TimeSampling(tstype, abcTimes));
	}
    }
    else
    {
	// invalid shutter times, disable motion blur
	myTimeSampling.reset(new TimeSampling(tstep, tstart));
	myBlurTimes.append(0);
    }
    UT_ASSERT(myBlurTimes.entries() >= 1);

    uint32_t idx = myArchive->addTimeSampling(*myTimeSampling);
    // add custom tag for pyalembic's cask wrapper.
    UT_WorkBuffer pname;
    pname.sprintf("%d.samples", idx);
    Alembic::Abc::OUInt32Property samp(myArchive->getTop().getProperties(),
				       pname.buffer());
    samp.set(nframes * myBlurTimes.entries());

    if(myOOptions.fullBounds())
	myBoxProperty = Alembic::AbcGeom::CreateOArchiveBounds(*myArchive, idx);
}

void
ROP_AbcArchive::setCookTime(fpreal tstart, exint idx)
{
    myCookTime = tstart + myBlurTimes(idx);
    ++mySampleCount;
}

void
ROP_AbcArchive::setBoundingBox(const UT_BoundingBox &box)
{
    if(myOOptions.fullBounds())
    {
	// avoid writing multiple samples for static bounds
	if(myHasCachedBounds && (myCachedBounds == box))
	    ++myCachedBoundsCount;
	else
	{
	    if(myHasCachedBounds)
	    {
		Box3d b = GABC_Util::getBox(myCachedBounds);
		for(; myCachedBoundsCount; --myCachedBoundsCount)
		    myBoxProperty.set(b);
		myHasCachedBounds = false;
	    }
	    else if(mySampleCount == 1)
	    {
		myCachedBounds = box;
		myCachedBoundsCount = 0;
		myHasCachedBounds = true;
	    }
	    myBoxProperty.set(GABC_Util::getBox(box));
	}
    }
}
