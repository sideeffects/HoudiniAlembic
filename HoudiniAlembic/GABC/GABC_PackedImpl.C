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
 * NAME:	GABC_PackedImpl.h (GABC Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_PackedImpl.h"
#include "GABC_PackedGT.h"
#include "GABC_Visibility.h"
#include <UT/UT_JSONParser.h>
#include <UT/UT_Debug.h>
#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GT/GT_Primitive.h>
#include <GT/GT_Util.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_DAConstantValue.h>
#include <GT/GT_PrimPointMesh.h>
#include <GT/GT_PrimitiveBuilder.h>

#if !defined(GABC_PRIMITIVE_TOKEN)
    #define GABC_PRIMITIVE_TOKEN	"AlembicRef"
    #define GABC_PRIMITIVE_LABEL	"Packed Alembic"
#endif

using namespace GABC_NAMESPACE;

namespace
{

static GT_PrimitiveHandle	theNullPrimitive;

class AlembicFactory : public GU_PackedFactory
{
public:
    AlembicFactory()
	: GU_PackedFactory(GABC_PRIMITIVE_TOKEN, GABC_PRIMITIVE_LABEL,
	                   "SOP_alembic")
    {
	registerIntrinsic("abctypename",
	    StringGetterCast(&GABC_PackedImpl::intrinsicNodeType));
	registerIntrinsic("abcfilename",
	    StdStringGetterCast(&GABC_PackedImpl::intrinsicFilename),
	    StdStringSetterCast(&GABC_PackedImpl::setFilename));
	registerIntrinsic("abcobjectpath",
	    StdStringGetterCast(&GABC_PackedImpl::intrinsicObjectPath),
	    StdStringSetterCast(&GABC_PackedImpl::setObjectPath));
	registerIntrinsic("abcframe",
	    FloatGetterCast(&GABC_PackedImpl::frame),
	    FloatSetterCast(&GABC_PackedImpl::setFrame));
	registerIntrinsic("abcanimation",
	    StringGetterCast(&GABC_PackedImpl::intrinsicAnimation));
	registerIntrinsic("abcusevisibility",
	    BoolGetterCast(&GABC_PackedImpl::useVisibility),
	    BoolSetterCast(&GABC_PackedImpl::setUseVisibility));
	registerIntrinsic("abcvisibility",
	    IntGetterCast(&GABC_PackedImpl::intrinsicVisibility));
	registerIntrinsic("abcfullvisibility",
	    IntGetterCast(&GABC_PackedImpl::intrinsicFullVisibility));
	registerIntrinsic("abcusetransform",
	    BoolGetterCast(&GABC_PackedImpl::useTransform),
	    BoolSetterCast(&GABC_PackedImpl::setUseTransform));
    }
    virtual ~AlembicFactory()
    {
    }

    virtual GU_PackedImpl	*create() const
				    { return new GABC_PackedImpl(); }
};

static AlembicFactory	*theFactory = NULL;

}

GU_PrimPacked *
GABC_PackedImpl::build(GU_Detail &gdp,
			const std::string &filename,
			const GABC_IObject &obj,
			fpreal frame,
			bool useTransform,
			bool useVisibility)
{
    UT_ASSERT(theFactory);
    GA_Primitive	*prim;
    GU_PrimPacked	*pack;
    GABC_PackedImpl	*abc;
    
    prim = gdp.getPrimitiveFactory().create(theFactory->typeId(), gdp);
    // Note:  The primitive is invalid until you do something like
    //   prim->setVertexPoint(gdp.appendPointOffset());
    // The Alembic creation code has an option to have a shared point for all
    // Alembic primitives, so this is handled separately.
    pack = UTverify_cast<GU_PrimPacked *>(prim);
    abc = UTverify_cast<GABC_PackedImpl *>(pack->implementation());
    abc->setFilename(filename);
    abc->setObject(obj);
    abc->setFrame(frame);
    abc->setUseTransform(useTransform);
    abc->setUseVisibility(useVisibility);
    return pack;
}

void
GABC_PackedImpl::install(GA_PrimitiveFactory *gafactory)
{
    UT_ASSERT(!theFactory);
    if (theFactory)
	return;
    theFactory = new AlembicFactory();
    GU_PrimPacked::registerPacked(gafactory, theFactory);

    // Now, register the GT primitive
    GABC_CollectPacked	*gt = new GABC_CollectPacked();
    gt->bind(theFactory->typeId());
}

bool
GABC_PackedImpl::isInstalled()
{
    return theFactory != NULL;
}

const GA_PrimitiveTypeId &
GABC_PackedImpl::typeId()
{
    UT_ASSERT(theFactory);
    return theFactory->typeId();
}


#ifdef USE_FAST_CACHE

// NOTE: This is simply a proof of concept used for testing. It should not
//       be enabled in a release build, as it 1) has no cap and 2) doesn't
//	 reload or release the data when the alembic file is reloaded or
//	 no longer referenced.

typedef UT_Map<fpreal, GT_PrimitiveHandle>	gabc_DeformCache;
typedef UT_Map<fpreal, UT_Matrix4D>		gabc_TransformCache;
typedef UT_Map<fpreal, UT_BoundingBox>		gabc_BoundsCache;
typedef UT_Map<fpreal, GABC_VisibilityType>	gabc_VisibilityCache;
    
class gabc_ObjectCacheItem
{
public:
    gabc_ObjectCacheItem()
	: geo_anim_type(GEO_ANIMATION_INVALID),
	  xform_anim_type(GEO_ANIMATION_INVALID),
	  bounds_anim_type(GEO_ANIMATION_INVALID),
	  vis_anim_type(GEO_ANIMATION_INVALID),
	  has_static_xform(false),
	  has_bounds(true),
	  vis_type(GABC_VISIBLE_DEFER),
	  deform_cache(new gabc_DeformCache),
	  transform_cache(new gabc_TransformCache),
	  bounds_cache(new gabc_BoundsCache),
	  vis_cache(new gabc_VisibilityCache),
	  prop_hash(-1) {}

    ~gabc_ObjectCacheItem()
	{
	    delete deform_cache;
	    delete transform_cache;
	    delete bounds_cache;
	    delete vis_cache;
	}

    // Geometry 
    GEO_AnimationType    geo_anim_type;
    GT_PrimitiveHandle   static_geo;
    gabc_DeformCache    *deform_cache;

    // Transform
    GEO_AnimationType    xform_anim_type;
    bool	         has_static_xform;
    UT_Matrix4D	         static_xform;
    gabc_TransformCache *transform_cache;

    // bounds
    GEO_AnimationType	 bounds_anim_type;
    bool		 has_bounds;
    UT_BoundingBox	 static_bounds;
    gabc_BoundsCache	*bounds_cache;

    // prophash
    int64		 prop_hash;

    // visibility
    GEO_AnimationType    vis_anim_type;
    GABC_VisibilityType	 vis_type;
    gabc_VisibilityCache *vis_cache;
};
   
typedef UT_Map<std::string, gabc_ObjectCacheItem *>	gabc_ObjectCache;
typedef UT_Map<std::string, gabc_ObjectCache *>		gabc_FileCache;
    
static gabc_FileCache theFileCache;

gabc_ObjectCacheItem *
getFileObject(const std::string &filename, const std::string &object_path)
{
    gabc_ObjectCacheItem *item = NULL;
    gabc_ObjectCache *cache = NULL;
    gabc_FileCache::iterator file_it = theFileCache.find(filename);
    
    if(file_it == theFileCache.end())
    {
	cache = new gabc_ObjectCache;
	theFileCache[filename] = cache;
    }
    else
	cache = file_it->second;

    gabc_ObjectCache::iterator obj_it = cache->find(object_path);
    if(obj_it == cache->end())
    {
	item = new gabc_ObjectCacheItem;
	(*cache)[ object_path ] = item;
    }
    else
	item = obj_it->second;

    return item;
}
   
gabc_ObjectCacheItem *
GABC_PackedImpl::getObjectCacheItem() const
{
    if(!myObjectCacheItem)
	myObjectCacheItem = getFileObject(filename(), objectPath());
	
    return myObjectCacheItem;
}


#endif


GABC_PackedImpl::GABC_PackedImpl()
    : GU_PackedImpl()
    , myObject()
    , myCache()
    , myFilename()
    , myObjectPath()
    , myFrame(0)
    , myUseTransform(true)
    , myUseVisibility(true)
    , myCachedUniqueID(false)
    , myUniqueID(0)
    , myConstVisibility(GABC_VISIBLE_DEFER)
    , myHasConstBounds(false)
#ifdef USE_FAST_CACHE
    , myObjectCacheItem(NULL)
#endif
{
}

GABC_PackedImpl::GABC_PackedImpl(const GABC_PackedImpl &src)
    : GU_PackedImpl(src)
    , myObject(src.myObject)
    , myCache()
    , myFilename(src.myFilename)
    , myObjectPath(src.myObjectPath)
    , myFrame(src.myFrame)
    , myUseTransform(src.myUseTransform)
    , myUseVisibility(src.myUseVisibility)
    , myCachedUniqueID(src.myCachedUniqueID)
    , myUniqueID(src.myUniqueID)
    , myConstVisibility(src.myConstVisibility)
    , myHasConstBounds(src.myHasConstBounds)
    , myConstBounds(src.myConstBounds)
#ifdef USE_FAST_CACHE
    , myObjectCacheItem(src.myObjectCacheItem)
#endif
{
}

GABC_PackedImpl::~GABC_PackedImpl()
{
}

GU_PackedFactory *
GABC_PackedImpl::getFactory() const
{
    return theFactory;
}

GU_PackedImpl *
GABC_PackedImpl::copy() const
{
    return new GABC_PackedImpl(*this);
}

bool
GABC_PackedImpl::isValid() const
{
    return myObject.valid();
}

void
GABC_PackedImpl::clearData()
{
    myObject = GABC_IObject();
    myFilename.clear();
    myObjectPath.clear();
    myFrame = 0;
    myUseTransform = true;
    myUseVisibility = true;
    myCache.clear();
}

bool
GABC_PackedImpl::load(const UT_Options &options, const GA_LoadMap &map)
{
    clearData();
    bool	bval;
    if (!options.importOption("filename", myFilename))
	myFilename = "";
    if (!options.importOption("object", myObjectPath))
	myObjectPath = "";
    if (!options.importOption("frame", myFrame))
	myFrame = 0;
    if (!options.importOption("usetransform", bval))
	bval = true;
    setUseTransform(bval);
    if (!options.importOption("usevisibility", bval))
	bval = true;
    setUseVisibility(bval);
    return true;
}

void
GABC_PackedImpl::update(const UT_Options &options)
{
    bool	changed = false;
    bool	bval;
    changed |= options.importOption("filename", myFilename);
    changed |= options.importOption("object", myObjectPath);
    changed |= options.importOption("frame", myFrame);
    changed |= options.importOption("usetransform", bval);
    setUseTransform(bval);
    changed |= options.importOption("usevisibility", bval);
    setUseVisibility(bval);
}

bool
GABC_PackedImpl::save(UT_Options &options, const GA_SaveMap &map) const
{
    options.setOptionS("filename", myFilename);
    options.setOptionS("object", myObjectPath);
    options.setOptionF("frame", myFrame);
    options.setOptionB("usetransform", myUseTransform);
    options.setOptionB("usevisibility", myUseVisibility);
    return true;
}

bool
GABC_PackedImpl::loadUnknownToken(const char *token,
	UT_JSONParser &p, const GA_LoadMap &map)
{
    UT_WorkBuffer	sval;
    fpreal64		fval;
    bool		bval;
    if (!strcmp(token, "filename"))
    {
	if (!p.parseString(sval))
	    return false;
	myFilename = sval.toStdString();
    }
    else if (!strcmp(token, "object"))
    {
	if (!p.parseString(sval))
	    return false;
	myObjectPath = sval.toStdString();
    }
    else if (!strcmp(token, "frame"))
    {
	if (!p.parseNumber(fval))
	    return false;
	myFrame = fval;
    }
    else if (!strcmp(token, "usetransform"))
    {
	if (!p.parseBool(bval))
	    return false;
	myUseTransform = bval;
    }
    else if (!strcmp(token, "usevisibility"))
    {
	if (!p.parseBool(bval))
	    return false;
	myUseVisibility = bval;
    }
    else
    {
	return GU_PackedImpl::loadUnknownToken(token, p, map);
    }
    return true;
}

bool
GABC_PackedImpl::getBounds(UT_BoundingBox &box) const
{
    const GABC_IObject	&iobj = object();
    if (!iobj.valid())
	return 0;

#ifdef USE_FAST_CACHE

    if(myHasConstBounds)
    {
	box = myConstBounds;
	return true;
    }

    gabc_ObjectCacheItem *obj = getObjectCacheItem();
    if(!obj->has_bounds)
	return false;

    bool fetched_bounds = false;
    
    if(obj->bounds_anim_type == GEO_ANIMATION_INVALID)
    {
	bool	isconst;
	if (iobj.getBoundingBox(box, myFrame, isconst))
	{
	    setBoxCache(box);
	    fetched_bounds = true;
	    obj->bounds_anim_type = isconst ? GEO_ANIMATION_CONSTANT
					    : GEO_ANIMATION_TRANSFORM;

	    if(isconst)
	    {
		myHasConstBounds = true;
		myConstBounds = box;
	    }
	    else
		myHasConstBounds = false;
	}
	else
	{
	    obj->has_bounds = false;
	    return false;
	}
    }
    
    if(obj->bounds_anim_type == GEO_ANIMATION_TRANSFORM)
    {
	gabc_BoundsCache::iterator bounds_it =
	    obj->bounds_cache->find(myFrame);

	if(bounds_it != obj->bounds_cache->end())
	{
	    // cached matrix
	    box = bounds_it->second;
	}
	else
	{
	    if(!fetched_bounds)
	    {
		bool isconst;
		if(iobj.getBoundingBox(box, myFrame, isconst))
		{
		    setBoxCache(box);
		}
		else
		    box.makeInvalid();
	    }
	    
	    (*obj->bounds_cache)[myFrame] = box;
	}	
    }
    else if(!fetched_bounds)
    {
	box = obj->static_bounds;
    }

    return true;
    
#else // !USE_FAST_CACHE
    
    bool	isconst;
    if (iobj.getBoundingBox(box, myFrame, isconst))
    {
	setBoxCache(box);
	return true;
    }
    return false;
    
#endif
}

bool
GABC_PackedImpl::getRenderingBounds(UT_BoundingBox &box) const
{
    switch (nodeType())
    {
	case GABC_POINTS:
	case GABC_CURVES:
	    break;
	default:
	    // Calling the primitive to get the "bounds" (not rendering bounds)
	    // will use the box cache if possible.
	    return getPrim()->getUntransformedBounds(box);
	return false;
	    break;
    }
    return object().getRenderingBoundingBox(box, myFrame);
}

void
GABC_PackedImpl::getVelocityRange(UT_Vector3 &vmin, UT_Vector3 &vmax) const
{
    vmin = 0;
    vmax = 0;
    if (!myObject.valid())
	return;

    GEO_AnimationType	atype;
    GT_Primitive::computeVelocityRange(vmin, vmax,
		    myObject.getVelocity(myFrame, atype));
}

void
GABC_PackedImpl::getWidthRange(fpreal &wmin, fpreal &wmax) const
{
    wmin = wmax = 0;
    if (!myObject.valid())
	return;

    GEO_AnimationType	atype;
    GT_DataArrayHandle	w = myObject.getWidth(myFrame, atype);
    if (w)
	w->getRange(wmin, wmax);
}

void
GABC_PackedImpl::getPrimitiveName(UT_WorkBuffer &wbuf) const
{
    if (UTisstring(objectPath().c_str()))
	wbuf.strcpy(objectPath().c_str());
    else
	GU_PackedImpl::getPrimitiveName(wbuf);
}

bool
GABC_PackedImpl::getLocalTransform(UT_Matrix4D &m) const
{
    if (!myUseTransform || !myObject.valid())
	return false;

#ifdef USE_FAST_CACHE

    bool has_matrix = false;
    gabc_ObjectCacheItem *obj = getObjectCacheItem();
 	
    // grab the animation type
    GEO_AnimationType anim = obj->xform_anim_type;

    if(anim == GEO_ANIMATION_INVALID)
    {
	myObject.getWorldTransform(m, myFrame, anim);

	has_matrix = true;
	obj->xform_anim_type = anim;
	if(anim == GEO_ANIMATION_CONSTANT)
	{
	    obj->static_xform = m;
	    return true;
	}
    }

    if(anim >= GEO_ANIMATION_TRANSFORM)
    {
	gabc_TransformCache::iterator trans_it =
	    obj->transform_cache->find(myFrame);

	if(trans_it != obj->transform_cache->end())
	{
	    // cached matrix
	    m = trans_it->second;
	}
	else
	{
	    if(!has_matrix)
		myObject.getWorldTransform(m, myFrame, anim);
	    
	    (*obj->transform_cache)[myFrame] = m;
	    obj->has_static_xform = false;
	}
    }
    else if(anim == GEO_ANIMATION_CONSTANT) // static transform
	m = obj->static_xform;
    
#else
    
    GEO_AnimationType	atype;
    myObject.getWorldTransform(m, myFrame, atype);
    
#endif
    
    return true;
}

bool
GABC_PackedImpl::unpackGeometry(GU_Detail &destgdp, bool allow_psoup) const
{
    GT_PrimitiveHandle	prim = fullGT();
    if (prim)
    {
	UT_Array<GU_Detail *>	details;
	GT_RefineParms		rparms;
	if (!allow_psoup)
	    rparms.setAllowPolySoup(false);

	GT_Util::makeGEO(details, prim, &rparms);
	for (exint i = 0; i < details.entries(); ++i)
	{
	    copyPrimitiveGroups(*details(i), false);
	    // Don't transform since GT conversion has already done that for us
	    unpackToDetail(destgdp, details(i), false);
	    delete details(i);
	}
    }
    return true;
}

bool
GABC_PackedImpl::unpack(GU_Detail &destgdp) const
{
    return unpackGeometry(destgdp, true);
}

bool
GABC_PackedImpl::unpackUsingPolygons(GU_Detail &destgdp) const
{
    return unpackGeometry(destgdp, false);
}

static UT_Lock	theLock;

#ifdef USE_FAST_CACHE
#include <Alembic/AbcGeom/All.h>
namespace
{
using namespace Alembic::Abc;
using namespace Alembic::AbcGeom;
    
GABC_VisibilityType
fetchVisibility(GABC_IObject &iobj, gabc_ObjectCacheItem *obj)
{
    // must be called from within a locked section.
    IVisibilityProperty	vprop = Alembic::AbcGeom::GetVisibilityProperty(
	const_cast<IObject &>(iobj.object()));
    if (vprop.valid())
    {
	if(vprop.isConstant())
	{
	    obj->vis_anim_type = GEO_ANIMATION_CONSTANT;
	    obj->vis_type = vprop.getValue((index_t)0) ? GABC_VISIBLE_VISIBLE
						       : GABC_VISIBLE_HIDDEN;
	    return obj->vis_type;
	}
	else
	{
	    exint		 nsamples = vprop.getNumSamples();
	    exint		 ntrue = 0;
	    for (exint i = 0; i < nsamples; ++i)
	    {
		index_t idx = i;
		bool vis = vprop.getValue(ISampleSelector(idx));
		fpreal t = vprop.getTimeSampling()->getSampleTime(idx);

	
		(*obj->vis_cache)[t] = vis ? GABC_VISIBLE_VISIBLE
					   : GABC_VISIBLE_HIDDEN;
		if(vis)
		    ntrue++;
	    }

	    // trivial visibility list, not really animated.
	    if(ntrue == 0 || ntrue == nsamples)
	    {
		obj->vis_anim_type = GEO_ANIMATION_CONSTANT;
		obj->vis_type = (ntrue != 0) ? GABC_VISIBLE_VISIBLE
					     : GABC_VISIBLE_HIDDEN;
		obj->vis_cache->clear();

		return obj->vis_type;
	    }
	    else
	    {
		obj->vis_anim_type = GEO_ANIMATION_TRANSFORM; // no anim_vis
		return GABC_VISIBLE_DEFER;
	    }
	}
    }
    else
    {
	GABC_IObject pobj = iobj.getParent();
	if(pobj.valid())
	    return fetchVisibility(pobj, obj);
    }

    obj->vis_anim_type = GEO_ANIMATION_CONSTANT;
    return GABC_VISIBLE_VISIBLE;
}
}
#endif

bool
GABC_PackedImpl::visibleGT() const
{
    if (!object().valid())
	return false;

#ifdef USE_FAST_CACHE
    if(!useVisibility())
	return true;
    
    if(myConstVisibility != GABC_VISIBLE_DEFER)
	return (myConstVisibility == GABC_VISIBLE_VISIBLE);
        
    gabc_ObjectCacheItem *obj = getObjectCacheItem();
    GABC_VisibilityType vis = GABC_VISIBLE_DEFER;

    if(obj->vis_anim_type == GEO_ANIMATION_INVALID)
    {
	{
	    GABC_AlembicLock	lock(myObject.archive());
	    vis = fetchVisibility(myObject, obj);
	}
	
	if(vis != GABC_VISIBLE_DEFER)
	{
	    if(obj->vis_anim_type == GEO_ANIMATION_CONSTANT)
		myConstVisibility = obj->vis_type;
	    
	    return (obj->vis_type != GABC_VISIBLE_HIDDEN);
	}
    }

    if(obj->vis_anim_type == GEO_ANIMATION_CONSTANT)
    {
	vis = obj->vis_type;
    }
    else
    {
	gabc_VisibilityCache::iterator vis_it = obj->vis_cache->find(frame());

	if(vis_it != obj->vis_cache->end())
	    return (vis_it->second != GABC_VISIBLE_HIDDEN);
    }

    return (obj->vis_type != GABC_VISIBLE_HIDDEN);
   
#else
    return myCache.visible(this);
#endif
}

GT_PrimitiveHandle
GABC_PackedImpl::fullGT(int load_style) const
{
    if (!object().valid())
	return GT_PrimitiveHandle();
    
#ifdef USE_FAST_CACHE
    GT_PrimitiveHandle h;
    GEO_AnimationType anim = GEO_ANIMATION_INVALID;
    gabc_ObjectCacheItem *obj = getObjectCacheItem();

    // First, see if the animation type is cached
    anim = obj->geo_anim_type;
    h = obj->static_geo;

    if(anim == GEO_ANIMATION_INVALID)
    {
	anim = animationType();
	obj->geo_anim_type = anim;
    }

    // Now, see if the deformed geometry is cached.
    if(anim == GEO_ANIMATION_ATTRIBUTE || anim == GEO_ANIMATION_TOPOLOGY)
    {
	gabc_DeformCache::iterator it =  obj->deform_cache->find(myFrame);
	if(it != obj->deform_cache->end())
	{
	    h = it->second;
	}
	else
	{
	    h = myCache.full(this, load_style);
	    // put deforming geo in the deformation cache
	    (*obj->deform_cache)[myFrame] = h;
	    if(h)
		h->setStaticGeometry( false );
	}
    }
    else if(!h)
    {
	h = myCache.full(this, load_style);
	if(h)
	    h->setStaticGeometry(true);

	// put static geo into the object cache.
	obj->static_geo = h;
    }
    
    return h;
#else
    return myCache.full(this, load_style);
#endif
}

GT_PrimitiveHandle
GABC_PackedImpl::instanceGT() const
{
    UT_AutoLock	lock(theLock);
    int		loadstyle = GABC_IObject::GABC_LOAD_FULL;
    // We don't want to copy over the attributes from the Houdini geometry
    loadstyle &= ~(GABC_IObject::GABC_LOAD_HOUDINI);
    // We don't want to transform into world space
    loadstyle |=  (GABC_IObject::GABC_LOAD_FORCE_UNTRANSFORMED);
    return fullGT(loadstyle);
}

GT_PrimitiveHandle
GABC_PackedImpl::pointGT() const
{
    if (!object().valid())
	return GT_PrimitiveHandle();
    return myCache.points(this);
}

GT_PrimitiveHandle
GABC_PackedImpl::boxGT() const
{
    if (!object().valid())
	return GT_PrimitiveHandle();
    return myCache.box(this);
}

GT_PrimitiveHandle
GABC_PackedImpl::centroidGT() const
{
    if (!object().valid())
	return GT_PrimitiveHandle();
    return myCache.centroid(this);
}

GT_TransformHandle
GABC_PackedImpl::xformGT() const
{
    if (!object().valid())
	return GT_TransformHandle();
    return myCache.xform(this);
}

const GABC_IObject &
GABC_PackedImpl::object() const
{
    if (!myObject.valid())
	myObject = GABC_Util::findObject(myFilename, myObjectPath);
    return myObject;
}

void
GABC_PackedImpl::setObject(const GABC_IObject &v)
{
    myObject = v;
    myObjectPath = myObject.objectPath();
    myCache.clear();
}

void
GABC_PackedImpl::setFilename(const std::string &v)
{
    if (myFilename != v)
    {
	myFilename = v;
	myCache.clear();
	markDirty();
    }
}

void
GABC_PackedImpl::setObjectPath(const std::string &v)
{
    if (myObjectPath != v)
    {
	myObjectPath = v;
	myCache.clear();
	markDirty();
    }
}

void
GABC_PackedImpl::setFrame(fpreal f)
{
    myFrame = f;
    myCache.updateFrame(f);
    markDirty();
}

void
GABC_PackedImpl::setUseTransform(bool v)
{
    if (v != myUseTransform)
    {
	myUseTransform = v;
	// This can affect animation type
	myCache.clear();
	markDirty();
    }
}

void
GABC_PackedImpl::setUseVisibility(bool v)
{
    if (v != myUseVisibility)
    {
	myUseVisibility = v;
	// This can affect animation type
	myCache.clear();
	markDirty();
    }
}

GEO_AnimationType
GABC_PackedImpl::animationType() const
{
    return myCache.animationType(this);
}

void
GABC_PackedImpl::GTCache::clear()
{
    delete myVisibility;
    myVisibility = NULL;
    myPrim = GT_PrimitiveHandle();
    myTransform = GT_TransformHandle();
    myRep = GEO_VIEWPORT_INVALID_MODE;
    myAnimationType = GEO_ANIMATION_INVALID;
    myFrame = 0;
    myLoadStyle = GABC_IObject::GABC_LOAD_FULL;
}

void
GABC_PackedImpl::GTCache::updateFrame(fpreal frame)
{
    if (frame == myFrame)
	return;
    myFrame = frame;
    switch (myAnimationType)
    {
	case GEO_ANIMATION_ATTRIBUTE:
	case GEO_ANIMATION_TOPOLOGY:
	    myPrim = GT_PrimitiveHandle();
	case GEO_ANIMATION_TRANSFORM:
	    myTransform = GT_TransformHandle();
	case GEO_ANIMATION_CONSTANT:
	case GEO_ANIMATION_INVALID:
	    break;
    }
}

const GT_PrimitiveHandle &
GABC_PackedImpl::GTCache::full(const GABC_PackedImpl *abc, int load_style)
{
    if (!visible(abc))
	return theNullPrimitive;
    if (!myPrim
	    || myRep != GEO_VIEWPORT_FULL
	    || myLoadStyle != load_style
	    || myFrame != abc->frame())
    {
	const GABC_IObject	&o = abc->object();
	if (o.valid())
	{
	    GEO_AnimationType	atype;
	    myFrame = abc->frame();
	    myRep = GEO_VIEWPORT_FULL;
	    myLoadStyle = load_style;
	    myPrim = o.getPrimitive(abc->getPrim(), myFrame, atype,
		    abc->getPrim()->attributeNameMap(), myLoadStyle);
	    if (atype > myAnimationType)
		myAnimationType = atype;
	}
    }
    if (myPrim)
	updateTransform(abc);
    return myPrim;
}

const GT_PrimitiveHandle &
GABC_PackedImpl::GTCache::points(const GABC_PackedImpl *abc)
{
    if (!visible(abc))
    {
	return theNullPrimitive;
    }
    if (!myPrim || myRep != GEO_VIEWPORT_POINTS || myFrame != abc->frame())
    {
	const GABC_IObject	&o = abc->object();
	if (o.valid())
	{
	    GEO_AnimationType	atype;
	    myFrame = abc->frame();
	    myRep = GEO_VIEWPORT_POINTS;
	    myPrim = o.getPointCloud(myFrame, atype);
	    if (atype > myAnimationType)
		myAnimationType = atype;
	}
    }
    if (myPrim)
	updateTransform(abc);
    return myPrim;
}

const GT_PrimitiveHandle &
GABC_PackedImpl::GTCache::box(const GABC_PackedImpl *abc)
{
    if (!visible(abc))
	return theNullPrimitive;
    if (!myPrim || myRep != GEO_VIEWPORT_BOX || myFrame != abc->frame())
    {
	const GABC_IObject	&o = abc->object();
	if (o.valid())
	{
	    bool		isconst;
	    UT_BoundingBox	box;
	    myFrame = abc->frame();
	    myRep = GEO_VIEWPORT_BOX;
	    if (o.getBoundingBox(box, myFrame, isconst))
	    {
		GT_BuilderStatus	err;
		myPrim = GT_PrimitiveBuilder::wireBox(err, box);
		if (myAnimationType < GEO_ANIMATION_ATTRIBUTE && !isconst)
		    myAnimationType = GEO_ANIMATION_ATTRIBUTE;
	    }
	}
    }
    if (myPrim)
	updateTransform(abc);
    return myPrim;
}

const GT_PrimitiveHandle &
GABC_PackedImpl::GTCache::centroid(const GABC_PackedImpl *abc)
{
    if (!visible(abc))
	return theNullPrimitive;
    if (!myPrim || myRep != GEO_VIEWPORT_CENTROID || myFrame != abc->frame())
    {
	const GABC_IObject	&o = abc->object();
	if (o.valid())
	{
	    bool		isconst;
	    UT_BoundingBox	box;
	    myFrame = abc->frame();
	    myRep = GEO_VIEWPORT_CENTROID;
	    if (o.getBoundingBox(box, myFrame, isconst))
	    {
		fpreal64		pos[3];
		pos[0] = box.xcenter();
		pos[1] = box.ycenter();
		pos[2] = box.zcenter();
		GT_AttributeListHandle	pt;
		pt = GT_AttributeList::createAttributeList(
			"P", new GT_RealConstant(1, pos, 3, GT_TYPE_POINT),
			NULL);
		myPrim = new GT_PrimPointMesh(pt,GT_AttributeListHandle());
		if (myAnimationType < GEO_ANIMATION_ATTRIBUTE && !isconst)
		    myAnimationType = GEO_ANIMATION_ATTRIBUTE;
	    }
	}
    }
    if (myPrim)
	updateTransform(abc);
    return myPrim;
}

GEO_AnimationType
GABC_PackedImpl::GTCache::animationType(const GABC_PackedImpl *abc)
{
    if (myAnimationType == GEO_ANIMATION_INVALID)
     	points(abc);	// Update lightest weight cache
    return myAnimationType;
}

bool
GABC_PackedImpl::GTCache::visible(const GABC_PackedImpl *abc)
{
    if (!abc->useVisibility())
    {
	delete myVisibility;
	myVisibility = NULL;
	return true;
    }
    if (!myVisibility)
    {
	const GABC_IObject	&o = abc->object();
	if (!o.valid())
	    return false;
	myVisibility = o.visibilityCache();
    }
    UT_ASSERT(myVisibility);
    myVisibility->update(abc->frame());
    if (myAnimationType <= GEO_ANIMATION_CONSTANT && myVisibility->animated())
    {
	myAnimationType = GEO_ANIMATION_TRANSFORM;
    }
    return myVisibility->visible();
}

GABC_VisibilityType
GABC_PackedImpl::computeVisibility(bool check_parent) const
{
    const GABC_IObject	&o = object();
    if (!o.valid())
	return GABC_VISIBLE_HIDDEN;

    bool		 animated;
    return o.visibility(animated, frame(), check_parent);
}

void
GABC_PackedImpl::GTCache::refreshTransform(const GABC_PackedImpl *abc)
{
    UT_AutoLock	lock(theLock);
    if (!myTransform)
    {
	const GABC_IObject	&o = abc->object();
	if (myAnimationType == GEO_ANIMATION_CONSTANT
		&& abc->useVisibility()
		&& o.visibilityCache()->animated())
	{
	    // Mark animated visibility as animated transforms
	    myAnimationType = GEO_ANIMATION_TRANSFORM;
	}

	UT_Matrix4D		xform;
	abc->getPrim()->getFullTransform4(xform);
	myTransform.reset(new GT_Transform(&xform, 1));
	if (myAnimationType == GEO_ANIMATION_CONSTANT
		&& abc->useTransform()
		&& abc->object().isTransformAnimated())
	{
	    myAnimationType = GEO_ANIMATION_TRANSFORM;
	}
    }
}

void
GABC_PackedImpl::GTCache::updateTransform(const GABC_PackedImpl *abc)
{
    UT_ASSERT(myPrim);
    if (myLoadStyle & GABC_IObject::GABC_LOAD_FORCE_UNTRANSFORMED)
    {
	myPrim->setPrimitiveTransform(GT_Transform::identity());
    }
    else
    {
	refreshTransform(abc);
	myPrim->setPrimitiveTransform(myTransform);
    }
}

void
GABC_PackedImpl::markDirty()
{
    // Don't compute the animation type
    switch (myCache.animationType())
    {
	case GEO_ANIMATION_CONSTANT:
	    break;
	case GEO_ANIMATION_TRANSFORM:
	    transformDirty();
	    break;
	case GEO_ANIMATION_INVALID:
	case GEO_ANIMATION_ATTRIBUTE:
	    attributeDirty();
	    break;
	default:
	    topologyDirty();
	    break;
    }
}

int64
GABC_PackedImpl::getPropertiesHash() const
{
    if(!myCachedUniqueID)
    {
#ifdef USE_FAST_CACHE
	gabc_ObjectCacheItem *obj = getObjectCacheItem();

	// yes, there is a 1 in 2^64 chance that this will give a false
	// positive for an uncached prop hash.
	if(obj->prop_hash != -1)
	    myUniqueID = obj->prop_hash;
	else
	{
#endif
	    if(!myObject.getPropertiesHash(myUniqueID))
	    {
		// HDF, likely. Hash the object path & filename to get an id.
		const int64 pathhash = UT_String::hash(objectPath().c_str());
		const int64 filehash = UT_String::hash(filename().c_str());
		
		myUniqueID = pathhash + SYSwang_inthash64(filehash);
	    }
	    
#ifdef USE_FAST_CACHE
	    obj->prop_hash = myUniqueID;
	}
#endif	    
	myCachedUniqueID = true;
    }
    
    return myUniqueID;
}
