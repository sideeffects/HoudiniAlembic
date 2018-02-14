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

#include "GABC_PackedGT.h"
#include "GABC_PackedImpl.h"
#include "GABC_IArchive.h"
#include <GT/GT_CatPolygonMesh.h>
#include <GT/GT_DAConstantValue.h>
#include <GT/GT_DAIndirect.h>
#include <GT/GT_DANumeric.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_GEOPrimCollectBoxes.h>
#include <GT/GT_GEOAttributeFilter.h>
#include <GT/GT_PrimCollect.h>
#include <GT/GT_PrimPointMesh.h>
#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_TransformArray.h>
#include <GT/GT_PackedGeoCache.h>
#include <GT/GT_PrimInstance.h>
#include <GT/GT_PrimPolygonMesh.h>
#include <GT/GT_Util.h>
#include <UT/UT_Debug.h>
#include <UT/UT_EnvControl.h>
#include <UT/UT_StackBuffer.h>
#include <SYS/SYS_Hash.h>
#include <tools/henv.h>

using namespace GABC_NAMESPACE;

static UT_StringMap<int> theInstanceCounts;

namespace
{

int gabcExprUseArchivePrims()
{
    static int theEnvVar = -1;
    if(theEnvVar == -1)
    {
	UT_String var(::HoudiniGetenv("HOUDINIX_OGL_ALEMBIC_ARCHIVES"));
	if(var.isstring())
	{
	    theEnvVar = var.toInt();
	    UTdebugPrint("(HOUDINIX_OGL_ALEMBIC_ARCHIVES) "
			 "Using Alembic Archives, level ", theEnvVar);
	}
	else
	    theEnvVar = 0;
    }
    return theEnvVar;
}

class gabc_CreateInstanceGeos
{
public:
    gabc_CreateInstanceGeos(
	const UT_StringMap< UT_Array<const GU_PrimPacked*> > &geo,
	const GT_GEODetailListHandle &geometry,
	const GT_RefineParms *ref_parms,
	UT_StringMap<GT_PrimitiveHandle> &gt_prims,
	bool collect_anim_info)
      : myGeos(geo), myGeometry(geometry), myRefineParms(ref_parms),
	myPrims(gt_prims), myCollectAnimInfo(collect_anim_info)
	{}
    
    void	operator()(const UT_BlockedRange<exint> &range) const
	{
	    GT_GEOAttributeFilter filter;
	    GT_DataArrayHandle mat_id, mat_remap;
	    
	    if(myRefineParms)
	    {
		mat_id = myRefineParms->getViewportMaterialAttrib();
		mat_remap = myRefineParms->getViewportMaterialRemapAttrib();
	    }
		

	    int index = 0;
	    for(auto itr = myGeos.begin(); itr != myGeos.end(); ++itr, index++)
	    {
		if(index < range.begin())
		    continue;
		if(index == range.end())
		    break;

		auto impl= UTverify_cast<const GABC_PackedImpl*>
		    (itr->second(0)->implementation());

		UT_StringHolder path = impl->object().getFullName();

		// No need to pack if there is only a single instance.
		if(itr->second.entries() == 1)
		{
		    GABC_PackedAlembic *packgt =
			new GABC_PackedAlembic(myGeometry->getGeometry(0),
					       itr->second(0),
					       mat_id, mat_remap, true);
	    
		    if(myCollectAnimInfo)
		    {
			bool vis_anim;
			impl->object().visibility(vis_anim, impl->frame(),true);

			packgt->initVisAnim();
			packgt->setAnimationType(impl->animationType());
			packgt->setVisibilityAnimated(vis_anim);
		    }
		    else
			packgt->initVisAnim();
		    
		    myPrims[path] = packgt;
		    continue;
		}
		

		// Instanced geometry
		GT_PrimitiveHandle geo = impl->instanceGT(true);
		GT_TransformArray *xforms = new GT_TransformArray;
		GT_TransformArrayHandle xformh = xforms;
		GT_GEOOffsetList offsets, voffsets;
		bool use_vertex = true;

		UT_Array<GEO_ViewportLOD> lod;
		GEO_AnimationType anim_type = GEO_ANIMATION_INVALID;
		if(myCollectAnimInfo)
		    anim_type = impl->animationType();
		
		for( auto pi : itr->second )
		{
		    GABC_PackedAlembic inst(myGeometry->getGeometry(0), pi,
					    mat_id, mat_remap, true);

		    xforms->append( inst.getFullTransform() );
		    offsets.append( inst.getPrim()->getMapOffset() );
		    if(inst.getPrim()->getVertexCount() > 0)
		       voffsets.append(inst.getPrim()->getVertexOffset(0));
		    else
			use_vertex = false; // avoid if one instance has 0 verts

		    auto impl= UTverify_cast<const GABC_PackedImpl*>
			(pi->implementation());
			
		    // check for an animated transform.
		    if(myCollectAnimInfo &&
		       anim_type < GEO_ANIMATION_TRANSFORM)
		    {
			bool vis_anim;
			impl->object().visibility(vis_anim, impl->frame(),true);
			if(impl->object().isTransformAnimated() || vis_anim)
			    anim_type = GEO_ANIMATION_TRANSFORM;
		    }

		    GEO_ViewportLOD prim_lod = GEO_VIEWPORT_HIDDEN;
		    if(impl->visibleGT())
			prim_lod = pi->viewportLOD();

		    lod.append(prim_lod);
		}

		// build vertex/prim/point list
		GT_AttributeListHandle prim_attribs;
		if(use_vertex)
		{
		    prim_attribs = 
		    myGeometry->getPrimitiveVertexAttributes(
			filter, offsets, voffsets,
			GT_GEODetailList::GEO_INCLUDE_POINT,
			GT_GEODetailList::GEO_SKIP_DETAIL);
		}
		else
		{
		    prim_attribs = 
		    myGeometry->getPrimitiveAttributes(filter, &offsets,
				       GT_GEODetailList::GEO_SKIP_DETAIL,false);
		}


		GT_AttributeListHandle detail_attribs =
		    myGeometry->getDetailAttributes(filter, false);
		
		GT_Util::addViewportLODAttribs(lod,prim_attribs,detail_attribs);

		if(mat_id)
		{
		    GU_DetailHandleAutoReadLock gdl(myGeometry->getGeometry(0));

		    auto index = new 
			GT_DANumeric<int64>(offsets.entries(),1);
		    for(int i=0; i<offsets.entries(); i++)
		    {
			GA_Offset off = offsets.get(i);
			index->set( (int64) gdl->primitiveIndex(off), i);
		    }

		    GT_DataArrayHandle imat_id =
			new GT_DAIndirect(index, mat_id);
		    prim_attribs = prim_attribs->addAttribute("MatID", imat_id,
							      true);
		    if(mat_remap)
		    {
			GT_DataArrayHandle imat_remap =
			    new GT_DAIndirect(index, mat_remap);
			prim_attribs = prim_attribs->addAttribute(
			    "MatRemap", imat_remap, true);
		    }
		}
		
		myPrims[path] = new GABC_PackedInstance(geo, xformh, anim_type,
							offsets, prim_attribs,
							detail_attribs,
						     GT_GEODetailListHandle());
	    }
	}

private:
    const UT_StringMap< UT_Array<const GU_PrimPacked *> > &myGeos;
    UT_StringMap<GT_PrimitiveHandle>		 &myPrims;
    const GT_GEODetailListHandle		  myGeometry;
    const GT_RefineParms			 *myRefineParms;
    const bool					  myCollectAnimInfo;
};


class CollectData : public GT_GEOPrimCollectData
{
public:
    CollectData(const GT_GEODetailListHandle &geometry,
		const GT_RefineParms *parms)
	: GT_GEOPrimCollectData()
	, myGeometry(geometry)
	, myUseViewportLOD(GT_GEOPrimPacked::useViewportLOD(parms))
	, mySkipInvisible(GT_RefineParms::getAlembicSkipInvisible(parms))
	, myAlembicInstancing(GT_RefineParms::getAlembicInstancing(parms))
	, myRefineParms(parms)
    { }
    virtual ~CollectData() {}

    bool	append(const GU_PrimPacked &prim)
    {
	auto impl= UTverify_cast<const GABC_PackedImpl*>(prim.implementation());
	if (mySkipInvisible
	    && !(myAlembicInstancing && myUseViewportLOD) 
	    && !impl->visibleGT())
	{
	    return true;	// Handled
	}
	if (myUseViewportLOD)
	{
	    switch (prim.viewportLOD())
	    {
		case GEO_VIEWPORT_HIDDEN:
		    return true;	// Handled
		case GEO_VIEWPORT_CENTROID:
		    myCentroidPrims.append(&prim);
		    return true;
		case GEO_VIEWPORT_BOX:
		    myBoxPrims.append(&prim);
		    return true;
	        default:
		    if(myAlembicInstancing || gabcExprUseArchivePrims() >= 2)
		    {
			assignToArchive(prim, impl);
			return true;
		    }
		    break;
	    }
	}
	if(myAlembicInstancing || gabcExprUseArchivePrims() == 1)
	{
	    bucketPrim(prim, impl);
	    return true;
	}
	return false;
    }

    // bucket by archive:prim into lists of instances.
    void bucketPrim(const GU_PrimPacked &prim,
		    const GABC_PackedImpl *impl)
	{
	    UT_WorkBuffer bucket_name;
	    UT_StringHolder path = impl->object().getSourcePath();

	    UT_StringHolder arch;
	    GT_PackedGeoCache::buildAlembicArchiveName(arch,
					impl->object().archive()->filenames());

	    bucket_name.sprintf("%s:%s", arch.c_str(), path.c_str());
	    auto entry = myInstanceAnim.find( bucket_name.buffer() );
	    GEO_AnimationType anim = impl->animationType();
	    if(entry == myInstanceAnim.end())
	    {
		anim = impl->animationType();
		myInstanceAnim[ bucket_name.buffer() ] = anim;
	    }
	    else
		anim = entry->second;
	    
	    if(anim > GEO_ANIMATION_TRANSFORM)
		bucket_name.appendSprintf("[%f]", impl->frame());

	    myInstanceGeo[ bucket_name.buffer() ].append(&prim);
	    myAlembicNames.append(bucket_name.buffer());
	}

    // only bucket by archive. will be bucketed later. 
    void assignToArchive(const GU_PrimPacked &prim,
			 const GABC_PackedImpl *impl)
	{
	    if(!impl->object().valid())
		return;
	    GT_PrimitiveHandle archive;

	    UT_StringHolder arch;
	    GT_PackedGeoCache::buildAlembicArchiveName(arch,
					impl->object().archive()->filenames());

	    auto entry = myViewportArchives.find( arch );
	    if(entry == myViewportArchives.end())
	    {
		archive = new GABC_PackedArchive(arch, myGeometry,
						 impl->object().archive());
		myViewportArchives[ arch ] = archive;
	    }
	    else
		archive = entry->second;

	    static_cast<GABC_PackedArchive *>(archive.get())->
		appendAlembic( prim.getMapOffset() );
	}
    
    class FillTask
    {
    public:
	FillTask(UT_BoundingBox *boxes,
		UT_Matrix4F *xforms,
		const UT_Array<const GU_PrimPacked *>&prims)
	    : myBoxes(boxes)
	    , myXforms(xforms)
	    , myPrims(prims)
	{
	}
	void	operator()(const UT_BlockedRange<exint> &range) const
	{
	    UT_Matrix4D		m4d;
	    for (exint i = range.begin(); i != range.end(); ++i)
	    {
		const GU_PrimPacked	&prim = *myPrims(i);
		prim.getUntransformedBounds(myBoxes[i]);
		prim.getFullTransform4(m4d);
		myXforms[i] = m4d;
	    }
	}
    private:
	UT_BoundingBox				*myBoxes;
	UT_Matrix4F				*myXforms;
	const UT_Array<const GU_PrimPacked *>	&myPrims;
    };

    GT_PrimitiveHandle	finishBoxes() const
    {
	exint			nbox = myBoxPrims.entries();
	exint			ncentroid = myCentroidPrims.entries();
	exint			nprims = SYSmax(nbox, ncentroid);

	if (!nprims)
	    return GT_PrimitiveHandle();

	GT_GEOPrimCollectBoxes		boxdata(myGeometry, true);
	UT_StackBuffer<UT_BoundingBox>	boxes(nprims);
	UT_StackBuffer<UT_Matrix4F>	xforms(nprims);
	if (nbox)
	{
	    //UTparallelFor(UT_BlockedRange<exint>(0, nbox),
	    UTserialFor(UT_BlockedRange<exint>(0, nbox),
		    FillTask(boxes, xforms, myBoxPrims));
	    for (exint i = 0; i < nbox; ++i)
	    {
		boxdata.appendBox(boxes[i], xforms[i],
			myBoxPrims(i)->getMapOffset(),
			myBoxPrims(i)->getVertexOffset(0),
			myBoxPrims(i)->getPointOffset(0));
	    }
	}
	if (ncentroid)
	{
	    //UTparallelFor(UT_BlockedRange<exint>(0, ncentroid),
	    UTserialFor(UT_BlockedRange<exint>(0, ncentroid),
		    FillTask(boxes, xforms, myCentroidPrims));
	    for (exint i = 0; i < ncentroid; ++i)
	    {
		boxdata.appendCentroid(boxes[i], xforms[i],
			myCentroidPrims(i)->getMapOffset(),
			myCentroidPrims(i)->getVertexOffset(0),
			myCentroidPrims(i)->getPointOffset(0));
	    }
	}
	return boxdata.getPrimitive();
    }

    GT_PrimitiveHandle  finishInstances() const
	{
	    GT_PrimitiveHandle result;
	    
	    if(myInstanceGeo.size() == 0)
		return result;

	    GT_PrimCollect *collect = NULL;
	    if(myInstanceGeo.size() > 1)
	    {
		collect = new GT_PrimCollect;
		result = collect;
	    }

	    // std::cerr << "# unique instances: " << myInstanceGeo.size()
	    // 	      << std::endl;
	    UT_StringMap<GT_PrimitiveHandle> prims;
	    for(auto name : myAlembicNames)
		prims[name] = nullptr;
	    gabc_CreateInstanceGeos task(myInstanceGeo, myGeometry,
					 myRefineParms, prims, false);
#if 1
	    UTparallelFor(UT_BlockedRange<exint>(0, myInstanceGeo.size()),
			  task, 1, 20);
#else
	    UTserialFor(UT_BlockedRange<exint>(0, myInstanceGeo.size()),
			task);
#endif
	    if(collect)
	    {
		for(auto name : myAlembicNames)
		    collect->appendPrimitive(prims[name]);
	    }
	    else
		result = prims[myAlembicNames(0)];

	    return result;
	}
    
    GT_PrimitiveHandle	finish() const
    {
	GT_PrimCollect *collect = new GT_PrimCollect;
	GT_PrimitiveHandle collecth = collect;
	
	GT_PrimitiveHandle instances = finishInstances();
	if(instances)
	    collect->appendPrimitive(instances);
	
	GT_PrimitiveHandle boxes = finishBoxes();
	if(boxes)
	    collect->appendPrimitive(boxes);

	for( auto p : myViewportArchives)
	    collect->appendPrimitive(p.second);

	return collecth;
    }

private:
    const GT_GEODetailListHandle	myGeometry;
    const GT_RefineParms	       *myRefineParms;
    UT_StringMap<GT_PrimitiveHandle>	myViewportArchives;
    
    // Viewport
    const bool				myUseViewportLOD;
    const bool				mySkipInvisible;
    const bool				myAlembicInstancing;
    UT_Array<const GU_PrimPacked *>	myBoxPrims;
    UT_Array<const GU_PrimPacked *>	myCentroidPrims;
    UT_StringMap< UT_Array<const GU_PrimPacked *> > myInstanceGeo;
    UT_StringMap< GEO_AnimationType >	myInstanceAnim;
    UT_StringArray			myAlembicNames;
};

} // namespace


// ---------------------------------------------------------- GABC_PackedAlembic

GABC_PackedAlembic::GABC_PackedAlembic(const GU_ConstDetailHandle &prim_gdh,
				       const GU_PrimPacked *prim,
				       const GT_DataArrayHandle &vp_mat,
				       const GT_DataArrayHandle &vp_remap,
				       bool build_packed_attribs)
    : GT_PackedAlembic(prim_gdh, prim, vp_mat, vp_remap, build_packed_attribs),
      myColorID(-2)
{
}

GABC_PackedAlembic::GABC_PackedAlembic(const GABC_PackedAlembic &src)
    : GT_PackedAlembic(src),
      myColorID(src.myColorID)
{
}

bool
GABC_PackedAlembic::updateGeoPrim(const GU_ConstDetailHandle &dtl,
				  const GT_RefineParms &refine)
{
    bool changed = false;
    
    const GU_PrimPacked *packed = nullptr;
    if(GAisValid(myOffset))
    {
	GU_DetailHandleAutoReadLock lock(dtl);
	const GU_Detail *gdp = lock.getGdp();
	packed=dynamic_cast<const GU_PrimPacked *>(gdp->getPrimitive(myOffset));

	if(packed)
	{
	    GT_GEOPrimPacked::setDetailPrim(dtl, packed);
	    
	    GA_LocalIntrinsic fr = packed->findIntrinsic("abcframe");
	    float frame = 0;
	    if(packed->getIntrinsic(fr, frame))
	    {
		if(!SYSisEqual(myFrame, frame))
		{
		    myFrame = frame;
		    changed = true;
		}
	    }

	    UT_Matrix4D transform;
	    auto pimpl = UTverify_cast<const GABC_PackedImpl *>(
		packed->implementation());

	    pimpl->setViewportCache(&myCache);
	    packed->getFullTransform4(transform);
	    pimpl->setViewportCache(nullptr);
			
	    if(myTransform != transform)
	    {
		myTransform = transform;
		changed = true;
	    }
	}
	
    }
    
    if(GT_GEOPrimPacked::updateGeoPrim(dtl, refine))
	changed = true;

    int cid = -2; // no Cd attrib
    if(getPointAttributes())
    {
	GT_DataArrayHandle ch = getPointAttributes()->get("Cd");
	if(ch)
	{
	    cid = ch->getDataId();
	    if(cid == -1 || cid != myColorID)
		changed = true;
	}
    }
    myColorID = cid;

    return changed;
}


void
GABC_PackedAlembic::initVisAnim()
{
    const GABC_PackedImpl *impl =
	UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    if(impl)
    {
	fpreal frame = 0.0;

	myAnimType = impl->animationType();
	if(myAnimType > GEO_ANIMATION_TRANSFORM)
	{
	    frame = impl->frame();
	}
	else if(myAnimType != GEO_ANIMATION_CONSTANT)
	    myCache.setTransformAnimated(true);
		
	myVisibleConst = impl->visibleGT(&myAnimVis);
	myCache.setVisibilityAnimated(myAnimVis);

	SYS_HashType hash = impl->getPropertiesHash();
	SYShashCombine(hash, SYSreal_hash(frame));
	myID = hash;

	myFrame = frame;

	UT_BoundingBox bbox;
	if(impl->getBounds(bbox))
	    GT_Util::addBBoxAttrib(bbox, myDetailAttribs);
    }
}

GABC_PackedAlembic::~GABC_PackedAlembic()
{
}


GT_PrimitiveHandle
GABC_PackedAlembic::getPointCloud(const GT_RefineParms *, bool &xform) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    xform = false;
    return impl->pointGT();
}

GT_PrimitiveHandle
GABC_PackedAlembic::getFullGeometry(const GT_RefineParms *parms,
				    bool &xform) const
{
    const GABC_PackedImpl	*impl;
    int				 load_style;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    xform = false;
    if (GT_GEOPrimPacked::useViewportLOD(parms))
    {
	load_style = GABC_IObject::GABC_LOAD_LEAN_AND_MEAN;
	if (GT_RefineParms::getViewportAlembicFaceSets(parms))
	    load_style |= GABC_IObject::GABC_LOAD_FACESETS;
	if (GT_RefineParms::getViewportAlembicArbGeometry(parms))
	{
	    load_style |= GABC_IObject::GABC_LOAD_ARBS;
	    load_style |= GABC_IObject::GABC_LOAD_HOUDINI;
	}

	if(visibilityAnimated())
	    load_style |= GABC_IObject::GABC_LOAD_IGNORE_VISIBILITY;

	if(parms && parms->getAlembicGLOptimize())
	    load_style |= GABC_IObject::GABC_LOAD_GL_OPTIMIZED;
	
	load_style |= GABC_IObject::GABC_LOAD_USE_GL_CACHE;
	load_style |= GABC_IObject::GABC_LOAD_NO_PACKED_ATTRIBS;
    }
    else
    {
	load_style = GABC_IObject::GABC_LOAD_FULL;
	if (!GT_RefineParms::getAlembicHoudiniAttribs(parms))
	    load_style &= ~GABC_IObject::GABC_LOAD_HOUDINI;
    }
    if(!parms->getAlembicSkipInvisible())
	load_style |= GABC_IObject::GABC_LOAD_IGNORE_VISIBILITY;

    GT_PrimitiveHandle prim = impl->fullGT(load_style);
    if(!prim && GT_GEOPrimPacked::useViewportLOD(parms))
    {
	// Placeholder so the viewport can at least draw bboxes and decorations.
	GT_AttributeListHandle null;
	prim = new GT_PrimPointMesh(null, null);
    }
    return prim;
}

#if 0
namespace
{
// Methods to add GT array values to a UT_Options (for instance keys)
static void
addFloat(UT_Options &options, const char *name, const GT_DataArrayHandle &h)
{
    UT_ASSERT(h->entries() == 1);
    if (h->getTupleSize() == 1)
	options.setOptionF(name, h->getF64(0));
    else
    {
	UT_Fpreal64Array	vals;
	vals.entries(h->getTupleSize());
	h->import(0, vals.array(), vals.entries());
	options.setOptionFArray(name, vals);
    }
}

static void
addInt(UT_Options &options, const char *name, const GT_DataArrayHandle &h)
{
    UT_ASSERT(h->entries() == 1);
    if (h->getTupleSize() == 1)
	options.setOptionI(name, h->getI64(0));
    else
    {
	UT_Int64Array	vals;
	vals.entries(h->getTupleSize());
	h->import(0, vals.array(), vals.entries());
	options.setOptionIArray(name, vals);
    }
}

static void
addString(UT_Options &options, const char *name, const GT_DataArrayHandle &h)
{
    UT_ASSERT(h->entries() == 1);
    if (h->getTupleSize() == 1)
	options.setOptionS(name, h->getS(0));
    else
    {
	UT_WorkBuffer	val;
	val.sprintf("[%s", h->getS(0));
	for (int i = 1; i < h->getTupleSize(); ++i)
	{
	    val.append(",");
	    val.append(h->getS(i));
	}
	options.setOptionS(name, val.buffer());
    }
}
}
#endif


bool
GABC_PackedAlembic::getInstanceKey(UT_Options &options) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());

    options.setOptionS("f", impl->filenamesJSON());
    // If the object instances another shape, we want to access the instance
    // shape so we can share geometry.  We don't want to harden the instance.
    // For non-instanced geometry, this should return the same as
    // impl->objectPath().
    std::string	ipath = impl->object().getSourcePath();
    if (ipath.length())
    {
	options.setOptionS("o", ipath);
    }
    else
    {
	// If there was an error accessing the instance path, just return the
	// object path verbatim.
	options.setOptionS("o", impl->objectPath());
    }
    if (SYSalmostEqual(impl->frame(), 0) || impl->isConstant())
	options.setOptionF("t", 0.0);
    else
	options.setOptionF("t", impl->frame());
    options.setOptionB("x", impl->useTransform());
    options.setOptionB("v", impl->useVisibility());

#if 0
    // Grab primitive attributes
    GT_AttributeListHandle	attribs = getInstanceAttributes();
    if (attribs)
    {
	//attribs->dumpList("Alembic", false);
	UT_WorkBuffer	oname;
	for (int i = 0; i < attribs->entries(); ++i)
	{
	    const GT_DataArrayHandle	&data = attribs->get(i);
	    const char			*aname = attribs->getName(i);
	    oname.sprintf("a:%s", aname);
	    if (GTisFloat(data->getStorage()))
		addFloat(options, oname.buffer(), data);
	    else if (GTisInteger(data->getStorage()))
	    {
		if (!strcmp(aname, "__primitive_id") ||
		    !strcmp(aname, "__vertex_id"))
		{
		    continue;
		}
		addInt(options, oname.buffer(), data);
	    }
	    else if (GTisString(data->getStorage()))
	    {
		if (!strcmp(aname, "shop_materialpath") ||
		    !strcmp(aname, "material_override") ||
		    !strcmp(aname, "varmap") ||
		    !strcmp(aname, "property_map"))
		{
		    continue;
		}
		addString(options, oname.buffer(), data);
	    }
	    else UT_ASSERT(0);
	}
    }
#endif
    return true;
}

bool
GABC_PackedAlembic::isVisible()
{
    if(!myAnimVis)
	return myVisibleConst;
    
    bool visible = true;
    if(!getCachedVisibility(visible))
    {
	const GABC_PackedImpl	*impl = 
	    UTverify_cast<const GABC_PackedImpl *>(getImplementation());
	
	visible = impl->visibleGT();
	cacheVisibility(visible);
    }

    return visible;
}

GT_TransformHandle
GABC_PackedAlembic::getLocalTransform() const
{
    UT_Matrix4D m;
    auto impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    if(impl->getLocalTransform(m))
	return new GT_Transform(&m, 1);

    return GT_TransformHandle();
}

GT_PrimitiveHandle
GABC_PackedAlembic::getInstanceGeometry(const GT_RefineParms *p,
					bool ignore_visibility) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    GT_PrimitiveHandle prim = impl->instanceGT(ignore_visibility);

    if(!prim && GT_GEOPrimPacked::useViewportLOD(p))
    {
	// Placeholder so the viewport can at least draw bboxes and decorations.
	GT_AttributeListHandle null;
	prim = new GT_PrimPointMesh(null, null);
    }

    return prim;
}

GT_TransformHandle
GABC_PackedAlembic::getInstanceTransform() const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return impl->xformGT();
}

GT_PrimitiveHandle
GABC_PackedAlembic::getBoxGeometry(const GT_RefineParms *) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return impl->boxGT();
}

GT_PrimitiveHandle
GABC_PackedAlembic::getCentroidGeometry(const GT_RefineParms *) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return impl->centroidGT();
}

bool
GABC_PackedAlembic::refine(GT_Refine &refiner,
			   const GT_RefineParms *parms) const
{
    if(gabcExprUseArchivePrims() == 0)
	return GT_GEOPrimPacked::refine(refiner,parms);
    
    GT_PrimitiveHandle	prim;
    UT_BoundingBox	box;
    bool		xform = true;

    int	lod = GT_RefineParms::getOverridePackedViewportLOD(parms);
    if( !(lod >= 0 && lod < GEO_VIEWPORT_NUM_MODES))
    {
	//TODO:
	// if (GT_RefineParms::getPackedViewportLOD(parms))
	//     lod = viewportLOD();
	// else 
	    lod = GEO_VIEWPORT_FULL;
    }
    // Otherwise return full refinement
    switch (lod)
    {
	case GEO_VIEWPORT_HIDDEN:
	    return false;
	    
	case GEO_VIEWPORT_CENTROID:
	    prim = getCentroidGeometry(parms);
	    break;
	case GEO_VIEWPORT_BOX:
	    prim = getBoxGeometry(parms);
	    break;
	case GEO_VIEWPORT_POINTS:
	    prim = getPointCloud(parms, xform);
	    break;
	case GEO_VIEWPORT_INVALID_MODE:
	case GEO_VIEWPORT_FULL:
	case GEO_VIEWPORT_SUBDIVISION:
	case GEO_VIEWPORT_DEFORM:
	    prim = getFullGeometry(parms, xform);
	    break;
    }
    
    if(!prim && GT_GEOPrimPacked::useViewportLOD(parms))
    {
	// Placeholder so the viewport can at least draw bboxes and decorations.
	GT_AttributeListHandle null;
	prim = new GT_PrimPointMesh(null, null);
    }

    
    refiner.addPrimitive(prim);
    return true;
}

bool 
GABC_PackedAlembic::getCachedGeometry(GT_PrimitiveHandle &ph) const
{
    auto impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    const GABC_IObject	&o = impl->object();
    const int64 version = 0;
    UT_StringHolder cache_name;

    UT_StringHolder arch;
    GT_PackedGeoCache::buildAlembicArchiveName(arch, o.archive()->filenames());

    GT_PackedGeoCache::buildAlembicName(cache_name,
					o.getSourcePath().c_str(),
					arch.c_str(),
					impl->frame());
    ph = GT_PackedGeoCache::findInstance(cache_name, version,
					   impl->currentLoadStyle(), nullptr);
    return ph != nullptr;
}


void
GABC_PackedAlembic::cacheTransform(const GT_TransformHandle &th)
{
    auto impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    UT_Matrix4D mat;
    th->getMatrix(mat);
    myCache.cacheTransform(impl->frame(), mat );
}

bool
GABC_PackedAlembic::getCachedTransform(GT_TransformHandle &th) const
{
    auto impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    UT_Matrix4D mat;
    if(myCache.getTransform(impl->frame(), mat))
    {
	th = new GT_Transform(&mat, 1);
	return true;
    }
    return false;
}

void
GABC_PackedAlembic::cacheVisibility(bool visible)
{
    auto impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    myCache.cacheVisibility(impl->frame(), visible);
}

bool
GABC_PackedAlembic::getCachedVisibility(bool &visible) const
{
    auto impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return myCache.getVisibility(impl->frame(), visible);
}

// -------------------------------------------------------------------------

namespace
{
class gabc_BucketAlembics
{
public:
    gabc_BucketAlembics(const GU_Detail *dtl,
			const GA_OffsetArray &offsets,
			UT_StringArray &objects)
	: myDetail(dtl),
	  myOffsets(offsets),
	  myObjects(objects)
	{}

    gabc_BucketAlembics(const gabc_BucketAlembics &src, UT_Split)
	: myDetail(src.myDetail),
	  myOffsets(src.myOffsets),
	  myObjects(src.myObjects)
	{}

    void	operator()(const UT_BlockedRange<exint> &range)
	{
	    for(exint i = range.begin(); i != range.end(); ++i)
	    {
		const GU_PrimPacked *prim = static_cast<const GU_PrimPacked *>
		    (myDetail->getPrimitive(myOffsets(i)));
		auto impl= UTverify_cast<const GABC_PackedImpl*>
		    (prim->implementation());

		UT_StringHolder path = impl->object().getSourcePath();
		GEO_AnimationType anim = impl->animationType();
		if(anim > GEO_ANIMATION_TRANSFORM)
		{
		    UT_WorkBuffer frame; 
		    frame.sprintf("[%f]", impl->frame());
		    path += frame.buffer();
		}

		myBuckets[ path ].append(prim);
		myObjects[i] = impl->object().getFullName();
	    }
	}
  
    void	join(const gabc_BucketAlembics &other)
	{
	    for(auto itr : other.myBuckets)
	    {
		auto entry = myBuckets.find(itr.first);
		if(entry == myBuckets.end())
		{
		    auto &new_array = myBuckets[itr.first];
		    for(auto p : itr.second)
			new_array.append(p);
		}
		else
		{
		    for(auto p : itr.second)
			entry->second.append(p);
		}
	    }
	}

    const UT_StringMap< UT_Array<const GU_PrimPacked *> > &buckets() const
	{
	    return myBuckets;
	}

private:
    const GU_Detail *myDetail;
    const GA_OffsetArray &myOffsets;
    UT_StringMap< UT_Array<const GU_PrimPacked *> > myBuckets;
    UT_StringArray &myObjects;
};

class gabc_GenerateMeshes
{
public:
    gabc_GenerateMeshes(const UT_Array<GT_PrimitiveHandle> &alem_meshes,
			const GT_RefineParms *parms)
	: myAlembics(alem_meshes),
	  myParms(parms)
	{}

    gabc_GenerateMeshes(const gabc_GenerateMeshes &src, UT_Split)
	: myAlembics(src.myAlembics),
	  myParms(src.myParms)
	{}
    
    void operator()(const UT_BlockedRange<exint> &range)
	{
	    for(int i = range.begin(); i< range.end(); i++)
	    {
		auto pack =
		    UTverify_cast<GABC_PackedAlembic *>(myAlembics(i).get());

		// ignore visibility if visibility is animated, we want the mesh
		bool ignore_vis = pack->visibilityAnimated();
		
		// This is the expensive part, the reason this is threaded.
		GT_PrimitiveHandle geoh = pack->getInstanceGeometry(myParms,
								    ignore_vis);
		if(geoh)
		{
		    if(geoh->getPrimitiveType() == GT_PRIM_POLYGON_MESH ||
		       geoh->getPrimitiveType() == GT_PRIM_SUBDIVISION_MESH ||
		       geoh->getPrimitiveType() == GT_PRIM_POLYGON)
		    {
			myMeshes.append(myAlembics(i));
		    }
		    else
			myOthers.append(myAlembics(i));
		}
		else
		    myOthers.append(myAlembics(i));
	    }
	}
    void join(const gabc_GenerateMeshes &other)
	{
	    myMeshes.concat(other.myMeshes);
	    myOthers.concat(other.myOthers);
	}

    UT_Array<GT_PrimitiveHandle>	&meshes() { return myMeshes; }
    UT_Array<GT_PrimitiveHandle>	&prims() { return myOthers; }

private:
    const UT_Array<GT_PrimitiveHandle> &myAlembics;
    UT_Array<GT_PrimitiveHandle>	myMeshes, myOthers;
    const GT_RefineParms	       *myParms;
};

void combineMeshes(const UT_Array<GT_PrimitiveHandle> &meshes,
		   UT_IntArray &combined,
		   UT_Array<GT_PrimitiveHandle> &shapes,
		   UT_Array<GT_PrimitiveHandle> &const_shapes,
		   UT_Array<GT_PrimitiveHandle> &transform_shapes,
		   GT_PrimitiveHandle &mesh,
		   int max_faces, int max_meshes, int max_mesh_faces)
{
    if(combined.entries() > 1)
    {
	UT_Array< GT_CatPolygonMesh > const_merge_meshes;
	UT_Array< GT_CatPolygonMesh > anim_merge_meshes;
	UT_Array<UT_Array<GT_PrimitiveHandle> > anim_merge_packs;
	UT_Array<SYS_HashType> const_merge_ids;
	UT_Array<SYS_HashType> anim_merge_ids;
	
	for(auto idx : combined)
	{
	    auto pack = UTverify_cast<GABC_PackedAlembic *>(meshes(idx).get());	
	    if(pack->getCachedGeometry(mesh))
	    {
		if(mesh->getPrimitiveType() != GT_PRIM_POLYGON_MESH &&
		   mesh->getPrimitiveType() != GT_PRIM_SUBDIVISION_MESH &&
		   mesh->getPrimitiveType() != GT_PRIM_POLYGON)
		{
		    shapes.append(meshes(idx));
		    continue;
		}
		
		int64 pid = 0;
		pack->getUniqueID(pid);
		
		if(pack->animationType() == GEO_ANIMATION_TRANSFORM ||
		   pack->visibilityAnimated())
		{
		    const GT_PrimPolygonMesh *cmesh =
			UTverify_cast<GT_PrimPolygonMesh *>(mesh.get());

		    GT_AttributeListHandle detail_attribs =
			cmesh->getDetailAttributes();

		    GT_DataArrayHandle ptidx;
		    auto trans_index =
		     new GT_DAConstantValue<uint8>(1,uint8(1),1);
		    ptidx = trans_index;

		    if(detail_attribs)
		    {
			detail_attribs = detail_attribs->addAttribute(
			    "PrimTransformIndex", ptidx, true);
		    }
		    else
		    {
			detail_attribs =
			    GT_AttributeList::createAttributeList(
				"PrimTransformIndex", trans_index, NULL);
		    }

		    auto vis_index = 
		     new GT_DAConstantValue<uint8>(1,uint8(1),1);
		    ptidx = vis_index;
		    detail_attribs = detail_attribs->addAttribute(
			"PrimVisibilityIndex", ptidx, true);
		    
		    GT_PrimitiveHandle pmesh =
			new GT_PrimPolygonMesh(*cmesh,
					       cmesh->getPointAttributes(),
					       cmesh->getVertexAttributes(),
					       cmesh->getUniformAttributes(),
					       detail_attribs);
		     
		    bool found_match = false;
		    GT_CatPolygonMesh::gt_CatAppendReason reason =
			GT_CatPolygonMesh::APPEND_OK;
		    for(int i=0; i<anim_merge_meshes.entries(); i++)
			if(anim_merge_meshes(i).canAppend(pmesh, &reason))
			{
			    const int tidx = anim_merge_packs(i).entries();

			    if(pack->animationType() == GEO_ANIMATION_TRANSFORM)
				trans_index->set(uint8(tidx+1));
			    if(pack->visibilityAnimated())
				vis_index->set(uint8(tidx+1));
			    
			    anim_merge_packs(i).append(meshes(idx));

			    SYShashCombine<int64>(anim_merge_ids(i), pid);
			    
			    anim_merge_meshes(i).append(pmesh);
			    
			    found_match = true;
			    break;
			}

		    if(!found_match)
		    {
			if(reason == GT_CatPolygonMesh::APPEND_MESH_TOO_LARGE ||
			   reason == GT_CatPolygonMesh::APPEND_UNSUPPORTED_TYPE)
			{
			    transform_shapes.append(meshes(idx));
			}
			else if(reason != GT_CatPolygonMesh::APPEND_NO_FACES)
			{
			    anim_merge_meshes.append();
			    auto &cm = anim_merge_meshes.last();
			    cm.setTotalFaceCount(max_faces);
			    cm.setMaxMeshCount(max_meshes);
			    cm.setMaxMeshFaceCount(max_mesh_faces);
			    cm.append(pmesh);
			    anim_merge_packs.append();
			    anim_merge_packs.last().append(meshes(idx));
			    anim_merge_ids.append(SYS_HashType(pid));
			}
		    }
		}
		else // no animation
		{
		    GT_TransformHandle trh = pack->getInstanceTransform();
		    GT_CatPolygonMesh::gt_CatAppendReason reason =
			GT_CatPolygonMesh::APPEND_OK;
		    bool found_match = false;
		    for(int i=0; i<const_merge_meshes.entries(); i++)
			if(const_merge_meshes(i).canAppend(mesh, &reason))
			{
			    const_merge_meshes(i).append(mesh, trh);
			    SYShashCombine<int64>(const_merge_ids(i), pid);
			    found_match = true;
			    break;
			}

		    if(!found_match)
		    {
			if(reason == GT_CatPolygonMesh::APPEND_MESH_TOO_LARGE ||
			   reason == GT_CatPolygonMesh::APPEND_UNSUPPORTED_TYPE)
			{
			    const_shapes.append(meshes(idx));
			}
			else if(reason != GT_CatPolygonMesh::APPEND_NO_FACES)
			{
			    const_merge_meshes.append();
			    auto &cm = const_merge_meshes.last();
			    cm.setTotalFaceCount(max_faces);
			    cm.setMaxMeshCount(max_meshes);
			    cm.setMaxMeshFaceCount(max_mesh_faces);
			    cm.append(mesh, trh);
			    const_merge_ids.append(SYS_HashType(pid));
			}
		    }
		}
	    }
	}

	for(int i=0; i<anim_merge_meshes.entries(); i++)
	{
	    GT_PrimitiveHandle merged_mesh =  anim_merge_meshes(i).result();
	    if(merged_mesh)
	    {
		const int64 id = anim_merge_ids(i);

		// build a scratch array for the matrices
		const int prim_count=anim_merge_meshes(i).getNumSourceMeshes();
		GT_DataArrayHandle trans_array =
		  new GT_Real32Array(prim_count*4,4);
		GT_DataArrayHandle vis_array = 
		  new GT_UInt8Array(prim_count,1);

		auto mesh =
		    UTverify_cast<GT_PrimPolygonMesh *>(merged_mesh.get());
		GT_AttributeListHandle detail;

		if(merged_mesh->getDetailAttributes())
		{
		    detail = merged_mesh->getDetailAttributes()->
			addAttribute("PrimTransform",trans_array, true);
		    detail = detail->
			addAttribute("PrimVisibility", vis_array, true);
		}
		else
		{
		    detail =
			GT_AttributeList::createAttributeList("PrimTransform",
							      trans_array.get(),
							      "PrimVisibility",
							      vis_array.get(),
							      NULL);
		}
		
		merged_mesh = new GT_PrimPolygonMesh(*mesh,
						 mesh->getPointAttributes(),
						 mesh->getVertexAttributes(),
						 mesh->getUniformAttributes(),
						 detail);
		
		// UTdebugPrint("Mesh size",UTverify_cast<GT_PrimPolygonMesh*>
		// 		 (merged_mesh.get())->getFaceCount());
		shapes.append(new GT_PackedAlembicMesh(merged_mesh, id,
						       anim_merge_packs(i)));
	    }
	}
	
	for(int i=0; i<const_merge_meshes.entries(); i++)
	{
	    GT_PrimitiveHandle merged_mesh =  const_merge_meshes(i).result();
	    if(merged_mesh)
	    {
		const int64 id = const_merge_ids(i);
		// UTdebugPrint("Mesh size",UTverify_cast<GT_PrimPolygonMesh*>
		// 		 (merged_mesh.get())->getFaceCount());
		shapes.append(new GT_PackedAlembicMesh(merged_mesh, id));
	    }
	}
    }
    else if(combined.entries() == 1)
    {
	GT_PrimitiveHandle mesh = meshes(combined(0));
	auto pack = UTverify_cast<GABC_PackedAlembic *>(mesh.get());
	if(pack->animationType() == GEO_ANIMATION_TRANSFORM ||
	   pack->visibilityAnimated())
	{
	    transform_shapes.append(mesh);
	}
	else
	    const_shapes.append(mesh);
    }
    else if(combined.entries() == 0 && mesh)
    {
	auto pack = UTverify_cast<GABC_PackedAlembic *>(mesh.get());
	if(pack->animationType() == GEO_ANIMATION_TRANSFORM ||
	   pack->visibilityAnimated())
	{
	    transform_shapes.append(mesh);
	}
	else
	    const_shapes.append(mesh);
    }
    combined.entries(0);
}

} // end anon namespace


GABC_PackedArchive::GABC_PackedArchive(const UT_StringHolder &arch,
				       const GT_GEODetailListHandle &dlist,
				       const GABC_IArchivePtr &archive)
    : GT_PackedAlembicArchive(arch, dlist),
      myArchive(archive)
{
}

//#define TIME_BUCKETTING
#ifdef TIME_BUCKETTING
#include <UT/UT_StopWatch.h>
#define START_TIMER()	timer.start()
#define PRINT_TIMER(x)  UTdebugPrint(x, timer.getTime())
#else
#define START_TIMER()	
#define PRINT_TIMER(x)  
#endif

#define USE_PRELOAD_STREAMS
#define DEFAULT_NUM_STREAMS 4

// uncomment to debug a specific combined mesh, and set to the mesh index (0-N).
//#define DEBUG_COMBINED_SHAPES 0

bool
GABC_PackedArchive::bucketPrims(const GT_PackedAlembicArchive *prev_archive,
				const GT_RefineParms *parms,
				bool force_update)
{
    myAlembicVersion = GT_PackedGeoCache::getAlembicVersion(myName.c_str());
    
    if(!force_update && prev_archive && archiveMatch(prev_archive))
	return false;

    int num_streams = 1; 
#ifdef USE_PRELOAD_STREAMS
    if(myArchive->isOgawa() &&
       !GT_PackedGeoCache::hasAlembicArchive(myName.c_str()))
    {
	num_streams = DEFAULT_NUM_STREAMS;
	int env = UT_EnvControl::getInt(ENV_HOUDINI_ALEMBIC_OGAWA_STREAMS);
	if(env > 0)
	{
	    num_streams = env;
	    if(num_streams > 1)
	    {
		if(num_streams > myAlembicOffsets.entries())
		    num_streams = myAlembicOffsets.entries();
		else if(num_streams*2 > myAlembicOffsets.entries())
		    num_streams = myAlembicOffsets.entries()/2;
	    }
	}
	else
	{
	    int nprocs = UT_Thread::getNumProcessors();
	    int chunks = SYSmax(1, myAlembicOffsets.entries() / nprocs);
	    num_streams = SYSmin(myAlembicOffsets.entries() / chunks,
				 nprocs);
	}
	// UTdebugPrint("Loading OGAWA using ",
	//  	     num_streams, myAlembicOffsets.entries());
	myArchive->reopenStream(num_streams);
    }
#endif

#ifdef TIME_BUCKETTING
    UT_StopWatch timer;
    timer.start();
#endif
    myAlembicObjects.setSize( myAlembicOffsets.entries() );
    // Sort the collected Alembic primitives into buckets for instancing.
    GU_DetailHandleAutoReadLock gdplock(myDetailList->getGeometry(0));
    gabc_BucketAlembics btask(gdplock.getGdp(),
			      myAlembicOffsets,
			      myAlembicObjects);

    UTparallelReduce(UT_BlockedRange<exint>(0, myAlembicOffsets.entries()),
		     btask);
    
    PRINT_TIMER("bucket primitives");
    START_TIMER();

    // Create GT primitives for the buckets
    UT_StringMap<GT_PrimitiveHandle> prims;
    for(auto name : myAlembicObjects)
	prims[name] = nullptr;
    
    gabc_CreateInstanceGeos gttask(btask.buckets(), myDetailList, parms, prims,
				   true);

    UTparallelFor(UT_BlockedRange<exint>(0, btask.buckets().size()), gttask);

    PRINT_TIMER("Create primitives");

    UT_Array<GT_PrimitiveHandle> alem_meshes;
    int ccount = 0;
    // Sort the primitives into buckets based on animation.
    for(auto name : myAlembicObjects)
    {
	GT_PrimitiveHandle p = prims[name];
	GEO_AnimationType type = GEO_ANIMATION_INVALID;
	
	GABC_PackedAlembic *single =dynamic_cast<GABC_PackedAlembic *>(p.get());
	if(single)
	{
	    type = single->animationType();
	    single->setAlembicVersion(myAlembicVersion);
	}
	else
	{
	    // Currently only have one other GT type produced.
	    GABC_PackedInstance *inst =
		UTverify_cast<GABC_PackedInstance *>(p.get());
	    type = inst->animationType();
	    inst->setAlembicVersion(myAlembicVersion);
	}

	
	if(type <= GEO_ANIMATION_TRANSFORM)
	{
	    if(single && gabcExprUseArchivePrims() == 3)
		alem_meshes.append(p);
	    else if(type == GEO_ANIMATION_CONSTANT)
		myConstShapes.append(p);
	    else
		myTransformShapes.append(p);
	}
	else
	    myDeformShapes.append(p);
    }

    if(alem_meshes.entries())
    {
	gabc_GenerateMeshes task(alem_meshes, parms);
	UT_BlockedRange<exint> range(0,alem_meshes.entries());

	START_TIMER();
	
#ifdef USE_PRELOAD_STREAMS
	// Currently this is slower for Ogawa unless multiple streams are used.
	// Will need to
	// open the archive with multiple handles to be fast.
	// HDF5 is not threadsafe and should always be single threaded.
	if(num_streams > 1)
	{
	    UT_ASSERT(myArchive->isOgawa());
	    int grain_size = SYSmax(alem_meshes.entries() /num_streams, 1);

	    //UTdebugPrint("Threaded load", grain_size, alem_meshes.entries());
	    UTparallelReduce(range, task, 1, grain_size);
	}
	else
#endif
	{
	    UTserialReduce(range, task);
	}

	PRINT_TIMER("Geometry load time");

	myConstShapes.concat(task.prims());

	UT_Array<GT_PrimitiveHandle> &meshes = task.meshes();
	if(meshes.entries() > 1)
	{
	    int index = 0;
	    UT_IntArray combined;
	    const int max_mesh_size = parms?parms->getMaxPolyMeshSize():1000000;
	    const int max_prims = 255;
	    const int max_faces = 50000;

	    for(auto mesh : meshes)
	    {
		auto pack = UTverify_cast<GABC_PackedAlembic *>(mesh.get());
		GT_PrimitiveHandle meshh;
		if(pack->getCachedGeometry(meshh))
		    combined.append(index);
		else
		{
		    if(pack->animationType() == GEO_ANIMATION_CONSTANT)
			myConstShapes.append(pack);
		    else
			myTransformShapes.append(pack);
		}

		index++;
	    }
		    
	    if(combined.entries() > 0)
	    {
		GT_PrimitiveHandle null_shape;
		combineMeshes(meshes, combined, myCombinedShapes, myConstShapes,
			      myTransformShapes, null_shape,
			      max_mesh_size, max_prims, max_faces);
		ccount++;
	    }
	}
	else
	    myConstShapes.concat(meshes);
    }
    
    if(num_streams != 1)
	myArchive->reopenStream(1);

#ifdef DEBUG_COMBINED_SHAPES
    if(myCombinedShapes.isValidIndex(DEBUG_COMBINED_SHAPES))
    {
	GT_PrimitiveHandle comb = myCombinedShapes(DEBUG_COMBINED_SHAPES);
	myCombinedShapes.clear();
	myCombinedShapes.append(comb);
    }
    else
	myCombinedShapes.clear();
#endif

#if 0
    UTdebugPrint("# const    = ", myConstShapes.entries());
    UTdebugPrint("# combined = ", myCombinedShapes.entries());
    UTdebugPrint("# anim     = ", myTransformShapes.entries());
    UTdebugPrint("# deform   = ", myDeformShapes.entries());
#endif		  
    return true;
}

bool
GABC_PackedArchive::archiveMatch(const GT_PackedAlembicArchive *archive) const
{
    // This checks if a new archive is compatible with another, usually for
    // in-place update purposes.
    auto &offsets = archive->getAlembicOffsets();
    auto &objects = archive->getAlembicObjects();

    // Archives are not the same, or don't have the same contents.
    if(myName != archive->archiveName())
    {
	//UTdebugPrint("Name", myName, archive->archiveName());
	return false;
    }
    if(myAlembicOffsets.entries() != offsets.entries())
    {
	// UTdebugPrint("offsets differ", myAlembicOffsets.entries(),
	// 	     offsets.entries());
	return false;
    }

    // Archive was reloaded
    if(archive && archive->getAlembicVersion() != myAlembicVersion)
    {
	//UTdebugPrint("versions differ");
	return false;
    }

    GU_DetailHandleAutoReadLock this_lock(myDetailList->getGeometry(0));
    const GU_Detail *this_dtl = this_lock.getGdp();

    for(int i=0; i<myAlembicOffsets.entries(); i++)
    {
	GA_Offset src_off = myAlembicOffsets(i);

	// If offsets are not the same, in-place updates can't occur.
	if(src_off != offsets(i))
	{
	    //UTdebugPrint("Offset mismatch", src_off);
	    return false;
	}
	
	const GU_PrimPacked *this_prim = static_cast<const GU_PrimPacked *>
	    (this_dtl->getPrimitive(src_off));
	auto this_impl = UTverify_cast<const GABC_PackedImpl*>
	    (this_prim->implementation());

	// If the objects are not the same, inplace updates can't occur.
	if(objects(i) != this_impl->object().getFullName().c_str())
	{
	    //UTdebugPrint("Obj mismatch");
	    return false;
	}
    }

    return true;
}

// -------------------------------------------------------------------------

GABC_PackedInstance::GABC_PackedInstance()
    : GT_PrimInstance(),
      myAnimType(GEO_ANIMATION_INVALID)
{
}

GABC_PackedInstance::GABC_PackedInstance(const GABC_PackedInstance &src)
    : GT_PrimInstance(src),
      myAnimType(src.myAnimType)
{
    myCache.entries( entries() );
    
    if(myAnimType >= GEO_ANIMATION_TRANSFORM)
    {
	for(int i=0; i<myCache.entries(); i++)
	{
	    myCache(i).setVisibilityAnimated(true);
	    myCache(i).setTransformAnimated(true);
	}
    }
}

GABC_PackedInstance::GABC_PackedInstance(
    const GT_PrimitiveHandle &geometry,
    const GT_TransformArrayHandle &transforms,
    GEO_AnimationType anim,
    const GT_GEOOffsetList &packed_prim_offsets,
    const GT_AttributeListHandle &uniform,
    const GT_AttributeListHandle &detail,
    const GT_GEODetailListHandle &source)
    : GT_PrimInstance(geometry, transforms, packed_prim_offsets,
		      uniform, detail, source),
      myAnimType(anim)
{
    myCache.entries( entries() );
    
    if(myAnimType >= GEO_ANIMATION_TRANSFORM)
    {
	for(int i=0; i<myCache.entries(); i++)
	{
	    myCache(i).setVisibilityAnimated(true);
	    myCache(i).setTransformAnimated(true);
	}
    }
}

GABC_PackedInstance::~GABC_PackedInstance()
{
}

bool
GABC_PackedInstance::updateGeoPrim(const GU_ConstDetailHandle &dtl,
				   const GT_RefineParms &refine)
{
    //static UT_Vector3 theZero(0.0);
    
    bool updated = GT_Primitive::updateGeoPrim(dtl, refine);

    if(myUniform)
    {
	const GT_DataArrayHandle &prim = myUniform->get("__primitive_id");
	if(prim)
	{
	    // Geometry
	    GU_DetailHandleAutoReadLock	gdp(dtl);
	    GA_Offset off = GA_Offset(prim->getI64(0));
	    auto pprim = UTverify_cast<const GU_PrimPacked *>
		(gdp->getPrimitive(off));
	    if(pprim)
	    {
		// Update geometry
		GT_PrimitiveHandle geo =
		    UTverify_cast<const GABC_PackedImpl *>(
			pprim->implementation())->instanceGT(true);
		if(geo != myGeometry)
		{
		    myGeometry = geo;
		    updated = true;
		}
	    }
	    
	    if(myTransforms)
	    {
		const int n = prim->entries();
		GU_DetailHandleAutoReadLock	gdp(dtl);

		if(n != myTransforms->entries())
		    updated = true;
		
		myTransforms->setEntries(n);
	    
		for(int i=0; i<n; i++)
		{
		    const GA_Offset off = GA_Offset(prim->getI64(i));
		    auto pprim = UTverify_cast<const GU_PrimPacked *>
			(gdp->getPrimitive(off));

		    if(pprim)
		    {
			UT_Matrix4D transform;
			auto pimpl = UTverify_cast<const GABC_PackedImpl *>(
			    pprim->implementation());

			pimpl->setViewportCache(&myCache(i));
			pprim->getFullTransform4(transform);
			pimpl->setViewportCache(nullptr);
			
			UT_Matrix4D old;
			myTransforms->get(i)->getMatrix(old);
			if(old != transform)
			{
			    updated = true;
			    myTransforms->
				set(i, new GT_Transform(&transform, 1));
			}
		    }
		}
	    }
	    // Update Visibility
	    GT_DataArrayHandle lod = myUniform->get("__view_lod");
	    if(lod)
	    {
		auto loda = UTverify_cast<GT_DANumeric<int> *>(lod.get());
		int lod_mask = 0;
		
		for(int i=0; i<prim->entries(); i++)
		{
		    int lod = GEO_VIEWPORT_HIDDEN;

		    auto pprim = UTverify_cast<const GU_PrimPacked *>
			(gdp->getPrimitive(off));
		    if(pprim && pprim->viewportLOD() != GEO_VIEWPORT_HIDDEN)
		    {
			bool visible = false;
			auto pimpl = UTverify_cast<const GABC_PackedImpl *>(
			    pprim->implementation());
			const fpreal t = pimpl->frame();

			if(!myCache(i).getVisibility(t, visible))
			{
			    visible = pimpl->useVisibility() ? 
				      pimpl->visibleGT() : true;
			    
			    myCache(i).cacheVisibility(t, visible);
			}
		    
			lod = visible ? pprim->viewportLOD()
				      : GEO_VIEWPORT_HIDDEN;
			if(lod > GEO_VIEWPORT_FULL ||
			   lod == GEO_VIEWPORT_INVALID_MODE)
			    lod = GEO_VIEWPORT_FULL;
		    }

		    if(loda->getI32(i) != lod)
			updated = true;
		    
		    loda->set(lod, i);
		    lod_mask |= (1<<lod);
		}
		
		GT_DataArrayHandle lodm = myUniform->get("__view_lod_mask");
		if(lodm)
		{
		    auto lodma = UTverify_cast<GT_DANumeric<int> *>(lodm.get());
		    lodma->set(lod_mask, 0);
		}
	    }
	}
    }
    return updated;
}

// -------------------------------------------------------------------------

GABC_CollectPacked::~GABC_CollectPacked()
{
}

GT_GEOPrimCollectData *
GABC_CollectPacked::beginCollecting(const GT_GEODetailListHandle &geometry,
				    const GT_RefineParms *parms) const
{
    return new CollectData(geometry, parms);
}

GT_PrimitiveHandle
GABC_CollectPacked::collect(const GT_GEODetailListHandle &geo,
			    const GEO_Primitive *const* prim,
			    int nsegments, GT_GEOPrimCollectData *data) const
{
    CollectData		*collector = data->asPointer<CollectData>();
    const GU_PrimPacked *pack = UTverify_cast<const GU_PrimPacked *>(prim[0]);
    if (!collector->append(*pack))
    {
	GT_DataArrayHandle nullh;
	return GT_PrimitiveHandle(new GABC_PackedAlembic(geo->getGeometry(0),
							 pack, nullh, nullh));
    }
    return GT_PrimitiveHandle();
}

GT_PrimitiveHandle
GABC_CollectPacked::endCollecting(const GT_GEODetailListHandle &geometry,
				  GT_GEOPrimCollectData *data) const
{
    CollectData			*collector = data->asPointer<CollectData>();
    return collector->finish();
}
