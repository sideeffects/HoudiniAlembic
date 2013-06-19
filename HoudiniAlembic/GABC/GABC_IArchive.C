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

//#define GABC_OGAWA

#include "GABC_IArchive.h"
#include "GABC_IObject.h"
#include <UT/UT_Map.h>
#include <UT/UT_SysClone.h>
#include <UT/UT_String.h>
#include <UT/UT_WorkArgs.h>

#if defined(GABC_OGAWA)
    #include <Alembic/AbcCoreFactory/All.h>
#else
    #include <Alembic/AbcCoreHDF5/All.h>
#endif

using namespace GABC_NAMESPACE;

//#define SHOW_COUNTS
#include <UT/UT_ShowCounts.h>
UT_COUNTER(theCount, "GABC_IArchive")

namespace
{
    typedef Alembic::Abc::IObject			IObject;
    typedef Alembic::Abc::IArchive			IArchive;
#if defined(GABC_OGAWA)
    typedef Alembic::AbcCoreFactory::IFactory		IFactory;
#endif
}

UT_Lock	*GABC_IArchive::theLock = NULL;

GABC_IArchivePtr
GABC_IArchive::open(const std::string &path)
{
    if (!theLock)
    {
	theLock = new UT_Lock();
	GABC_IObject::init();
    }
    return GABC_IArchivePtr(new GABC_IArchive(path));
}

GABC_IArchive::GABC_IArchive(const std::string &path)
    : myFilename(path)
    , myPurged(false)
{
    UT_INC_COUNTER(theCount);
    if (UTisstring(path.c_str()) && UTaccess(path.c_str(), R_OK) == 0)
    {
	try
	{
#if defined(GABC_OGAWA)
	    IFactory	factory;
	    myArchive = factory.getArchive(path);
#else
	    myArchive = IArchive(Alembic::AbcCoreHDF5::ReadArchive(), path);
#endif
	}
	catch (const std::exception &e)
	{
	    myError = e.what();
	    myArchive = IArchive();
	}
    }
}

GABC_IArchive::~GABC_IArchive()
{
    UT_DEC_COUNTER(theCount);
    GABC_AlembicLock	lock(*this);	// Lock for member data deletion
    if (!purged())
	purgeObjects();	// Clear all my objects out
}

void
GABC_IArchive::resolveObject(GABC_IObject &obj)
{
    GABC_AlembicLock	lock(*this);

    UT_ASSERT(!purged());
    if (!valid())
	return;
    UT_String		path(obj.objectPath());
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
    GABC_IArchive	*me = const_cast<GABC_IArchive *>(this);
    IObject	root = me->myArchive.getTop();
    return GABC_IObject(me, root);
}

void
GABC_IArchive::purgeObjects()
{
    GABC_AlembicLock	lock(*this); // Need lock for myObjects

    UT_ASSERT(!purged());
    myPurged = true;
    SetType::iterator		end = myObjects.end();
    for (SetType::iterator it = myObjects.begin(); it != end; ++it)
    {
	(*it)->purge();
    }
    myArchive = IArchive();
}

void
GABC_IArchive::reference(GABC_IItem *item)
{
    UT_ASSERT(!myObjects.count(item));
    UT_ASSERT(!purged());
    myObjects.insert(item);
}

void
GABC_IArchive::unreference(GABC_IItem *item)
{
    UT_ASSERT(myObjects.count(item));
    myObjects.erase(item);
}
