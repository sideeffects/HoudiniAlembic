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
 * NAME:	ROP_AbcGTInstance.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#include "ROP_AbcGTInstance.h"
#include <UT/UT_WorkBuffer.h>
#include <GABC/GABC_Util.h>
#include <GT/GT_PrimInstance.h>
#include <GT/GT_GEOPrimPacked.h>
#include "ROP_AbcContext.h"
#include "ROP_AbcGTCompoundShape.h"

using namespace GABC_NAMESPACE;

namespace
{
    typedef Alembic::Abc::OObject		OObject;
    typedef Alembic::Abc::M44d			M44d;
    typedef Alembic::AbcGeom::OXform		OXform;
    typedef Alembic::AbcGeom::XformSample	XformSample;
}

void
ROP_AbcGTInstance::Instance::first(const OObject &parent,
	GABC_OError &err,
	const ROP_AbcContext &ctx,
	const UT_Matrix4D &xform,
	const std::string &name)
{
    XformSample	sample;
    M44d	m = GABC_Util::getM(xform);

    sample.setMatrix(m);
    myOXform = OXform(parent, name, ctx.timeSampling());
    myOXform.getSchema().set(sample);
}

ROP_AbcGTCompoundShape *
ROP_AbcGTInstance::Instance::setGeometry(GABC_OError &err,
	const ROP_AbcContext &ctx,
	const GT_PrimitiveHandle &g,
	const std::string &name,
	bool subd_mode,
	bool add_unused_pts)
{
    ROP_AbcGTCompoundShape	*geo = new ROP_AbcGTCompoundShape(name,
					    subd_mode, add_unused_pts);
    if (!geo->first(g, myOXform, err, ctx, true))
    {
	delete geo;
	return NULL;
    }
    return geo;
}

void
ROP_AbcGTInstance::Instance::setGeometry(const OObject &child, const std::string &name)
{
    myOXform.addChildInstance(child, name);
}
void
ROP_AbcGTInstance::Instance::update(const UT_Matrix4D &xform)
{
    XformSample	sample;
    M44d	m = GABC_Util::getM(xform);

    // TODO: Check whether transform has changed
    sample.setMatrix(m);
    myOXform.getSchema().set(sample);
}

ROP_AbcGTInstance::ROP_AbcGTInstance(const std::string &name)
    : myName(name)
    , myGeometry(NULL)
{
}

ROP_AbcGTInstance::~ROP_AbcGTInstance()
{
    delete myGeometry;
}

bool
ROP_AbcGTInstance::first(const OObject &parent, GABC_OError &err,
	const ROP_AbcContext &ctx, const GT_PrimitiveHandle &prim,
	bool subd_mode, bool add_unused_pts)
{
    UT_Matrix4D		m;
    switch (prim->getPrimitiveType())
    {
	case GT_PRIM_INSTANCE:
	{
	    const GT_PrimInstance	*iprim;
	    UT_WorkBuffer		 nbuf;

	    iprim = UTverify_cast<const GT_PrimInstance *>(prim.get());
	    const GT_TransformArrayHandle	&xforms = iprim->transforms();
	    exint				 icount = iprim->entries();
	    for (exint i = 0; i < icount; ++i)
	    {
		myInstances.append(Instance());
		Instance	&inst = myInstances.last();
		xforms->get(i)->getMatrix(m);
		if (i == 0)
		{
		    inst.first(parent, err, ctx, m, myName);
		    myGeometry = inst.setGeometry(err, ctx, iprim->geometry(),
			    myName, subd_mode, add_unused_pts);
		    if (!myGeometry)
			return false;
		}
		else
		{
		    nbuf.sprintf("%s_instance_%d", myName.c_str(), (int)i);
		    inst.first(parent, err, ctx, m, nbuf.buffer());
		    // myGeometry is ROP_AbcGTCompoundShape.C
		    // myGeometry has no container (single shape)
		    // So it returns myShape(0)
		    // Which is a
		    inst.setGeometry(myGeometry->getShape(), myName);
		}
	    }
	}
	break;
	case GT_GEO_PACKED:
	{
	    // We want to put a transform in the hierarchy before the geometry
	    // for the packed primitive.
	    const GT_GEOPrimPacked	*pprim;
	    GT_TransformHandle		 xform;
	    GT_PrimitiveHandle		 pgeo;
	    pprim = UTverify_cast<const GT_GEOPrimPacked *>(prim.get());
	    myInstances.append(Instance());
	    Instance	&inst = myInstances.last();

	    pprim->geometryAndTransform(NULL, pgeo, xform);
	    if (xform)
		xform->getMatrix(m);
	    else
		m.identity();
	    inst.first(parent, err, ctx, m, myName);
	    myGeometry = inst.setGeometry(err, ctx, pgeo,
		    myName, subd_mode, add_unused_pts);
	    if (!myGeometry)
		return false;
	}
	break;
	default:
	    return false;
    }
    return true;
}

bool
ROP_AbcGTInstance::update(GABC_OError &err, const ROP_AbcContext &ctx,
    const GT_PrimitiveHandle &prim)
{
    UT_Matrix4D m;
    switch (prim->getPrimitiveType())
    {
	case GT_PRIM_INSTANCE:
	{
	    const GT_PrimInstance	*iprim;

	    iprim = UTverify_cast<const GT_PrimInstance *>(prim.get());
	    // First, update the geometry
	    if (!myGeometry->update(iprim->geometry(), err, ctx))
		return false;

	    // Now, update the transforms
	    const GT_TransformArrayHandle	&xforms = iprim->transforms();
	    exint				 icount;
	    icount = SYSmin(xforms->entries(), myInstances.entries());

	    for (exint i = 0; i < icount; ++i)
	    {
		xforms->get(i)->getMatrix(m);
		myInstances(i).update(m);
	    }
	}
	break;
	case GT_GEO_PACKED:
	{
	    const GT_GEOPrimPacked	*pprim;
	    GT_TransformHandle		 xform;
	    GT_PrimitiveHandle		 pgeo;
	    pprim = UTverify_cast<const GT_GEOPrimPacked *>(prim.get());

	    pprim->geometryAndTransform(NULL, pgeo, xform);
	    if (!myGeometry->update(pgeo, err, ctx))
		return false;
	    if (myInstances.entries() == 1)
	    {
		if (xform)
		    xform->getMatrix(m);
		else
		    m.identity();
		myInstances(0).update(m);
	    }
	}
	break;
	default:
	    UT_ASSERT(0);
	    return false;
    }
    return true;
}

Alembic::Abc::OObject
ROP_AbcGTInstance::getOObject() const
{
    if (myInstances.entries())
	return SYSconst_cast(this)->myInstances(0).oxform().getParent();
    return Alembic::Abc::OObject();
}
