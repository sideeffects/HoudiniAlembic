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

using namespace GABC_NAMESPACE;

namespace
{

class GABC_API GABC_PackedGT : public GT_GEOPrimPacked
{
public:
    GABC_PackedGT(const GU_PrimPacked *prim)
	: GT_GEOPrimPacked(prim)
    {
    }
    GABC_PackedGT(const GABC_PackedGT &src)
	: GT_GEOPrimPacked(src)
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
private:
};


class CollectData : public GT_GEOPrimCollectData
{
public:
    CollectData(const GT_GEODetailListHandle &geometry,
	    bool useViewportLOD,
	    bool doInstancing)
	: GT_GEOPrimCollectData()
	, myUseViewportLOD(useViewportLOD)
	, myDoInstancing(doInstancing)
    {
    }
    virtual ~CollectData() {}

    bool	append(const GU_PrimPacked &prim)
    {
	const GABC_PackedImpl	*impl;
	impl = UTverify_cast<const GABC_PackedImpl *>(prim.implementation());
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
		    // Fall through to generic handling
		    break;
	    }
	}
	if (myDoInstancing)
	{
	    myFullPrims.append(&prim);
	    return true;
	}
	return false;
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

	GT_GEOPrimCollectBoxes		boxdata(true);
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
			myCentroidPrims(i)->getPointOffset(0));
	    }
	}
	return boxdata.getPrimitive();
    }

    static const GABC_PackedImpl *getImpl(const GU_PrimPacked *prim)
    {
	const GU_PackedImpl	*impl = prim->implementation();
	return UTverify_cast<const GABC_PackedImpl *>(impl);
    }

    class InstanceGroup
    {
    public:
	InstanceGroup()
	    : myPrims()
	    , myPrim0(0)
	    , myOrder(-1)
	{
	}
	GT_PrimitiveHandle	makePrimitive() const
	{
	    const GABC_PackedImpl	*impl = getImpl(myPrim0);
	    if (myPrims.entries() == 0)
		return impl->fullGT(GABC_IObject::GABC_LOAD_FULL);

	    int		load_style = GABC_IObject::GABC_LOAD_FULL;
	    // Now, strip out any Houdini primitive attributes, these will be
	    // put on the instance.
	    load_style &= ~GABC_IObject::GABC_LOAD_HOUDINI;
	    // We also want to force the geometry to be untransformed since the
	    // instance will pick up the transforms.
	    load_style |=  GABC_IObject::GABC_LOAD_FORCE_UNTRANSFORMED;
	    GT_PrimitiveHandle		geo = impl->fullGT(load_style);
	    GT_TransformArrayHandle	xforms(new GT_TransformArray());
	    GT_GEOOffsetList		vertices;
	    xforms->setEntries(myPrims.entries());
	    for (exint i = 0; i < myPrims.entries(); ++i)
	    {
		impl = getImpl(myPrims(i));
		vertices.append(myPrims(i)->getVertexOffset(0));
		xforms->set(i, impl->xformGT());
	    }
	    GT_GEOAttributeFilter	 filter;
	    const GU_Detail		*gdp;
	    GT_AttributeListHandle	 uniform, detail;
	    gdp = UTverify_cast<const GU_Detail *>(myPrims(0)->getParent());
	    GT_GEODetailList	 glist(&gdp, 1);
	    uniform = glist.getVertexAttributes(filter, &vertices,
				    GT_GEODetailList::GEO_INCLUDE_POINT,
				    GT_GEODetailList::GEO_INCLUDE_PRIMITIVE);
	    detail = glist.getDetailAttributes(filter);
	    return new GT_PrimInstance(geo, xforms, uniform, detail);
	}
	void	append(const GU_PrimPacked *p)
	{
	    if (!myPrim0)
	    {
		myPrim0 = p;
	    }
	    else
	    {
		if (!myPrims.entries())
		    myPrims.append(myPrim0);
		myPrims.append(p);
	    }
	}
	int	order() const			{ return myOrder; }
	UT_Array<const GU_PrimPacked *>	 myPrims;
	const GU_PrimPacked		*myPrim0;
	int				 myOrder;
    };
    class InstanceKey
    {
    public:
	InstanceKey(const GABC_PackedImpl &impl)
	    : myPath(impl.filename())
	    , myObject(impl.objectPath())
	    , myFrame(impl.frame())
	    , myUseTransform(impl.useTransform())
	{
	}
	uint		hash() const
	{
	    return UT_String::hash(myPath.c_str())
		^ UT_String::hash(myObject.c_str())
		^ SYSreal_hash(myFrame)
		^ (myUseTransform ? 0xc0ffee : 0);
	}
	bool		isEqual(const InstanceKey &k) const
	{
	    return myFrame == k.myFrame
		&& myUseTransform == k.myUseTransform
		&& myPath == k.myPath
		&& myObject == k.myObject;
	}
	bool		operator==(const InstanceKey &k) const
				{ return isEqual(k); }

    private:
	const std::string	&myPath;
	const std::string	&myObject;
	fpreal			 myFrame;
	bool			 myUseTransform;
    };
    class InstanceKeyHash
    {
    public:
	inline uint	operator()(const InstanceKey &k) const
			    { return k.hash(); }
    };

    typedef UT_Map<InstanceKey, InstanceGroup, InstanceKeyHash>	InstanceGroupMap;

    static GT_PrimitiveHandle
    makeInstanceGroups(const InstanceGroup **groups, exint size)
    {
	if (size == 1)
	    return groups[0]->makePrimitive();

	GT_PrimCollect	*c = new GT_PrimCollect();
	c->reserve(size);
	for (exint i = 0; i < size; ++i)
	    c->appendPrimitive(groups[i]->makePrimitive());
	return GT_PrimitiveHandle(c);
    }

    GT_PrimitiveHandle	finishFull() const
    {
	if (!myFullPrims.entries())
	    return GT_PrimitiveHandle();
	InstanceGroupMap	map;
	// Here we partition the primitives into "instance" groups.
	// We need to maintain primitive order for proper motion blur.
	for (exint i = 0; i < myFullPrims.entries(); ++i)
	{
	    const GABC_PackedImpl	*impl = getImpl(myFullPrims(i));
	    InstanceKey			 key(*impl);
	    InstanceGroup		&group = map[key];
	    if (group.myOrder < 0)
		group.myOrder = map.size()-1;
	    group.append(myFullPrims(i));
	}
	exint					ngroups = map.size();
	UT_StackBuffer<const InstanceGroup *>	groups(ngroups);
	for (InstanceGroupMap::const_iterator it = map.begin();
		it != map.end(); ++it)
	{
	    groups[it->second.myOrder] = &(it->second);
	}
	return makeInstanceGroups(groups, ngroups);
    }

    GT_PrimitiveHandle	finish() const
    {
	GT_PrimitiveHandle	boxes = finishBoxes();
	GT_PrimitiveHandle	full = finishFull();

	if (!boxes)
	    return full;
	if (!full)
	    return boxes;
	GT_PrimCollect	*collect = dynamic_cast<GT_PrimCollect *>(boxes.get());
	if (collect)
	{
	    // Just append to the existing collection
	    collect->appendPrimitive(full);
	    return boxes;
	}
	collect = dynamic_cast<GT_PrimCollect *>(full.get());
	if (collect)
	{
	    // Just append to the existing collection
	    collect->appendPrimitive(boxes);
	    return boxes;
	}
	collect = new GT_PrimCollect();
	collect->appendPrimitive(boxes);
	collect->appendPrimitive(full);
	return GT_PrimitiveHandle(collect);
    }

private:
    UT_Array<const GU_PrimPacked *>	myBoxPrims;
    UT_Array<const GU_PrimPacked *>	myCentroidPrims;
    UT_Array<const GU_PrimPacked *>	myFullPrims;
    bool				myUseViewportLOD;
    bool				myDoInstancing;
};

GT_PrimitiveHandle
GABC_PackedGT::getPointCloud(const GT_RefineParms *, bool &) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return impl->pointGT();
}

GT_PrimitiveHandle
GABC_PackedGT::getFullGeometry(const GT_RefineParms *parms, bool &) const
{
    const GABC_PackedImpl	*impl;
    int				 load_style;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    if (GT_GEOPrimPacked::useViewportLOD(parms))
    {
	load_style = GABC_IObject::GABC_LOAD_LEAN_AND_MEAN;
	if (GT_RefineParms::getViewportAlembicFaceSets(parms))
	    load_style |= GABC_IObject::GABC_LOAD_FACESETS;
	if (GT_RefineParms::getViewportAlembicArbGeometry(parms))
	    load_style |= GABC_IObject::GABC_LOAD_ARBS;
    }
    else
    {
	load_style = GABC_IObject::GABC_LOAD_FULL;
    }
    return impl->fullGT(load_style);
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

GABC_CollectPacked::~GABC_CollectPacked()
{
}

GT_GEOPrimCollectData *
GABC_CollectPacked::beginCollecting(const GT_GEODetailListHandle &geometry,
				const GT_RefineParms *parms) const
{
    return new CollectData(geometry,
	    GT_GEOPrimPacked::useViewportLOD(parms),
	    GT_RefineParms::getAlembicInstancing(parms));
}

GT_PrimitiveHandle
GABC_CollectPacked::collect(const GT_GEODetailListHandle &geo,
	const GEO_Primitive *const* prim,
	int nsegments, GT_GEOPrimCollectData *data) const
{
    CollectData		*collector = data->asPointer<CollectData>();
    const GU_PrimPacked *pack = UTverify_cast<const GU_PrimPacked *>(prim[0]);
    if (!collector->append(*pack))
	return GT_PrimitiveHandle(new GABC_PackedGT(pack));
    return GT_PrimitiveHandle();
}

GT_PrimitiveHandle
GABC_CollectPacked::endCollecting(const GT_GEODetailListHandle &geometry,
				GT_GEOPrimCollectData *data) const
{
    CollectData			*collector = data->asPointer<CollectData>();
    return collector->finish();
}
