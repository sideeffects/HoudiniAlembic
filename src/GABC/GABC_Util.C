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

#include "GABC_Util.h"
#include "GABC_OArrayProperty.h"
#include "GABC_OScalarProperty.h"
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreHDF5/All.h>
#include <GT/GT_PackedGeoCache.h>
#include <GT/GT_AttributeList.h>
#include <GT/GT_DAIndexedString.h>
#include <GT/GT_DANumeric.h>
#include <UT/UT_CappedCache.h>
#include <UT/UT_ErrorLog.h>
#include <UT/UT_FSA.h>
#include <UT/UT_FSATable.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_JSONWriter.h>
#include <UT/UT_PathSearch.h>
#include <UT/UT_SharedPtr.h>
#include <UT/UT_StringArray.h>
#include <UT/UT_SymbolTable.h>
#include <UT/UT_SysClone.h>
#include <UT/UT_WorkBuffer.h>
#include <FS/FS_Info.h>
#include <hboost/tokenizer.hpp>

using namespace GABC_NAMESPACE;

namespace
{
    class ArchiveCacheEntry;
    class LocalWorldXform;

    using M44d = Alembic::Abc::M44d;

    using ISampleSelector = Alembic::Abc::ISampleSelector;

    using IObject = Alembic::Abc::IObject;
    using ObjectHeader = Alembic::Abc::ObjectHeader;
    using ObjectReaderPtr = Alembic::Abc::ObjectReaderPtr;

    using DataType = Alembic::Abc::DataType;
    using PlainOldDataType = Alembic::Abc::PlainOldDataType;
    using WrapExistingFlag = Alembic::Abc::WrapExistingFlag;

    // PROPERTIES
    using PropertyHeader = Alembic::Abc::PropertyHeader;

    // INPUT PROPERTIES
    using CompoundPropertyReaderPtr = Alembic::Abc::CompoundPropertyReaderPtr;
    using ICompoundProperty = Alembic::Abc::ICompoundProperty;
    using IArrayProperty = Alembic::Abc::IArrayProperty;
    using IScalarProperty = Alembic::Abc::IScalarProperty;

    // OUTPUT PROPERTIES
    using BasePropertyWriterPtr = Alembic::Abc::BasePropertyWriterPtr;
    using CompoundPropertyWriter = Alembic::Abc::CompoundPropertyWriter;
    using CompoundPropertyWriterPtr = Alembic::Abc::CompoundPropertyWriterPtr;
    using OBaseProperty = Alembic::Abc::OBaseProperty;
    using OCompoundProperty = Alembic::Abc::OCompoundProperty;

    // ARRAY PROPERTIES
    using IV2sArrayProperty = Alembic::Abc::IV2sArrayProperty;
    using IV2iArrayProperty = Alembic::Abc::IV2iArrayProperty;
    using IV2fArrayProperty = Alembic::Abc::IV2fArrayProperty;
    using IV2dArrayProperty = Alembic::Abc::IV2dArrayProperty;

    using IV3sArrayProperty = Alembic::Abc::IV3sArrayProperty;
    using IV3iArrayProperty = Alembic::Abc::IV3iArrayProperty;
    using IV3fArrayProperty = Alembic::Abc::IV3fArrayProperty;
    using IV3dArrayProperty = Alembic::Abc::IV3dArrayProperty;

    using IP2sArrayProperty = Alembic::Abc::IP2sArrayProperty;
    using IP2iArrayProperty = Alembic::Abc::IP2iArrayProperty;
    using IP2fArrayProperty = Alembic::Abc::IP2fArrayProperty;
    using IP2dArrayProperty = Alembic::Abc::IP2dArrayProperty;

    using IP3sArrayProperty = Alembic::Abc::IP3sArrayProperty;
    using IP3iArrayProperty = Alembic::Abc::IP3iArrayProperty;
    using IP3fArrayProperty = Alembic::Abc::IP3fArrayProperty;
    using IP3dArrayProperty = Alembic::Abc::IP3dArrayProperty;

    using IBox2sArrayProperty = Alembic::Abc::IBox2sArrayProperty;
    using IBox2iArrayProperty = Alembic::Abc::IBox2iArrayProperty;
    using IBox2fArrayProperty = Alembic::Abc::IBox2fArrayProperty;
    using IBox2dArrayProperty = Alembic::Abc::IBox2dArrayProperty;

    using IBox3sArrayProperty = Alembic::Abc::IBox3sArrayProperty;
    using IBox3iArrayProperty = Alembic::Abc::IBox3iArrayProperty;
    using IBox3fArrayProperty = Alembic::Abc::IBox3fArrayProperty;
    using IBox3dArrayProperty = Alembic::Abc::IBox3dArrayProperty;

    using IM33fArrayProperty = Alembic::Abc::IM33fArrayProperty;
    using IM33dArrayProperty = Alembic::Abc::IM33dArrayProperty;
    using IM44fArrayProperty = Alembic::Abc::IM44fArrayProperty;
    using IM44dArrayProperty = Alembic::Abc::IM44dArrayProperty;

    using IQuatfArrayProperty = Alembic::Abc::IQuatfArrayProperty;
    using IQuatdArrayProperty = Alembic::Abc::IQuatdArrayProperty;

    using IC3hArrayProperty = Alembic::Abc::IC3hArrayProperty;
    using IC3fArrayProperty = Alembic::Abc::IC3fArrayProperty;
    using IC3cArrayProperty = Alembic::Abc::IC3cArrayProperty;

    using IC4hArrayProperty = Alembic::Abc::IC4hArrayProperty;
    using IC4fArrayProperty = Alembic::Abc::IC4fArrayProperty;
    using IC4cArrayProperty = Alembic::Abc::IC4cArrayProperty;

    using IN2fArrayProperty = Alembic::Abc::IN2fArrayProperty;
    using IN2dArrayProperty = Alembic::Abc::IN2dArrayProperty;

    using IN3fArrayProperty = Alembic::Abc::IN3fArrayProperty;
    using IN3dArrayProperty = Alembic::Abc::IN3dArrayProperty;

    // SCALAR PROPERTIES
    using IV2sProperty = Alembic::Abc::IV2sProperty;
    using IV2iProperty = Alembic::Abc::IV2iProperty;
    using IV2fProperty = Alembic::Abc::IV2fProperty;
    using IV2dProperty = Alembic::Abc::IV2dProperty;
                                   
    using IV3sProperty = Alembic::Abc::IV3sProperty;
    using IV3iProperty = Alembic::Abc::IV3iProperty;
    using IV3fProperty = Alembic::Abc::IV3fProperty;
    using IV3dProperty = Alembic::Abc::IV3dProperty;
                                   
    using IP2sProperty = Alembic::Abc::IP2sProperty;
    using IP2iProperty = Alembic::Abc::IP2iProperty;
    using IP2fProperty = Alembic::Abc::IP2fProperty;
    using IP2dProperty = Alembic::Abc::IP2dProperty;
                                   
    using IP3sProperty = Alembic::Abc::IP3sProperty;
    using IP3iProperty = Alembic::Abc::IP3iProperty;
    using IP3fProperty = Alembic::Abc::IP3fProperty;
    using IP3dProperty = Alembic::Abc::IP3dProperty;
                                   
    using IBox2sProperty = Alembic::Abc::IBox2sProperty;
    using IBox2iProperty = Alembic::Abc::IBox2iProperty;
    using IBox2fProperty = Alembic::Abc::IBox2fProperty;
    using IBox2dProperty = Alembic::Abc::IBox2dProperty;
                                   
    using IBox3sProperty = Alembic::Abc::IBox3sProperty;
    using IBox3iProperty = Alembic::Abc::IBox3iProperty;
    using IBox3fProperty = Alembic::Abc::IBox3fProperty;
    using IBox3dProperty = Alembic::Abc::IBox3dProperty;
                                   
    using IM33fProperty = Alembic::Abc::IM33fProperty;
    using IM33dProperty = Alembic::Abc::IM33dProperty;
    using IM44fProperty = Alembic::Abc::IM44fProperty;
    using IM44dProperty = Alembic::Abc::IM44dProperty;
                                   
    using IQuatfProperty = Alembic::Abc::IQuatfProperty;
    using IQuatdProperty = Alembic::Abc::IQuatdProperty;
                                   
    using IC3hProperty = Alembic::Abc::IC3hProperty;
    using IC3fProperty = Alembic::Abc::IC3fProperty;
    using IC3cProperty = Alembic::Abc::IC3cProperty;
                                   
    using IC4hProperty = Alembic::Abc::IC4hProperty;
    using IC4fProperty = Alembic::Abc::IC4fProperty;
    using IC4cProperty = Alembic::Abc::IC4cProperty;
                                   
    using IN2fProperty = Alembic::Abc::IN2fProperty;
    using IN2dProperty = Alembic::Abc::IN2dProperty;
                                   
    using IN3fProperty = Alembic::Abc::IN3fProperty;
    using IN3dProperty = Alembic::Abc::IN3dProperty;

    // INPUT OBJECTS AND SCHEMAS
    using IXform = Alembic::AbcGeom::IXform;
    using IPolyMesh = Alembic::AbcGeom::IPolyMesh;
    using ISubD = Alembic::AbcGeom::ISubD;
    using ICurves = Alembic::AbcGeom::ICurves;
    using IPoints = Alembic::AbcGeom::IPoints;
    using INuPatch = Alembic::AbcGeom::INuPatch;
    using IFaceSet = Alembic::AbcGeom::IFaceSet;
    using IXformSchema = Alembic::AbcGeom::IXformSchema;
    using IPolyMeshSchema = Alembic::AbcGeom::IPolyMeshSchema;
    using ISubDSchema = Alembic::AbcGeom::ISubDSchema;
    using ICurvesSchema = Alembic::AbcGeom::ICurvesSchema;
    using IPointsSchema = Alembic::AbcGeom::IPointsSchema;
    using INuPatchSchema = Alembic::AbcGeom::INuPatchSchema;
    using IFaceSetSchema = Alembic::AbcGeom::IFaceSetSchema;
    using ICamera = Alembic::AbcGeom::ICamera;
    using XformSample = Alembic::AbcGeom::XformSample;
    using IVisibilityProperty = Alembic::AbcGeom::IVisibilityProperty;

    using PathList = GABC_Util::PathList;
    using PropertyMap = GABC_Util::PropertyMap;
    using PropertyMapInsert = GABC_Util::PropertyMapInsert;

    using AbcTransformMap = UT_StringMap<LocalWorldXform>;
    using AbcVisibilityMap = UT_StringMap<GABC_VisibilityType>;
    using ArchiveCacheEntryPtr = UT_SharedPtr<ArchiveCacheEntry>;

    using HeaderMap = UT_SortedMap<std::string, const PropertyHeader *>;
    using HeaderMapInsert = std::pair<std::string, const PropertyHeader *>;

    using UPSample = std::pair<GT_DataArrayHandle, PlainOldDataType>;
    using UPSampleMap = UT_Map<GABC_OProperty *, UPSample>;
    using UPSampleMapInsert = std::pair<GABC_OProperty *, UPSample>;

    static UT_Lock		theFileLock;
    static UT_Lock		theOCacheLock;
    static UT_Lock		theXCacheLock;
    static UT_Lock		theVisibilityCacheLock;

    const WrapExistingFlag gabcWrapExisting = Alembic::Abc::kWrapExisting;

    // Convenience class for resetting a Walker objects filenames to its
    // original values.
    class WalkPushFile
    {
    public:
	WalkPushFile(GABC_Util::Walker &walk, const std::vector<std::string> &filenames)
	    : myWalk(walk)
	    , myFilenames(walk.filenames())
	{
	    myWalk.setFilenames(filenames);
	}
	~WalkPushFile()
	{
	    myWalk.setFilenames(myFilenames);
	}

    private:
	GABC_Util::Walker  &myWalk;
	std::vector<std::string> myFilenames;
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

    // Used to store Visibility objects as items in a UT_CappedCache.
    class ArchiveVisibilityItem : public UT_CappedItem
    {
    public:
	ArchiveVisibilityItem()
	    : UT_CappedItem()
	    , myVisibility(GABC_VISIBLE_DEFER)
	{}
	ArchiveVisibilityItem(GABC_VisibilityType vis)
	    : UT_CappedItem()
	    , myVisibility(vis)
	{}

	virtual int64 getMemoryUsage() const  { return sizeof(*this); }
	GABC_VisibilityType visibility() const   { return myVisibility; }

    private:
	GABC_VisibilityType myVisibility;
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
	using ArchiveEventHandler = GABC_Util::ArchiveEventHandler;
	using ArchiveEventHandlerPtr = GABC_Util::ArchiveEventHandlerPtr;
	using HandlerSetType = UT_Set<ArchiveEventHandlerPtr>;

        ArchiveCacheEntry()
	    : myCache("abcObjects", 8)
	    , myDynamicXforms("abcTransforms", 64)
	    , myDynamicVisibility("abcVisibility", 64)
	    , myDynamicFullVisibility("abcFullVisibility", 64)
	    , myXformCacheBuilt(false)
        {}
        virtual ~ArchiveCacheEntry()
        {}

	void
	purge()
	{
	    if (myArchive)
	    {
		for (auto it = myHandlers.begin(); it != myHandlers.end(); ++it)
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
		if (root.getChildNodeType(i) == GABC_XFORM)
		{
		    kid = root.getChild(i);
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
            ensureValidTransformCache();

            auto it = myStaticXforms.find(fullpath);
            if (it != myStaticXforms.end())
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
            ensureValidTransformCache();

            auto it = myStaticXforms.find(fullpath);
            if (it != myStaticXforms.end())
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
	    ensureValidTransformCache();
	    return myStaticXforms.count(obj.getFullName().c_str()) == 0;
	}

	/// Find the full world transform for an object
	bool
	getWorldTransform(M44d &x,
	        const GABC_IObject &obj,
	        fpreal now,
                bool &isConstant,
                bool &inheritsXform)
        {
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
                auto xitem = UTverify_cast<ArchiveTransformItem *>(item.get());

                x = xitem->getWorld();
                isConstant = xitem->isConstant();
                inheritsXform = xitem->inheritsXform();
            }
            else
            {
		UT_AutoLock	lock(myTransformLock);

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

	GABC_VisibilityType
	getVisibilityInternal(const GABC_IObject &obj,
	        fpreal now,
		bool &animated,
		bool check_parent)
        {
	    animated = false;

            std::string path = obj.getFullName();
	    ArchiveObjectKey key(path.c_str(), now);
	    if(check_parent)
	    {
		// check if it is in our static full visibility cache
		auto it = myStaticFullVisibility.find(path);
		if (it != myStaticFullVisibility.end())
		    return it->second;

		// check if it is in our dynamic full visibility cache
		UT_CappedItemHandle item = myDynamicFullVisibility.findItem(key);
		if (item)
		{
		    animated = true;

		    ArchiveVisibilityItem *vitem =
			    UTverify_cast<ArchiveVisibilityItem *>(item.get());
		    return vitem->visibility();
		}
	    }

	    // check if it is in our static visibility cache
	    GABC_VisibilityType vis = GABC_VISIBLE_DEFER;
	    auto it = myStaticVisibility.find(path);
	    if (it != myStaticVisibility.end())
	    {
		vis = it->second;
		if(!check_parent || vis != GABC_VISIBLE_DEFER)
		    return vis;
	    }
	    else
	    {
		// check if it is in our dynamic visibility cache
		UT_CappedItemHandle item = myDynamicVisibility.findItem(key);
		if (item)
		{
		    animated = true;

		    ArchiveVisibilityItem *vitem =
			    UTverify_cast<ArchiveVisibilityItem *>(item.get());
		    vis = vitem->visibility();
		    if(!check_parent || vis != GABC_VISIBLE_DEFER)
			return vis;
		}
		else
		{
		    // compute visibility
		    IObject o = obj.object();
		    IVisibilityProperty vprop =
				Alembic::AbcGeom::GetVisibilityProperty(o);
		    if (vprop.valid())
		    {
			animated = !vprop.isConstant();
			ISampleSelector iss(now);

			switch (vprop.getValue(iss))
			{
			    default:
				UT_ASSERT(0 && "Strange visibility value");
				// fall through...

			    case -1:
				vis = GABC_VISIBLE_DEFER;
				break;

			    case 0:
				vis = GABC_VISIBLE_HIDDEN;
				break;

			    case 1:
				vis = GABC_VISIBLE_VISIBLE;
				break;
			}
		    }

		    // cache computed visibility
		    if(animated)
			myDynamicVisibility.addItem(key, new ArchiveVisibilityItem(vis));
		    else
			myStaticVisibility[path] = vis;
		}
	    }

	    if(check_parent && vis == GABC_VISIBLE_DEFER)
	    {
		GABC_IObject parent = obj.getParent();
		if(parent.valid())
		{
		    // recurse up to parent
		    bool parent_animated;
		    vis = getVisibilityInternal(parent, now, parent_animated, true);
		    animated |= parent_animated;
		}
		else
		    vis = GABC_VISIBLE_VISIBLE;

		// cache visibility
		if(animated)
		    myDynamicFullVisibility.addItem(key, new ArchiveVisibilityItem(vis));
		else
		    myStaticFullVisibility[path] = vis;
	    }

	    return vis;
	}

	/// Get an object's visibility
	GABC_VisibilityType
	getVisibility(const GABC_IObject &obj,
	        fpreal now,
		bool &animated,
		bool check_parent)
        {
            UT_AutoLock	lock(theVisibilityCacheLock);
	    return getVisibilityInternal(obj, now, animated, check_parent);
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
            using Separator = hboost::char_separator<char>;
            using Tokenizer = hboost::tokenizer<Separator>;

            Tokenizer   tokenizer(objectPath, Separator( "/" ));
            for (auto iter = tokenizer.begin();
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
	void    setError(const std::string &e)          { myError = e; }

    private:
	GABC_IObject		root()
        {
            return myArchive ? myArchive->getTop() : GABC_IObject();
        }

	GABC_IArchivePtr	myArchive;
	std::string		myError;
	PathList		myObjectList;
	PathList		myFullObjectList;
	bool			myXformCacheBuilt;
	AbcTransformMap		myStaticXforms;
	UT_CappedCache		myCache;
	UT_CappedCache		myDynamicXforms;
	AbcVisibilityMap	myStaticVisibility;
	AbcVisibilityMap	myStaticFullVisibility;
	UT_CappedCache		myDynamicVisibility;
	UT_CappedCache		myDynamicFullVisibility;
	HandlerSetType		myHandlers;
	UT_Lock			myTransformLock;
    };

    //-*************************************************************************

    size_t g_maxCache = 50;

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
    pathMap(const std::string &path)
    {
	if (path == "")
	    return false;
	FS_Info	finfo(path.c_str());
	return finfo.hasAccess(FS_READ);
    }

    class ArchiveCache
    {
    public:
	ArchiveCache() {}
	~ArchiveCache() { clearArchive(); }

	ArchiveCacheEntryPtr
	findArchive(const std::vector<std::string> &paths)
	{
	    for (auto &it : paths)
	    {
		if (!pathMap(it))
		{
		    badFileWarning(it);
		    return ArchiveCacheEntryPtr();
		}
	    }

	    auto I = myItems.find(paths);
	    if (I != myItems.end())
		return I->second;

	    return ArchiveCacheEntryPtr();
	}

	ArchiveCacheEntryPtr
	loadArchive(const std::vector<std::string> &paths)
	{
	    UT_AutoLock lock(theFileLock);

	    auto I = myItems.find(paths);
	    if (I != myItems.end())
		return I->second;

	    if (!paths.size())
		return ArchiveCacheEntryPtr(new ArchiveCacheEntry());

	    for (auto &it : paths)
	    {
		if (!pathMap(it))
		{
		    badFileWarning(it);
		    return ArchiveCacheEntryPtr(new ArchiveCacheEntry());
		}
	    }

	    auto entry = ArchiveCacheEntryPtr(new ArchiveCacheEntry());
	    entry->setArchive(GABC_IArchive::open(paths));
	    while (myItems.size() >= g_maxCache)
	    {
		long d = static_cast<long>(
			std::floor(static_cast<double>(std::rand())
				    / RAND_MAX * g_maxCache - 0.5));
		if (d < 0)
		    d = 0;

		auto it = myItems.begin();
		for (; d > 0; --d)
		    ++it;
		removeCacheEntry(it->first);
	    }

	    addCacheEntry(paths, entry);
	    return entry;
	}

	void
	clearArchive(const char *path=nullptr)
	{
	    if(path)
	    {
		std::string tmp = path;
		auto it = myKeysWithPath.find(tmp);
		if(it != myKeysWithPath.end())
		{
		    auto keys = it->second;
		    for(auto &paths : keys)
			removeCacheEntry(paths);
		}
	    }
	    else
	    {
		for (auto &it : myItems)
		    it.second->purge();

		myItems.clear();
		myKeysWithPath.clear();
	    }
	}

    private:
	void
	addCacheEntry(const std::vector<std::string> &paths,
		      const ArchiveCacheEntryPtr &entry)
	{
	    for(auto &path : paths)
		myKeysWithPath[path].insert(paths);

	    myItems[paths] = entry;
	}

	void
	removeCacheEntry(const std::vector<std::string> &paths)
	{
	    for(auto &p : paths)
	    {
		auto it = myKeysWithPath.find(p);
		it->second.erase(paths);
		if(it->second.empty())
		    myKeysWithPath.erase(p);
	    }

	    auto it = myItems.find(paths);
	    it->second->purge();
	    myItems.erase(it);
	}

	UT_Map<std::vector<std::string>, ArchiveCacheEntryPtr> myItems;
	UT_Map<std::string, UT_Set<std::vector<std::string> > > myKeysWithPath;
    };

    // for now, leak the pointer to the archive cache so we don't
    // crash at shutdown
    ArchiveCache *g_archiveCache(new ArchiveCache);

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
    importUserPropertyHelper(UT_JSONWriter *data_writer,
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

                success = importUserPropertyHelper(data_writer,
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
                                            time,
					    GEO_PackedNameMapPtr());

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

    // we want to walk children in sorted order
    UT_SortedMap<std::string, GABC_IObject, UTnumberedStringCompare> child_map;
    for (exint i = 0; i < nkids; ++i)
    {
	// FIXME: can we update this so we do not need to call getChild() on
	// every instanced piece of geometry?
	GABC_IObject child = obj.getChild(i);
	child_map.emplace(child.getName(), child);
    }

    for (auto &it : child_map)
    {
	// Returns false on interrupt
	if (!ArchiveCacheEntry::walkTree(it.second, *this))
	    return false;
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

#define GABC_CONST_STR(VAR, VAL) \
    const UT_StringHolder GABC_Util::VAR(UTmakeUnsafeRef(VAL));

GABC_CONST_STR(theLockGeometryParameter, "abc_lock_geom");
GABC_CONST_STR(theUserPropsValsAttrib,   "abc_userProperties");
GABC_CONST_STR(theUserPropsMetaAttrib,   "abc_userPropertiesMetadata");

#define YSTR(X)	#X		// Stringize
#define XSTR(X)	YSTR(X)		// Expand the stringized version
const char *
GABC_Util::getAlembicCompileNamespace()
{
    return XSTR(ALEMBIC_VERSION_NS);
}

bool
GABC_Util::walk(const std::vector<std::string> &filenames,
        Walker &walker,
        const UT_StringArray &objects)
{
    auto cacheEntry = g_archiveCache->loadArchive(filenames);
    WalkPushFile walkfile(walker, filenames);

    for (exint i = 0; i < objects.entries(); ++i)
    {
	std::string     path(objects(i));
	GABC_IObject obj = findObject(filenames, path);

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
GABC_Util::walk(const std::vector<std::string> &filenames,
        Walker &walker,
        const UT_Set<std::string> &objects)
{
    auto cacheEntry = g_archiveCache->loadArchive(filenames);
    WalkPushFile walkfile(walker, filenames);

    walker.myBadArchive = !cacheEntry->isValid();
    for (auto it = objects.begin(); it != objects.end(); ++it)
    {
	GABC_IObject obj = findObject(filenames, *it);

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
GABC_Util::walk(const std::vector<std::string> &filenames, GABC_Util::Walker &walker)
{
    auto cacheEntry = g_archiveCache->loadArchive(filenames);

    walker.myBadArchive = !cacheEntry->isValid();
    if (walker.myBadArchive)
    {
	return false;
    }

    WalkPushFile walkfile(walker, filenames);
    return cacheEntry->walk(walker);
}

void
GABC_Util::clearCache(const char *filename)
{
    g_archiveCache->clearArchive(filename);
    GT_PackedGeoCache::clearAlembics(filename);
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
GABC_Util::findObject(const std::vector<std::string> &filenames,
	const std::string &objectpath)
{
    auto cacheEntry = g_archiveCache->loadArchive(filenames);
    return cacheEntry->isValid()
            ? cacheEntry->getObject(objectpath)
            : GABC_IObject();
}

GABC_IObject
GABC_Util::findObject(const std::vector<std::string> &filenames, ObjectReaderPtr reader)
{
    auto cacheEntry = g_archiveCache->loadArchive(filenames);
    return cacheEntry->isValid()
            ? cacheEntry->getObject(reader)
            : GABC_IObject();
}

bool
GABC_Util::getLocalTransform(const std::vector<std::string> &filenames,
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
	auto cacheEntry = g_archiveCache->loadArchive(filenames);
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
GABC_Util::getWorldTransform(const std::vector<std::string> &filenames,
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
	auto cacheEntry = g_archiveCache->loadArchive(filenames);
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
GABC_Util::getWorldTransform(
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
	    auto &filenames = obj.archive()->filenames();
	    auto cacheEntry = g_archiveCache->loadArchive(filenames);
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
GABC_Util::isTransformAnimated(const GABC_IObject &obj)
{
    bool    animated = false;

    if (obj.valid())
    {
	try
	{
	    auto &filenames = obj.archive()->filenames();
	    auto cacheEntry = g_archiveCache->loadArchive(filenames);
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

GABC_VisibilityType
GABC_Util::getVisibility(
    const GABC_IObject &obj,
    fpreal sample_time,
    bool &animated,
    bool check_parent)
{
    GABC_VisibilityType vis = GABC_VISIBLE_HIDDEN;
    animated = false;
    if (obj.valid())
    {
	try
	{
	    auto &filenames = obj.archive()->filenames();
	    auto cacheEntry = g_archiveCache->loadArchive(filenames);
	    UT_ASSERT_P(cacheEntry->getObject(obj.getFullName()).valid());
	    vis = cacheEntry->getVisibility(obj, sample_time, animated,
					    check_parent);
	}
	catch (const std::exception &)
	{
	    vis = GABC_VISIBLE_HIDDEN;
	    animated = false;
	}
    }

    return vis;
}

bool
GABC_Util::addEventHandler(const std::vector<std::string> &paths,
	const GABC_Util::ArchiveEventHandlerPtr &handler)
{
    if (handler)
    {
	auto arch = g_archiveCache->findArchive(paths);
	if (arch && arch->addHandler(handler))
	    return true;
    }

    return false;
}

const PathList &
GABC_Util::getObjectList(
    const std::vector<std::string> &filenames, bool with_fsets)
{
    static PathList theEmptyList;

    try
    {
	auto cacheEntry = g_archiveCache->loadArchive(filenames);
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
GABC_Util::importUserPropertyDictionary(UT_JSONWriter *data_writer,
        UT_JSONWriter *meta_writer,
        const GABC_IObject &obj,
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

    auto uprops = obj.getUserProperties();
    success = importUserPropertyHelper(data_writer,
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
GABC_Util::exportUserPropertyDictionary(UT_AutoJSONParser &meta_data,
        UT_AutoJSONParser &vals_data,
        PropertyMap &up_map,
        OCompoundProperty *ancestor,
        GABC_OError &err,
        const GABC_OOptions &ctx)
{
    if (!ancestor && !up_map.size())
        return true;

    // Parse the beginning of the map.
    bool error_parsing = false;
    if (!meta_data->parseBeginMap(error_parsing)
	|| !vals_data->parseBeginMap(error_parsing))
    {
        err.warning("Error parsing user properties: no valid JSON map in "
                "attribute(s).");
        return false;
    }

    bool m_finished = meta_data->parseEndMap(error_parsing);
    bool v_finished = vals_data->parseEndMap(error_parsing);

    GABC_OProperty             *prop;
    OCompoundProperty           parent;
    UPSampleMap                 sample_map;
    UT_WorkBuffer               m_key;
    UT_WorkBuffer               v_key;
    UT_WorkBuffer               work_buffer;
    const char                 *name;
    bool                        success_parsing = true;

    // Keep reading the until one of the maps ends.
    while (!m_finished && !v_finished)
    {
        bool output = true;
        success_parsing = true;
        error_parsing = false;
        GT_Type gt_type = GT_TYPE_NONE;
        int array_size = 0;

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

        PropertyMap::iterator it = up_map.find(m_key.buffer());
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
                const char *up_path = m_key.buffer();

                while (true)
                {
                    name = up_path;
                    work_buffer.clear();
                    work_buffer.getNextToken(up_path, "/");

                    // Exit if no more tokens, we've found the property name.
                    if (!(*up_path))
                        break;

                    // Otherwise, this token is the name of a parent compound
                    // property. Check if a property with that name
                    // already exists.
                    OBaseProperty   bp = parent.getProperty(
                                            work_buffer.toStdString());
                    if (bp.valid())
                    {
                        // If a property exists, check that it is a compound
                        // property.
                        CompoundPropertyWriterPtr cpw = bp.getPtr()->asCompoundPtr();
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
            if (it != up_map.end())
                prop = it->second;
            else
            {
                err.warning("User property %s was not declared on first frame, "
                        "so it was ignored.",
                        m_key.buffer());
                output = false;
            }
        }

        //
        //  Read metadata
        //

        success_parsing &= meta_data->parseBeginArray(error_parsing);

        // Read storage type
        success_parsing &= meta_data->parseString(work_buffer);

        // Read Alembic POD
        success_parsing &= meta_data->parseString(work_buffer);
	PlainOldDataType pod = AbcPOD(work_buffer.buffer());

        // Read tuple size
	int tuple_size;
        success_parsing &= meta_data->parseValue(tuple_size);

        // Read tuple interpretation
        if (tuple_size > 1)
        {
            success_parsing &= meta_data->parseString(work_buffer);
            gt_type = GTtype(work_buffer.buffer());

            if ((gt_type == GT_TYPE_MATRIX) && (tuple_size == 9))
                gt_type = GT_TYPE_MATRIX3;
            else if ((gt_type == GT_TYPE_BOX) && (tuple_size == 4))
                gt_type = GT_TYPE_BOX2;
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
	GT_DataArrayHandle data_array;
	exint   items = (array_size ? array_size : 1);

	switch (pod)
	{
	    case Alembic::Util::kBooleanPOD:
	    case Alembic::Util::kInt8POD:
		data_array = parseJSONValuesArray<GT_Int8Array>(
			vals_data, items, tuple_size, gt_type);
		break;

	    case Alembic::Util::kUint8POD:
		data_array = parseJSONValuesArray<GT_UInt8Array>(
			vals_data, items, tuple_size, gt_type);
		break;

	    case Alembic::Util::kInt16POD:
		data_array = parseJSONValuesArray<GT_Int16Array>(
			vals_data, items, tuple_size, gt_type);
		break;

	    case Alembic::Util::kUint16POD:
	    case Alembic::Util::kInt32POD:
		data_array = parseJSONValuesArray<GT_Int32Array>(
			vals_data, items, tuple_size, gt_type);
		break;

	    case Alembic::Util::kUint32POD:
	    case Alembic::Util::kInt64POD:
	    case Alembic::Util::kUint64POD:
		data_array = parseJSONValuesArray<GT_Int64Array>(
			vals_data, items, tuple_size, gt_type);
		break;

	    case Alembic::Util::kFloat16POD:
		data_array = parseJSONValuesArray<GT_Real16Array>(
			vals_data, items, tuple_size, gt_type);
		break;

	    case Alembic::Util::kFloat32POD:
		data_array = parseJSONValuesArray<GT_Real32Array>(
			vals_data, items, tuple_size, gt_type);
		break;

	    case Alembic::Util::kFloat64POD:
		data_array = parseJSONValuesArray<GT_Real64Array>(
			vals_data, items, tuple_size, gt_type);
		break;

	    case Alembic::Util::kStringPOD:
	    case Alembic::Util::kWstringPOD:
		{
		    // There is no special Alembic data type that is a
		    // string tuple. The equivalent would be an array of
		    // strings.
		    UT_ASSERT(tuple_size == 1);
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
		break;

	    default:
		// Since this is a type error, it has to do with
		// reading metadata.
		err.warning("Error reading user property metadata: "
			"unrecognized Houdini storage type.");
		return false;
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
                    prop = new GABC_OArrayProperty();
                else
                    prop = new GABC_OScalarProperty();

                if (!prop->start(parent, name, data_array, err, ctx, pod))
                    err.warning("Skipping property %s.", m_key.buffer());
                else
                    up_map.insert(PropertyMapInsert(m_key.toStdString(), prop));
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
        for (PropertyMap::iterator it = up_map.begin(); it != up_map.end(); ++it)
        {
            prop = it->second;
	    UPSampleMap::iterator s_it = sample_map.find(prop);

            if (s_it == sample_map.end())
            {
                err.warning("Could not find property %s. Reusing previous sample.",
                        it->first.c_str());
                prop->updateFromPrevious();
            }
            else
            {
		UPSample *sample = &s_it->second;

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

void
GABC_Util::CollisionResolver::resolve(std::string &name) const
{
    auto it = myMaxId.find(name);
    exint val = (it != myMaxId.end()) ? it->second + 1 : 1;
    UT_WorkBuffer buf;
    buf.append(name.c_str());
    buf.appendSprintf("_%" SYS_PRId64, val);
    name = buf.buffer();
}

void
GABC_Util::CollisionResolver::add(const std::string &name)
{
    exint n = name.length();
    const char *s = name.c_str();
    for(exint i = n; i > 0; --i)
    {
	char c = s[i - 1];
	if(c < '0' || c > '9')
	{
	    if(c == '_' && i < n)
	    {
		UT_WorkBuffer buf;
		buf.strncpy(s, i - 1);
		std::string base_name(buf.buffer());
		exint val = 0;
		const char *si = &s[i];
		while(*si)
		    val = 10 * val + *(si++) - '0';

		auto it = myMaxId.find(base_name);
		if(it != myMaxId.end())
		{
		    if(val > it->second)
			it->second = val;
		}
		else
		    myMaxId.emplace(base_name, val);
	    }
	    break;
	}
    }
}

fpreal
GABC_Util::getSampleIndex(fpreal t,
    const TimeSamplingPtr &itime,
    exint nsamp,
    index_t &i0,
    index_t &i1)
{
    static const fpreal timeBias = 0.0001;

    nsamp = SYSmax(nsamp, 1);
    std::pair<index_t, chrono_t> t0 = itime->getFloorIndex(t, nsamp);
    i0 = i1 = t0.first;
    if (nsamp == 1 || SYSisEqual(t, t0.second, timeBias))
	return 0;

    std::pair<index_t, chrono_t> t1 = itime->getCeilIndex(t, nsamp);
    i1 = t1.first;
    if (i0 == i1)
	return 0;

    fpreal bias = (t - t0.second) / (t1.second - t0.second);
    if (SYSisEqual(bias, 1, timeBias))
    {
	i0 = i1;
	return 0;
    }

    return bias;
}
