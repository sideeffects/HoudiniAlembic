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

#include "GABC_Util.h"
#include "GABC_OArrayProperty.h"
#include "GABC_OScalarProperty.h"
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreHDF5/All.h>
#include <GT/GT_AttributeList.h>
#include <GT/GT_DAIndexedString.h>
#include <GT/GT_DANumeric.h>
#include <UT/UT_CappedCache.h>
#include <UT/UT_ErrorLog.h>
#include <UT/UT_FSA.h>
#include <UT/UT_FSATable.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_PathSearch.h>
#include <UT/UT_SharedPtr.h>
#include <UT/UT_StringArray.h>
#include <UT/UT_SymbolTable.h>
#include <UT/UT_SysClone.h>
#include <UT/UT_WorkBuffer.h>
#include <FS/FS_Info.h>
#include <boost/tokenizer.hpp>

using namespace GABC_NAMESPACE;

namespace
{
    class ArchiveCacheEntry;
    class LocalWorldXform;

    typedef Alembic::Abc::index_t		    index_t;
    typedef Alembic::Abc::chrono_t		    chrono_t;
    typedef Alembic::Abc::M44d			    M44d;

    typedef Alembic::Abc::ISampleSelector	    ISampleSelector;
    typedef Alembic::Abc::TimeSamplingPtr	    TimeSamplingPtr;

    typedef Alembic::Abc::IObject                   IObject;
    typedef Alembic::Abc::ObjectHeader		    ObjectHeader;
    typedef Alembic::Abc::ObjectReaderPtr           ObjectReaderPtr;

    typedef Alembic::Abc::DataType                  DataType;
    typedef Alembic::Abc::PlainOldDataType          PlainOldDataType;
    typedef Alembic::Abc::WrapExistingFlag	    WrapExistingFlag;

    // PROPERTIES
    typedef Alembic::Abc::PropertyHeader            PropertyHeader;

    // INPUT PROPERTIES
    typedef Alembic::Abc::CompoundPropertyReaderPtr CompoundPropertyReaderPtr;
    typedef Alembic::Abc::ICompoundProperty         ICompoundProperty;
    typedef Alembic::Abc::IArrayProperty            IArrayProperty;
    typedef Alembic::Abc::IScalarProperty           IScalarProperty;

    // OUTPUT PROPERTIES
    typedef Alembic::Abc::BasePropertyWriterPtr	    BasePropertyWriterPtr;
    typedef Alembic::Abc::CompoundPropertyWriter    CompoundPropertyWriter;
    typedef Alembic::Abc::CompoundPropertyWriterPtr CompoundPropertyWriterPtr;
    typedef Alembic::Abc::OBaseProperty	            OBaseProperty;
    typedef Alembic::Abc::OCompoundProperty	    OCompoundProperty;

    // ARRAY PROPERTIES
    typedef Alembic::Abc::IV2sArrayProperty         IV2sArrayProperty;
    typedef Alembic::Abc::IV2iArrayProperty         IV2iArrayProperty;
    typedef Alembic::Abc::IV2fArrayProperty         IV2fArrayProperty;
    typedef Alembic::Abc::IV2dArrayProperty         IV2dArrayProperty;

    typedef Alembic::Abc::IV3sArrayProperty         IV3sArrayProperty;
    typedef Alembic::Abc::IV3iArrayProperty         IV3iArrayProperty;
    typedef Alembic::Abc::IV3fArrayProperty         IV3fArrayProperty;
    typedef Alembic::Abc::IV3dArrayProperty         IV3dArrayProperty;

    typedef Alembic::Abc::IP2sArrayProperty         IP2sArrayProperty;
    typedef Alembic::Abc::IP2iArrayProperty         IP2iArrayProperty;
    typedef Alembic::Abc::IP2fArrayProperty         IP2fArrayProperty;
    typedef Alembic::Abc::IP2dArrayProperty         IP2dArrayProperty;

    typedef Alembic::Abc::IP3sArrayProperty         IP3sArrayProperty;
    typedef Alembic::Abc::IP3iArrayProperty         IP3iArrayProperty;
    typedef Alembic::Abc::IP3fArrayProperty         IP3fArrayProperty;
    typedef Alembic::Abc::IP3dArrayProperty         IP3dArrayProperty;

    typedef Alembic::Abc::IBox2sArrayProperty       IBox2sArrayProperty;
    typedef Alembic::Abc::IBox2iArrayProperty       IBox2iArrayProperty;
    typedef Alembic::Abc::IBox2fArrayProperty       IBox2fArrayProperty;
    typedef Alembic::Abc::IBox2dArrayProperty       IBox2dArrayProperty;

    typedef Alembic::Abc::IBox3sArrayProperty       IBox3sArrayProperty;
    typedef Alembic::Abc::IBox3iArrayProperty       IBox3iArrayProperty;
    typedef Alembic::Abc::IBox3fArrayProperty       IBox3fArrayProperty;
    typedef Alembic::Abc::IBox3dArrayProperty       IBox3dArrayProperty;

    typedef Alembic::Abc::IM33fArrayProperty        IM33fArrayProperty;
    typedef Alembic::Abc::IM33dArrayProperty        IM33dArrayProperty;
    typedef Alembic::Abc::IM44fArrayProperty        IM44fArrayProperty;
    typedef Alembic::Abc::IM44dArrayProperty        IM44dArrayProperty;

    typedef Alembic::Abc::IQuatfArrayProperty       IQuatfArrayProperty;
    typedef Alembic::Abc::IQuatdArrayProperty       IQuatdArrayProperty;

    typedef Alembic::Abc::IC3hArrayProperty         IC3hArrayProperty;
    typedef Alembic::Abc::IC3fArrayProperty         IC3fArrayProperty;
    typedef Alembic::Abc::IC3cArrayProperty         IC3cArrayProperty;

    typedef Alembic::Abc::IC4hArrayProperty         IC4hArrayProperty;
    typedef Alembic::Abc::IC4fArrayProperty         IC4fArrayProperty;
    typedef Alembic::Abc::IC4cArrayProperty         IC4cArrayProperty;

    typedef Alembic::Abc::IN2fArrayProperty         IN2fArrayProperty;
    typedef Alembic::Abc::IN2dArrayProperty         IN2dArrayProperty;

    typedef Alembic::Abc::IN3fArrayProperty         IN3fArrayProperty;
    typedef Alembic::Abc::IN3dArrayProperty         IN3dArrayProperty;

    // SCALAR PROPERTIES
    typedef Alembic::Abc::IV2sProperty              IV2sProperty;
    typedef Alembic::Abc::IV2iProperty              IV2iProperty;
    typedef Alembic::Abc::IV2fProperty              IV2fProperty;
    typedef Alembic::Abc::IV2dProperty              IV2dProperty;

    typedef Alembic::Abc::IV3sProperty              IV3sProperty;
    typedef Alembic::Abc::IV3iProperty              IV3iProperty;
    typedef Alembic::Abc::IV3fProperty              IV3fProperty;
    typedef Alembic::Abc::IV3dProperty              IV3dProperty;

    typedef Alembic::Abc::IP2sProperty              IP2sProperty;
    typedef Alembic::Abc::IP2iProperty              IP2iProperty;
    typedef Alembic::Abc::IP2fProperty              IP2fProperty;
    typedef Alembic::Abc::IP2dProperty              IP2dProperty;

    typedef Alembic::Abc::IP3sProperty              IP3sProperty;
    typedef Alembic::Abc::IP3iProperty              IP3iProperty;
    typedef Alembic::Abc::IP3fProperty              IP3fProperty;
    typedef Alembic::Abc::IP3dProperty              IP3dProperty;

    typedef Alembic::Abc::IBox2sProperty            IBox2sProperty;
    typedef Alembic::Abc::IBox2iProperty            IBox2iProperty;
    typedef Alembic::Abc::IBox2fProperty            IBox2fProperty;
    typedef Alembic::Abc::IBox2dProperty            IBox2dProperty;

    typedef Alembic::Abc::IBox3sProperty            IBox3sProperty;
    typedef Alembic::Abc::IBox3iProperty            IBox3iProperty;
    typedef Alembic::Abc::IBox3fProperty            IBox3fProperty;
    typedef Alembic::Abc::IBox3dProperty            IBox3dProperty;

    typedef Alembic::Abc::IM33fProperty             IM33fProperty;
    typedef Alembic::Abc::IM33dProperty             IM33dProperty;
    typedef Alembic::Abc::IM44fProperty             IM44fProperty;
    typedef Alembic::Abc::IM44dProperty             IM44dProperty;

    typedef Alembic::Abc::IQuatfProperty            IQuatfProperty;
    typedef Alembic::Abc::IQuatdProperty            IQuatdProperty;

    typedef Alembic::Abc::IC3hProperty              IC3hProperty;
    typedef Alembic::Abc::IC3fProperty              IC3fProperty;
    typedef Alembic::Abc::IC3cProperty              IC3cProperty;

    typedef Alembic::Abc::IC4hProperty              IC4hProperty;
    typedef Alembic::Abc::IC4fProperty              IC4fProperty;
    typedef Alembic::Abc::IC4cProperty              IC4cProperty;

    typedef Alembic::Abc::IN2fProperty              IN2fProperty;
    typedef Alembic::Abc::IN2dProperty              IN2dProperty;

    typedef Alembic::Abc::IN3fProperty              IN3fProperty;
    typedef Alembic::Abc::IN3dProperty              IN3dProperty;

    // INPUT OBJECTS AND SCHEMAS
    typedef Alembic::AbcGeom::IXform		    IXform;
    typedef Alembic::AbcGeom::IPolyMesh		    IPolyMesh;
    typedef Alembic::AbcGeom::ISubD		    ISubD;
    typedef Alembic::AbcGeom::ICurves		    ICurves;
    typedef Alembic::AbcGeom::IPoints		    IPoints;
    typedef Alembic::AbcGeom::INuPatch		    INuPatch;
    typedef Alembic::AbcGeom::IFaceSet		    IFaceSet;
    typedef Alembic::AbcGeom::IXformSchema	    IXformSchema;
    typedef Alembic::AbcGeom::IPolyMeshSchema	    IPolyMeshSchema;
    typedef Alembic::AbcGeom::ISubDSchema	    ISubDSchema;
    typedef Alembic::AbcGeom::ICurvesSchema	    ICurvesSchema;
    typedef Alembic::AbcGeom::IPointsSchema	    IPointsSchema;
    typedef Alembic::AbcGeom::INuPatchSchema	    INuPatchSchema;
    typedef Alembic::AbcGeom::IFaceSetSchema	    IFaceSetSchema;
    typedef Alembic::AbcGeom::ICamera		    ICamera;
    typedef Alembic::AbcGeom::XformSample	    XformSample;

    typedef GABC_Util::PathList			    PathList;
    typedef GABC_Util::PropertyMap                  PropertyMap;
    typedef GABC_Util::PropertyMapInsert            PropertyMapInsert;

    typedef UT_SymbolMap<LocalWorldXform>               AbcTransformMap;
    typedef UT_SharedPtr<ArchiveCacheEntry>             ArchiveCacheEntryPtr;
    typedef UT_Map<std::string, ArchiveCacheEntryPtr>   ArchiveCache;

    typedef UT_SortedMap<std::string, const PropertyHeader *>   HeaderMap;
    typedef std::pair<std::string, const PropertyHeader *>      HeaderMapInsert;

    typedef std::pair<GT_DataArrayHandle, PlainOldDataType> UPSample;
    typedef UT_Map<GABC_OProperty *, UPSample>              UPSampleMap;
    typedef std::pair<GABC_OProperty *, UPSample>           UPSampleMapInsert;

    static UT_Lock		theFileLock;
    static UT_Lock		theOCacheLock;
    static UT_Lock		theXCacheLock;

    const WrapExistingFlag gabcWrapExisting = Alembic::Abc::kWrapExisting;

    // Convenience class for resetting a Walker objects filename to it's
    // original value.
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
	GABC_Util::Walker  &myWalk;
	std::string         myFilename;
    };

    // Stores world (cumulative) transforms for objects in the cache. These
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
	LocalWorldXform(const M44d &l,
	        const M44d &w,
	        bool is_const,
	        bool inherits)
	    : myLocal(l)
	    , myWorld(w)
	    , myConstant(is_const)
	    , myInheritsXform(inherits)
	{}
	LocalWorldXform(const LocalWorldXform &x)
	    : myLocal(x.myLocal)
	    , myWorld(x.myWorld)
	    , myConstant(x.myConstant)
	    , myInheritsXform(x.myInheritsXform)
	{}

	const M44d &getLocal() const	    { return myLocal; }
	const M44d &getWorld() const	    { return myWorld; }
	bool        isConstant() const	    { return myConstant; }
	bool        inheritsXform() const   { return myInheritsXform; }

    private:
	M44d        myLocal;
	M44d        myWorld;
	bool        myConstant;
	bool        myInheritsXform;
    };

    // Used to store LocalWorldXform objects as items in a UT_CappedCache.
    class ArchiveTransformItem : public UT_CappedItem
    {
    public:
	ArchiveTransformItem()
	    : UT_CappedItem()
	    , myX()
	{}
	ArchiveTransformItem(const M44d &l,
	        const M44d &w,
	        bool is_const,
	        bool inherits_xform)
	    : UT_CappedItem()
	    , myX(l, w, is_const, inherits_xform)
	{}

	virtual int64   getMemoryUsage() const  { return sizeof(*this); }
	const M44d     &getLocal() const        { return myX.getLocal(); }
	const M44d     &getWorld() const        { return myX.getWorld(); }
	bool            isConstant() const      { return myX.isConstant(); }
	bool            inheritsXform() const   { return myX.inheritsXform(); }

    private:
	LocalWorldXform	myX;
    };

    // Used to store GABC_IObject objects as items in a UT_CappedCache.
    class ArchiveObjectItem : public UT_CappedItem
    {
    public:
	ArchiveObjectItem()
	    : UT_CappedItem()
	    , myIObject()
	{}
	ArchiveObjectItem(const GABC_IObject &obj)
	    : UT_CappedItem()
	    , myIObject(obj)
	{}

	// Approximate usage
	virtual int64       getMemoryUsage() const { return 1024; }
	const GABC_IObject &getObject() const { return myIObject; }

    private:
	GABC_IObject	 myIObject;
    };

    // Key object for the UT_CappedCaches. Key is a path within an archive
    // and a time.
    class ArchiveObjectKey : public UT_CappedKey
    {
    public:
	ArchiveObjectKey(const char *key, fpreal sample_time = 0)
	    : UT_CappedKey()
	    , myKey(UT_String::ALWAYS_DEEP, key)
	    , myTime(sample_time)
	{}
	virtual ~ArchiveObjectKey() {}

	virtual UT_CappedKey    *duplicate() const
        {
            return new ArchiveObjectKey(myKey, myTime);
        }
	virtual unsigned int    getHash() const
        {
            uint    hash = SYSreal_hash(myTime);
            hash = SYSwang_inthash(hash)^myKey.hash();
            return hash;
        }
	virtual bool            isEqual(const UT_CappedKey &cmp) const
        {
            const ArchiveObjectKey  *key = UTverify_cast<const ArchiveObjectKey *>(&cmp);
            return (myKey == key->myKey) && SYSalmostEqual(myTime, key->myTime);
        }

    private:
	UT_String   myKey;
	fpreal      myTime;
    };

    // This class caches data for a single Alembic archive. Stores lists of
    // all the objects contained in the archive. Caches the objects in the
    // archive, and their transforms (static and non-static).
    class ArchiveCacheEntry
    {
    public:
	typedef GABC_Util::ArchiveEventHandler      ArchiveEventHandler;
	typedef GABC_Util::ArchiveEventHandlerPtr   ArchiveEventHandlerPtr;
	typedef UT_Set<ArchiveEventHandlerPtr>      HandlerSetType;

        ArchiveCacheEntry()
	    : myCache("abcObjects", 8)
	    , myDynamicXforms("abcTransforms", 64)
	    , myXformCacheBuilt(false)
        {}
        virtual ~ArchiveCacheEntry()
        {}

	void
	purge()
	{
	    if (myArchive)
	    {
		for (HandlerSetType::iterator it = myHandlers.begin();
			it != myHandlers.end();
			++it)
		{
		    const ArchiveEventHandlerPtr   &handler = *it;
		    if (handler->archive() == myArchive.get())
		    {
			handler->cleared();
			handler->setArchivePtr(NULL);
		    }
		}
		myArchive->purgeObjects();
	    }
	}

	bool
	addHandler(const ArchiveEventHandlerPtr &handler)
        {
            if (myArchive && handler)
            {
                myHandlers.insert(handler);
                handler->setArchivePtr(myArchive.get());
                return true;
            }

            return false;
        }

	bool
	walk(GABC_Util::Walker &walker)
        {
            if (!walker.preProcess(root()))
            {
                return false;
            }

            return walkTree(root(), walker);
        }

	inline void
	ensureValidTransformCache()
	{
	    if (!myXformCacheBuilt)
	    {
		// Double lock
		UT_AutoLock	lock(theXCacheLock);
		if (!myXformCacheBuilt)
		{
		    M44d	id;
		    id.makeIdentity();
		    buildTransformCache(root(), "", id);
		    SYSstoreFence();
		    myXformCacheBuilt = true;
		}
	    }
	}

	// Build a cache of constant (non-changing) transforms
	void
	buildTransformCache(const GABC_IObject &root,
	        const char *path,
                const M44d &parent)
	{
            GABC_IObject    kid;
            M44d            world;
	    UT_WorkBuffer   fullpath;

	    for (size_t i = 0; i < root.getNumChildren(); ++i)
	    {
	        kid = root.getChild(i);
		if (kid.nodeType() == GABC_XFORM)
		{
                    IXform          xform(kid.object(), gabcWrapExisting);
                    IXformSchema   &xs = xform.getSchema();
                    XformSample     xsample;
                    M44d            localXform;
                    bool            inherits;

		    world = parent;
		    if (xs.isConstant())
		    {
			xsample = xs.getValue(ISampleSelector(0.0));
			inherits = xs.getInheritsXforms();
			localXform = xsample.getMatrix();

			if (inherits)
			{
			    world = localXform * parent;
                        }
			else
			{
			    world = localXform;
                        }

			fullpath.sprintf("%s/%s", path, kid.getName().c_str());
			myStaticXforms[fullpath.buffer()] = LocalWorldXform(localXform,
			        world,
			        true,
			        inherits);

			buildTransformCache(kid, fullpath.buffer(), world);
		    }
		}
	    }
	}

	/// Check to see if there's a const local transform cached
	bool
	staticLocalTransform(const char *fullpath, M44d &xform)
        {
            AbcTransformMap::const_map_iterator     it;

            ensureValidTransformCache();

            it = myStaticXforms.find(fullpath);
            if (it != myStaticXforms.map_end())
            {
                xform = it->second.getLocal();
                return true;
            }

            return false;
        }

	/// Check to see if there's a const world transform cached
	bool
	staticWorldTransform(const char *fullpath, M44d &xform)
        {
            AbcTransformMap::const_map_iterator     it;

            ensureValidTransformCache();

            it = myStaticXforms.find(fullpath);
            if (it != myStaticXforms.map_end())
            {
                xform = it->second.getWorld();
                return true;
            }

            return false;
        }

	/// Get an object's local transform
	static void
	getLocalTransform(M44d &x,
	        const GABC_IObject &obj,
	        fpreal now,
		bool &isConstant,
		bool &inheritsXform)
        {
            if (!obj.localTransform(now, x, isConstant, inheritsXform))
            {
                isConstant = true;
                inheritsXform = true;
                x.makeIdentity();
            }
        }

	bool
	isObjectAnimated(const GABC_IObject &obj)
	{
	    AbcTransformMap::const_map_iterator     it;
	    std::string                             path = obj.getFullName();

	    ensureValidTransformCache();

	    it = myStaticXforms.find(path.c_str());
	    return it == myStaticXforms.map_end();
	}

	/// Find the full world transform for an object
	bool
	getWorldTransform(M44d &x,
	        const GABC_IObject &obj,
	        fpreal now,
                bool &isConstant,
                bool &inheritsXform)
        {
            UT_AutoLock	lock(theXCacheLock);

            std::string         path = obj.getFullName();
            ArchiveObjectKey    key(path.c_str(), now);
            UT_CappedItemHandle item;
            isConstant = true;

            // First, check if we have a static
            if (staticWorldTransform(path.c_str(), x))
            {
                return true;
            }

            // Now check to see if it's in the dynamic cache
            item = myDynamicXforms.findItem(key);
            if (item)
            {
                ArchiveTransformItem   *xitem = UTverify_cast<ArchiveTransformItem *>(item.get());

                x = xitem->getWorld();
                isConstant = xitem->isConstant();
                inheritsXform = xitem->inheritsXform();
            }
            else
            {
                // Get our local transform
                GABC_IObject    dad = obj.getParent();
                M44d            localXform;

                getLocalTransform(localXform,
                        obj,
                        now,
                        isConstant,
                        inheritsXform);

                if (dad.valid() && inheritsXform)
                {
		    M44d	dm;
		    bool	dadConst;
		    bool	dadInherit;
                    getWorldTransform(dm, dad, now, dadConst, dadInherit);

                    if (!dadConst)
                    {
                        isConstant = false;
                    }
                    if (inheritsXform)
                    {
                        x = localXform * dm;
                    }
                }
                else
                {
                    x = localXform;	// World transform same as local
                }

                myDynamicXforms.addItem(key,
                        new ArchiveTransformItem(localXform,
                                x,
                                isConstant,
                                inheritsXform));
            }

            return true;
        }

	/// Find an object in the object cache -- this prevents having to
	/// traverse from the root every time we need an object.
	GABC_IObject
	findObject(const GABC_IObject &parent,
	        UT_WorkBuffer &fullpath,
	        const char *component)
        {
            UT_AutoLock	lock(theOCacheLock);
            fullpath.append("/");
            fullpath.append(component);

            ArchiveObjectKey    key(fullpath.buffer());
            GABC_IObject        kid;
            UT_CappedItemHandle item = myCache.findItem(key);

            if (item)
            {
                kid = UTverify_cast<ArchiveObjectItem *>(item.get())->getObject();
            }
            else
            {
                kid = parent.getChild(component);
                if (kid.valid())
                {
                    myCache.addItem(key, new ArchiveObjectItem(kid));
                }
            }

            return kid;
        }

        void
        tokenizeObjectPath(const std::string & objectPath, PathList & pathList)
        {
            typedef boost::char_separator<char> Separator;
            typedef boost::tokenizer<Separator> Tokenizer;

            Tokenizer   tokenizer(objectPath, Separator( "/" ));
            for (Tokenizer::iterator iter = tokenizer.begin();
                    iter != tokenizer.end();
                    ++iter)
            {
                if (iter->empty())
                {
                    continue;
                }

                pathList.push_back(*iter);
            }
        }

	/// Given a path to the object, return the object
	GABC_IObject
	getObject(const std::string &objectPath)
	{
	    GABC_IObject    curr = root();
	    PathList        pathList;
	    UT_WorkBuffer   fullpath;

	    tokenizeObjectPath(objectPath, pathList);
	    for (PathList::const_iterator i = pathList.begin();
		    (i != pathList.end()) && curr.valid();
		    ++i)
	    {
		curr = findObject(curr, fullpath, (*i).c_str());
	    }

	    return curr;
	}

	//
	GABC_IObject
	getObject(ObjectReaderPtr reader)
	{
	    return GABC_IObject(myArchive, IObject(reader, gabcWrapExisting));
	}

	class PathListWalker : public GABC_Util::Walker
	{
	public:
	    PathListWalker(PathList &objects, PathList &full)
		: myObjectList(objects)
		, myFullObjectList(full)
	    {}

	    virtual bool    process(const GABC_IObject &obj)
	    {
		if (obj.nodeType() != GABC_FACESET)
		{
		    myObjectList.push_back(obj.getFullName());
                }
		myFullObjectList.push_back(obj.getFullName());

		return true;
	    }

	private:
	    PathList   &myObjectList;
	    PathList   &myFullObjectList;
	};

	const PathList &
	getObjectList(bool full)
        {
            if (isValid() && !myFullObjectList.size())
            {
                PathListWalker	func(myObjectList,
                                        myFullObjectList);
                walk(func);
            }
            return full ? myFullObjectList : myObjectList;
        }

	static bool
	walkTree(const GABC_IObject &node,
	        GABC_Util::Walker &walker)
        {
            if (walker.interrupted())
            {
                return false;
            }

            if (walker.process(node))
            {
                walker.walkChildren(node);
            }
            return true;
        }

	bool
	isValid() const
	{
            return myArchive && myArchive->valid();
        }

	const GABC_IArchivePtr &
	archive()
	{
	    return myArchive;
        }

	void    setArchive(const GABC_IArchivePtr &a)   { myArchive = a; }
	void    setError(const std::string &e)          { error = e; }

    private:
	GABC_IObject		root()
        {
            return myArchive ? myArchive->getTop() : GABC_IObject();
        }

	GABC_IArchivePtr	myArchive;
	std::string		error;
	PathList		myObjectList;
	PathList		myFullObjectList;
	bool			myXformCacheBuilt;
	AbcTransformMap		myStaticXforms;
	UT_CappedCache		myCache;
	UT_CappedCache		myDynamicXforms;
	HandlerSetType		myHandlers;
    };

    //-*************************************************************************

    size_t g_maxCache = 50;
    //for now, leak the pointer to the archive cache so we don't
    //crash at shutdown
    ArchiveCache *g_archiveCache(new ArchiveCache);

    //-*************************************************************************

    static void
    badFileWarning(const std::string &path)
    {
	static UT_Set<std::string>  warnedFiles;

	if (UTisstring(path.c_str()) && !warnedFiles.count(path))
	{
	    warnedFiles.insert(path);
	    UT_ErrorLog::mantraError("Bad Alembic Archive: '%s'", path.c_str());
	}
    }

    static bool
    pathMap(UT_String &path)
    {
	if (!path.isstring())
	{
	    return false;
        }
	FS_Info	finfo(path.buffer());
	return finfo.hasAccess(FS_READ);
    }

    static ArchiveCacheEntryPtr
    FindArchive(const std::string &path)
    {
        ArchiveCache::iterator  I;
	UT_String               spath(path.c_str());

	if (pathMap(spath))
	{
	    I = g_archiveCache->find(spath.buffer());
	    if (I != g_archiveCache->end())
	    {
		return I->second;
	    }
	}

	badFileWarning(path);
	return ArchiveCacheEntryPtr();
    }

    static ArchiveCacheEntryPtr
    LoadArchive(const std::string &path)
    {
	UT_AutoLock	lock(theFileLock);

        ArchiveCache::iterator  I = g_archiveCache->find(path);
        ArchiveCacheEntryPtr    entry;
	UT_String               spath(path.c_str());

        if (I != g_archiveCache->end())
        {
            return (*I).second;
        }
	if (!pathMap(spath))
	{
	    badFileWarning(path);
	    return ArchiveCacheEntryPtr(new ArchiveCacheEntry());
	}

        entry = ArchiveCacheEntryPtr(new ArchiveCacheEntry);
	entry->setArchive(GABC_IArchive::open(spath.buffer()));
        while (g_archiveCache->size() >= g_maxCache)
        {
            long d = static_cast<long>(
                    std::floor(   static_cast<double>(std::rand())
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

    static void
    ClearArchiveFile(const std::string &path)
    {
        ArchiveCache::iterator it = g_archiveCache->find(path);
	if (it != g_archiveCache->end())
	{
	    it->second->purge();
	    g_archiveCache->erase(it);
	}
    }

    static void
    ClearArchiveCache()
    {
	for (ArchiveCache::iterator it = g_archiveCache->begin();
		it != g_archiveCache->end();
		++it)
	{
	    it->second->purge();
	}

	delete g_archiveCache;
	g_archiveCache = new ArchiveCache;
    }

    static std::string
    getInterpretation(const PropertyHeader &head, const DataType &dt)
    {
        PlainOldDataType    pod = dt.getPod();
        std::string         interpretation = "UNIDENTIFIED";
        int                 extent = dt.getExtent();

        if (head.isArray())
        {
            if (extent == 1)
            {
                interpretation = "";
            }
            else if (extent == 2)
            {
                if (pod == Alembic::Abc::kInt16POD)
                {
                    if (IV2sArrayProperty::matches(head))
                    {
                        interpretation = IV2sArrayProperty::getInterpretation();
                    }
                    else if (IP2sArrayProperty::matches(head))
                    {
                        interpretation = IP2sArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kInt32POD)
                {
                    if (IV2iArrayProperty::matches(head))
                    {
                        interpretation = IV2iArrayProperty::getInterpretation();
                    }
                    else if (IP2iArrayProperty::matches(head))
                    {
                        interpretation = IP2iArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat32POD)
                {
                    if (IV2fArrayProperty::matches(head))
                    {
                        interpretation = IV2fArrayProperty::getInterpretation();
                    }
                    else if (IP2fArrayProperty::matches(head))
                    {
                        interpretation = IP2fArrayProperty::getInterpretation();
                    }
                    else if (IN2fArrayProperty::matches(head))
                    {
                        interpretation = IN2fArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat64POD)
                {
                    if (IV2dArrayProperty::matches(head))
                    {
                        interpretation = IV2dArrayProperty::getInterpretation();
                    }
                    else if (IP2dArrayProperty::matches(head))
                    {
                        interpretation = IP2dArrayProperty::getInterpretation();
                    }
                    else if (IN2dArrayProperty::matches(head))
                    {
                        interpretation = IN2dArrayProperty::getInterpretation();
                    }
                }
            }
            else if (extent == 3)
            {
                if (pod == Alembic::Abc::kUint8POD)
                {
                    if (IC3cArrayProperty::matches(head))
                    {
                        interpretation = IC3cArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kInt16POD)
                {
                    if (IV3sArrayProperty::matches(head))
                    {
                        interpretation = IV3sArrayProperty::getInterpretation();
                    }
                    else if (IP3sArrayProperty::matches(head))
                    {
                        interpretation = IP3sArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kInt32POD)
                {
                    if (IV3iArrayProperty::matches(head))
                    {
                        interpretation = IV3iArrayProperty::getInterpretation();
                    }
                    else if (IP3iArrayProperty::matches(head))
                    {
                        interpretation = IP3iArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat16POD)
                {
                    if (IC3hArrayProperty::matches(head))
                    {
                        interpretation = IC3hArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat32POD)
                {
                    if (IV3fArrayProperty::matches(head))
                    {
                        interpretation = IV3fArrayProperty::getInterpretation();
                    }
                    else if (IP3fArrayProperty::matches(head))
                    {
                        interpretation = IP3fArrayProperty::getInterpretation();
                    }
                    else if (IN3fArrayProperty::matches(head))
                    {
                        interpretation = IN3fArrayProperty::getInterpretation();
                    }
                    else if (IC3fArrayProperty::matches(head))
                    {
                        interpretation = IC3fArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat64POD)
                {
                    if (IV3dArrayProperty::matches(head))
                    {
                        interpretation = IV3dArrayProperty::getInterpretation();
                    }
                    else if (IP3dArrayProperty::matches(head))
                    {
                        interpretation = IP3dArrayProperty::getInterpretation();
                    }
                    else if (IN3dArrayProperty::matches(head))
                    {
                        interpretation = IN3dArrayProperty::getInterpretation();
                    }
                }
            }
            else if (extent == 4)
            {
                if (pod == Alembic::Abc::kUint8POD)
                {
                    if (IC4cArrayProperty::matches(head))
                    {
                        interpretation = IC4cArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kInt16POD)
                {
                    if (IBox2sArrayProperty::matches(head))
                    {
                        interpretation = IBox2sArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kInt32POD)
                {
                    if (IBox2iArrayProperty::matches(head))
                    {
                        interpretation = IBox2iArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat16POD)
                {
                    if (IC4hArrayProperty::matches(head))
                    {
                        interpretation = IC4hArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat32POD)
                {
                    if (IBox2fArrayProperty::matches(head))
                    {
                        interpretation = IBox2fArrayProperty::getInterpretation();
                    }
                    else if (IQuatfArrayProperty::matches(head))
                    {
                        interpretation = IQuatfArrayProperty::getInterpretation();
                    }
                    else if (IC4fArrayProperty::matches(head))
                    {
                        interpretation = IC4fArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat64POD)
                {
                    if (IBox2dArrayProperty::matches(head))
                    {
                        interpretation = IBox2dArrayProperty::getInterpretation();
                    }
                    else if (IQuatdArrayProperty::matches(head))
                    {
                        interpretation = IQuatdArrayProperty::getInterpretation();
                    }
                }
            }
            else if (extent == 6)
            {
                if (pod == Alembic::Abc::kInt16POD)
                {
                    if (IBox3sArrayProperty::matches(head))
                    {
                        interpretation = IBox3sArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kInt32POD)
                {
                    if (IBox3iArrayProperty::matches(head))
                    {
                        interpretation = IBox3iArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat32POD)
                {
                    if (IBox3fArrayProperty::matches(head))
                    {
                        interpretation = IBox3fArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat64POD)
                {
                    if (IBox3dArrayProperty::matches(head))
                    {
                        interpretation = IBox3dArrayProperty::getInterpretation();
                    }
                }
            }
            else if (extent == 9)
            {
                if (pod == Alembic::Abc::kFloat32POD)
                {
                    if (IM33fArrayProperty::matches(head))
                    {
                        interpretation = IM33fArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat64POD)
                {
                    if (IM33dArrayProperty::matches(head))
                    {
                        interpretation = IM33dArrayProperty::getInterpretation();
                    }
                }
            }
            else if (extent == 16)
            {
                if (pod == Alembic::Abc::kFloat32POD)
                {
                    if (IM44fArrayProperty::matches(head))
                    {
                        interpretation = IM44fArrayProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat64POD)
                {
                    if (IM44dArrayProperty::matches(head))
                    {
                        interpretation = IM44dArrayProperty::getInterpretation();
                    }
                }
            }
        }
        else
        {
            UT_ASSERT(head.isScalar());

            if (extent == 1)
            {
                interpretation = "";
            }
            else if (extent == 2)
            {
                if (pod == Alembic::Abc::kInt16POD)
                {
                    if (IV2sProperty::matches(head))
                    {
                        interpretation = IV2sProperty::getInterpretation();
                    }
                    else if (IP2sProperty::matches(head))
                    {
                        interpretation = IP2sProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kInt32POD)
                {
                    if (IV2iProperty::matches(head))
                    {
                        interpretation = IV2iProperty::getInterpretation();
                    }
                    else if (IP2iProperty::matches(head))
                    {
                        interpretation = IP2iProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat32POD)
                {
                    if (IV2fProperty::matches(head))
                    {
                        interpretation = IV2fProperty::getInterpretation();
                    }
                    else if (IP2fProperty::matches(head))
                    {
                        interpretation = IP2fProperty::getInterpretation();
                    }
                    else if (IN2fProperty::matches(head))
                    {
                        interpretation = IN2fProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat64POD)
                {
                    if (IV2dProperty::matches(head))
                    {
                        interpretation = IV2dProperty::getInterpretation();
                    }
                    else if (IP2dProperty::matches(head))
                    {
                        interpretation = IP2dProperty::getInterpretation();
                    }
                    else if (IN2dProperty::matches(head))
                    {
                        interpretation = IN2dProperty::getInterpretation();
                    }
                }
            }
            else if (extent == 3)
            {
                if (pod == Alembic::Abc::kUint8POD)
                {
                    if (IC3cProperty::matches(head))
                    {
                        interpretation = IC3cProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kInt16POD)
                {
                    if (IV3sProperty::matches(head))
                    {
                        interpretation = IV3sProperty::getInterpretation();
                    }
                    else if (IP3sProperty::matches(head))
                    {
                        interpretation = IP3sProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kInt32POD)
                {
                    if (IV3iProperty::matches(head))
                    {
                        interpretation = IV3iProperty::getInterpretation();
                    }
                    else if (IP3iProperty::matches(head))
                    {
                        interpretation = IP3iProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat16POD)
                {
                    if (IC3hProperty::matches(head))
                    {
                        interpretation = IC3hProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat32POD)
                {
                    if (IV3fProperty::matches(head))
                    {
                        interpretation = IV3fProperty::getInterpretation();
                    }
                    else if (IP3fProperty::matches(head))
                    {
                        interpretation = IP3fProperty::getInterpretation();
                    }
                    else if (IN3fProperty::matches(head))
                    {
                        interpretation = IN3fProperty::getInterpretation();
                    }
                    else if (IC3fProperty::matches(head))
                    {
                        interpretation = IC3fProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat64POD)
                {
                    if (IV3dProperty::matches(head))
                    {
                        interpretation = IV3dProperty::getInterpretation();
                    }
                    else if (IP3dProperty::matches(head))
                    {
                        interpretation = IP3dProperty::getInterpretation();
                    }
                    else if (IN3dProperty::matches(head))
                    {
                        interpretation = IN3dProperty::getInterpretation();
                    }
                }
            }
            else if (extent == 4)
            {
                if (pod == Alembic::Abc::kUint8POD)
                {
                    if (IC4cProperty::matches(head))
                    {
                        interpretation = IC4cProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kInt16POD)
                {
                    if (IBox2sProperty::matches(head))
                    {
                        interpretation = IBox2sProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kInt32POD)
                {
                    if (IBox2iProperty::matches(head))
                    {
                        interpretation = IBox2iProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat16POD)
                {
                    if (IC4hProperty::matches(head))
                    {
                        interpretation = IC4hProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat32POD)
                {
                    if (IBox2fProperty::matches(head))
                    {
                        interpretation = IBox2fProperty::getInterpretation();
                    }
                    else if (IQuatfProperty::matches(head))
                    {
                        interpretation = IQuatfProperty::getInterpretation();
                    }
                    else if (IC4fProperty::matches(head))
                    {
                        interpretation = IC4fProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat64POD)
                {
                    if (IBox2dProperty::matches(head))
                    {
                        interpretation = IBox2dProperty::getInterpretation();
                    }
                    else if (IQuatdProperty::matches(head))
                    {
                        interpretation = IQuatdProperty::getInterpretation();
                    }
                }
            }
            else if (extent == 6)
            {
                if (pod == Alembic::Abc::kInt16POD)
                {
                    if (IBox3sProperty::matches(head))
                    {
                        interpretation = IBox3sProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kInt32POD)
                {
                    if (IBox3iProperty::matches(head))
                    {
                        interpretation = IBox3iProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat32POD)
                {
                    if (IBox3fProperty::matches(head))
                    {
                        interpretation = IBox3fProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat64POD)
                {
                    if (IBox3dProperty::matches(head))
                    {
                        interpretation = IBox3dProperty::getInterpretation();
                    }
                }
            }
            else if (extent == 9)
            {
                if (pod == Alembic::Abc::kFloat32POD)
                {
                    if (IM33fProperty::matches(head))
                    {
                        interpretation = IM33fProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat64POD)
                {
                    if (IM33dProperty::matches(head))
                    {
                        interpretation = IM33dProperty::getInterpretation();
                    }
                }
            }
            else if (extent == 16)
            {
                if (pod == Alembic::Abc::kFloat32POD)
                {
                    if (IM44fProperty::matches(head))
                    {
                        interpretation = IM44fProperty::getInterpretation();
                    }
                }
                else if (pod == Alembic::Abc::kFloat64POD)
                {
                    if (IM44dProperty::matches(head))
                    {
                        interpretation = IM44dProperty::getInterpretation();
                    }
                }
            }
        }

        return interpretation;
    }

    // Corresponds to PlainOldDataType in Alembic::Util::PlainOldDataType.h
    //
    // Contains an entry for wide strings (kWstringPOD), though we never
    // use it.
    static UT_FSATable  theAlembicPODTable(
        Alembic::Util::kBooleanPOD,     "bool",
        Alembic::Util::kInt8POD,        "int8",
        Alembic::Util::kUint8POD,       "uint8",
        Alembic::Util::kInt16POD,       "int16",
        Alembic::Util::kUint16POD,      "uint16",
        Alembic::Util::kInt32POD,       "int32",
        Alembic::Util::kUint32POD,      "uint32",
        Alembic::Util::kInt64POD,       "int64",
        Alembic::Util::kUint64POD,      "uint64",
        Alembic::Util::kFloat16POD,     "float16",
        Alembic::Util::kFloat32POD,     "float32",
        Alembic::Util::kFloat64POD,     "float64",
        Alembic::Util::kStringPOD,      "string",
        Alembic::Util::kWstringPOD,     "wstring",

        -1,                             NULL
    );
    static const char *
    AbcPOD(PlainOldDataType pod)
    {
        return theAlembicPODTable.getToken(pod);
    }
    static PlainOldDataType
    AbcPOD(const char *pod)
    {
        return (PlainOldDataType)theAlembicPODTable.findSymbol(pod);
    }

    static bool
    writeUserPropertyHelper(UT_JSONWriter *data_writer,
            UT_JSONWriter *meta_writer,
            const GABC_IObject &obj,
            ICompoundProperty &uprops,
            std::string &base,
            fpreal time)
    {
        CompoundPropertyReaderPtr   cprp = GetCompoundPropertyReaderPtr(uprops);
        HeaderMap                   hmap;
        const PropertyHeader       *header;
        UT_WorkBuffer               metadata;
        std::string                 interpretation;
        std::string                 name;
        exint                       num_props = uprops
                                            ? uprops.getNumProperties()
                                            : 0;
        bool                        success = true;

        // Go through every user property and map the name of the property
        // to the property header in a sorted map.
        //
        // In HDF5 Alembic files, the order that user properties are
        // declared is not preserved. However, the order is preserved in Ogawa.
        // Since both formats are valid for using to store an Alembic
        // archive, and what type of format is used is not exposed to the
        // user, it likely means that the order of the user properties has
        // no significance in Alembic and the behaviour is undefined. Thus,
        // we order the properties ourselves in alphabetical order.
        for (exint i = 0; i < num_props; ++i)
        {
            const PropertyHeader    &head = uprops.getPropertyHeader(i);
            hmap.insert(HeaderMapInsert(head.getName(), &head));
        }

        for (HeaderMap::iterator it = hmap.begin(); it != hmap.end(); ++it)
        {
            // The name of a user property is constructed as a path:
            // the names of the compound properties that a property is
            // nested within are delimited by the '/' character, and the
            // final token is the name of the property.
            //
            // EX:  Property C inside compound property B, itself inside
            //      compound property A     =   A/B/C
            name = base.empty() ? it->first : base + "/" + it->first;
            header = it->second;

            // Recurse for compound properties
            if (header->isCompound())
            {
                ICompoundProperty   child(cprp->getCompoundProperty(it->first),
                                            gabcWrapExisting);

                success = writeUserPropertyHelper(data_writer,
                        meta_writer,
                        obj,
                        child,
                        name,
                        time);
            }
            else
            {
                const DataType     &dt = header->getDataType();
                GT_DataArrayHandle  da = obj.convertIProperty(uprops,
                                            *header,
                                            time);

                // Writes GT_DataArray to JSON array, creating nested arrays
                // if tuple size is greater than 1.
                if (data_writer)
                {
                    // User property name
                    success = data_writer->jsonKey(name.c_str());

                    // Array values
                    if (!success || !da || !da->save(*data_writer, false)) {
                        return false;
                    }
                }

                if (meta_writer)
                {
                    int tuple_size = dt.getExtent();

                    // User property name
                    success = meta_writer->jsonKey(name.c_str());

                    success &= meta_writer->jsonBeginArray();
                    // Houdini GT storage type
                    success &= meta_writer->jsonString(
                            GTstorage(da->getStorage()));
                    // Alembic storage type
                    success &= meta_writer->jsonString(AbcPOD(dt.getPod()));
                    // Tuple size
                    success &= meta_writer->jsonInt(tuple_size);
                    // Tuple interpretation
                    if (tuple_size > 1)
                    {
                        success &= meta_writer->jsonString(
                                getInterpretation(*header, dt).c_str());
                    }
                    // Array size
                    if (header->isArray())
                    {
                        success &= meta_writer->jsonInt(da->entries());
                    }

                    success &= meta_writer->jsonEndArray(false);
                }
            }

            if (!success)
            {
                break;
            }
        }

        return success;
    }

    // Parse numeric JSON values into a GT_DANumeric data array.
    template <typename T>
    static GT_DataArrayHandle
    parseJSONValuesArray(UT_AutoJSONParser &parser,
            int items,
            int tuple_size,
            GT_Type gt_type)
    {
        T                      *data = new T(items,
                                        tuple_size,
                                        gt_type);
        typename T::data_type  *direct = data->data();
        bool                    success_parsing = true;
        bool                    error_parsing = false;

        success_parsing &= parser->parseBeginArray(error_parsing);
        if (tuple_size > 1)
        {
            for (int i = 0; i < items; ++i)
            {
                success_parsing &= parser->parseBeginArray(error_parsing);
                for (int j = 0; j < tuple_size; ++j)
                {
                    success_parsing &= parser->parseValue(*direct);
                    ++direct;
                }
                success_parsing &= parser->parseEndArray(error_parsing);
            }
        }
        else
        {
            items *= tuple_size;
            for (int i = 0; i < items; ++i)
            {
                success_parsing &= parser->parseValue(*direct);
                ++direct;
            }
        }
        success_parsing &= parser->parseEndArray(error_parsing);

        if (!success_parsing || error_parsing)
        {
            return GT_DataArrayHandle();
        }

        return GT_DataArrayHandle(data);
    }
}

//-----------------------------------------------
//  Walker
//-----------------------------------------------

bool
GABC_Util::Walker::walkChildren(const GABC_IObject &obj)
{
    exint   nkids = obj.getNumChildren();

    for (exint i = 0; i < nkids; ++i)
    {
	// Returns false on interrupt
	if (!ArchiveCacheEntry::walkTree(obj.getChild(i), *this))
	{
	    return false;
        }
    }

    return true;
}

void
GABC_Util::Walker::computeTimeRange(const GABC_IObject &obj)
{
    TimeSamplingPtr ts = obj.timeSampling();
    if (ts)
    {
        exint nSamples = obj.numSamples();

        // If the number of samples is zero it's an invalid range.
        if (nSamples == 0)
            return;

        if (!myComputedTimes)
        {
            myStartTime = ts->getSampleTime(0);
            myEndTime = ts->getSampleTime(nSamples - 1);
            myComputedTimes = true;
        }
        else
        {
            // Expand the start time backwards, expand the end time forwards.
            myStartTime = SYSmin(myStartTime, ts->getSampleTime(0));
            myEndTime = SYSmax(myEndTime, ts->getSampleTime(nSamples - 1));
        }
    }
}


//------------------------------------------------
//  ArchiveEventHandler
//------------------------------------------------

void
GABC_Util::ArchiveEventHandler::stopReceivingEvents()
{
    myArchive = NULL;
}

//------------------------------------------------
//  GABC_Util
//------------------------------------------------

const UT_DeepString GABC_Util::theLockGeometryParameter("abc_lock_geom");
const UT_DeepString GABC_Util::theUserPropsValsAttrib("abc_userProperties");
const UT_DeepString GABC_Util::theUserPropsMetaAttrib(
        "abc_userPropertiesMetadata");

#define YSTR(X)	#X		// Stringize
#define XSTR(X)	YSTR(X)		// Expand the stringized version
const char *
GABC_Util::getAlembicCompileNamespace()
{
    return XSTR(ALEMBIC_VERSION_NS);
}

bool
GABC_Util::walk(const std::string &filename,
        Walker &walker,
        const UT_StringArray &objects)
{
    ArchiveCacheEntryPtr    cacheEntry = LoadArchive(filename);
    WalkPushFile            walkfile(walker, filename);

    for (exint i = 0; i < objects.entries(); ++i)
    {
	std::string     path(objects(i));
	GABC_IObject    obj = findObject(filename, path);

	if (obj.valid())
	{
	    if (!walker.preProcess(obj))
	    {
		return false;
            }
	    if (!cacheEntry->walkTree(obj, walker))
	    {
		return false;
            }
	}
    }

    return true;
}

bool
GABC_Util::walk(const std::string &filename,
        Walker &walker,
        const UT_Set<std::string> &objects)
{
    ArchiveCacheEntryPtr    cacheEntry = LoadArchive(filename);
    WalkPushFile            walkfile(walker, filename);

    walker.myBadArchive = !cacheEntry->isValid();
    for (auto it = objects.begin(); it != objects.end(); ++it)
    {
	GABC_IObject    obj = findObject(filename, *it);

	if (obj.valid())
	{
	    if (!walker.preProcess(obj))
	    {
		return false;
            }
	    if (!cacheEntry->walkTree(obj, walker))
	    {
		return false;
            }
	}
    }

    return true;
}

bool
GABC_Util::walk(const std::string &filename, GABC_Util::Walker &walker)
{
    ArchiveCacheEntryPtr    cacheEntry = LoadArchive(filename);

    walker.myBadArchive = !cacheEntry->isValid();
    if (walker.myBadArchive)
    {
	return false;
    }

    WalkPushFile            walkfile(walker, filename);
    return cacheEntry->walk(walker);
}

void
GABC_Util::clearCache(const char *filename)
{
    if (filename)
    {
        ClearArchiveFile(filename);
    }
    else
    {
        ClearArchiveCache();
    }
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
    ArchiveCacheEntryPtr    cacheEntry = LoadArchive(filename);
    return cacheEntry->isValid()
            ? cacheEntry->getObject(objectpath)
            : GABC_IObject();
}

GABC_IObject
GABC_Util::findObject(const std::string &filename, ObjectReaderPtr reader)
{
    ArchiveCacheEntryPtr    cacheEntry = LoadArchive(filename);
    return cacheEntry->isValid()
            ? cacheEntry->getObject(reader)
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
    M44d    lxform;
    bool    success = false;

    try
    {
	ArchiveCacheEntryPtr    cacheEntry = LoadArchive(filename);
	if (cacheEntry->isValid())
	{
	    GABC_IObject        obj = cacheEntry->getObject(objectpath);
	    if (obj.valid())
	    {
		cacheEntry->getLocalTransform(lxform,
		        obj,
                        sample_time,
                        isConstant,
                        inheritsXform);
		success = true;
	    }
	}
    }
    catch (const std::exception &)
    {
	success = false;
    }

    if (success)
    {
	xform = UT_Matrix4D(lxform.x);
    }
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
    bool    success = false;
    M44d    wxform;

    try
    {
	ArchiveCacheEntryPtr    cacheEntry = LoadArchive(filename);
	if (cacheEntry->isValid())
	{
	    GABC_IObject        obj = cacheEntry->getObject(objectpath);
	    if (obj.valid())
	    {
		success = cacheEntry->getWorldTransform(wxform,
		        obj,
		        sample_time,
		        isConstant,
		        inheritsXform);
	    }
	}
    }
    catch (const std::exception &)
    {
	success = false;
    }

    if (success)
    {
	xform = UT_Matrix4D(wxform.x);
    }
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
    bool    success = false;
    M44d    wxform;

    if (obj.valid())
    {
	try
	{
	    ArchiveCacheEntryPtr	cacheEntry = LoadArchive(filename);
	    UT_ASSERT_P(cacheEntry->getObject(obj.getFullName()).valid());
	    success = cacheEntry->getWorldTransform(wxform,
	            obj,
                    sample_time,
                    isConstant,
                    inheritsXform);
	}
	catch (const std::exception &)
	{
	    success = false;
	}
    }

    if (success)
    {
	xform = UT_Matrix4D(wxform.x);
    }
    return success;
}

bool
GABC_Util::isTransformAnimated(const std::string &filename,
	const GABC_IObject &obj)
{
    bool    animated = false;

    if (obj.valid())
    {
	try
	{
	    ArchiveCacheEntryPtr    cacheEntry = LoadArchive(filename);
	    UT_ASSERT_P(cacheEntry->getObject(obj.getFullName()).valid());
	    animated = cacheEntry->isObjectAnimated(obj);
	}
	catch (const std::exception &)
	{
	    animated = false;
	}
    }

    return animated;
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
	    {
                return true;
            }
	}
    }

    return false;
}

const PathList &
GABC_Util::getObjectList(const std::string &filename, bool with_fsets)
{
    static PathList theEmptyList;

    try
    {
	ArchiveCacheEntryPtr	cacheEntry = LoadArchive(filename);
	return cacheEntry->getObjectList(with_fsets);
    }
    catch (const std::exception &)
    {}

    return theEmptyList;
}

bool
GABC_Util::isABCPropertyAnimated(ICompoundProperty arb)
{
    return !isABCPropertyConstant(arb);
}

bool
GABC_Util::isABCPropertyConstant(ICompoundProperty arb)
{
    exint   narb = arb ? arb.getNumProperties() : 0;
    bool    is_const = true;

    for (exint i = 0; i < narb; ++i)
    {
        const PropertyHeader    &head = arb.getPropertyHeader(i);

        if (head.isArray())
        {
            IArrayProperty      prop(arb, head.getName());
            is_const = prop.isConstant();
        }
        else if (head.isScalar())
        {
            IScalarProperty     prop(arb, head.getName());
            is_const = prop.isConstant();
        }
        else if (head.isCompound())
        {
            ICompoundProperty   prop(arb, head.getName());
            is_const = isABCPropertyConstant(prop);
        }
        else
        {
            UT_ASSERT(0 && "Unhandled property storage");
        }

        if (!is_const)
        {
            return false;
        }
    }

    return true;
}

bool
GABC_Util::writeUserPropertyDictionary(UT_JSONWriter *data_writer,
        UT_JSONWriter *meta_writer,
        const GABC_IObject &obj,
        ICompoundProperty &uprops,
        fpreal time)
{
    std::string base;
    bool        success = true;

    // At least one of the writers should not be NULL
    if (!(data_writer || meta_writer))
    {
        return false;
    }

    if (data_writer)
    {
        success = data_writer->jsonBeginMap();
    }
    if (meta_writer)
    {
        success &= meta_writer->jsonBeginMap();
    }
    if (!success)
        return false;

    success = writeUserPropertyHelper(data_writer,
            meta_writer,
            obj,
            uprops,
            base,
            time);

    if (data_writer)
    {
        success &= data_writer->jsonEndMap();
    }
    if (meta_writer)
    {
        success &= meta_writer->jsonEndMap();
    }

    return success;
}

// Report an error when parsing the user properties maps is unsuccessful.
#define USER_PROPS_PARSE_ERROR \
do { \
    if (!success_parsing || error_parsing) \
    { \
        err.warning("Error parsing user properties occurred while parsing %s.", \
                m_key.buffer()); \
        return false; \
    } \
} while(false)

bool
GABC_Util::readUserPropertyDictionary(UT_AutoJSONParser &meta_data,
        UT_AutoJSONParser &vals_data,
        PropertyMap &up_map,
        OCompoundProperty *ancestor,
        GABC_OError &err,
        const GABC_OOptions &ctx)
{
    CompoundPropertyWriterPtr   cpw;
    GABC_OProperty             *prop;
    GT_DataArrayHandle          data_array;
    GT_Storage                  gt_storage;
    GT_Type                     gt_type;
    OCompoundProperty           parent;
    PlainOldDataType            pod;
    PropertyMap::iterator       it;
    UPSample                   *sample;
    UPSampleMap                 sample_map;
    UPSampleMap::iterator       s_it;
    UT_WorkBuffer               m_key;
    UT_WorkBuffer               v_key;
    UT_WorkBuffer               work_buffer;
    int                         tuple_size;
    int                         array_size;
    const char                 *name;
    const char                 *up_path;
    bool                        output;
    bool                        m_finished;
    bool                        v_finished;
    bool                        success_parsing = true;
    bool                        error_parsing = false;

    if (!ancestor && !up_map.size())
    {
        return true;
    }

    // Parse the beginning of the map.
    if (!meta_data->parseBeginMap(error_parsing)
            || !vals_data->parseBeginMap(error_parsing))
    {
        err.warning("Error parsing user properties: no valid JSON map in "
                "attribute(s).");
        return false;
    }

    m_finished = meta_data->parseEndMap(error_parsing);
    v_finished = vals_data->parseEndMap(error_parsing);

    // Keep reading the until one of the maps ends.
    while (!m_finished && !v_finished)
    {
        output = true;
        success_parsing = true;
        error_parsing = false;
        gt_type = GT_TYPE_NONE;
        array_size = 0;

        // Parse the name of the next user property. The names from both
        // dictionaries should match.
        success_parsing &= meta_data->parseKey(m_key);
        success_parsing &= vals_data->parseKey(v_key);

        USER_PROPS_PARSE_ERROR;

        if (m_key != v_key)
        {
            err.warning("Error parsing user properties: order mismatch between "
                    "maps (%s vs %s).",
                    m_key.buffer(),
                    v_key.buffer());
            return false;
        }

        it = up_map.find(m_key.buffer());
        if (ancestor)
        {
            // If creating a new GABC_OProperty, check that one doesn't
            // already exist with the path given. Next, parse the name of
            // of the property from the path. While parsing, create any
            // parent compound properties that need to be created, reuse
            // existing properties if they have already been created.
            if (it != up_map.end())
            {
                err.warning("User property %s declared multiple times, ignoring"
                        " multiple declarations.",
                        m_key.buffer());
                output = false;
            }
            else
            {
                parent = *ancestor;
                up_path = m_key.buffer();

                while (true)
                {
                    name = up_path;
                    work_buffer.clear();
                    work_buffer.getNextToken(up_path, "/");

                    // Exit if no more tokens, we've found the property name.
                    if (!(*up_path))
                    {
                        break;
                    }

                    // Otherwise, this token is the name of a parent compound
                    // property. Check if a property with that name
                    // already exists.
                    OBaseProperty   bp = parent.getProperty(
                                            work_buffer.toStdString());
                    if (bp.valid())
                    {
                        // If a property exists, check that it is a compound
                        // property.
                        cpw = bp.getPtr()->asCompoundPtr();
                        if (cpw.get())
                        {
                            // If so, use the existing property as the parent.
                            parent = OCompoundProperty(
                                    cpw,
                                    gabcWrapExisting,
                                    GetErrorHandlerPolicy(parent));
                        }
                        else
                        {
                            // Otherwise, we have an overlap: user is trying
                            // to create simple and compound properties with
                            // the same name.
                            err.warning("User property %s already exists as "
                                    "simple property. Ignoring %s.",
                                    work_buffer.buffer(),
                                    m_key.buffer());
                            output = false;
                            break;
                        }
                    }
                    else
                    {
                        // If no existing property found, make one.
                        parent = OCompoundProperty(parent,
                                work_buffer.toStdString());
                    }
                }

            }

            // If we've found the property name and parent without error,
            // check that no compound property exists with that name under
            // this parent, otherwise we have a similar overlap as above.
            if (output)
            {
                OBaseProperty bp = parent.getProperty(work_buffer.toStdString());
                if (bp.valid())
                {
                    err.warning("User property %s already exists as compound "
                            "property. Ignoring %s.",
                            name,
                            m_key.buffer());
                    output = false;
                }
            }
        }
        else
        {
            // If updating an existing property, make sure it exists.
            if (it == up_map.end())
            {
                err.warning("User property %s was not declared on first frame, "
                        "so it was ignored.",
                        m_key.buffer());
                output = false;
            }
            else
            {
                prop = it->second;
            }
        }

        //
        //  Read metadata
        //

        success_parsing &= meta_data->parseBeginArray(error_parsing);

        // Read storage type
        success_parsing &= meta_data->parseString(work_buffer);
        gt_storage = GTstorage(work_buffer.buffer());

        // Read Alembic POD
        success_parsing &= meta_data->parseString(work_buffer);
        pod = AbcPOD(work_buffer.buffer());

        // Read tuple size
        success_parsing &= meta_data->parseValue(tuple_size);

        // Read tuple interpretation
        if (tuple_size > 1)
        {
            success_parsing &= meta_data->parseString(work_buffer);
            gt_type = GTtype(work_buffer.buffer());

            if ((gt_type == GT_TYPE_MATRIX) && (tuple_size == 9))
            {
                gt_type = GT_TYPE_MATRIX3;
            }
            else if ((gt_type == GT_TYPE_BOX) && (tuple_size == 4))
            {
                gt_type = GT_TYPE_BOX2;
            }
        }

        // Read array size
        if (!meta_data->parseEndArray(error_parsing))
        {
            success_parsing &= meta_data->parseValue(array_size);

            success_parsing &= meta_data->parseEndArray(error_parsing);
        }

        USER_PROPS_PARSE_ERROR;

        //
        //  Read data
        //

        if (gt_storage == GT_STORE_STRING)
        {
            // There is no special Alembic data type that is a string tuple.
            // The equivalent would be an array of strings.
            UT_ASSERT(tuple_size == 1);
            int                 items = (array_size ? array_size : 1);
            GT_DAIndexedString *data = new GT_DAIndexedString(items);

            success_parsing &= vals_data->parseBeginArray(error_parsing);
            for (int i = 0; i < items; ++i)
            {
                success_parsing &= vals_data->parseString(work_buffer);
                data->setString(i, 0, work_buffer.buffer());
            }
            success_parsing &= vals_data->parseEndArray(error_parsing);

            USER_PROPS_PARSE_ERROR;

            data_array = GT_DataArrayHandle(data);
        }
        else
        {
            exint   items = (array_size ? array_size : 1);

            switch (gt_storage)
            {
                case GT_STORE_UINT8:
                    data_array = parseJSONValuesArray<GT_UInt8Array>(
                            vals_data,
                            items,
                            tuple_size,
                            gt_type);
                    break;

                case GT_STORE_INT32:
                    data_array = parseJSONValuesArray<GT_Int32Array>(
                            vals_data,
                            items,
                            tuple_size,
                            gt_type);
                    break;

                case GT_STORE_INT64:
                    data_array = parseJSONValuesArray<GT_Int64Array>(
                            vals_data,
                            items,
                            tuple_size,
                            gt_type);
                    break;

                case GT_STORE_REAL16:
                    data_array = parseJSONValuesArray<GT_Real16Array>(
                            vals_data,
                            items,
                            tuple_size,
                            gt_type);
                    break;

                case GT_STORE_REAL32:
                    data_array = parseJSONValuesArray<GT_Real32Array>(
                            vals_data,
                            items,
                            tuple_size,
                            gt_type);
                    break;

                case GT_STORE_REAL64:
                    data_array = parseJSONValuesArray<GT_Real64Array>(
                            vals_data,
                            items,
                            tuple_size,
                            gt_type);
                    break;

                default:
                    // Since this is a type error, it has to do with
                    // reading metadata.
                    err.warning("Error reading user property metadata: "
                            "unrecognized Houdini storage type.");
                    return false;
            }
        }

        if (!data_array)
        {
            err.warning("Error occured while parsing user property %s.",
                    m_key.buffer());
            return false;
        }

        //
        //  Start/update properties with data
        //

        // If a non-fatal error occurs, we want to continue, and thus must
        // parse the remaining data. However, we don't want to output the
        // data we parsed. Use the 'output' flag to keep track if the parsed
        // data should be used or not.
        if (output)
        {
            if (ancestor)
            {
                if (array_size)
                {
                    prop = new GABC_OArrayProperty();
                }
                else
                {
                    prop = new GABC_OScalarProperty();
                }

                if (!prop->start(parent, name, data_array, err, ctx, pod))
                {
                    err.warning("Skipping property %s.", m_key.buffer());
                }
                else
                {
                    up_map.insert(PropertyMapInsert(m_key.toStdString(), prop));
                }
            }
            else
            {
                // When updating existing properties, the parsed data arrays
                // are stored. If the entire map was parsed without
                // incident, then the updates are applied. Otherwise, they
                // are discarded.
                sample_map.insert(UPSampleMapInsert(prop,
                        UPSample(data_array, pod)));
            }
        }

        m_finished = meta_data->parseEndMap(error_parsing);
        v_finished = vals_data->parseEndMap(error_parsing);
    }

    USER_PROPS_PARSE_ERROR;

    // If this turns out to be too slow/inefficient, it can be moved into the
    // GABC_OProperties by creating a second cache that stores potential
    // updates, and applying them once the JSON map has been successfully
    // parsed. This would be faster (cache created and destroyed with property,
    // no extra map, etc.) but doesn't abstract the conditions under which to
    // update from the update functions as this solution does.
    if (!ancestor)
    {
        for (it = up_map.begin(); it != up_map.end(); ++it)
        {
            prop = it->second;
            s_it = sample_map.find(prop);

            if (s_it == sample_map.end())
            {
                err.warning("Could not find property %s. Reusing previous sample.",
                        it->first.c_str());
                prop->updateFromPrevious();
            }
            else
            {
                sample = &(s_it->second);

                if (!prop->update(sample->first, err, ctx, sample->second))
                {
                    err.warning("Updating property %s using previous sample.",
                            it->first.c_str());
                    prop->updateFromPrevious();
                }
            }
        }
    }

    // TODO: Since we stop checking after one of the dictionaries ends,
    //       it's possible that the dictionary with extra entries is malformed
    //       beyond the point we stop checking, but we read it "correctly".
    // TODO: It's also possible that both maps in the dictionary end correctly,
    //       then are followed by lots of garbage data, but we stopped checking
    //       after the maps ended.
    if (!m_finished)
    {
        err.warning("More metadata entries than data entries, extra entries "
                "were ignored.");
    }
    if (!v_finished)
    {
        err.warning("More data entries than metadata entries, extra entries "
                "were ignored.");
    }

    return true;
}
