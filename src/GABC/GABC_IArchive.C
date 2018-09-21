/*
 * Copyright (c) 2018
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

#include "GABC_IArchive.h"
#include "GABC_IObject.h"
#include <UT/UT_Map.h>
#include <UT/UT_SysClone.h>
#include <UT/UT_String.h>
#include <UT/UT_Access.h>
#include <UT/UT_FileStat.h>
#include <UT/UT_PathSearch.h>
#include <UT/UT_WorkArgs.h>

#if defined(GABC_OGAWA)
    #include <Alembic/AbcCoreFactory/All.h>
#endif
#include <Alembic/AbcCoreHDF5/All.h>

using namespace GABC_NAMESPACE;

//#define SHOW_COUNTS
#include <UT/UT_ShowCounts.h>
UT_COUNTER(theCount, "GABC_IArchive")

namespace
{
    using IObject = Alembic::Abc::IObject;
    using IArchive = Alembic::Abc::IArchive;
#if defined(GABC_OGAWA)
    using IFactory = Alembic::AbcCoreFactory::IFactory;
#endif
    using ArchiveCache = UT_Map<std::vector<std::string>, GABC_IArchive *>;

    static ArchiveCache	theArchiveCache;
}

UT_Lock	*GABC_IArchive::theLock = NULL;

std::string
GABC_IArchive::filenamesToString(const std::vector<std::string> &filenames)
{
    UT_WorkBuffer buffer;
    for (int i = 0; i < filenames.size(); ++i)
    {
	if(i)
	   buffer.append(", ");
	buffer.append(filenames[i].c_str());
    }
    return buffer.toStdString();
}

GABC_IArchivePtr
GABC_IArchive::open(const std::vector<std::string> &paths, int num_streams)
{
    if (!theLock)
    {
	theLock = new UT_Lock();
	GABC_IObject::init();
    }

    UT_AutoLock	lock(*theLock);
    auto it = theArchiveCache.find(paths);
    if (it != theArchiveCache.end())
	return it->second;

    GABC_IArchive *arch = new GABC_IArchive(paths, num_streams);
    theArchiveCache.emplace(paths, arch);
    return GABC_IArchivePtr(arch);
}

void
GABC_IArchive::closeAndDelete()
{
    UT_AutoLock	lock(*theLock);
    // In the time between the atomic decrement and the lock acquisition, it's
    // possible another thread my have called open on my path.  This would have
    // incremented my reference count.
    if (myRefCount.load() != 0)
    {
	// It's happened!
	UT_ASSERT(theArchiveCache.find(myFileNames) != theArchiveCache.end());
	return;
    }
    delete this;
}

GABC_IArchive::GABC_IArchive(const std::vector<std::string> &paths, int num_streams)
    : myFileNames(paths)
    , myPurged(false)
    , myIsOgawa(false)
{
    UT_INC_COUNTER(theCount);

    openArchive(paths, num_streams);
}

GABC_IArchive::~GABC_IArchive()
{
    UT_DEC_COUNTER(theCount);
    GABC_AlembicLock	lock(*this);	// Lock for member data deletion

    if(!myPurged)
    {
	for (auto &it : myObjects)
	    it->purge();
    }
    theArchiveCache.erase(myFileNames);
}

void
GABC_IArchive::openArchive(
    const std::vector<std::string> &paths, int num_streams)
{
    myArchive.reset();

    if (paths.size() == 0)
	return;

    IFactory   factory;
    bool canAccess = true;

    std::vector<std::string> mapped_paths;
    for (int i = 0; i < paths.size(); ++i)
    {
	UT_String tmp = paths[i].c_str();
	UT_PathSearch::pathMap(tmp);

	UT_FileStat file_stat;
	if(UTfileStat(tmp.c_str(), &file_stat)
	    || !file_stat.isFile()
	    || (UTaccess(tmp.c_str(), R_OK) < 0))
	{
	    canAccess = false;
	    break;
	}
	mapped_paths.push_back(tmp.c_str());
    }

    // Try to open using standard Ogawa file access
    if (canAccess)
    {
#if defined(GABC_OGAWA)
	IFactory	factory;

	if(num_streams != -1)
	    factory.setOgawaNumStreams(num_streams);

	try
	{
	    IFactory::CoreType	archive_type;
	    myArchive = factory.getArchive(mapped_paths, archive_type);
	    myIsOgawa = (archive_type == IFactory::kOgawa);
	}
	catch (const std::exception &e)
	{
	    myError = e.what();
	    myArchive = IArchive();
	}
    }

    if (paths.size() == 1
	&& !myArchive.valid()
	&& openStream(paths[0], num_streams))
    {
	std::vector<std::istream *> stream_list;
	for(auto &e : myStreams)
	    stream_list.push_back(e.myStream);
	IFactory::CoreType	archive_type;
	try
	{
	    myArchive = factory.getArchive(stream_list, archive_type);
	    myError.clear();
	    myIsOgawa = (archive_type == IFactory::kOgawa);
	}
	catch (const std::exception &)
	{
	    // It may still be an HDF5 archive
	    clearStream();
	    myArchive = IArchive();
	    myIsOgawa = false;
	}
    }
#endif
    if (!myArchive.valid())
    {
	// Try HDF5 -- the stream interface only works with Ogawa
	if (mapped_paths.size() > 1)
	    throw std::ios_base::failure("Alembic layers are not supported with HDF5");

	if (canAccess)
	{
	    myIsOgawa = false;
	    try
	    {
		myArchive = IArchive(Alembic::AbcCoreHDF5::ReadArchive(),
				     mapped_paths[0]);
	    }
	    catch (const std::exception &e)
	    {
		myError = e.what();
		myArchive = IArchive();
	    }
	}
    }
}

void
GABC_IArchive::reopenStream(int num_streams)
{
    if(myArchive)
    {
	clearStream();
	openArchive(myFileNames, num_streams);

        for (auto &it : myObjects)
	{
	    GABC_IObject *object = dynamic_cast<GABC_IObject *>(it);
	    if(object)
		resolveObject(*object);
	}
    }
}

bool
GABC_IArchive::openStream(const std::string &path, int num_streams)
{
    num_streams = SYSmax(1, num_streams);

    myStreams.entries(num_streams);
    for(int i=0; i<num_streams; i++)
    {
	myStreams(i).myReader = new gabc_istream(path.c_str(), NULL);
	if (!myStreams(i).myReader->isValid())
	{
	    UT_WorkBuffer	wbuf;
	    wbuf.sprintf("Unable to open '%s'", path.c_str());
	    myError = wbuf.toStdString();
	    clearStream();
	    return false;
	}
	myStreams(i).myStreamBuf = new gabc_streambuf(*myStreams(i).myReader);
	myStreams(i).myStream = new std::istream(myStreams(i).myStreamBuf);
    }
    return true;
}

void
GABC_IArchive::clearStream()
{
    myStreams.clear();
}

void
GABC_IArchive::resolveObject(GABC_IObject &obj)
{
    GABC_AlembicLock	lock(*this);

    if (!valid())
	return;
    UT_String		path(obj.getFullName());
    UT_WorkArgs		args;
    path.tokenize(args, '/');
    IObject	curr = myArchive.getTop();
    for (int i = 0; curr.valid() && i < args.entries(); ++i)
    {
	if (UTisstring(args(i)))
	    curr = curr.getChild(args(i));
    }
    obj.setObject(curr);
}

GABC_IObject
GABC_IArchive::getTop() const
{
    if (!valid())
	return GABC_IObject();

    GABC_AlembicLock	 lock(*this);
    return GABC_IObject(const_cast<GABC_IArchive *>(this), myArchive.getTop());
}

void
GABC_IArchive::purgeObjects()
{
    GABC_AlembicLock    lock(*this);

    UT_ASSERT(!myPurged);
    myPurged = true;
    for (auto &it : myObjects)
        it->purge();
    myArchive = IArchive();
    clearStream();
    theArchiveCache.erase(myFileNames);
}

void
GABC_IArchive::reference(GABC_IItem *item)
{
    UT_ASSERT(!myObjects.count(item));
    myObjects.insert(item);
}

void
GABC_IArchive::unreference(GABC_IItem *item)
{
    UT_ASSERT(myObjects.count(item));
    myObjects.erase(item);
}
