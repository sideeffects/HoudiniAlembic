/*
 * Copyright (c) 2019
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
#include <Alembic/Abc/TypedPropertyTraits.h>
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
#include <UT/UT_UniquePtr.h>
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
    using AbcBoundingBoxMap = UT_StringMap<UT_BoundingBox>;
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
    static UT_Lock		theBoundingBoxCacheLock;

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

    // Used to store bounding boxes as items in a UT_CappedCache.
    class ArchiveBoundingBoxItem : public UT_CappedItem
    {
    public:
	ArchiveBoundingBoxItem()
	    : UT_CappedItem()
	{
	    myBox.makeInvalid();
	}
	ArchiveBoundingBoxItem(const UT_BoundingBox &box)
	    : UT_CappedItem()
	    , myBox(box)
	{}

	virtual int64 getMemoryUsage() const  { return sizeof(*this); }
	void getBoundingBox(UT_BoundingBox &box) const { box = myBox; }

    private:
	UT_BoundingBox myBox;
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
	    : myXformCacheBuilt(false)
	    , myPurged(false)
	    , myCache("Alembic Object Cache", 8)
	    , myDynamicXforms("Alembic Transform Cache", 64)
	    , myDynamicVisibility("Alembic Visibility Cache", 64)
	    , myDynamicFullVisibility("Alembic Full Visibility Cache", 64)
	    , myDynamicBoundingBoxes("Alembic Bounding Box Cache", 64)
        {}
        virtual ~ArchiveCacheEntry()
	{
	    if (myArchive)
	    {
		for (auto it = myHandlers.begin(); it != myHandlers.end(); ++it)
		{
		    const ArchiveEventHandlerPtr   &handler = *it;
		    if (handler->archive() == myArchive.get())
		    {
			handler->cleared(myPurged);
			handler->setArchivePtr(NULL);
		    }
		}
	    }
	}

	bool
	addHandler(const ArchiveEventHandlerPtr &handler)
        {
            if (!myArchive || !handler)
		return false;

	    myHandlers.insert(handler);
	    handler->setArchivePtr(myArchive.get());
	    return true;
        }

	bool
	walk(GABC_Util::Walker &walker)
        {
            if (!walker.preProcess(root()))
                return false;

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
			    world = localXform * parent;
			else
			    world = localXform;

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

	/// Check to see if there's a const world transform cached
	bool
	staticWorldTransform(const char *fullpath, M44d &xform)
        {
            ensureValidTransformCache();

            auto it = myStaticXforms.find(fullpath);
	    if (it == myStaticXforms.end())
		return false;

	    xform = it->second.getWorld();
	    return true;
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
	    if(!obj.valid())
		return false;

	    GABC_NodeType type = obj.nodeType();
	    if(type != GABC_XFORM)
	    {
		if(type == GABC_ROOT)
		{
		    x.makeIdentity();
		    isConstant = true;
		    inheritsXform = false;
		    return true;
		}

		return getWorldTransform(x, obj.getParent(), now, isConstant,
					 inheritsXform);
	    }

            const std::string &path = obj.getFullName();
            ArchiveObjectKey key(path.c_str(), now);
            UT_CappedItemHandle item;
            isConstant = true;

            // First, check if we have a static
            if (staticWorldTransform(path.c_str(), x))
                return true;

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
                        isConstant = false;
                    if (inheritsXform)
                        x = localXform * dm;
                }
                else
                    x = localXform;	// World transform same as local

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

            const std::string &path = obj.getFullName();
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

	bool
	getBoundingBoxInternal(const GABC_IObject &obj,
	        fpreal now,
		UT_BoundingBox &box,
		bool &isconst)
        {
	    isconst = true;

            const std::string &path = obj.getFullName();

	    // check if it is in our static bounds cache
	    auto it = myStaticBoundingBoxes.find(path);
	    if (it != myStaticBoundingBoxes.end())
	    {
		box = it->second;
		return true;
	    }

	    // check if it is in our dynamic bounds cache
	    ArchiveObjectKey key(path.c_str(), now);
	    UT_CappedItemHandle item = myDynamicBoundingBoxes.findItem(key);
	    if (item)
	    {
		isconst = false;

		ArchiveBoundingBoxItem *bitem =
			UTverify_cast<ArchiveBoundingBoxItem *>(item.get());
		bitem->getBoundingBox(box);
		return true;
	    }

	    bool success = obj.getBoundingBox(box, now, isconst);

	    if(success)
	    {
		// cache computed bounds
		if(isconst)
		    myStaticBoundingBoxes[path] = box;
		else
		    myDynamicBoundingBoxes.addItem(key, new ArchiveBoundingBoxItem(box));
	    }
	    return success;
	}

	/// Get an object's bounds
	bool
	getBoundingBox(const GABC_IObject &obj,
	        fpreal now,
		UT_BoundingBox &box,
		bool &isconst)
        {
            UT_AutoLock	lock(theBoundingBoxCacheLock);
	    return getBoundingBoxInternal(obj, now, box, isconst);
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
                    myCache.addItem(key, new ArchiveObjectItem(kid));
            }

            return kid;
        }

        void
        tokenizeObjectPath(const std::string & objectPath, PathList & pathList)
        {
            using Separator = hboost::char_separator<char>;
            using Tokenizer = hboost::tokenizer<Separator>;

            Tokenizer   tokenizer(objectPath, Separator( "/" ));
	    for(auto iter = tokenizer.begin(); iter != tokenizer.end(); ++iter)
            {
                if (iter->empty())
                    continue;

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
		    myObjectList.push_back(obj.getFullName());

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
                PathListWalker	func(myObjectList, myFullObjectList);
                walk(func);
            }
            return full ? myFullObjectList : myObjectList;
        }

	static bool
	walkTree(const GABC_IObject &node,
	        GABC_Util::Walker &walker)
        {
            if (walker.interrupted())
                return false;

            if (walker.process(node))
                walker.walkChildren(node);
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

	void
	setArchive(const std::vector<std::string> &paths)
	{
	    myAccessTimes.clear();

	    UT_FileStat stat;
	    for(auto &p : paths)
	    {
		if(!UTfileStat(p.c_str(), &stat))
		    myAccessTimes.emplace(p, stat.myModTime);
	    }
	    myArchive = GABC_IArchive::open(paths);
	}

	bool
	clearIfModified() const
	{
	    UT_FileStat stat;
	    for(auto &it : myAccessTimes)
	    {
		if(UTfileStat(it.first.c_str(), &stat)
		    || it.second != stat.myModTime)
		{
		    GABC_Util::clearCache(it.first.c_str());
		    return true;
		}
	    }
	    return false;
	}

	void purgeObjects()
	{
	    archive()->purgeObjects();
	    myPurged = true;
	}

	void    setError(const std::string &e)          { myError = e; }

    private:
	GABC_IObject		root()
        {
            return myArchive ? myArchive->getTop() : GABC_IObject();
        }

	GABC_IArchivePtr	myArchive;
	UT_Map<std::string, time_t> myAccessTimes;
	std::string		myError;
	PathList		myObjectList;
	PathList		myFullObjectList;
	bool			myXformCacheBuilt;
	bool			myPurged;
	AbcTransformMap		myStaticXforms;
	UT_CappedCache		myCache;
	UT_CappedCache		myDynamicXforms;
	AbcVisibilityMap	myStaticVisibility;
	AbcVisibilityMap	myStaticFullVisibility;
	UT_CappedCache		myDynamicVisibility;
	UT_CappedCache		myDynamicFullVisibility;
	AbcBoundingBoxMap	myStaticBoundingBoxes;
	UT_CappedCache		myDynamicBoundingBoxes;
	HandlerSetType		myHandlers;
	UT_Lock			myTransformLock;
    };

    struct AlembicTraitEquivalence
    {
	AlembicTraitEquivalence(const char *n, const char *i,
	    int e, GT_Type g, PlainOldDataType p)
	    : myName(n), myInterpretation(i), myExtent(e),
	      myGtType(g), myPodEnum(p) {}

	const char	*myName;
	const char	*myInterpretation;
	int		 myExtent;
	GT_Type		 myGtType;
	PlainOldDataType myPodEnum;
    };

    #define GABC_TRAIT_EQUIVALENCE(NAME, GTTYPE)		\
	AlembicTraitEquivalence(#NAME,				\
	    Alembic::Abc::NAME##TPTraits::interpretation(),	\
	    Alembic::Abc::NAME##TPTraits::extent, GTTYPE,	\
	    Alembic::Abc::NAME##TPTraits::pod_enum)

    static AlembicTraitEquivalence theEquivalanceArray[] =
    {
	GABC_TRAIT_EQUIVALENCE(Boolean,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(Int8,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(Int16,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(Int32,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(Int64,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(Uint8,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(Uint16,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(Uint32,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(Uint64,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(Float16,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(Float32,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(Float64,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(String,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(Wstring,		GT_TYPE_NONE),
	GABC_TRAIT_EQUIVALENCE(V2s,		GT_TYPE_VECTOR),
	GABC_TRAIT_EQUIVALENCE(V2i,		GT_TYPE_VECTOR),
	GABC_TRAIT_EQUIVALENCE(V2f,		GT_TYPE_VECTOR),
	GABC_TRAIT_EQUIVALENCE(V2d,		GT_TYPE_VECTOR),
	GABC_TRAIT_EQUIVALENCE(V3s,		GT_TYPE_VECTOR),
	GABC_TRAIT_EQUIVALENCE(V3i,		GT_TYPE_VECTOR),
	GABC_TRAIT_EQUIVALENCE(V3f,		GT_TYPE_VECTOR),
	GABC_TRAIT_EQUIVALENCE(V3d,		GT_TYPE_VECTOR),
	GABC_TRAIT_EQUIVALENCE(P2s,		GT_TYPE_POINT),
	GABC_TRAIT_EQUIVALENCE(P2i,		GT_TYPE_POINT),
	GABC_TRAIT_EQUIVALENCE(P2f,		GT_TYPE_POINT),
	GABC_TRAIT_EQUIVALENCE(P2d,		GT_TYPE_POINT),
	GABC_TRAIT_EQUIVALENCE(P3s,		GT_TYPE_POINT),
	GABC_TRAIT_EQUIVALENCE(P3i,		GT_TYPE_POINT),
	GABC_TRAIT_EQUIVALENCE(P3f,		GT_TYPE_POINT),
	GABC_TRAIT_EQUIVALENCE(P3d,		GT_TYPE_POINT),
	GABC_TRAIT_EQUIVALENCE(Box2s,		GT_TYPE_BOX2),
	GABC_TRAIT_EQUIVALENCE(Box2i,		GT_TYPE_BOX2),
	GABC_TRAIT_EQUIVALENCE(Box2f,		GT_TYPE_BOX2),
	GABC_TRAIT_EQUIVALENCE(Box2d,		GT_TYPE_BOX2),
	GABC_TRAIT_EQUIVALENCE(Box3s,		GT_TYPE_BOX),
	GABC_TRAIT_EQUIVALENCE(Box3i,		GT_TYPE_BOX),
	GABC_TRAIT_EQUIVALENCE(Box3f,		GT_TYPE_BOX),
	GABC_TRAIT_EQUIVALENCE(Box3d,		GT_TYPE_BOX),
	GABC_TRAIT_EQUIVALENCE(M33f,		GT_TYPE_MATRIX3),
	GABC_TRAIT_EQUIVALENCE(M33d,		GT_TYPE_MATRIX3),
	GABC_TRAIT_EQUIVALENCE(M44f,		GT_TYPE_MATRIX),
	GABC_TRAIT_EQUIVALENCE(M44d,		GT_TYPE_MATRIX),
	GABC_TRAIT_EQUIVALENCE(Quatf,		GT_TYPE_QUATERNION),
	GABC_TRAIT_EQUIVALENCE(Quatd,		GT_TYPE_QUATERNION),
	GABC_TRAIT_EQUIVALENCE(C3h,		GT_TYPE_COLOR),
	GABC_TRAIT_EQUIVALENCE(C3f,		GT_TYPE_COLOR),
	GABC_TRAIT_EQUIVALENCE(C3c,		GT_TYPE_COLOR),
	GABC_TRAIT_EQUIVALENCE(C4h,		GT_TYPE_COLOR),
	GABC_TRAIT_EQUIVALENCE(C4f,		GT_TYPE_COLOR),
	GABC_TRAIT_EQUIVALENCE(C4c,		GT_TYPE_COLOR),
	GABC_TRAIT_EQUIVALENCE(N2f,		GT_TYPE_NORMAL),
	GABC_TRAIT_EQUIVALENCE(N2d,		GT_TYPE_NORMAL),
	GABC_TRAIT_EQUIVALENCE(N3f,		GT_TYPE_NORMAL),
	GABC_TRAIT_EQUIVALENCE(N3d,		GT_TYPE_NORMAL),
    };

    #undef GABC_TRAIT_EQUIVALENCE

    class UserPropStorage
    {
    public:
	UserPropStorage(UT_AutoJSONParser &meta_data,
	    UT_AutoJSONParser &vals_data, GABC_OError &err)
	{
	    bool dummy;
	    bool succ_parsing = true;
	    bool err_parsing  = false;
	    UT_WorkBuffer key;

	    // Parse the meta data.
	    for(auto meta_it = meta_data->beginMap();
		!meta_it.atEnd(); meta_it.advance())
	    {
		if(!meta_it.getKey(key))
		{
		    err.warning("Invalid key while parsing "
			"user property metadatas.");
		    break;
		}

		myProps[key.buffer()].reset(new Item());

		// Populates type identifier.
		if(!meta_data->parseBeginArray(dummy))
		{
		    UT_WorkBuffer typeName;
		    succ_parsing &= meta_data->parseString(typeName);
		    myProps[key.buffer()]->setDataType(typeName.buffer());
		}
		// For backward compatibility.
		else
		{
		    int extent, array_size = 0;
		    UT_WorkBuffer pod, interpretation;

		    // Read storage type
		    succ_parsing &= meta_data->parseString(pod);
		    // Read Alembic POD
		    succ_parsing &= meta_data->parseString(pod);
		    // Read tuple size
		    succ_parsing &= meta_data->parseValue(extent);
		    // Read tuple interpretation
		    if(extent > 1)
			succ_parsing &= meta_data->parseString(interpretation);
		    // Read array size
		    if (!meta_data->parseEndArray(dummy))
		    {
			succ_parsing &= meta_data->parseValue(array_size);
			succ_parsing &= meta_data->parseEndArray(err_parsing);
		    }

		    myProps[key.buffer()]->setDataType(pod.buffer(),
			interpretation.buffer(), extent,
			array_size > 0 ? true : false);

		}

		if(!succ_parsing || err_parsing)
		{
		    err.warning("Error parsing user property metadata "
			"occurred while parsing %s.", key.buffer());
		    break;
		}
	    }

	    // Parse the values
	    for(auto vals_it = vals_data->beginMap();
		!vals_it.atEnd(); vals_it.advance())
	    {
		if(!vals_it.getKey(key))
		{
		    err.warning("Invalid key while parsing "
			"user property values.");
		    break;
		}

		auto iter = myProps.find(key.buffer());

		if(iter == myProps.end() || !iter->second->isMetaValid())
		{
		    err.warning("Invalid metadata while parsing the "
			"value of %s.", key.buffer());
		    break;
		}

		iter->second->setDataValue(vals_data, err);
	    }

	    // Sweep the bad data
	    for(auto it = myProps.begin(); it != myProps.end();)
	    {
		if(!it->second->isDataValid())
		{
		    err.warning("Error parsing user property value "
			"occurred while parsing %s.", it->first.c_str());
		    it = myProps.erase(it);
		}
		else
		    ++it;
	    }
	}

	UserPropStorage(const GABC_IObject &obj, fpreal time)
	{
	    ICompoundProperty userProps = obj.getUserProperties();
	    importCompoundProps(obj, userProps,
		UT_String::getEmptyString(), time);

	    // Sweep the bad data
	    for(auto it = myProps.begin(); it != myProps.end();)
	    {
		if(!it->second->isMetaValid() || !it->second->isDataValid())
		    it = myProps.erase(it);
		else
		    ++it;
	    }
	}

	void getKeys(UT_SortedStringSet &tokens) const
	{
	    for(auto it = myProps.begin(); it != myProps.end(); ++it)
		tokens.insert(it->first);
	}

	void writeJSONMetas(UT_JSONWriter &meta_writer)
	{
	    meta_writer.jsonBeginMap();

	    for(auto it = myProps.begin(); it != myProps.end(); ++it)
	    {
		UT_String typeName;
		it->second->getDataType(typeName);

		meta_writer.jsonKey(it->first.c_str());
		meta_writer.jsonString(typeName.c_str());
	    }

	    meta_writer.jsonEndMap();
	}

	void writeJSONValues(UT_JSONWriter &vals_writer)
	{
	    vals_writer.jsonBeginMap();

	    for(auto it = myProps.begin(); it != myProps.end(); ++it)
	    {
		vals_writer.jsonKey(it->first.c_str());
		it->second->getDataValue()->save(vals_writer, false);
	    }

	    vals_writer.jsonEndMap();
	}

	void updateOProperty(PropertyMap &propMap,
			     OCompoundProperty *ancestor,
			     const GABC_OOptions &ctx,
			     const GABC_LayerOptions &lopt,
			     GABC_LayerOptions::LayerType ltype,
			     GABC_OError &err)
	{
	    // Creates new properties
	    if(ancestor)
	    {
		for(auto it = myProps.begin(); it != myProps.end(); ++it)
		{
		    GABC_OProperty *prop;
		    auto propltype = lopt.getUserPropType(
			ancestor->getObject().getFullName().c_str(),
			it->first, ltype);

		    if(propltype == GABC_LayerOptions::LayerType::NONE)
			continue;

		    if (propMap.find(it->first.c_str()) != propMap.end())
		    {
			err.warning("User property %s declared multiple "
			    "times, ignoring multiple declarations.",
			    it->first.c_str());
			continue;
		    }

		    UT_WorkBuffer name;
		    const char *path = it->first.c_str();
		    OCompoundProperty parent = *ancestor;
		    while(true)
		    {
			name.getNextToken(path, "/");

			if(!(*path))
			    break;

			OBaseProperty baseProp = parent.getProperty(
			    name.toStdString());

			if(baseProp.valid())
			{
			    auto compPtr = baseProp.getPtr()->asCompoundPtr();
			    if(compPtr.get())
			    {
				parent = OCompoundProperty(compPtr,
				    gabcWrapExisting,
				    GetErrorHandlerPolicy(parent));
			    }
			    else
			    {
				err.warning("User property %s already "
				    "exists as simple property. Ignoring %s.",
				    name.buffer(), it->first.c_str());
				parent = OCompoundProperty();
				break;
			    }
			}
			else
			{
			    // If no existing property found, make one.
			    parent = OCompoundProperty(
				parent, name.toStdString());
			}
		    }
		    if(!parent.valid())
			continue;

		    if(it->second->isArray())
			prop = new GABC_OArrayProperty(propltype);
		    else // IsScalar
			prop = new GABC_OScalarProperty(propltype);

		    if (!prop->start(parent, name.buffer(),
			it->second->getDataValue(), err,
			ctx, it->second->getPodEnum()))
		    {
			err.warning("Skipping property %s.",
			    it->first.c_str());
			continue;
		    }

		    propMap.insert(PropertyMapInsert(
			it->first.toStdString(), prop));
		}
	    }

	    // Update existing properties
	    else
	    {
		for(auto it = propMap.begin(); it != propMap.end(); ++it)
		{
		    const char *name = it->first.c_str();
		    GABC_OProperty *prop = it->second;
		    auto dataPair = myProps.find(name);

		    if(dataPair == myProps.end())
		    {
			err.warning("Could not find property %s. "
			    "Reusing previous sample.", name);
			prop->updateFromPrevious();
		    }
		    else
		    {
			if(!prop->update(dataPair->second->getDataValue(),
			    err, ctx, dataPair->second->getPodEnum()))
			{
			    err.warning("Could not update property %s. "
				"Reusing previous sample.", name);
			    prop->updateFromPrevious();
			}
		    }
		}
	    }
	}

    private:
	void importCompoundProps(const GABC_IObject &obj,
				 ICompoundProperty &folder,
				 const UT_StringRef &prefix,
				 fpreal time)
	{
	    exint	 numProps = folder.valid() ?
				    folder.getNumProperties() : 0;

	    for(exint i = 0; i < numProps; ++i)
	    {
		const PropertyHeader &header = folder.getPropertyHeader(i);

		UT_WorkBuffer name(header.getName().c_str());
		if(prefix.isstring())
		{
		    name.prepend("/");
		    name.prepend(prefix.c_str());
		}

		if(header.isCompound())
		{
		    ICompoundProperty child(folder.getPtr()->
			getCompoundProperty(header.getName()),
			gabcWrapExisting);

		    importCompoundProps(obj, child, name.buffer(), time);
		    continue;
		}

		myProps[name.buffer()].reset(new Item());
		myProps[name.buffer()]->setDataType(header);
		myProps[name.buffer()]->setDataValue(obj.convertIProperty(
		    folder, header, time, GEO_PackedNameMapPtr()));
	    }
	}

	class Item
	{
	public:
	     Item() : myIsArray(false) {}
	    ~Item() {}

	    bool isArray() const { return myIsArray; }
	    bool isMetaValid() const { return myTraitPtr ? true : false; }
	    bool isDataValid() const { return myData ? true : false; }

	    void
	    getDataType(UT_String &typeName) const
	    {
		UT_ASSERT(isMetaValid());
		typeName.harden(myTraitPtr->myName);
		if(myIsArray)
		    typeName.append("[]");
	    }

	    PlainOldDataType
	    getPodEnum()
	    {
		UT_ASSERT(isMetaValid());
		return myTraitPtr->myPodEnum;
	    }

	    const GT_DataArrayHandle &
	    getDataValue() const
	    {
		UT_ASSERT(isDataValid());
		return myData;
	    }

	    void
	    setDataType(const PropertyHeader &header)
	    {
		if(header.isCompound())
		{
		    myTraitPtr = nullptr;
		    myIsArray = false;
		}
		else
		{
		    myTraitPtr = getTraitPtr(header);
		    myIsArray = header.isArray();
		}
	    }

	    void
	    setDataType(const UT_String &typeName)
	    {
		if(typeName.endsWith("[]"))
		{
		    UT_String scalarName(typeName);
		    scalarName.eraseTail(2);
		    myTraitPtr = getTraitPtr(scalarName);
		    myIsArray = true;
		}
		else
		{
		    myTraitPtr = getTraitPtr(typeName);
		    myIsArray = false;
		}
	    }

	    void
	    setDataType(const UT_String &pod,
		const UT_String &interpretation,
		int extent, bool isArray)
	    {
		// Corresponds to PlainOldDataType in PlainOldDataType.h
		//
		// Contains an entry for wide strings (kWstringPOD), though
		// we never use it.
		static UT_FSATable  theAlembicPODTable(
		    Alembic::Util::kBooleanPOD,     "bool",
		    Alembic::Util::kInt8POD,	    "int8",
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

		    -1,				    NULL
		);

		auto abcPod = (PlainOldDataType)
		    theAlembicPODTable.findSymbol(pod.c_str());
		myTraitPtr = getTraitPtr(abcPod, interpretation, extent);
		myIsArray = isArray;
	    }

	    void
	    setDataValue(const GT_DataArrayHandle &data)
	    {
		myData = data;
	    }

	    void
	    setDataValue(UT_AutoJSONParser &parser, GABC_OError &err)
	    {
		switch(myTraitPtr->myPodEnum)
		{
		    case Alembic::Util::kBooleanPOD:
		    case Alembic::Util::kInt8POD:
			myData = parseJSONNumericArray<GT_Int8Array>(
			    parser, myTraitPtr->myExtent, myTraitPtr->myGtType);
			break;

		    case Alembic::Util::kUint8POD:
			myData = parseJSONNumericArray<GT_UInt8Array>(
			    parser, myTraitPtr->myExtent, myTraitPtr->myGtType);
			break;

		    case Alembic::Util::kInt16POD:
			myData = parseJSONNumericArray<GT_Int16Array>(
			    parser, myTraitPtr->myExtent, myTraitPtr->myGtType);
			break;

		    case Alembic::Util::kUint16POD:
		    case Alembic::Util::kInt32POD:
			myData = parseJSONNumericArray<GT_Int32Array>(
			    parser, myTraitPtr->myExtent, myTraitPtr->myGtType);
			break;

		    case Alembic::Util::kUint32POD:
		    case Alembic::Util::kInt64POD:
		    case Alembic::Util::kUint64POD:
			myData = parseJSONNumericArray<GT_Int64Array>(
			    parser, myTraitPtr->myExtent, myTraitPtr->myGtType);
			break;

		    case Alembic::Util::kFloat16POD:
			myData = parseJSONNumericArray<GT_Real16Array>(
			    parser, myTraitPtr->myExtent, myTraitPtr->myGtType);
			break;

		    case Alembic::Util::kFloat32POD:
			myData = parseJSONNumericArray<GT_Real32Array>(
			    parser, myTraitPtr->myExtent, myTraitPtr->myGtType);
			break;

		    case Alembic::Util::kFloat64POD:
			myData = parseJSONNumericArray<GT_Real64Array>(
			    parser, myTraitPtr->myExtent, myTraitPtr->myGtType);
			break;

		    case Alembic::Util::kStringPOD:
		    case Alembic::Util::kWstringPOD:
			// There is no special Alembic data type that is a
			// string tuple. The equivalent would be an array of
			// strings.
			UT_ASSERT(myTraitPtr->myExtent == 1);
			myData = parseJSONStringArray(parser);
			break;

		    default:
			// Since this is a type error, it has to do with
			// reading metadata.
			err.warning("Error reading user property metadata: "
			    "unrecognized Houdini storage type.");
		}

		if (!myData)
		{
		    err.warning("Error parsing user property values: "
			"no valid JSON map in attribute(s).");
		}
	    }

	private:
	    static AlembicTraitEquivalence *
	    getTraitPtr(const UT_StringRef &typeName)
	    {
		for(auto &trait : theEquivalanceArray)
		{
		    if(::strcmp(trait.myName, typeName.c_str()) == 0)
			return &trait;
		}
		return nullptr;
	    }

	    static AlembicTraitEquivalence *
	    getTraitPtr(PlainOldDataType pod,
		const UT_StringRef &interpretation, int extent)
	    {
		for(auto &trait : theEquivalanceArray)
		{
		    if(trait.myPodEnum == pod
		    && ::strcmp(trait.myInterpretation,
			interpretation.c_str()) == 0
		    && trait.myExtent == extent)
		    {
			return &trait;
		    }
		}
		return nullptr;
	    }

	    static AlembicTraitEquivalence *
	    getTraitPtr(const PropertyHeader &header)
	    {
		return getTraitPtr(header.getDataType().getPod(),
		    header.getMetaData().get("interpretation"),
		    header.getDataType().getExtent());
	    }

	    static GT_DataArrayHandle
	    parseJSONStringArray(UT_AutoJSONParser &parser)
	    {
		UT_WorkBuffer buffer;
		GT_DAIndexedString *data = new GT_DAIndexedString(0);
		bool succ_parsing = true;
		bool err_parsing = false;

		int index = 0;
		for(auto array_it = parser->beginArray();
		    !array_it.atEnd(); array_it.advance(), ++index)
		{
		    succ_parsing &= parser->parseString(buffer);

		    // Efficiently bumps the array size.
		    if(data->entries() <= index)
			data->resize(UTbumpAlloc(index + 1));

		    data->setString(index, 0, buffer.buffer());
		}

		// Since the IndexedString doesn't holds the capacity,
		// we set the precise size for the correct displaying.
		if(data->entries() > index)
		    data->resize(index);

		if (!succ_parsing || err_parsing)
		    return GT_DataArrayHandle();

		return GT_DataArrayHandle(data);
	    }

	    template <typename T>
	    static GT_DataArrayHandle
	    parseJSONNumericArray(UT_AutoJSONParser &parser,
				  int tuple_size,
				  GT_Type gt_type)
	    {
		T			*data = new T(0, tuple_size, gt_type);
		typename T::data_type	 value;
		bool			 succ_parsing = true;
		bool			 err_parsing = false;

		int index = 0;
		for(auto array_it = parser->beginArray();
		    !array_it.atEnd(); array_it.advance(), ++index)
		{
		    if(tuple_size > 1)
		    {
			succ_parsing &= parser->parseBeginArray(err_parsing);

			// Expands the array.
			succ_parsing &= parser->parseValue(value);
			data->append(value);

			// Fills out the rest of the elements.
			for(int j = 1; j < tuple_size; ++j)
			{
			    succ_parsing &= parser->parseValue(value);
			    data->set(value, index, j);
			}

			succ_parsing &= parser->parseEndArray(err_parsing);
		    }
		    else
		    {
			succ_parsing &= parser->parseValue(value);
			data->append(value);
		    }
		}

		if (!succ_parsing || err_parsing)
		    return GT_DataArrayHandle();

		return GT_DataArrayHandle(data);
	    }

	    bool				 myIsArray;
	    AlembicTraitEquivalence		*myTraitPtr;
	    GT_DataArrayHandle			 myData;
	};

	typedef UT_UniquePtr<Item>		    ItemHandle;
	UT_SortedMap<UT_StringHolder, ItemHandle>   myProps;
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
	    if (I != myItems.end() && !I->second->clearIfModified())
		return I->second;

	    return ArchiveCacheEntryPtr();
	}

	ArchiveCacheEntryPtr
	loadArchive(const std::vector<std::string> &paths)
	{
	    UT_AutoLock lock(theFileLock);

	    auto I = myItems.find(paths);
	    if (I != myItems.end() && !I->second->clearIfModified())
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
	    entry->setArchive(paths);
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
		removeCacheEntry(it->first, false);
	    }

	    addCacheEntry(paths, entry);
	    return entry;
	}

	void
	clearArchive(const char *path=nullptr, bool purge=false)
	{
	    if(path)
	    {
		std::string tmp = path;
		auto it = myKeysWithPath.find(tmp);
		if(it != myKeysWithPath.end())
		{
		    auto keys = it->second;
		    for(auto &paths : keys)
			removeCacheEntry(paths, purge);
		}
	    }
	    else
	    {
		if(purge)
		{
		    for(auto &it : myItems)
			it.second->purgeObjects();
		}
		myItems.clear();
		myKeysWithPath.clear();
	    }
	}

    private:
	void
	addCacheEntry(const std::vector<std::string> &paths,
		      const ArchiveCacheEntryPtr &entry)
	{
	    for(auto &p : paths)
		myKeysWithPath[p].insert(paths);

	    myItems[paths] = entry;
	}

	void
	removeCacheEntry(const std::vector<std::string> &paths, bool purge)
	{
	    for(auto &p : paths)
	    {
		auto it = myKeysWithPath.find(p);
		it->second.erase(paths);
		if(it->second.empty())
		    myKeysWithPath.erase(p);
	    }

	    if(purge)
	    {
		auto it = myItems.find(paths);
		if(it != myItems.end())
		    it->second->purgeObjects();
	    }
	    myItems.erase(paths);
	}

	UT_Map<std::vector<std::string>, ArchiveCacheEntryPtr> myItems;
	UT_Map<std::string, UT_Set<std::vector<std::string> > > myKeysWithPath;
    };

    // for now, leak the pointer to the archive cache so we don't
    // crash at shutdown
    ArchiveCache *g_archiveCache(new ArchiveCache);
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
    //try
    {
	auto cacheEntry = g_archiveCache->loadArchive(filenames);
	walker.myBadArchive = !cacheEntry->isValid();
	if (!walker.myBadArchive)
	{
	    WalkPushFile walkfile(walker, filenames);

	    for (exint i = 0; i < objects.entries(); ++i)
	    {
		std::string     path(objects(i));
		GABC_IObject obj = findObject(filenames, path);

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
    }
    //catch (const std::exception &)
    {
	walker.myBadArchive = true;
    }
    return false;
}

bool
GABC_Util::walk(const std::vector<std::string> &filenames,
        Walker &walker,
        const UT_Set<std::string> &objects)
{
    //try
    {
	auto cacheEntry = g_archiveCache->loadArchive(filenames);
	walker.myBadArchive = !cacheEntry->isValid();
	if (!walker.myBadArchive)
	{
	    WalkPushFile walkfile(walker, filenames);
	    for (auto it = objects.begin(); it != objects.end(); ++it)
	    {
		GABC_IObject obj = findObject(filenames, *it);

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
    }
    //catch (const std::exception &)
    {
	walker.myBadArchive = true;
    }

    return false;
}

bool
GABC_Util::walk(const std::vector<std::string> &filenames, GABC_Util::Walker &walker)
{
    //try
    {
	auto cacheEntry = g_archiveCache->loadArchive(filenames);

	walker.myBadArchive = !cacheEntry->isValid();
	if (!walker.myBadArchive)
	{
	    WalkPushFile walkfile(walker, filenames);
	    return cacheEntry->walk(walker);
	}
    }
    //catch (const std::exception &)
    {
	walker.myBadArchive = true;
    }

    return false;
}

void
GABC_Util::clearCache(const char *filename)
{
    g_archiveCache->clearArchive(filename, true);
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
	xform = UT_Matrix4D(lxform.x);
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
	xform = UT_Matrix4D(wxform.x);
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
	xform = UT_Matrix4D(wxform.x);
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
	}
    }

    return vis;
}

bool
GABC_Util::getBoundingBox(
    const GABC_IObject &obj,
    fpreal sample_time,
    UT_BoundingBox &box,
    bool &isconst)
{
    isconst = true;
    if (obj.valid())
    {
	try
	{
	    auto &filenames = obj.archive()->filenames();
	    auto cacheEntry = g_archiveCache->loadArchive(filenames);
	    UT_ASSERT_P(cacheEntry->getObject(obj.getFullName()).valid());
	    return cacheEntry->getBoundingBox(obj, sample_time, box, isconst);
	}
	catch (const std::exception &)
	{
	}
    }

    return false;
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
            return false;
    }

    return true;
}

bool
GABC_Util::importUserPropertyDictionary(UT_JSONWriter *vals_writer,
	UT_JSONWriter *meta_writer,
        const GABC_IObject &obj,
        fpreal time)
{
    UserPropStorage storage(obj, time);

    if(meta_writer)
	storage.writeJSONMetas(*meta_writer);

    if(vals_writer)
	storage.writeJSONValues(*vals_writer);

    return true;
}

void
GABC_Util::exportUserPropertyDictionary(UT_AutoJSONParser &meta_data,
        UT_AutoJSONParser &vals_data,
        PropertyMap &up_map,
        OCompoundProperty *ancestor,
        GABC_OError &err,
        const GABC_OOptions &ctx,
	const GABC_LayerOptions &lopt,
	GABC_LayerOptions::LayerType ltype)
{
    UserPropStorage storage(meta_data, vals_data, err);
    storage.updateOProperty(up_map, ancestor, ctx, lopt, ltype, err);
}

void
GABC_Util::getUserPropertyTokens(UT_SortedStringSet &tokens,
	UT_AutoJSONParser &meta_data,
	UT_AutoJSONParser &vals_data,
	GABC_OError &err)
{
    UserPropStorage storage(meta_data, vals_data, err);
    storage.getKeys(tokens);
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
		if(it == myMaxId.end())
		    myMaxId.emplace(base_name, val);
		else if(val > it->second)
		    it->second = val;
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
