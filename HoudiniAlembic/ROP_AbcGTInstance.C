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

#include "ROP_AbcContext.h"
#include "ROP_AbcGTInstance.h"
#include "ROP_AbcGTCompoundShape.h"
#include <GABC/GABC_Util.h>
#include <GT/GT_PrimInstance.h>
#include <GT/GT_GEOPrimPacked.h>
#include <UT/UT_WorkBuffer.h>

using namespace GABC_NAMESPACE;

void
ROP_AbcGTInstance::Instance::first(const OObject &parent,
	GABC_OError &err,
	const ROP_AbcContext &ctx,
	const UT_Matrix4D &xform,
	const std::string &name,
	ObjectVisibility vis)
{
    XformSample	sample;
    M44d	m = GABC_Util::getM(xform);

    myOXform = OXform(parent, name, ctx.timeSampling());
    myVisibility = Alembic::AbcGeom::CreateVisibilityProperty(
            myOXform,
            ctx.timeSampling());

    sample.setMatrix(m);
    myVisibility.set(vis);
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
                                            NULL,
                                            NULL,
                                            NULL,
                                            NULL,
                                            false,
					    subd_mode,
					    add_unused_pts,
					    myGeoLock,
					    ctx);
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
ROP_AbcGTInstance::Instance::update(const UT_Matrix4D &xform,
	ObjectVisibility vis)
{
    XformSample	sample;
    M44d	m = GABC_Util::getM(xform);

    // TODO: Check whether transform has changed
    sample.setMatrix(m);
    myVisibility.set(vis);
    myOXform.getSchema().set(sample);
}

void
ROP_AbcGTInstance::Instance::updateFromPrevious(ObjectVisibility vis)
{
    myVisibility.set(vis);
    myOXform.getSchema().setFromPrevious();
}

ROP_AbcGTInstance::ROP_AbcGTInstance(const std::string &name, bool geo_lock)
    : myName(name)
    , myGeometry(NULL)
    , myGeoLock(geo_lock)
{
}

ROP_AbcGTInstance::~ROP_AbcGTInstance()
{
    delete myGeometry;
}

bool
ROP_AbcGTInstance::first(const OObject &parent,
        GABC_OError &err,
	const ROP_AbcContext &ctx,
	const GT_PrimitiveHandle &prim,
	bool subd_mode,
	bool add_unused_pts,
        ObjectVisibility vis)
{
    UT_Matrix4D     instance_mat;
    UT_Matrix4D     prim_mat;

    myParent = parent;
    prim->getPrimitiveTransform()->getMatrix(prim_mat);

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
		myInstances.append(Instance(myGeoLock));
		Instance    &inst = myInstances.last();

		xforms->get(i)->getMatrix(instance_mat);
		instance_mat = prim_mat * instance_mat;

		if (i == 0)
		{
		    inst.first(parent, err, ctx, instance_mat, myName, vis);
		    myGeometry = inst.setGeometry(err, ctx, iprim->geometry(),
			    myName, subd_mode, add_unused_pts);
		    if (!myGeometry)
			return false;
		}
		else
		{
		    nbuf.sprintf("%s_instance_%d", myName.c_str(), (int)i);
		    inst.first(parent, err, ctx, instance_mat, nbuf.buffer(), vis);
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
	    myInstances.append(Instance(myGeoLock));
	    Instance	&inst = myInstances.last();

	    pprim->geometryAndTransform(NULL, pgeo, xform);

	    if (xform)
	    {
		xform->getMatrix(instance_mat);
                instance_mat = prim_mat * instance_mat;
            }
	    else
	    {
		instance_mat = prim_mat;
            }

	    inst.first(parent, err, ctx, instance_mat, myName, vis);
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
ROP_AbcGTInstance::update(const GT_PrimitiveHandle &prim,
        const ROP_AbcContext &ctx,
        GABC_OError &err,
        ObjectVisibility vis)
{
    UT_Matrix4D     instance_mat;
    UT_Matrix4D     prim_mat;

    prim->getPrimitiveTransform()->getMatrix(prim_mat);
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
	    const GT_TransformArrayHandle  &xforms = iprim->transforms();
	    UT_WorkBuffer nbuf;
	    exint xformcount = xforms->entries();
	    for (exint i = 0; i < xformcount; ++i)
	    {
		xforms->get(i)->getMatrix(instance_mat);
		instance_mat = prim_mat * instance_mat;
		if(i < myInstances.entries())
		    myInstances(i).update(instance_mat, vis);
		else
		{
		    myInstances.append(Instance(myGeoLock));
		    Instance    &inst = myInstances.last();

		    nbuf.sprintf("%s_instance_%d", myName.c_str(), (int)i);
		    inst.first(myParent, err, ctx, instance_mat, nbuf.buffer(), Alembic::AbcGeom::kVisibilityHidden);
		    inst.setGeometry(myGeometry->getShape(), myName);
		    for (exint j = 1; j < ctx.elapsedFrames(); ++j)
			myInstances(i).updateFromPrevious(Alembic::AbcGeom::kVisibilityHidden);
		    myInstances(i).updateFromPrevious(vis);
		}
	    }
	    for(exint i = xformcount; i < myInstances.entries(); ++i)
		myInstances(i).updateFromPrevious(Alembic::AbcGeom::kVisibilityHidden);
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
                {
                    xform->getMatrix(instance_mat);
                    instance_mat = prim_mat * instance_mat;
                }
                else
                {
                    instance_mat = prim_mat;
                }
		myInstances(0).update(instance_mat, vis);
	    }
	}
	break;

	default:
	    UT_ASSERT(0);
	    return false;
    }
    return true;
}

bool
ROP_AbcGTInstance::updateFromPrevious(GABC_OError &err,
        int primType,
        ObjectVisibility vis,
        exint frames)
{
    switch (primType)
    {
	case GT_PRIM_INSTANCE:
	{
	    for (exint i = 0; i < myInstances.entries(); ++i)
	    {
	        for (exint j = 0; j < frames; ++j)
		    myInstances(i).updateFromPrevious(vis);
	    }
	}
	break;

	case GT_GEO_PACKED:
	{
	    if (myInstances.entries() == 1)
	    {
	        for (exint j = 0; j < frames; ++j)
		    myInstances(0).updateFromPrevious(vis);
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

void
ROP_AbcGTInstance::dump(int indent) const
{
    printf("%*sInstance %p (%d instances)\n", indent, "",
	    myGeometry, (int)myInstances.entries());
}
