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
		myID = impl->getPropertiesHash();
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


class CollectData : public GT_GEOPrimCollectData
{
public:
    CollectData(const GT_GEODetailListHandle &geometry,
	    bool useViewportLOD)
	: GT_GEOPrimCollectData()
	, myGeometry(geometry)
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
		    // Fall through to generic handling
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

    GT_PrimitiveHandle	finish() const
    {
	return finishBoxes();
    }

private:
    const GT_GEODetailListHandle	myGeometry;
    UT_Array<const GU_PrimPacked *>	myBoxPrims;
    UT_Array<const GU_PrimPacked *>	myCentroidPrims;
    bool				myUseViewportLOD;
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
    if (impl->isConstant())
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

GABC_CollectPacked::~GABC_CollectPacked()
{
}

GT_GEOPrimCollectData *
GABC_CollectPacked::beginCollecting(const GT_GEODetailListHandle &geometry,
				const GT_RefineParms *parms) const
{
    return new CollectData(geometry,
	    GT_GEOPrimPacked::useViewportLOD(parms));
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
