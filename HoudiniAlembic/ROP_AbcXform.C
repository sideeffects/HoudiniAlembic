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
 * NAME:	ROP_AbcXform.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#include "ROP_AbcXform.h"
#include <UT/UT_WorkBuffer.h>
#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_Camera.h>
#include <SOP/SOP_Node.h>
#include "ROP_AbcCamera.h"
#include "ROP_AbcContext.h"
#include "ROP_AbcShape.h"
#include "ROP_AbcError.h"

ROP_AbcXform::ROP_AbcXform(OBJ_Node *obj,
		const char *name,
		Alembic::AbcGeom::OObject parent,
		Alembic::AbcGeom::TimeSamplingPtr timeSampling,
		bool time_dep)
    : myObject(obj)
    , myXform(NULL)
    , myShape(NULL)
    , myCamera(NULL)
    , myTimeDependent(time_dep)
    , myIdentity(false)
    , myChildNames(new UT_SymbolTable())
{
    myXform = new Alembic::AbcGeom::OXform(parent, name, timeSampling);
}

ROP_AbcXform::ROP_AbcXform(OBJ_Node *obj, ROP_AbcXform *parent)
    : myObject(obj)
    , myXform(parent->getNode())
    , myShape(NULL)
    , myCamera(NULL)
    , myTimeDependent(false)
    , myIdentity(true)
    , myChildNames(parent->myChildNames)
{
}

ROP_AbcXform::~ROP_AbcXform()
{
    delete myCamera;
    delete myShape;
    if (!myIdentity)
    {
	delete myXform;
	delete myChildNames;
    }
}

OBJ_Node *
ROP_AbcXform::getObject() const
{
    return myObject;
}

const char *
ROP_AbcXform::getFullPath(UT_WorkBuffer &path) const
{
    myObject->getFullPath(path);
    return path.buffer();
}

ROP_AbcCamera *
ROP_AbcXform::addCamera(ROP_AbcError &err, ROP_AbcContext &context,
		    Alembic::AbcGeom::TimeSamplingPtr timeRange)
{
    UT_ASSERT(!myCamera);
    UT_ASSERT(myXform);
    UT_ASSERT(myObject);
    OBJ_Camera	*cam = static_cast<OBJ_Camera *>(getObject());
    bool	time_dep = ROP_AbcCamera::isCameraTimeDependent(cam);

    myCamera = new ROP_AbcCamera(cam, *myXform, timeRange, time_dep);
    return myCamera;
}


ROP_AbcShape *
ROP_AbcXform::addShape(ROP_AbcError &err, ROP_AbcContext &context,
		    Alembic::AbcGeom::TimeSamplingPtr timeRange)
{
    UT_ASSERT(!myShape);
    UT_ASSERT(myXform);
    UT_ASSERT(myObject);

    SOP_Node				*sop = myObject->getRenderSopPtr();
    if (!sop)
    {
	UT_WorkBuffer	fullpath;
	err.warning("%s has no render SOP", getFullPath(fullpath));
	return NULL;
    }

    UT_WorkBuffer	unique;

    myShape = new ROP_AbcShape(sop, myXform);
    myShape->create(err, getUniqueShapeName(sop->getName(), unique),
		context, timeRange);
    if (!myShape->getCount())
    {
	delete myShape;
	myShape = NULL;
    }
    return myShape;
}

bool
ROP_AbcXform::writeSample(ROP_AbcError &err, ROP_AbcContext &context)
{
    if (!myIdentity)
    {
	UT_DMatrix4				xform;
	Alembic::AbcGeom::XformSample	xsample;
	if (!myObject->getLocalTransform(context.context(), xform))
	{
	    UT_WorkBuffer	fullpath;
	    return err.error("Error cooking transform %s at time %g",
			    getFullPath(fullpath), context.time());
	}
	Alembic::AbcGeom::M44d	abcm((const double (*)[4])xform.data());
	xsample.setMatrix(abcm);
	myXform->getSchema().set(xsample);
    }
    return true;
}

const char *
ROP_AbcXform::getUniqueName(UT_SymbolTable &nametable, const char *base,
	UT_WorkBuffer &storage)
{
    UT_Thing	thing;
    const char	*name = base;
    if (nametable.findSymbol(name, &thing))
    {
	// Collision -- TODO: O(N^2) loop
	for (exint i = 1; true; ++i)
	{
	    storage.sprintf("_%s_unique%" SYS_PRId64, base, i);
	    if (!nametable.findSymbol(storage.buffer(), &thing))
	    {
		name = storage.buffer();
		break;
	    }
	}
    }
    // Record the unique name
    nametable.addSymbol(name, (void *)NULL);
    return name;
}
