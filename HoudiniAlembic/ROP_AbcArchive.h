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

#ifndef __ROP_AbcArchive__
#define __ROP_AbcArchive__

#include <Alembic/Abc/All.h>

#include <FS/FS_Writer.h>
#include <GABC/GABC_OError.h>
#include <GABC/GABC_OOptions.h>
#include <SYS/SYS_Types.h>
#include <UT/UT_Array.h>
#include <UT/UT_BoundingBox.h>
#include <UT/UT_SharedPtr.h>
#include <UT/UT_UniquePtr.h>

typedef GABC_NAMESPACE::GABC_OError GABC_OError;
typedef GABC_NAMESPACE::GABC_OOptions GABC_OOptions;

typedef Alembic::Abc::OArchive OArchive;
typedef Alembic::Abc::OBox3dProperty OBox3dProperty;
typedef Alembic::Abc::OObject OObject;
typedef Alembic::Abc::TimeSamplingPtr TimeSamplingPtr;

class ROP_AbcArchive
{
    class rop_OOptions : public GABC_OOptions
    {
    public:
	rop_OOptions(const TimeSamplingPtr &ts) : myTimeSampling(ts) {}

	virtual const TimeSamplingPtr &timeSampling() const
	    { return myTimeSampling; }

    private:
	const TimeSamplingPtr &myTimeSampling;
    };

public:
    ROP_AbcArchive(const char *filename, bool ogawa, GABC_OError &err);

    /// Returns true if the archive was successfully opened for writing.
    bool isValid() { return myArchive != nullptr; }

    /// @{
    /// Time sampling
    void setTimeSampling(int nframes, fpreal tstart, fpreal tend,
			 int mb_samples,
			 fpreal shutter_open, fpreal shutter_close);
    TimeSamplingPtr getTimeSampling() const { return myTimeSampling; }
    /// @}

    /// @{
    /// Manage sub-frame time sampling.  To write a single frame, you'll want
    /// to have code like:
    /// @code
    ///   void writeFrame(fpreal time)
    ///   {
    ///       for (exint i = 0; i < archive.samplesPerFrame(); ++i)
    ///       {
    ///           archive.setCookTime(time, i);
    ///           ...
    ///       }
    ///   }
    /// @endcode
    exint getSamplesPerFrame() const { return myBlurTimes.entries(); }
    void setCookTime(fpreal tstart, exint idx);
    fpreal getCookTime() const { return myCookTime; }
    /// @}

    /// Total number of samples being exported
    exint getSampleCount() const { return mySampleCount; }

    /// returns the archive's top OObject
    const OObject getTop() const { return myArchive->getTop(); }

    /// Sets the computed box.
    void setBoundingBox(const UT_BoundingBox &box);

    /// The options used during Alembic archive export
    GABC_OOptions &getOOptions() { return myOOptions; }
    /// Handler for error messages and warnings during the export of
    /// Alembic geometry.
    GABC_OError &getOError() { return myOError; }

private:
    UT_UniquePtr<FS_Writer> myWriter;
    UT_UniquePtr<OArchive> myArchive;

    TimeSamplingPtr myTimeSampling;
    UT_Array<fpreal> myBlurTimes;
    UT_BoundingBox myCachedBounds;
    exint mySampleCount;
    exint myCachedBoundsCount;
    bool myHasCachedBounds;
    fpreal myCookTime;
    OBox3dProperty myBoxProperty;

    rop_OOptions myOOptions;
    GABC_OError &myOError;
};

typedef UT_SharedPtr<ROP_AbcArchive> ROP_AbcArchivePtr;

#endif
