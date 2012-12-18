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
 * NAME:	GABC_Util.C (GABC Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_Util.h"
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreHDF5/All.h>
#include <UT/UT_CappedCache.h>
#include <UT/UT_WorkBuffer.h>
#include <UT/UT_StringArray.h>
#include <UT/UT_FSA.h>
#include <UT/UT_SharedPtr.h>
#include <UT/UT_SysClone.h>
#include <UT/UT_SymbolTable.h>
#include <boost/tokenizer.hpp>

namespace
{
    typedef Alembic::Abc::M44d			M44d;
    typedef Alembic::Abc::index_t		index_t;
    typedef Alembic::Abc::chrono_t		chrono_t;
    typedef Alembic::Abc::ICompoundProperty	ICompoundProperty;
    typedef Alembic::Abc::ISampleSelector	ISampleSelector;
    typedef Alembic::Abc::ObjectHeader		ObjectHeader;
    typedef Alembic::Abc::TimeSamplingPtr	TimeSamplingPtr;
    typedef Alembic::Abc::WrapExistingFlag	WrapExistingFlag;
    typedef Alembic::AbcGeom::IXform		IXform;
    typedef Alembic::AbcGeom::IXformSchema	IXformSchema;
    typedef Alembic::AbcGeom::IPolyMesh		IPolyMesh;
    typedef Alembic::AbcGeom::ISubD		ISubD;
    typedef Alembic::AbcGeom::ICurves		ICurves;
    typedef Alembic::AbcGeom::IPoints		IPoints;
    typedef Alembic::AbcGeom::INuPatch		INuPatch;
    typedef Alembic::AbcGeom::IFaceSet		IFaceSet;
    typedef Alembic::AbcGeom::IPolyMeshSchema	IPolyMeshSchema;
    typedef Alembic::AbcGeom::ISubDSchema	ISubDSchema;
    typedef Alembic::AbcGeom::ICurvesSchema	ICurvesSchema;
    typedef Alembic::AbcGeom::IPointsSchema	IPointsSchema;
    typedef Alembic::AbcGeom::INuPatchSchema	INuPatchSchema;
    typedef Alembic::AbcGeom::IFaceSetSchema	IFaceSetSchema;
    typedef Alembic::AbcGeom::ICamera		ICamera;
    typedef Alembic::AbcGeom::XformSample	XformSample;
    typedef GABC_Util::PathList			PathList;

    const WrapExistingFlag gabcWrapExisting = Alembic::Abc::kWrapExisting;

    // Stores world (cumulative) transforms for objects in the cache.  These
    // are stored at sample times but not at intermediate time samples.
    class LocalWorldXform
    {
    public:
	LocalWorldXform()
	    : myLocal()
	    , myWorld()
	    , myConstant(false)
	    , myInheritsXform(true)
	{
	}
	LocalWorldXform(const M44d &l, const M44d &w,
			bool is_const, bool inherits)
	    : myLocal(l)
	    , myWorld(w)
	    , myConstant(is_const)
	    , myInheritsXform(inherits)
	{
	}
	LocalWorldXform(const LocalWorldXform &x)
	    : myLocal(x.myLocal)
	    , myWorld(x.myWorld)
	    , myConstant(x.myConstant)
	    , myInheritsXform(x.myInheritsXform)
	{
	}

	const M44d		&getLocal() const	{ return myLocal; }
	const M44d		&getWorld() const	{ return myWorld; }
	bool		 isConstant() const	{ return myConstant; }
	bool		 inheritsXform() const	{ return myInheritsXform; }
    private:
	M44d		myLocal;
	M44d		myWorld;
	bool		myConstant;
	bool		myInheritsXform;
    };

    class ArchiveTransformItem : public UT_CappedItem
    {
    public:
	ArchiveTransformItem()
	    : UT_CappedItem()
	    , myX()
	{
	}
	ArchiveTransformItem(const M44d &l, const M44d &w,
			bool is_const, bool inherits_xform)
	    : UT_CappedItem()
	    , myX(l, w, is_const, inherits_xform)
	{
	}

	virtual int64	 getMemoryUsage() const	{ return sizeof(*this); }
	const M44d	&getLocal() const	{ return myX.getLocal(); }
	const M44d	&getWorld() const	{ return myX.getWorld(); }
	bool		 isConstant() const	{ return myX.isConstant(); }
	bool		 inheritsXform() const	{ return myX.inheritsXform(); }
    private:
	LocalWorldXform	myX;
    };

    class WalkPushFile
    {
    public:
	WalkPushFile(GABC_Util::Walker &walk, const std::string &filename)
	    : myWalk(walk)
	    , myFilename(walk.filename())
	{
	    myWalk.setFilename(filename);
	}
	~WalkPushFile()
	{
	    myWalk.setFilename(myFilename);
	}
    private:
	GABC_Util::Walker	&myWalk;
	std::string		 myFilename;
    };

    typedef UT_SymbolMap<LocalWorldXform>	AbcTransformMap;

    void DecomposeXForm(
            const Imath::M44d &mat,
            Imath::V3d &scale,
            Imath::V3d &shear,
            Imath::Quatd &rotation,
            Imath::V3d &translation
    )
    {
        Imath::M44d mat_remainder(mat);

        // Extract Scale, Shear
        Imath::extractAndRemoveScalingAndShear(mat_remainder, scale, shear);

        // Extract translation
        translation.x = mat_remainder[3][0];
        translation.y = mat_remainder[3][1];
        translation.z = mat_remainder[3][2];

        // Extract rotation
        rotation = extractQuat(mat_remainder);
    }

    M44d RecomposeXForm(
            const Imath::V3d &scale,
            const Imath::V3d &shear,
            const Imath::Quatd &rotation,
            const Imath::V3d &translation
    )
    {
        Imath::M44d scale_mtx, shear_mtx, rotation_mtx, translation_mtx;

        scale_mtx.setScale(scale);
        shear_mtx.setShear(shear);
        rotation_mtx = rotation.toMatrix44();
        translation_mtx.setTranslation(translation);

        return scale_mtx * shear_mtx * rotation_mtx * translation_mtx;
    }


    // when amt is 0, a is returned
    inline double lerp(double a, double b, double amt)
    {
        return (a + (b-a)*amt);
    }

    Imath::V3d lerp(const Imath::V3d &a, const Imath::V3d &b, double amt)
    {
        return Imath::V3d(lerp(a[0], b[0], amt),
                          lerp(a[1], b[1], amt),
                          lerp(a[2], b[2], amt));
    }


    void
    TokenizeObjectPath(const std::string & objectPath, PathList & pathList)
    {
        typedef boost::char_separator<char> Separator;
        typedef boost::tokenizer<Separator> Tokenizer;
        Tokenizer tokenizer( objectPath, Separator( "/" ) );
        for ( Tokenizer::iterator iter = tokenizer.begin() ;
                iter != tokenizer.end() ; ++iter )
        {
            if ( (*iter).empty() ) { continue; }
            pathList.push_back( *iter );
        }
    }

    class ArchiveObjectKey : public UT_CappedKey
    {
    public:
	ArchiveObjectKey(const char *key, fpreal sample_time = 0)
	    : UT_CappedKey()
	    , myKey(UT_String::ALWAYS_DEEP, key)
	    , myTime(sample_time)
	{
	}
	virtual ~ArchiveObjectKey() {}
	virtual UT_CappedKey	*duplicate() const
				 {
				     return new ArchiveObjectKey(myKey, myTime);
				 }
	virtual unsigned int	 getHash() const
				 {
				     uint	hash = SYSreal_hash(myTime);
				     hash = SYSwang_inthash(hash)^myKey.hash();
				     return hash;
				 }
	virtual bool		 isEqual(const UT_CappedKey &cmp) const
				 {
				     const ArchiveObjectKey	*key = UTverify_cast<const ArchiveObjectKey *>(&cmp);
				     return myKey == key->myKey && 
					    SYSalmostEqual(myTime, key->myTime);
				 }
    private:
	UT_String	myKey;
	fpreal		myTime;
    };

    class ArchiveObjectItem : public UT_CappedItem
    {
    public:
	ArchiveObjectItem()
	    : UT_CappedItem()
	    , myIObject()
	{
	}
	ArchiveObjectItem(const GABC_IObject &obj)
	    : UT_CappedItem()
	    , myIObject(obj)
	{
	}
	// Approximate usage
	virtual int64		 getMemoryUsage() const { return 1024; }
	const GABC_IObject	&getObject() const { return myIObject; }
    private:
	GABC_IObject	 myIObject;
    };

    struct ArchiveCacheEntry
    {
	typedef GABC_Util::ArchiveEventHandler		ArchiveEventHandler;
	typedef GABC_Util::ArchiveEventHandlerPtr	ArchiveEventHandlerPtr;

	typedef UT_Set<ArchiveEventHandlerPtr>	HandlerSetType;
        ArchiveCacheEntry()
	    : myCache("abcObjects", 2)
	    , myDynamicXforms("abxTransforms", 4)
        {
        }

        ~ArchiveCacheEntry()
        {
	    if (myArchive)
	    {
		for (HandlerSetType::iterator it = myHandlers.begin();
			it != myHandlers.end(); ++it)
		{
		    const ArchiveEventHandlerPtr	&handler = *it;
		    if (handler->archive() == myArchive.get())
		    {
			handler->cleared();
			handler->setArchivePtr(NULL);
		    }
		}
		myArchive->purgeObjects();
	    }
        }

	bool	addHandler(const ArchiveEventHandlerPtr &handler)
		{
		    if (myArchive && handler)
		    {
			myHandlers.insert(handler);
			handler->setArchivePtr(myArchive.get());
			return true;
		    }
		    return false;
		}

	bool	walk(GABC_Util::Walker &walker)
		{
		    if (!walker.preProcess(root()))
			return false;
		    return walkTree(root(), walker);
		}

	// Build a cache of constant (non-changing) transforms
	void	buildTransformCache(const GABC_IObject &root, const char *path,
				    const M44d &parent)
	{
	    UT_WorkBuffer	fullpath;
	    for (size_t i = 0; i < root.getNumChildren(); ++i)
	    {
		GABC_IObject	kid = root.getChild(i);
		if (kid.nodeType() == GABC_XFORM)
		{
		    IXform		xform(kid.object(), gabcWrapExisting);
		    IXformSchema	&xs = xform.getSchema();
		    M44d		world;
		    world = parent;
		    if (xs.isConstant())
		    {
			fullpath.sprintf("%s/%s", path, kid.getName().c_str());
			XformSample	xsample = xs.getValue(ISampleSelector(0.0));
			bool	inherits = xs.getInheritsXforms();
			M44d	localXform = xsample.getMatrix();
			if (inherits)
			    world = localXform * parent;
			else
			    world = localXform;
			myStaticXforms[fullpath.buffer()] =
			    LocalWorldXform(localXform, world, true, inherits);
			buildTransformCache(kid, fullpath.buffer(), world);
		    }
		}
	    }
	}

	/// Check to see if there's a const local transform cached
	bool	staticLocalTransform(const char *fullpath, M44d &xform)
		{
		    if (myStaticXforms.size() == 0)
		    {
			M44d	id;
			id.makeIdentity();
			buildTransformCache(root(), "", id);
		    }
		    AbcTransformMap::const_iterator	it;
		    it = myStaticXforms.find(fullpath);
		    if (it != myStaticXforms.map_end())
		    {
			xform = it->second.getLocal();
			return true;
		    }
		    return false;
		}
	/// Check to see if there's a const world transform cached
	bool	staticWorldTransform(const char *fullpath, M44d &xform)
		{
		    if (myStaticXforms.size() == 0)
		    {
			M44d	id;
			id.makeIdentity();
			buildTransformCache(root(), "", id);
		    }
		    AbcTransformMap::const_iterator	it;
		    it = myStaticXforms.find(fullpath);
		    if (it != myStaticXforms.map_end())
		    {
			xform = it->second.getWorld();
			return true;
		    }
		    return false;
		}

	/// Get an object's local transform
	void	getLocalTransform(M44d &x, const GABC_IObject &obj, fpreal now,
			bool &isConstant, bool &inheritsXform)
		{
		    if (!obj.localTransform(now, x, isConstant, inheritsXform))
		    {
			isConstant = true;
			inheritsXform = true;
			x.makeIdentity();
		    }
		}

	/// Find the full world transform for an object
	bool	getWorldTransform(M44d &x, const GABC_IObject &obj, fpreal now,
			bool &isConstant, bool &inheritsXform)
		{
		    isConstant = true;
		    // First, check if we have a static 
		    std::string	path = obj.getFullName();
		    if (staticWorldTransform(path.c_str(), x))
			return true;

		    // Now check to see if it's in the dynamic cache
		    ArchiveObjectKey	key(path.c_str(), now);
		    UT_CappedItemHandle	item = myDynamicXforms.findItem(key);
		    if (item)
		    {
			ArchiveTransformItem	*xitem;
			xitem =UTverify_cast<ArchiveTransformItem*>(item.get());
			x = xitem->getWorld();
			isConstant = xitem->isConstant();
			inheritsXform = xitem->inheritsXform();
		    }
		    else
		    {
			// Get our local transform
			M44d		localXform;
			GABC_IObject	dad = obj.getParent();

			getLocalTransform(localXform, obj, now,
				isConstant, inheritsXform);
			if (dad.valid() && inheritsXform)
			{
			    M44d	dm;
			    bool	dadConst;
			    getWorldTransform(dm, dad, now, dadConst,
				    inheritsXform);
			    if (!dadConst)
				isConstant = false;
			    if (inheritsXform)
				x = localXform * dm;
			}
			else
			{
			    x = localXform;	// World transform same as local
			}
			myDynamicXforms.addItem(key,
			    new ArchiveTransformItem(localXform, x,
					    isConstant, inheritsXform));
		    }
		    return true;
		}

	/// Find an object in the object cache -- this prevents having to
	/// traverse from the root every time we need an object.
	GABC_IObject	findObject(const GABC_IObject &parent,
			    UT_WorkBuffer &fullpath, const char *component)
		{
		    fullpath.append("/");
		    fullpath.append(component);
		    ArchiveObjectKey	key(fullpath.buffer());
		    GABC_IObject	kid;
		    UT_CappedItemHandle item = myCache.findItem(key);
		    if (item)
		    {
			kid = UTverify_cast<ArchiveObjectItem *>(item.get())->getObject();
		    }
		    else
		    {
			kid = parent.getChild(component);
			if (kid.valid())
			    myCache.addItem(key, new ArchiveObjectItem(kid));
		    }
		    return kid;
		}

	/// Given a path to the object, return the object
	GABC_IObject getObject(const std::string &objectPath)
	{
	    PathList		pathList;
	    GABC_IObject	curr = root();
	    UT_WorkBuffer	fullpath;

	    TokenizeObjectPath(objectPath, pathList);
	    for (PathList::const_iterator I = pathList.begin();
		    I != pathList.end() && curr.valid(); ++I)
	    {
		curr = findObject(curr, fullpath, (*I).c_str());
	    }
	    return curr;
	}

	class PathListWalker : public GABC_Util::Walker
	{
	public:
	    PathListWalker(PathList &objects)
		: myObjects(objects)
	    {
	    }

	    virtual bool	process(const GABC_IObject &obj)
	    {
		myObjects.push_back(obj.getFullName());
		return true;
	    }
	private:
	    PathList				&myObjects;
	};

	const PathList	&getObjectList()
			{
			    if (isValid() && !myObjectList.size())
			    {
				PathListWalker	func(myObjectList);
				walk(func);
			    }
			    return myObjectList;
			}

	static bool	walkTree(const GABC_IObject &node,
				GABC_Util::Walker &walker)
			{
			    if (walker.interrupted())
				return false;
			    if (walker.process(node))
				walker.walkChildren(node);
			    return true;
			}

	bool	isValid() const		{ return myArchive && myArchive->valid(); }
	void	setArchive(const GABC_IArchivePtr &a)	{ myArchive = a; }
	void	setError(const std::string &e)	{ error = e; }

	const GABC_IArchivePtr	&archive()	{ return myArchive; }

    private:
	GABC_IObject		root()	{ return myArchive->getTop(); }
	GABC_IArchivePtr	myArchive;
	std::string		error;
	PathList		myObjectList;
	AbcTransformMap		myStaticXforms;
	UT_CappedCache		myCache;
	UT_CappedCache		myDynamicXforms;
	HandlerSetType		myHandlers;
    };

    typedef UT_SharedPtr<ArchiveCacheEntry>		ArchiveCacheEntryPtr;
    typedef UT_Map<std::string, ArchiveCacheEntryPtr>	ArchiveCache;

    //-*************************************************************************

    size_t g_maxCache = 50;
    //for now, leak the pointer to the archive cache so we don't
    //crash at shutdown
    ArchiveCache *g_archiveCache(new ArchiveCache);

    //-*************************************************************************

    ArchiveCacheEntryPtr
    FindArchive(const std::string &path)
    {
	if (UTisstring(path.c_str()) && UTaccess(path.c_str(), R_OK) == 0)
	{
	    ArchiveCache::iterator I = g_archiveCache->find(path);
	    if (I != g_archiveCache->end())
	    {
		return (*I).second;
	    }
	}
	return ArchiveCacheEntryPtr();
    }

    ArchiveCacheEntryPtr
    LoadArchive(const std::string &path)
    {
	if (!UTisstring(path.c_str()) || UTaccess(path.c_str(), R_OK) != 0)
	{
	    return ArchiveCacheEntryPtr(new ArchiveCacheEntry());
	}
        ArchiveCache::iterator I = g_archiveCache->find(path);
        if (I != g_archiveCache->end())
        {
            return (*I).second;
        }
        ArchiveCacheEntryPtr entry = ArchiveCacheEntryPtr(
                new ArchiveCacheEntry);
	entry->setArchive(GABC_IArchive::open(path));
        while (g_archiveCache->size() >= g_maxCache)
        {
            long d = static_cast<long>(std::floor(
                    static_cast<double>(std::rand())
                            / RAND_MAX * g_maxCache - 0.5));
            if (d < 0)
            {
                d = 0;
            }
            ArchiveCache::iterator it = g_archiveCache->begin();
            for (; d > 0; --d)
            {
                ++it;
            }
            g_archiveCache->erase(it);
        }
        (*g_archiveCache)[path] = entry;
        return entry;
    }

    void
    ClearArchiveFile(const std::string &path)
    {
        ArchiveCache::iterator it = g_archiveCache->find(path);
	if (it != g_archiveCache->end())
	{
	    g_archiveCache->erase(it);
	}
    }
    void
    ClearArchiveCache()
    {
	delete g_archiveCache;
	g_archiveCache = new ArchiveCache;
    }
}

GABC_Util::Walker::~Walker()
{
}

bool
GABC_Util::Walker::preProcess(const GABC_IObject &)
{
    return true;
}

bool
GABC_Util::Walker::walkChildren(const GABC_IObject &obj)
{
    exint	nkids = obj.getNumChildren();
    for (exint i = 0; i < nkids; ++i)
    {
	// Returns false on interrupt
	if (!ArchiveCacheEntry::walkTree(obj.getChild(i), *this))
	    return false;
    }
    return true;
}

bool
GABC_Util::walk(const std::string &filename, Walker &walker,
			    const UT_StringArray &objects)
{
    ArchiveCacheEntryPtr	cacheEntry = LoadArchive(filename);
    WalkPushFile		walkfile(walker, filename);
    for (exint i = 0; i < objects.entries(); ++i)
    {
	std::string	path(objects(i));
	GABC_IObject	obj = findObject(filename, path);
	if (obj.valid())
	{
	    if (!walker.preProcess(obj))
		return false;
	    if (!cacheEntry->walkTree(obj, walker))
		return false;
	}
    }
    return true;
}

bool
GABC_Util::walk(const std::string &filename, GABC_Util::Walker &walker)
{
    ArchiveCacheEntryPtr	cacheEntry = LoadArchive(filename);
    WalkPushFile		walkfile(walker, filename);
    return cacheEntry->isValid() ? cacheEntry->walk(walker) : false;
}

void
GABC_Util::clearCache(const char *filename)
{
    if (filename)
	ClearArchiveFile(filename);
    else
	ClearArchiveCache();
}

void
GABC_Util::setFileCacheSize(int nfiles)
{
    g_maxCache = SYSmax(nfiles, 1);
}

int
GABC_Util::fileCacheSize()
{
    return g_maxCache;
}

GABC_IObject
GABC_Util::findObject(const std::string &filename,
	const std::string &objectpath)
{
    ArchiveCacheEntryPtr	cacheEntry = LoadArchive(filename);
    return cacheEntry->isValid() ? cacheEntry->getObject(objectpath)
		: GABC_IObject();
}


bool
GABC_Util::getLocalTransform(const std::string &filename,
	const std::string &objectpath,
	fpreal sample_time,
	UT_Matrix4D &xform,
	bool &isConstant,
	bool &inheritsXform)
{
    bool	success = false;
    M44d	lxform;
    try
    {
	ArchiveCacheEntryPtr	cacheEntry = LoadArchive(filename);
	if (cacheEntry->isValid())
	{
	    GABC_IObject	obj = cacheEntry->getObject(objectpath);
	    if (obj.valid())
	    {
		cacheEntry->getLocalTransform(lxform, obj,
				    sample_time, isConstant, inheritsXform);
		success = true;
	    }
	}
    }
    catch (const std::exception &)
    {
	success = false;
    }
    if (success)
	xform = UT_Matrix4D(lxform.x);
    return success;
}

bool
GABC_Util::getWorldTransform(const std::string &filename,
	const std::string &objectpath,
	fpreal sample_time,
	UT_Matrix4D &xform,
	bool &isConstant,
	bool &inheritsXform)
{
    bool	success = false;
    M44d	wxform;
    try
    {
	ArchiveCacheEntryPtr	cacheEntry = LoadArchive(filename);
	if (cacheEntry->isValid())
	{
	    GABC_IObject	obj = cacheEntry->getObject(objectpath);
	    if (obj.valid())
	    {
		success = cacheEntry->getWorldTransform(wxform, obj,
				    sample_time, isConstant, inheritsXform);
	    }
	}
    }
    catch (const std::exception &)
    {
	success = false;
    }
    if (success)
	xform = UT_Matrix4D(wxform.x);
    return success;
}

bool
GABC_Util::getWorldTransform(const std::string &filename,
	const GABC_IObject &obj,
	fpreal sample_time,
	UT_Matrix4D &xform,
	bool &isConstant,
	bool &inheritsXform)
{
    bool	success = false;
    M44d	wxform;
    if (obj.valid())
    {
	try
	{
	    ArchiveCacheEntryPtr	cacheEntry = LoadArchive(filename);
	    UT_ASSERT(cacheEntry->getObject(obj.getFullName()).valid());
	    success = cacheEntry->getWorldTransform(wxform, obj,
				    sample_time, isConstant, inheritsXform);
	}
	catch (const std::exception &)
	{
	    success = false;
	}
    }
    if (success)
	xform = UT_Matrix4D(wxform.x);
    return success;
}

GABC_Util::ArchiveEventHandler::~ArchiveEventHandler()
{
}

void
GABC_Util::ArchiveEventHandler::stopReceivingEvents()
{
    myArchive = NULL;
}

bool
GABC_Util::addEventHandler(const std::string &path,
	const GABC_Util::ArchiveEventHandlerPtr &handler)
{
    if (handler)
    {
	ArchiveCacheEntryPtr	arch = FindArchive(path);
	if (arch)
	{
	    if (arch->addHandler(handler))
		return true;
	}
    }
    return false;
}

const PathList &
GABC_Util::getObjectList(const std::string &filename)
{
    try
    {
	ArchiveCacheEntryPtr	cacheEntry = LoadArchive(filename);
	return cacheEntry->getObjectList();
    }
    catch (const std::exception &)
    {
    }
    static PathList	theEmptyList;
    return theEmptyList;
}
