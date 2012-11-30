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
 * NAME:	GABC_IArchive.h ( GABC Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_IArchive.h"
#include "GABC_IObject.h"
#include <UT/UT_Map.h>
#include <UT/UT_SysClone.h>
#include <UT/UT_String.h>
#include <UT/UT_WorkArgs.h>
#include <Alembic/AbcCoreHDF5/All.h>

#define SHOW_COUNTS
#include <UT/UT_ShowCounts.h>
UT_COUNTER(theCount, "GABC_IArchive")

namespace
{
    typedef UT_Map<std::string, GABC_IArchivePtr>	ArchiveCache;
    typedef Alembic::Abc::IObject			IObject;
    typedef Alembic::Abc::IArchive			IArchive;

#if 0
    class objectCacheItem
    {
    public:
	objectCacheItem()
	    : myObject()
	{
	}
	objectCacheItem(const GABC_IObject &obj)
	    : myObject(obj)
	{
	}
	objectCacheItem(const objectCacheItem &src)
	    : myObject(src.myObject)
	{
	}
	objectCacheItem	&operator=(const objectCacheItem &src)
	{
	    myObject = src.myObject;
	    return *this;
	}
    private:
	GABC_IObject	myObject;
    };
#endif

    static ArchiveCache			*theArchiveCache;

    static ArchiveCache &
    getCache()
    {
	if (!theArchiveCache)
	    theArchiveCache = new ArchiveCache();
	return *theArchiveCache;
    }
}

#if 0
// Must be in global namespace
class gabc_ObjectCache
{
public:
    typedef UT_Map<std::string, objectCacheItem>	MapType;
    gabc_ObjectCache()
	: myCache()
    {
    }
    ~gabc_ObjectCache()
    {
	clear();
    }
    void	clear()	{ myCache.clear(); }
    void	addObjects(const GABC_IObject &head)
		{
		    exint		nkids = head.getNumChildren();
		    myCache[head.getFullName()] = objectCacheItem(head);
		    for (exint i = 0; i < nkids; ++i)
			addObjects(head.getChild(i));
		}
private:
    MapType	myCache;
};
#endif

UT_Lock	*GABC_IArchive::theLock = NULL;

GABC_IArchivePtr
GABC_IArchive::open(const std::string &path)
{
    if (!theLock)
    {
	theLock = new UT_Lock();
	GABC_IObject::init();
    }

    ArchiveCache		&cache = getCache();
    ArchiveCache::iterator	 it = cache.find(path);
    GABC_IArchivePtr		 arch;

    if (it != cache.end())
    {
	arch = it->second;
    }
    else
    {
	arch = new GABC_IArchive(path);
	cache[path] = arch;
    }
    return arch;
}

void
GABC_IArchive::closeArchive(const std::string &path)
{
    UT_AutoLock	lock(*theLock);

    ArchiveCache		&cache = getCache();
    ArchiveCache::iterator	it = cache.find(path);
    if (it != cache.end())
    {
	it->second->purgeObjects();
	cache.erase(it);
    }
}

void
GABC_IArchive::closeAll()
{
    ArchiveCache		&cache = getCache();
    cache.clear();
}

GABC_IArchive::GABC_IArchive(const std::string &path)
    : myFilename(path)
{
    UT_INC_COUNTER(theCount);
    if (UTisstring(path.c_str()) && UTaccess(path.c_str(), R_OK) == 0)
    {
	try
	{
	    myArchive = IArchive(Alembic::AbcCoreHDF5::ReadArchive(), path);
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
    GABC_AutoLock	lock(*this);	// Lock for member data deletion
    purgeObjects();	// Clear all my objects out
}

#if 0
GABC_IObject
GABC_IArchive::root() const
{
    return findObject("/");
}

GABC_IObject
GABC_IArchive::findObject(const std::string &objectpath) const
{
    return findObject(objectpath.c_str());
}

GABC_IObject
GABC_IArchive::findObject(const char *objectpath) const
{
    return GABC_IObject();
}
#endif

void
GABC_IArchive::resolveObject(GABC_IObject &obj)
{
    GABC_AutoLock	lock(*this);

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
    obj.setObject(IObject());
}

GABC_IObject
GABC_IArchive::getTop() const
{
    if (!valid())
	return GABC_IObject();

    GABC_AutoLock	lock(*this);
    GABC_IArchive	*me = const_cast<GABC_IArchive *>(this);
    IObject	root = me->myArchive.getTop();
    return GABC_IObject(me, root);
}

void
GABC_IArchive::purgeObjects()
{
    GABC_AutoLock	lock(*this);

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
    myObjects.insert(item);
}

void
GABC_IArchive::unreference(GABC_IItem *item)
{
    UT_ASSERT(myObjects.count(item));
    myObjects.erase(item);
}
