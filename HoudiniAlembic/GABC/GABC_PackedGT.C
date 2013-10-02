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
#include <GT/GT_RefineParms.h>

using namespace GABC_NAMESPACE;

namespace
{

class CollectData : public GT_GEOPrimCollectData
{
public:
    CollectData(const GT_GEODetailListHandle &geometry, bool useViewportLOD)
	: GT_GEOPrimCollectData()
	, myUseViewportLOD(useViewportLOD)
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
		    break;
	    }
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
    GT_PrimitiveHandle	finish() const
    {
	GT_GEOPrimCollectBoxes	boxdata(true);
	exint			nbox = myBoxPrims.entries();
	exint			ncentroid = myCentroidPrims.entries();
	exint			nprims = SYSmax(nbox, ncentroid);

	if (!nprims)
	    return GT_PrimitiveHandle();

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

private:
    UT_Array<const GU_PrimPacked *>	myBoxPrims;
    UT_Array<const GU_PrimPacked *>	myCentroidPrims;
    bool				myUseViewportLOD;
};

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
    return new CollectData(geometry, GT_GEOPrimPacked::useViewportLOD(parms));
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
