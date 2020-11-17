/*
 * Copyright (c) 2020
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
#include "GABC_IArchive.h"
#include <GT/GT_CatPolygonMesh.h>
#include <GT/GT_DAConstantValue.h>
#include <GT/GT_DAIndirect.h>
#include <GT/GT_DANumeric.h>
#include <GT/GT_GEOAttributeFilter.h>
#include <GT/GT_GEOPrimCollectBoxes.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_Names.h>
#include <GT/GT_PackedGeoCache.h>
#include <GT/GT_PrimCollect.h>
#include <GT/GT_PrimInstance.h>
#include <GT/GT_PrimPointMesh.h>
#include <GT/GT_PrimPolygonMesh.h>
#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_TransformArray.h>
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

void gabcViewportObjName(const GABC_PackedImpl *impl,
			 UT_StringHolder &path)
{
    path = impl->object().getSourcePath();
    
    GEO_AnimationType anim = impl->animationType();
    if(anim > GEO_ANIMATION_TRANSFORM)
    {
	// Geometry is deforming.
	UT_WorkBuffer frame; 
	frame.sprintf("[%f]", impl->frame());
	path += frame.buffer();
    }
}

class gabc_CreateInstanceGeos
{
public:
    // Mantra & other non-viewport uses 
    gabc_CreateInstanceGeos(
	    const UT_StringMap< UT_Array<const GU_PrimPacked*> > &geo,
	    const GT_GEODetailListHandle &geometry,
	    UT_Array<GT_PrimitiveHandle> &gt_prims)
	: myGeos(geo), myGeometry(geometry)
	, myRefineParms(nullptr)
	, myPrims(nullptr)
	, myFlatPrims(&gt_prims)
	, myViewportProcessing(false)
	{}

    // Viewport constructor
    gabc_CreateInstanceGeos(
	    const UT_StringMap< UT_Array<const GU_PrimPacked*> > &geo,
	    const GT_GEODetailListHandle &geometry,
	    const GT_RefineParms *ref_parms,
	    UT_StringMap<GT_PrimitiveHandle> &gt_prims)
      : myGeos(geo)
      , myGeometry(geometry)
      , myRefineParms(ref_parms)
      , myPrims(&gt_prims)
      , myFlatPrims(nullptr)
      , myViewportProcessing(true)
	{}
    
    void	operator()(const UT_BlockedRange<exint> &range) const
	{
	    if(myViewportProcessing)
		processViewport(range);
	    else
		process(range);
	}

    void process(const UT_BlockedRange<exint> &range) const
	{
	    // Simple process loop for mantra and other non-viewport uses
	    GT_GEOAttributeFilter filter;
	    int index = 0;
	    for(auto itr = myGeos.begin(); itr != myGeos.end(); ++itr, index++)
	    {
		if(index < range.begin())
		    continue;
		if(index == range.end())
		    break;
		
		auto impl= UTverify_cast<const GABC_PackedImpl*>
		    (itr->second(0)->sharedImplementation());

		// No need to pack if there is only a single instance.
		if(itr->second.entries() == 1)
		{
		    (*myFlatPrims)(index) =
			new GT_GEOPrimPacked(myGeometry->getGeometry(0),
					     itr->second(0));
		}
		else
		{
		    GT_PrimitiveHandle geo = impl->instanceGT();
		    GT_TransformArray *xforms = new GT_TransformArray;
		    GT_TransformArrayHandle xformh = xforms;
		    GT_GEOOffsetList offsets, voffsets;
		    bool use_vertex = true;

		    for( auto pi : itr->second )
		    {
			UT_Matrix4D transform;
			pi->getFullTransform4(transform);
			xforms->append( new GT_Transform(&transform, 1) );
			offsets.append( pi->getMapOffset() );
			if(pi->getVertexCount() > 0)
			    voffsets.append(pi->getVertexOffset(0));
			else
			    use_vertex = false; // avoid if one inst has 0 verts
		    }
		    
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
		    
		    (*myFlatPrims)(index) =
			new GT_PrimInstance(geo, xformh, offsets,
					    prim_attribs, detail_attribs);
		}
	    }
	}
    
    void processViewport(const UT_BlockedRange<exint> &range) const
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
		    (itr->second(0)->sharedImplementation());

		const UT_StringHolder &path = itr->first;

		// No need to pack if there is only a single instance.
		if(itr->second.entries() == 1)
		{
		    GABC_PackedAlembic *packgt =
			new GABC_PackedAlembic(myGeometry->getGeometry(0),
					       itr->second(0),
					       mat_id, mat_remap, true);
	    
		    bool vis_anim;
		    impl->object().visibility(vis_anim, impl->frame(),true);

		    packgt->initVisAnim();
		    packgt->setAnimationType(impl->animationType());
		    packgt->setVisibilityAnimated(vis_anim);

		    UT_ASSERT(myPrims->find(path) != myPrims->end());
		    (*myPrims)[path] = packgt;
		    continue;
		}
		

		// Instanced geometry
		GT_PrimitiveHandle geo = impl->instanceGT(true);
		GT_TransformArray *xforms = new GT_TransformArray;
		GT_TransformArrayHandle xformh = xforms;
		GT_GEOOffsetList offsets, voffsets;
		bool use_vertex = true;

		UT_Array<GEO_ViewportLOD> lod;
		GEO_AnimationType anim_type = impl->animationType();
		
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
			(pi->sharedImplementation());
			
		    // check for an animated transform.
		    if(anim_type < GEO_ANIMATION_TRANSFORM)
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
		
		(*myPrims)[path] =
		    new GABC_PackedInstance(geo, xformh, anim_type,
					    offsets, prim_attribs,
					    detail_attribs,
					    GT_GEODetailListHandle());
	    }
	}

private:
    const UT_StringMap< UT_Array<const GU_PrimPacked *> > &myGeos;
    UT_StringMap<GT_PrimitiveHandle>		 *myPrims;
    UT_Array<GT_PrimitiveHandle>		 *myFlatPrims;
    const GT_GEODetailListHandle		  myGeometry;
    const GT_RefineParms			 *myRefineParms;
    const bool					  myViewportProcessing;
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
    { }
    ~CollectData() override {}

    bool	append(const GU_PrimPacked &prim)
    {
	auto impl= UTverify_cast<const GABC_PackedImpl*>(prim.sharedImplementation());
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

    // bucket by archive:prim into lists of instances (mantra & non-viewport)
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

    // only bucket by archive. will be bucketed later (Viewport)
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
						 impl->object().archive(),
						 myViewportArchives.size());
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

	    UT_Array<GT_PrimitiveHandle> prims;
	    prims.entries( myInstanceGeo.size() );
	    
	    UT_BlockedRange<exint> range(0, prims.entries());
	    gabc_CreateInstanceGeos task(myInstanceGeo, myGeometry, prims);
#if 1
	    UTparallelFor(range,  task, 1, 20);
#else
	    UTserialFor(range, task);
#endif
	    if(collect)
	    {
		for(const auto &p : prims)
		    collect->appendPrimitive(p);
	    }
	    else
		result = prims(0);

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

	// This ensures that the order that the archives were added in is
	// preserved.
	if(myViewportArchives.size())
	{
	    UT_Array<GABC_PackedArchive *> archives;
	    archives.entries(myViewportArchives.size());
	    for( auto p : myViewportArchives)
	    {
		auto aa = static_cast<GABC_PackedArchive*>(p.second.get());
		archives(aa->getIndex()) = aa;
	    }
	    for(auto p : archives)
		collect->appendPrimitive(p);
	}

	return collecth;
    }

private:
    const GT_GEODetailListHandle	myGeometry;
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
      myColorID(-2),
      myTransform(1)
{
}

GABC_PackedAlembic::GABC_PackedAlembic(const GABC_PackedAlembic &src)
    : GT_PackedAlembic(src),
      myColorID(src.myColorID),
      myTransform(src.myTransform)
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
            if(GAisValidLocalIntrinsic(fr))
            {
                float frame = 0;
                if(packed->getIntrinsic(fr, frame))
                {
                    if(!SYSisEqual(myFrame, frame))
                    {
                        myFrame = frame;
                        changed = true;
                    }
                }
            }

	    UT_Matrix4D transform;
	    auto pimpl = UTverify_cast<const GABC_PackedImpl *>(
		packed->sharedImplementation());

	    myCache.getTransform(pimpl, transform);
	    packed->multiplyByPrimTransform(transform);
	    if(myTransform != transform)
	    {
		myTransform = transform;
		changed = true;
	    }
	    UT_BoundingBox bbox;
	    if(pimpl->getBounds(bbox))
		GT_Util::addBBoxAttrib(bbox, myDetailAttribs);
	}
    }
    
    if(GT_PackedAlembic::updateGeoPrim(dtl, refine))
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

    if(changed)
	myHasChanged = true;
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
	    frame = impl->frame();
	
	if(myAnimType != GEO_ANIMATION_CONSTANT)
	    myCache.setTransformAnimated(true);

	bool vis = false;
	myVisibleConst = impl->visibleGT(&vis);
	myCache.setVisibilityAnimated(vis);
	myAnimVis = vis;

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
    return impl->pointGT(getPrim());
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
    if(!GT_RefineParms::getAlembicSkipInvisible(parms))
	load_style |= GABC_IObject::GABC_LOAD_IGNORE_VISIBILITY;

    GT_PrimitiveHandle prim = impl->fullGT(getPrim(), load_style);
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
		if (!strcmp(aname, GT_Names::primitive_id) ||
		    !strcmp(aname, GT_Names::vertex_id))
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
    getCachedVisibility(visible);
    return visible;
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
    return impl->xformGT(getPrim());
}

GT_PrimitiveHandle
GABC_PackedAlembic::getBoxGeometry(const GT_RefineParms *) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return impl->boxGT(getPrim());
}

GT_PrimitiveHandle
GABC_PackedAlembic::getCentroidGeometry(const GT_RefineParms *) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return impl->centroidGT(getPrim());
}

bool
GABC_PackedAlembic::refine(GT_Refine &refiner,
			   const GT_RefineParms *parms) const
{
    if(gabcExprUseArchivePrims() == 0)
	return GT_PackedAlembic::refine(refiner,parms);
    
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
GABC_PackedAlembic::getCachedTransform(GT_TransformHandle &th) const
{
    auto impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    UT_Matrix4D mat;
    myCache.getTransform(impl, mat);
    th = new GT_Transform(&mat, 1);
}

void
GABC_PackedAlembic::getCachedVisibility(bool &visible) const
{
    auto impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    myCache.getVisibility(impl, visible);
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
		    (prim->sharedImplementation());

		UT_StringHolder path;
		gabcViewportObjName(impl, path);

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
				"PrimTransformIndex", trans_index);
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
							      trans_array,
							      "PrimVisibility",
							      vis_array);
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
				       const GABC_IArchivePtr &archive,
				       int index)
    : GT_PackedAlembicArchive(arch, dlist),
      myArchive(archive),
      myIndex(index)
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
	//   	     num_streams, myAlembicOffsets.entries());
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
    for(auto itr : btask.buckets())
	prims[itr.first] = nullptr;
    
    gabc_CreateInstanceGeos gttask(btask.buckets(), myDetailList, parms, prims);

    UTparallelFor(UT_BlockedRange<exint>(0, btask.buckets().size()), gttask);

    PRINT_TIMER("Create primitives");

    UT_Array<GT_PrimitiveHandle> alem_meshes;
    int ccount = 0;
    // Sort the primitives into buckets based on animation.
    for(auto offset : myAlembicOffsets)
    {
	const GU_PrimPacked *prim = static_cast<const GU_PrimPacked *>
	    (gdplock->getPrimitive(offset));
	auto impl= UTverify_cast<const GABC_PackedImpl*>
	    (prim->sharedImplementation());

	UT_StringHolder name;
	gabcViewportObjName(impl, name);
	
   	GT_PrimitiveHandle p = prims[name];
	if(!p)
	    continue;
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

	prims[name] = nullptr;
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

	    for(const auto &mesh : meshes)
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
	    (this_prim->sharedImplementation());

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
  : GT_AlembicInstance()
{}

GABC_PackedInstance::GABC_PackedInstance(const GABC_PackedInstance &src)
    : GT_AlembicInstance(src)
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
    GEO_AnimationType animation,
    const GT_GEOOffsetList &packed_prim_offsets,
    const GT_AttributeListHandle &uniform,
    const GT_AttributeListHandle &detail,
    const GT_GEODetailListHandle &source)
    : GT_AlembicInstance(geometry, transforms,animation,packed_prim_offsets,
			 uniform, detail, source)
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
{}

bool
GABC_PackedInstance::updateGeoPrim(const GU_ConstDetailHandle &dtl,
				   const GT_RefineParms &refine)
{
    //static UT_Vector3 theZero(0.0);

    bool updated = GT_Primitive::updateGeoPrim(dtl, refine);

    if(myUniform)
    {
	const GT_DataArrayHandle &prim = myUniform->get(GT_Names::primitive_id);
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
			pprim->sharedImplementation())->instanceGT(true);
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
			    pprim->sharedImplementation());

			myCache(i).getTransform(pimpl, transform);
			pprim->multiplyByPrimTransform(transform);
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
	    GT_DataArrayHandle lod = myUniform->get(GT_Names::view_lod);
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
			    pprim->sharedImplementation());

			myCache(i).getVisibility(pimpl, visible);
		    
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
		
		GT_DataArrayHandle
		    lodm = myUniform->get(GT_Names::view_lod_mask);
		if(lodm)
		{
		    auto lodma = UTverify_cast<GT_DANumeric<int> *>(lodm.get());
		    lodma->set(lod_mask, 0);
		}
	    }
	}
    }
    if(!myHasChanged && updated)
	myHasChanged = true;
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

// -------------------------------------------------------------------------

void
GABC_AlembicCache::getVisibility(const GABC_PackedImpl *impl, bool &visible)
{
    UT_ASSERT(impl);

    if (!impl->useVisibility())
    {
	visible = true;
	return;
    }

    fpreal frame = myVisibilityAnimated ? impl->frame() : 0.0;
    auto iter = myVisibility.find(frame);
    if (iter != myVisibility.end())
    {
	visible = iter->second;
	return;
    }

    visible = impl->visibleGT(&myVisibilityAnimated);
    myVisibility.emplace(frame, visible);
}

void
GABC_AlembicCache::getTransform(const GABC_PackedImpl *impl,
    UT_Matrix4D &transform)
{
    UT_ASSERT(impl);

    if (!impl->useTransform())
    {
	transform.identity();
	return;
    }

    fpreal frame = myTransformAnimated ? impl->frame() : 0.0;
    auto iter = myTransform.find(frame);
    if (iter != myTransform.end())
    {
	transform = iter->second;
	return;
    }

    bool isconst, inherits;
    GABC_Util::getWorldTransform(impl->object(), frame, transform, isconst, inherits);
    myTransformAnimated = !isconst;
    myTransform.emplace(frame, transform);
}
