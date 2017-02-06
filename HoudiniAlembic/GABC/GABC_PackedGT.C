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
 * NAME:	GABC_PackedGT.h (GABC Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_PackedGT.h"
#include "GABC_PackedImpl.h"
#include <UT/UT_StackBuffer.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_GEOPrimCollectBoxes.h>
#include <GT/GT_GEOAttributeFilter.h>
#include <GT/GT_PrimCollect.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_TransformArray.h>
#include <GT/GT_PrimInstance.h>
#include <SYS/SYS_Hash.h>

// Enable to bucket geometry into instances. This currently incurs a penalty
// for Alembic playback.
//#define INSTANCED_ALEMBIC_PRIM_SUPPORT


using namespace GABC_NAMESPACE;

namespace
{
    
 
class GABC_PackedGT : public GT_GEOPrimPacked
{
public:
    GABC_PackedGT(const GU_ConstDetailHandle &prim_gdh,
		  const GU_PrimPacked *prim)
	: GT_GEOPrimPacked(prim_gdh, prim),
	  myID(0)
    {
	if(prim)
	{
	    const GABC_PackedImpl *impl =
	       UTverify_cast<const GABC_PackedImpl *>(prim->implementation());
	    if(impl)
	    {
		fpreal frame = 0.0;
		if(impl->animationType() > GEO_ANIMATION_TRANSFORM)
		    frame = impl->frame();
		SYS_HashType hash = impl->getPropertiesHash();
		SYShashCombine(hash, SYSreal_hash(frame));
		myID = hash;
	    }
	}
    }
    
    GABC_PackedGT(const GABC_PackedGT &src)
	: GT_GEOPrimPacked(src),
	  myID(src.myID)
    {
    }
    virtual ~GABC_PackedGT()
    {
    }

    virtual const char	*className() const	{ return "GABC_PackedGT"; }
    virtual GT_PrimitiveHandle	doSoftCopy() const
				    { return new GABC_PackedGT(*this); }

    virtual GT_PrimitiveHandle	getPointCloud(const GT_RefineParms *p,
					bool &xform) const;
    virtual GT_PrimitiveHandle	getFullGeometry(const GT_RefineParms *p,
					bool &xform) const;
    virtual GT_PrimitiveHandle	getBoxGeometry(const GT_RefineParms *p) const;
    virtual GT_PrimitiveHandle	getCentroidGeometry(const GT_RefineParms *p) const;

    virtual bool		canInstance() const	{ return true; }
    virtual bool		getInstanceKey(UT_Options &options) const;
    virtual GT_PrimitiveHandle	getInstanceGeometry(const GT_RefineParms *p) const;
    virtual GT_TransformHandle	getInstanceTransform() const;

    virtual bool		getUniqueID(int64 &id) const
				{ id = myID; return true; }
private:
    int64	myID;
};
    
class gabc_CreateInstanceGeos
{
public:
    gabc_CreateInstanceGeos(
	const UT_StringMap< UT_Array<const GU_PrimPacked*> > &geo,
	const GT_GEODetailListHandle &geometry,
	const GT_RefineParms *ref_parms,
	UT_Array<GT_PrimitiveHandle> &gt_prims)
	: myGeos(geo), myGeometry(geometry), myRefineParms(ref_parms),
	  myPrims(gt_prims)
	{}
    
    void	operator()(const UT_BlockedRange<exint> &range) const
	{
	    GT_GEOAttributeFilter filter;

	    int index = 0;
	    for(auto itr = myGeos.begin(); itr != myGeos.end(); ++itr, index++)
	    {
		if(index < range.begin())
		    continue;
		if(index == range.end())
		    break;
		
		GABC_PackedGT *packinst =
		    new GABC_PackedGT(myGeometry->getGeometry(0),
				      itr->second(0));

		if(itr->second.entries() == 1)
		{
		    myPrims(index) = packinst;
		    continue;
		}

		// Instanced geometry
		GT_PrimitiveHandle geo =
		    packinst->getInstanceGeometry(myRefineParms);
		GT_TransformArray *xforms = new GT_TransformArray;
		GT_TransformArrayHandle xformh = xforms;
		GT_GEOOffsetList offsets, voffsets;
		bool use_vertex = true;
		
		for( auto pi : itr->second )
		{
		    GABC_PackedGT inst(myGeometry->getGeometry(0), pi);

		    xforms->append( inst.getFullTransform() );
		    offsets.append( inst.getPrim()->getMapOffset() );
		    if(inst.getPrim()->getVertexCount() > 0)
		       voffsets.append(inst.getPrim()->getVertexOffset(0));
		    else
			use_vertex = false; // avoid if one instance has 0 verts
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
		
		myPrims(index) = new GT_PrimInstance(geo, xformh, offsets,
						    prim_attribs,detail_attribs,
						    GT_GEODetailListHandle());
		delete packinst;
	    }
	}

private:
    const UT_StringMap< UT_Array<const GU_PrimPacked *> > &myGeos;
    UT_Array<GT_PrimitiveHandle>		 &myPrims;
    const GT_GEODetailListHandle		  myGeometry;
    const GT_RefineParms			 *myRefineParms;
};


class CollectData : public GT_GEOPrimCollectData
{
public:
    CollectData(const GT_GEODetailListHandle &geometry,
		const GT_RefineParms *parms)
	: GT_GEOPrimCollectData()
	, myGeometry(geometry)
	, myUseViewportLOD(GT_GEOPrimPacked::useViewportLOD(parms))
	, myRefineParms(parms)
    { }
    virtual ~CollectData() {}

    bool	append(const GU_PrimPacked &prim)
    {
	auto impl= UTverify_cast<const GABC_PackedImpl*>(prim.implementation());
	if (!impl->visibleGT())
	    return true;	// Handled
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
		    break;
	    }
	}
#ifdef INSTANCED_ALEMBIC_PRIM_SUPPORT
	bucketPrim(prim, impl);
	return true;
#endif
	return false;
    }

    void bucketPrim(const GU_PrimPacked &prim,
		    const GABC_PackedImpl *impl)
	{
	    UT_WorkBuffer bucket_name;
	    UT_StringHolder path = impl->object().getSourcePath();
	    UT_StringHolder arch = impl->object().archive()->filename();
	    
	    bucket_name.sprintf("%s:%s", arch.c_str(), path.c_str());
	    
	    auto entry = myInstanceAnim.find( bucket_name.buffer() );
	    GEO_AnimationType anim;
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
	    UT_Array<GT_PrimitiveHandle> prims;
	    prims.entries( myInstanceGeo.size() );
	    gabc_CreateInstanceGeos task(myInstanceGeo, myGeometry,
					 myRefineParms, prims);
#if 1
	    UTparallelFor(UT_BlockedRange<exint>(0, myInstanceGeo.size()),
			  task, 1, 20);
#else
	    UTserialFor(UT_BlockedRange<exint>(0, myInstanceGeo.size()),
			task);
#endif

	    if(collect)
	    {
		for(int i=0; i<prims.entries(); i++)
		    collect->appendPrimitive(prims(i));
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

	return collecth;
    }

private:
    const GT_GEODetailListHandle	myGeometry;
    const GT_RefineParms	       *myRefineParms;
    
    // Viewport
    const bool				myUseViewportLOD;
    UT_Array<const GU_PrimPacked *>	myBoxPrims;
    UT_Array<const GU_PrimPacked *>	myCentroidPrims;
    UT_StringMap< UT_Array<const GU_PrimPacked *> > myInstanceGeo;
    UT_StringMap< GEO_AnimationType >	myInstanceAnim;

};

GT_PrimitiveHandle
GABC_PackedGT::getPointCloud(const GT_RefineParms *, bool &xform) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    xform = false;
    return impl->pointGT();
}

GT_PrimitiveHandle
GABC_PackedGT::getFullGeometry(const GT_RefineParms *parms, bool &xform) const
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
    }
    else
    {
	load_style = GABC_IObject::GABC_LOAD_FULL;
	if (!GT_RefineParms::getAlembicHoudiniAttribs(parms))
	    load_style &= ~GABC_IObject::GABC_LOAD_HOUDINI;
    }
    return impl->fullGT(load_style);
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
GABC_PackedGT::getInstanceKey(UT_Options &options) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());

    options.setOptionS("f", impl->filename());
    options.setOptionS("o", impl->objectPath());
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

GT_PrimitiveHandle
GABC_PackedGT::getInstanceGeometry(const GT_RefineParms *p) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return impl->instanceGT();
}

GT_TransformHandle
GABC_PackedGT::getInstanceTransform() const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return impl->xformGT();
}

GT_PrimitiveHandle
GABC_PackedGT::getBoxGeometry(const GT_RefineParms *) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return impl->boxGT();
}

GT_PrimitiveHandle
GABC_PackedGT::getCentroidGeometry(const GT_RefineParms *) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return impl->centroidGT();
}

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
	return GT_PrimitiveHandle(new GABC_PackedGT(geo->getGeometry(0), pack));
    return GT_PrimitiveHandle();
}

GT_PrimitiveHandle
GABC_CollectPacked::endCollecting(const GT_GEODetailListHandle &geometry,
				  GT_GEOPrimCollectData *data) const
{
    CollectData			*collector = data->asPointer<CollectData>();
    return collector->finish();
}
