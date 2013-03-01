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

#include "ROP_AbcArchive.h"
#include "ROP_AbcContext.h"
#include <GABC/GABC_Util.h>
#include <GABC/GABC_OError.h>
#include <OP/OP_Director.h>
#include <UT/UT_Date.h>
#include <UT/UT_Version.h>
#include <Alembic/AbcCoreHDF5/All.h>

namespace
{
    typedef Alembic::Abc::OArchive	OArchive;
}

ROP_AbcArchive::ROP_AbcArchive()
    : myArchive()
    , myTimeDependent(false)
{
}

ROP_AbcArchive::~ROP_AbcArchive()
{
    close();
}

bool
ROP_AbcArchive::open(GABC_OError &err, const char *file)
{
    close();

    // Since HDF5 doesn't allow writing to a file already opened for reading,
    // clear any IArchives which might be hanging around.
    GABC_Util::clearCache();

    UT_WorkBuffer	version;
    UT_WorkBuffer	userinfo;
    UT_String		hip;
    UT_String		hipname;
    UT_String		timestamp;

    version.sprintf("Houdini%d.%d.%d",
	    UT_MAJOR_VERSION_INT,
	    UT_MINOR_VERSION_INT,
	    UT_BUILD_VERSION_INT);

    OPgetDirector()->getCommandManager()->getVariable("HIP", hip);
    OPgetDirector()->getCommandManager()->getVariable("HIPNAME", hipname);
    UT_Date::dprintf(timestamp, "%Y-%m-%d %H:%M:%S", time(0));
    userinfo.sprintf("Exported from %s/%s on %s",
	    static_cast<const char *>(hip),
	    static_cast<const char *>(hipname),
	    static_cast<const char *>(timestamp));
    try
    {
	myArchive = Alembic::Abc::CreateArchiveWithInfo(
			Alembic::AbcCoreHDF5::WriteArchive(),
			file,
			version.buffer(),
			userinfo.buffer(),
			Alembic::Abc::ErrorHandler::kThrowPolicy);
    }
    catch (const std::exception &e)
    {
	err.error("Error creating archive: %s", e.what());
	myArchive = Alembic::Abc::OArchive();
    }
    if (!myArchive.valid())
    {
	err.error("Unable to create archive: %s", file);
	myArchive = Alembic::Abc::OArchive();
	return false;
    }
    return true;
}

void
ROP_AbcArchive::close()
{
    try
    {
	myArchive = OArchive();
    }
    catch (const std::exception &e)
    {
	UT_ASSERT(0 && "Alembic exception on close!");
	fprintf(stderr, "Alembic exception on closing: %s\n", e.what());
    }
    myBox.makeInvalid();
    myTimeDependent = false;
    deleteChildren();
}

bool
ROP_AbcArchive::firstFrame(GABC_OError &err, const ROP_AbcContext &ctx)
{
    UT_ASSERT(myArchive.valid());
    if (!startChildren(myArchive.getTop(), err, ctx, myBox))
	return false;
    updateTimeDependentKids();
    return true;
}

bool
ROP_AbcArchive::nextFrame(GABC_OError &err, const ROP_AbcContext &ctx)
{
    return updateChildren(err, ctx, myBox);
}

bool
ROP_AbcArchive::start(const OObject &,
	GABC_OError &, const ROP_AbcContext &, UT_BoundingBox &)
{
    UT_ASSERT(0);
    return false;
}

bool
ROP_AbcArchive::update(GABC_OError &err,
	const ROP_AbcContext &ctx, UT_BoundingBox &box)
{
    UT_ASSERT(0);
    return false;
}

bool
ROP_AbcArchive::getLastBounds(UT_BoundingBox &box) const
{
    box = myBox;
    return true;
}

bool
ROP_AbcArchive::selfTimeDependent() const
{
    return myTimeDependent;
}
