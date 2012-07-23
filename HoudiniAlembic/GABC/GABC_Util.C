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
#include <boost/tokenizer.hpp>

namespace
{
    typedef Alembic::Abc::M44d			M44d;
    typedef Alembic::Abc::index_t		index_t;
    typedef Alembic::Abc::chrono_t		chrono_t;
    typedef Alembic::Abc::IArchive		IArchive;
    typedef Alembic::Abc::IObject		IObject;
    typedef Alembic::Abc::ICompoundProperty	ICompoundProperty;
    typedef Alembic::Abc::ISampleSelector	ISampleSelector;
    typedef Alembic::Abc::ObjectHeader		ObjectHeader;
    typedef Alembic::Abc::TimeSamplingPtr	TimeSamplingPtr;
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

    static UT_FSATable	theNodeTypeTable(
	GABC_Util::GABC_XFORM,		"Xform",
	GABC_Util::GABC_POLYMESH,	"PolyMesh",
	GABC_Util::GABC_SUBD,		"SubD",
	GABC_Util::GABC_CAMERA,		"Camera",
	GABC_Util::GABC_FACESET,	"FaceSet",
	GABC_Util::GABC_CURVES,		"Curves",
	GABC_Util::GABC_POINTS,		"Points",
	GABC_Util::GABC_NUPATCH,	"NuPatch",
	-1, NULL
    );

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

	const M44d	&getLocal() const	{ return myLocal; }
	const M44d	&getWorld() const	{ return myWorld; }
	bool		 isConstant() const	{ return myConstant; }
	bool		 inheritsXform() const	{ return myInheritsXform; }
    private:
	M44d	myLocal;
	M44d	myWorld;
	bool	myConstant;
	bool	myInheritsXform;
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

    #include <UT/UT_SymbolTable.h>
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

    // Returns whether the transform is constant
    void
    abcLocalTransform(M44d &localXform, IObject obj, fpreal sampleTime,
	    bool &isConstant, bool &inheritsXform)
    {
	IXform		xformObject(obj, Alembic::Abc::kWrapExisting);
	TimeSamplingPtr timeSampling =xformObject.getSchema().getTimeSampling();
	size_t		numSamples = xformObject.getSchema().getNumSamples();

	isConstant = xformObject.getSchema().isConstant();
	inheritsXform = xformObject.getSchema().getInheritsXforms();

	chrono_t inTime = sampleTime;
	chrono_t outTime = sampleTime;

	if (numSamples > 1)
	{
	    const chrono_t epsilon = 1.0 / 10000.0;

	    std::pair<index_t, chrono_t> floorIndex =
		    timeSampling->getFloorIndex(sampleTime, numSamples);

	    //make sure we're not equal enough
	    if (fabs(floorIndex.second - sampleTime) > epsilon)
	    {
		//make sure we're not before the first sample
		if (floorIndex.second < sampleTime)
		{
		    //make sure there's another sample available afterwards
		    if (floorIndex.first+1 < numSamples)
		    {
			inTime = floorIndex.second;
			outTime = timeSampling->getSampleTime(
				floorIndex.first+1);
		    }
		}
	    }
	}

	//interpolate if necessary
	if (inTime != outTime )
	{
	    XformSample inSample = xformObject.getSchema().getValue(
		    ISampleSelector(inTime));

	    XformSample outSample = xformObject.getSchema().getValue(
		    ISampleSelector(outTime));

	    double t = (sampleTime - inTime) / (outTime - inTime);

	    Imath::V3d s_l,s_r,h_l,h_r,t_l,t_r;
	    Imath::Quatd quat_l,quat_r;

	    DecomposeXForm(inSample.getMatrix(), s_l, h_l, quat_l, t_l);
	    DecomposeXForm(outSample.getMatrix(), s_r, h_r, quat_r, t_r);

	    if ((quat_l ^ quat_r) < 0)
	    {
		quat_r = -quat_r;
	    }

	    localXform = RecomposeXForm(lerp(s_l, s_r, t),
		     lerp(h_l, h_r, t),
		     Imath::slerp(quat_l, quat_r, t),
		     lerp(t_l, t_r, t));
	}
	else
	{
	    XformSample xformSample = xformObject.getSchema().getValue(
		    ISampleSelector(sampleTime));
	    localXform = xformSample.getMatrix();
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
	ArchiveObjectItem(const IObject &obj)
	    : UT_CappedItem()
	    , myIObject(obj)
	{
	}
	// Approximate usage
	virtual int64	getMemoryUsage() const	{ return 1024; }
	IObject		getObject() const	{ return myIObject; }
    private:
	IObject		 myIObject;
    };

    struct ArchiveCacheEntry
    {
        ArchiveCacheEntry()
	    : myCache("abcObjects", 2)
	    , myDynamicXforms("abxTransforms", 4)
        {
        }

        ~ArchiveCacheEntry()
        {
        }

	bool	walk(GABC_Util::Walker &walker)
		{
		    if (!walker.preProcess(root()))
			return false;
		    return walkTree(root(), walker);
		}

	// Build a cache of constant (non-changing) transforms
	void	buildTransformCache(IObject root, const char *path,
				    const M44d &parent)
	{
	    UT_WorkBuffer	fullpath;
	    for (size_t i = 0; i < root.getNumChildren(); ++i)
	    {
		const ObjectHeader	&ohead = root.getChildHeader(i);
		if (IXform::matches(ohead))
		{
		    IXform		xform(root, ohead.getName());
		    IXformSchema	&xs = xform.getSchema();
		    M44d		world;
		    world = parent;
		    if (xs.isConstant())
		    {
			fullpath.sprintf("%s/%s", path, ohead.getName().c_str());
			XformSample	xsample = xs.getValue(ISampleSelector(0.0));
			bool	inherits = xs.getInheritsXforms();
			M44d	localXform = xsample.getMatrix();
			if (inherits)
			    world = localXform * parent;
			else
			    world = localXform;
			myStaticXforms[fullpath.buffer()] =
			    LocalWorldXform(localXform, world, true, inherits);
			buildTransformCache(xform, fullpath.buffer(), world);
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
	void	getLocalTransform(M44d &x, const IObject &obj, fpreal now,
			bool &isConstant, bool &inheritsXform)
		{
		    if (IXform::matches(obj.getHeader()))
		    {
			abcLocalTransform(x, obj, now, isConstant, inheritsXform);
		    }
		    else
		    {
			x.makeIdentity();
		    }
		}

	/// Find the full world transform for an object
	bool	getWorldTransform(M44d &x, const IObject &obj, fpreal now,
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
			M44d	localXform;
			IObject	dad = const_cast<IObject &>(obj).getParent();

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
	IObject	findObject(IObject parent,
		    UT_WorkBuffer &fullpath, const char *component)
		{
		    fullpath.append("/");
		    fullpath.append(component);
		    ArchiveObjectKey	key(fullpath.buffer());
		    IObject		kid;
		    UT_CappedItemHandle	item = myCache.findItem(key);
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
	IObject getObject(const std::string &objectPath)
	{
	    PathList		pathList;
	    IObject		curr = root();
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

	    virtual bool	process(const IObject &obj)
				{
				    myObjects.push_back(obj.getFullName());
				    return true;
				}
	private:
	    PathList	&myObjects;
	};

	const PathList	&getObjectList()
			{
			    if (myArchive.valid() && !myObjectList.size())
			    {
				PathListWalker	func(myObjectList);
				walk(func);
			    }
			    return myObjectList;
			}

	static bool	walkTree(IObject node, GABC_Util::Walker &walker)
			{
			    if (walker.interrupted())
				return false;
			    if (walker.process(node))
				walker.walkChildren(node);
			    return true;
			}

	bool	isValid() const		{ return myArchive.valid(); }
	void	setArchive(const IArchive &a)	{ myArchive = a; }
	void	setError(const std::string &e)	{ error = e; }

    private:
	IObject		root()	{ return myArchive.getTop(); }
	IArchive	myArchive;
	std::string	error;
	PathList	myObjectList;
	AbcTransformMap myStaticXforms;
	UT_CappedCache	myCache;
	UT_CappedCache	myDynamicXforms;
    };

    typedef boost::shared_ptr<ArchiveCacheEntry> ArchiveCacheEntryRcPtr;
    typedef std::map<std::string, ArchiveCacheEntryRcPtr> ArchiveCache;

    //-*************************************************************************

    size_t g_maxCache = 50;
    //for now, leak the pointer to the archive cache so we don't
    //crash at shutdown
    ArchiveCache *g_archiveCache(new ArchiveCache);

    //-*************************************************************************

    ArchiveCacheEntryRcPtr
    LoadArchive(const std::string & path)
    {
        ArchiveCache::iterator I = g_archiveCache->find(path);
        if (I != g_archiveCache->end())
        {
            return (*I).second;
        }
        ArchiveCacheEntryRcPtr entry = ArchiveCacheEntryRcPtr(
                new ArchiveCacheEntry);
        try
        {
            entry->setArchive(
		    IArchive(::Alembic::AbcCoreHDF5::ReadArchive(), path)
		);
        }
        catch (const std::exception & e)
        {
            entry->setError(e.what());
        }
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
	    g_archiveCache->erase(it);
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
GABC_Util::Walker::preProcess(const IObject &)
{
    return true;
}

bool
GABC_Util::Walker::walkChildren(const IObject &node)
{
    IObject	&obj = const_cast<IObject &>(node);
    exint	nkids = obj.getNumChildren();
    for (exint i = 0; i < nkids; ++i)
    {
	// Returns false on interrupt
	if (!ArchiveCacheEntry::walkTree(obj.getChild(i), *this))
	    return false;
    }
    return true;
}

const char *
GABC_Util::nodeType(NodeType token)
{
    const char	*name = theNodeTypeTable.getToken(token);
    return name ? name : "<unknown>";
}

GABC_Util::NodeType
GABC_Util::nodeType(const char *name)
{
    return static_cast<GABC_Util::NodeType>(theNodeTypeTable.findSymbol(name));
}

GABC_Util::NodeType
GABC_Util::getNodeType(const IObject &obj)
{
    const ObjectHeader	&ohead = obj.getHeader();
    if (IXform::matches(ohead))
	return GABC_XFORM;
    if (IPolyMesh::matches(ohead))
	return GABC_POLYMESH;
    if (ISubD::matches(ohead))
	return GABC_SUBD;
    if (ICamera::matches(ohead))
	return GABC_CAMERA;
    if (IFaceSet::matches(ohead))
	return GABC_FACESET;
    if (ICurves::matches(ohead))
	return GABC_CURVES;
    if (IPoints::matches(ohead))
	return GABC_POINTS;
    if (INuPatch::matches(ohead))
	return GABC_NUPATCH;
    return GABC_UNKNOWN;
}

bool
GABC_Util::isMayaLocator(const IObject &obj)
{
    UT_ASSERT(IXform::matches(obj.getHeader()));
    IObject	*xform = const_cast<IObject *>(&obj);
    return xform->getProperties().getPropertyHeader("locator") != NULL;
}

template <typename T>
static bool
arrayIsConstant(Alembic::AbcGeom::ICompoundProperty arb,
	const Alembic::Abc::PropertyHeader &head)
{
    T	param(arb, head.getName());
    return param.isConstant();
}

static bool
abcArbsAreAnimated(ICompoundProperty arb)
{
    exint	narb = arb ? arb.getNumProperties() : 0;
    for (exint i = 0; i < narb; ++i)
    {
	if (!GABC_Util::isConstant(arb, i))
	    return true;
    }
    return false;
}

template <typename ABC_T, typename SCHEMA_T>
static GABC_Util::AnimationType
getAnimation(const IObject &obj)
{
    ABC_T				 prim(obj, Alembic::Abc::kWrapExisting);
    SCHEMA_T			&schema = prim.getSchema();
    GABC_Util::AnimationType	 atype;
    
    switch (schema.getTopologyVariance())
    {
	case Alembic::AbcGeom::kConstantTopology:
	    atype = GABC_Util::ANIMATION_CONSTANT;
	    if (abcArbsAreAnimated(schema.getArbGeomParams()))
		atype = GABC_Util::ANIMATION_ATTRIBUTE;
	    break;
	case Alembic::AbcGeom::kHomogenousTopology:
	    atype = GABC_Util::ANIMATION_ATTRIBUTE;
	    break;
	case Alembic::AbcGeom::kHeterogenousTopology:
	    atype = GABC_Util::ANIMATION_TOPOLOGY;
	    break;
    }
    return atype;
}

template <>
GABC_Util::AnimationType
getAnimation<IPoints, IPointsSchema>(const IObject &obj)
{
    IPoints			 prim(obj, Alembic::Abc::kWrapExisting);
    IPointsSchema		&schema = prim.getSchema();
    if (!schema.isConstant())
	return GABC_Util::ANIMATION_TOPOLOGY;
    if (abcArbsAreAnimated(schema.getArbGeomParams()))
	return GABC_Util::ANIMATION_ATTRIBUTE;
    return GABC_Util::ANIMATION_CONSTANT;
}

template <>
GABC_Util::AnimationType
getAnimation<IXform, IXformSchema>(const IObject &obj)
{
    IXform			 prim(obj, Alembic::Abc::kWrapExisting);
    IXformSchema		&schema = prim.getSchema();
    if (!schema.isConstant())
	return GABC_Util::ANIMATION_TOPOLOGY;
    if (abcArbsAreAnimated(schema.getArbGeomParams()))
	return GABC_Util::ANIMATION_ATTRIBUTE;
    return GABC_Util::ANIMATION_CONSTANT;
}


static GABC_Util::AnimationType
getLocatorAnimation(const IObject &obj)
{
    IXform				prim(obj, Alembic::Abc::kWrapExisting);
    Alembic::Abc::IScalarProperty	loc(prim.getProperties(), "locator");
    return loc.isConstant() ? GABC_Util::ANIMATION_CONSTANT
			    : GABC_Util::ANIMATION_ATTRIBUTE;
}

static bool
abcCurvesChangingTopology(const IObject &obj)
{
    // There's a bug in Alembic 1.0.5 which doesn't properly detect
    // heterogenous topology.
    ICurves		 curves(obj, Alembic::Abc::kWrapExisting);
    ICurvesSchema	&schema = curves.getSchema();
    if (!schema.getNumVerticesProperty().isConstant())
	return true;
    return false;
}

#define MATCH_ARB(TYPENAME) \
    if (Alembic::AbcGeom::TYPENAME::matches(head)) { \
	return arrayIsConstant<Alembic::AbcGeom::TYPENAME>(arb, head); \
    }

bool
GABC_Util::isConstant(Alembic::AbcGeom::ICompoundProperty arb, int idx)
{
    const Alembic::Abc::PropertyHeader	&head = arb.getPropertyHeader(idx);

    MATCH_ARB(IBoolGeomParam);
    MATCH_ARB(IUcharGeomParam);
    MATCH_ARB(ICharGeomParam);
    MATCH_ARB(IUInt16GeomParam);
    MATCH_ARB(IInt16GeomParam);
    MATCH_ARB(IUInt32GeomParam);
    MATCH_ARB(IInt32GeomParam);
    MATCH_ARB(IUInt64GeomParam);
    MATCH_ARB(IInt64GeomParam);
    MATCH_ARB(IHalfGeomParam);
    MATCH_ARB(IFloatGeomParam);
    MATCH_ARB(IDoubleGeomParam);
    MATCH_ARB(IStringGeomParam);
    MATCH_ARB(IWstringGeomParam);
    MATCH_ARB(IV2sGeomParam);
    MATCH_ARB(IV2iGeomParam);
    MATCH_ARB(IV2fGeomParam);
    MATCH_ARB(IV2dGeomParam);
    MATCH_ARB(IV3sGeomParam);
    MATCH_ARB(IV3iGeomParam);
    MATCH_ARB(IV3fGeomParam);
    MATCH_ARB(IV3dGeomParam);
    MATCH_ARB(IP2sGeomParam);
    MATCH_ARB(IP2iGeomParam);
    MATCH_ARB(IP2fGeomParam);
    MATCH_ARB(IP2dGeomParam);
    MATCH_ARB(IP3sGeomParam);
    MATCH_ARB(IP3iGeomParam);
    MATCH_ARB(IP3fGeomParam);
    MATCH_ARB(IP3dGeomParam);
    MATCH_ARB(IBox2sGeomParam);
    MATCH_ARB(IBox2iGeomParam);
    MATCH_ARB(IBox2fGeomParam);
    MATCH_ARB(IBox2dGeomParam);
    MATCH_ARB(IBox3sGeomParam);
    MATCH_ARB(IBox3iGeomParam);
    MATCH_ARB(IBox3fGeomParam);
    MATCH_ARB(IBox3dGeomParam);
    MATCH_ARB(IM33fGeomParam);
    MATCH_ARB(IM33dGeomParam);
    MATCH_ARB(IM44fGeomParam);
    MATCH_ARB(IM44dGeomParam);
    MATCH_ARB(IQuatfGeomParam);
    MATCH_ARB(IQuatdGeomParam);
    MATCH_ARB(IC3hGeomParam);
    MATCH_ARB(IC3fGeomParam);
    MATCH_ARB(IC3cGeomParam);
    MATCH_ARB(IC4hGeomParam);
    MATCH_ARB(IC4fGeomParam);
    MATCH_ARB(IC4cGeomParam);
    MATCH_ARB(IN2fGeomParam);
    MATCH_ARB(IN2dGeomParam);
    MATCH_ARB(IN3fGeomParam);
    MATCH_ARB(IN3dGeomParam);
    return false;
}

GABC_Util::AnimationType
GABC_Util::getAnimationType(const std::string &filename, const IObject &node,
	bool include_transform)
{
    AnimationType	atype = ANIMATION_CONSTANT;
    UT_ASSERT_P(node.valid());
    // Set the topology based on a combination of conditions.
    // We initialize based on the shape topology, but if the shape topology is
    // constant, there are still various factors which can make the primitive
    // non-constant (i.e. Homogeneous).
    switch (GABC_Util::getNodeType(node))
    {
	case GABC_Util::GABC_POLYMESH:
	    atype = getAnimation<IPolyMesh, IPolyMeshSchema>(node);
	    break;
	case GABC_Util::GABC_SUBD:
	    atype = getAnimation<ISubD, ISubDSchema>(node);
	    break;
	case GABC_Util::GABC_CURVES:
	    atype = getAnimation<ICurves, ICurvesSchema>(node);
	    // There's a bug in Alembic 1.0.5 detecting changing topology on
	    // curves.
	    if (atype != ANIMATION_TOPOLOGY
		    && abcCurvesChangingTopology(node))
	    {
		atype = ANIMATION_TOPOLOGY;
	    }
	    break;
	case GABC_Util::GABC_POINTS:
	    atype = getAnimation<IPoints, IPointsSchema>(node);
	    break;
	case GABC_Util::GABC_NUPATCH:
	    atype = getAnimation<INuPatch, INuPatchSchema>(node);
	    break;
	case GABC_Util::GABC_XFORM:
	    if (GABC_Util::isMayaLocator(node))
		atype = getLocatorAnimation(node);
	    else
		atype = getAnimation<IXform, IXformSchema>(node);
	    break;
	default:
	    atype = ANIMATION_TOPOLOGY;
	    break;
    }
    if (atype == ANIMATION_CONSTANT && include_transform)
    {
	UT_Matrix4D	xform;
	bool		is_const;
	IObject		parent = const_cast<IObject &>(node).getParent();
	if (parent.valid())
	{
	    bool	inheritsXform;
	    // Check to see if transform is non-constant
	    if (GABC_Util::getWorldTransform(filename, parent.getFullName(),
			    0, xform, is_const, inheritsXform))
	    {
		if (!is_const)
		    atype = ANIMATION_TRANSFORM;
	    }
	}
    }
    return atype;
}

bool
GABC_Util::walk(const std::string &filename, Walker &walker,
			    const UT_StringArray &objects)
{
    ArchiveCacheEntryRcPtr	cacheEntry = LoadArchive(filename);
    WalkPushFile		walkfile(walker, filename);
    for (exint i = 0; i < objects.entries(); ++i)
    {
	std::string	path(objects(i));
	IObject	obj = findObject(filename, path);
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
    ArchiveCacheEntryRcPtr	cacheEntry = LoadArchive(filename);
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

IObject
GABC_Util::findObject(const std::string &filename,
	const std::string &objectpath)
{
    ArchiveCacheEntryRcPtr	cacheEntry = LoadArchive(filename);
    return cacheEntry->isValid() ? cacheEntry->getObject(objectpath)
		: IObject();
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
	ArchiveCacheEntryRcPtr	cacheEntry = LoadArchive(filename);
	if (cacheEntry->isValid())
	{
	    IObject	obj = cacheEntry->getObject(objectpath);
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
	ArchiveCacheEntryRcPtr	cacheEntry = LoadArchive(filename);
	if (cacheEntry->isValid())
	{
	    IObject	obj = cacheEntry->getObject(objectpath);
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
	const IObject &obj,
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
	    ArchiveCacheEntryRcPtr	cacheEntry = LoadArchive(filename);
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

const PathList &
GABC_Util::getObjectList(const std::string &filename)
{
    try
    {
	ArchiveCacheEntryRcPtr	cacheEntry = LoadArchive(filename);
	return cacheEntry->getObjectList();
    }
    catch (const std::exception &)
    {
    }
    static PathList	theEmptyList;
    return theEmptyList;
}
