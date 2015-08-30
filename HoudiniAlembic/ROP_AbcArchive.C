/*
 * Copyright (c) 2015
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
#include <SYS/SYS_Version.h>
#include <Alembic/AbcCoreHDF5/All.h>
#if defined(GABC_OGAWA)
    #include <Alembic/AbcCoreOgawa/All.h>
#endif

using namespace GABC_NAMESPACE;

namespace
{
    typedef Alembic::Abc::OArchive	OArchive;
    typedef Alembic::Abc::Box3d		Box3d;

    OArchive
    openHDF5(const char *filename, const char *version, const char *userinfo)
    {
	return Alembic::Abc::CreateArchiveWithInfo(
			Alembic::AbcCoreHDF5::WriteArchive(),
			filename,
			version,
			userinfo,
			Alembic::Abc::ErrorHandler::kThrowPolicy);
    }

    OArchive
    openOgawa(const char *filename, const char *version, const char *userinfo)
    {
#if defined(GABC_OGAWA)
	return Alembic::Abc::CreateArchiveWithInfo(
			Alembic::AbcCoreOgawa::WriteArchive(),
			filename,
			version,
			userinfo,
			Alembic::Abc::ErrorHandler::kThrowPolicy);
#else
	// No Ogawa support
	return openHDF5(filename, version, userinfo);
#endif
    }
}

ROP_AbcArchive::ROP_AbcArchive()
    : myArchive()
    , myTSIndex(-1)
    , myTimeDependent(false)
{
    myBox.makeInvalid();
}

ROP_AbcArchive::~ROP_AbcArchive()
{
    close();
}



bool
ROP_AbcArchive::open(GABC_OError &err, const char *file, const char *format)
{
    close();

    // Since HDF5 doesn't allow writing to a file already opened for reading,
    // clear any IArchives which might be hanging around.
    GABC_Util::clearCache();

    UT_WorkBuffer	version;
    UT_WorkBuffer	userinfo;
    UT_String		hipfile;
    UT_WorkBuffer	timestamp;

    version.sprintf("Houdini%s", SYS_Version::full());

    OPgetDirector()->getCommandManager()->getVariable("HIPFILE", hipfile);
    UT_Date::dprintf(timestamp, "%Y-%m-%d %H:%M:%S", time(0));
    userinfo.sprintf("Exported from %s on %s",
	    hipfile.buffer(), timestamp.buffer());
    try
    {
	if (!strcmp(format, "hdf5"))
	    myArchive = openHDF5(file, version.buffer(), userinfo.buffer());
	else
	{
#if !defined(GABC_OGAWA)
	    if (!strcmp(format, "ogawa"))
	    {
		// User has specified Ogawa, but with no Ogawa support.
		err.warningString("Ogawa not supported in this version "
			    "of Houdini");
	    }
#endif
	    // Default format
	    myArchive = openOgawa(file, version.buffer(), userinfo.buffer());
	}
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
    myTSIndex = myArchive.addTimeSampling(*(ctx.timeSampling()));
    myBox.makeInvalid();
    if (!startChildren(myArchive.getTop(), err, ctx, myBox))
	return false;
    if (ctx.fullBounds())
    {
	myBoxProp = Alembic::AbcGeom::CreateOArchiveBounds(myArchive, myTSIndex);
	Box3d	b3 = GABC_Util::getBox(myBox);
	myBoxProp.set(b3);
    }
    // Note this is a "custom" tag on exports to export the time samples
    if (myTSIndex >= 0)
    {
	UT_WorkBuffer	pname;
	pname.sprintf("%d.samples", myTSIndex);
	Alembic::Abc::OUInt32Property	samp(myArchive.getTop().getProperties(),
		pname.buffer());
	samp.set(ctx.totalSamples());
    }
    updateTimeDependentKids();
    return true;
}

bool
ROP_AbcArchive::nextFrame(GABC_OError &err, const ROP_AbcContext &ctx)
{
    myBox.makeInvalid();
    bool status = updateChildren(err, ctx, myBox);
    if (ctx.fullBounds())
    {
	Box3d	b3 = GABC_Util::getBox(myBox);
	myBoxProp.set(b3);
    }
    return status;
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
