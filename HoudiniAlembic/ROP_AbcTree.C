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
 * NAME:	ROP_AbcTree.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#include "ROP_AbcTree.h"
#include "ROP_AlembicInclude.h"
#include <UT/UT_Version.h>
#include <UT/UT_Date.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_WorkBuffer.h>
#include <OP/OP_Director.h>
#include <OBJ/OBJ_Node.h>
#include "ROP_AbcXform.h"
#include "ROP_AbcShape.h"
#include "ROP_AbcCamera.h"

typedef Alembic::Abc::chrono_t	chrono_t;

ROP_AbcTree::ROP_AbcTree()
    : myArchive()
    , myRoot(NULL)
    , myTimeRange()
    , myStartTime(0)
    , myTimeStep(1)
    , myFirstFrame(true)
    , myCollapseNodes(false)
    , myContext()
    , myShutterOpen(0)
    , myShutterClose(1)
    , mySamples(2)
{
}

ROP_AbcTree::~ROP_AbcTree()
{
    close();
}

bool
ROP_AbcTree::open(ROP_AbcError &err, OP_Node *root, const char *filename,
		    fpreal tstart, fpreal tstep)
{
    close();

    UT_WorkBuffer	version;
    UT_WorkBuffer	userinfo;
    UT_String		hip;
    UT_String		hipname;
    UT_String		timestamp;

    version.sprintf("Houdini%d.%d.%d",
	    UT_MAJOR_VERSION_INT,
	    UT_MINOR_VERSION_INT,
	    UT_BUILD_VERSION_INT);

    GABC_Util::clearCache();

    OPgetDirector()->getCommandManager()->getVariable("HIP", hip);
    OPgetDirector()->getCommandManager()->getVariable("HIPNAME", hipname);
    UT_Date::dprintf(timestamp, "%Y-%m-%d %H:%M:%S", time(0));
    userinfo.sprintf("Exported from %s/%s on %s",
	    static_cast<const char *>(hip),
	    static_cast<const char *>(hipname),
	    static_cast<const char *>(timestamp));
    try
    {
	myArchive = CreateArchiveWithInfo(
			Alembic::AbcCoreHDF5::WriteArchive(),
			filename,
			version.buffer(),
			userinfo.buffer(),
			Alembic::Abc::ErrorHandler::kThrowPolicy);
    }
    catch (const std::exception &e)
    {
	err.error("Error creating archive: %s", e.what());
	myArchive = Alembic::Abc::OArchive();
    }
    if (!myArchive.valid())
    {
	err.error("Unable to create archive: %s", filename);
	myArchive = Alembic::Abc::OArchive();
	return false;
    }
    myRoot = root;
    myStartTime = tstart;
    myTimeStep = tstep;
    // We want frame numbers to match between Houdini & Alembic.
    // Since Houdini Frame 1=Houdini Time 0, we want the Alembic time to match
    // Houdini time + time step (Alembic frame 1)
    // We can't use the tstep since the ROP may have a non-integer time step.
    // We want to match frame 1 (not the ROP's time increment).
    fpreal spf = CHgetManager()->getSecsPerSample();
    fpreal startSec = tstart+spf;
    if (mySamples < 2)
	myEnableMotionBlur = false;
    if (!myEnableMotionBlur)
    {
        myTimeRange.reset(new Alembic::Abc::TimeSampling(tstep, startSec));
    }
    else if (mySamples > 1 && myShutterOpen < myShutterClose)
    {
        fpreal offset = (myShutterClose - myShutterOpen) / mySamples;
        fpreal curVal = myShutterOpen;
        // if motion blur is enabled, check if it is uniform sampling
        if (myShutterOpen < 1e-4 && fabs(myShutterClose-1) < 1e-4)  // uniform
        {
            myTimeRange.reset(new Alembic::Abc::TimeSampling(tstep/mySamples, startSec));
            for (int i = 0; i < mySamples; ++i, curVal += offset)
                myOffsetFrames.push_back(curVal*spf);
        }
        else
        {
            std::vector< chrono_t > sampleTimes;
            for (int i = 0; i < mySamples; ++i, curVal += offset)
            {
                myOffsetFrames.push_back(curVal*spf);
                sampleTimes.push_back(startSec+myOffsetFrames.back());
            }
            if (1-myShutterClose > 1e-4)
            {
                myOffsetFrames.push_back(myShutterClose*spf);
                sampleTimes.push_back(startSec+myOffsetFrames.back());
            }

	    Alembic::Abc::TimeSamplingType tSamplingType(sampleTimes.size(), spf);
            myTimeRange.reset(new Alembic::Abc::TimeSampling(tSamplingType, sampleTimes));
        }
    }
    return true;
}

void
ROP_AbcTree::close()
{
    myFirstFrame = true;
    for (UT_HashTable::traverser it = myStaticObjects.begin(); !it.atEnd(); ++it)
    {
	delete it.thing().asPointer<ROP_AbcXform>();
    }
    for (UT_HashTable::traverser it = myDynamicObjects.begin(); !it.atEnd(); ++it)
    {
	delete it.thing().asPointer<ROP_AbcXform>();
    }
    myStaticObjects.clear();
    myDynamicObjects.clear();
    myDynamicShapes.resize(0);
    try
    {
	myArchive = Alembic::Abc::OArchive();
    }
    catch (const std::exception &e)
    {
	UT_ASSERT(0 && "Alembic exception on close");
	fprintf(stderr, "Alembic exception on closing: %s\n", e.what());
    }
    myTimeRange.reset();
    myRoot = NULL;
}

ROP_AbcXform *
ROP_AbcTree::findXform(OP_Node *node) const
{
    UT_Hash_Int	hash(node->getUniqueId());
    UT_Thing	thing;
    if (myStaticObjects.findSymbol(hash, &thing))
	return thing.asPointer<ROP_AbcXform>();
    if (myDynamicObjects.findSymbol(hash, &thing))
	return thing.asPointer<ROP_AbcXform>();
    return NULL;
}

ROP_AbcXform *
ROP_AbcTree::addXform(ROP_AbcError &err, OBJ_Node *obj, ROP_AbcXform *parent)
{
    UT_Hash_Int		hash(obj->getUniqueId());
    UT_DMatrix4		m44;

    myContext.setTime(myStartTime);

    obj->getLocalTransform(myContext.context(), m44);
    bool		 tdep = obj->isTimeDependent(myContext.context());
    ROP_AbcXform	*xform;
    
    if (parent && myCollapseNodes && !tdep && m44.isIdentity())
    {
	UT_ASSERT(parent);
	xform = new ROP_AbcXform(obj, parent);
    }
    else
    {
	const char	*name = obj->getName();
	UT_WorkBuffer	 unique;
	if (parent)
	{
	    // It's possible to have a SOP & object name collision
	    name = parent->getUniqueChildName(name, unique);
	}
	xform = new ROP_AbcXform(obj, name,
		parent ? *parent->getNode() : myArchive.getTop(),
		myTimeRange, tdep);
    }
    if (tdep)
	myDynamicObjects.addSymbol(hash, xform);
    else
	myStaticObjects.addSymbol(hash, xform);

    if (obj->getObjectType() == OBJ_GEOMETRY)
    {
	xform->addShape(err, myContext, myTimeRange);
    }
    if (obj->getObjectType() & OBJ_CAMERA)
    {
	xform->addCamera(err, myContext, myTimeRange);
    }
    return xform;
}

ROP_AbcXform *
ROP_AbcTree::addObject(ROP_AbcError &err, OBJ_Node *obj)
{
    if (obj == myRoot)
	return NULL;
    UT_AutoInterrupt	interrupt("Adding object to alembic tree",
				err.getInterrupt());

    ROP_AbcXform	*me = findXform(obj);
    if (!err.wasInterrupted())
    {
	if (me)
	{
	    UT_WorkBuffer	path;
	    obj->getFullPath(path);
	    err.warning("Object %s added multiple times", path.buffer());
	}
	else
	{
	    try
	    {
		me = doAddObject(err, obj);
	    }
	    catch (const std::exception &e)
	    {
		err.error("Error creating alembic tree: %s", e.what());
		me = NULL;
	    }
	}
    }
    return me;
}

ROP_AbcXform *
ROP_AbcTree::doAddObject(ROP_AbcError &err, OBJ_Node *obj)
{
    UT_WorkBuffer	 fullpath;

    // Check for duplicate additions
    ROP_AbcXform	*me = findXform(obj);
    if (me)
	return me;

    if (err.wasInterrupted())
	return NULL;

    OP_Node	*parent;
    // First, walk the parent transform hierarchy
    parent = obj->getParentObject();
    // If we're at a root, we need to check if we're in a subnet
    if (!parent)
	parent = obj->getParent();
    if (parent == myRoot || obj == myRoot)
    {
	return addXform(err, obj, NULL);
    }
    if (!parent)
    {
	err.errorString("Traversed to root without finding object network");
	return NULL;	// Child not part of my object tree
    }

    ROP_AbcXform	*dad = findXform(parent);
    if (!dad)
    {
	OBJ_Node	*oparent = parent->castToOBJNode();
	if (!oparent)
	{
	    parent->getFullPath(fullpath);
	    err.error("Object %s not in the correct object network",
		    fullpath.buffer());
	    return NULL;
	}
	dad = doAddObject(err, oparent);
	if (!dad)
	{
	    obj->getFullPath(fullpath);
	    err.error("  Invalid parent for %s", fullpath.buffer());
	    return NULL;
	}
    }
    return addXform(err, obj, dad);
}

static bool
writeFirstFrame(ROP_AbcError &err,
	const UT_HashTable &table,
	UT_PtrArray<ROP_AbcShape *> &dshapes,
	UT_PtrArray<ROP_AbcCamera *> &dcameras,
	ROP_AbcContext &context)
{
    UT_AutoInterrupt	interrupt("Building alembic objects",
				err.getInterrupt());

    if (!interrupt.wasInterrupted())
    {
	UT_WorkBuffer	fullpath;
	for (UT_HashTable::traverser it = table.begin(); !it.atEnd(); ++it)
	{
	    ROP_AbcXform	*obj = it.thing().asPointer<ROP_AbcXform>();
	    ROP_AbcShape	*shape = obj->getShape();
	    ROP_AbcCamera	*cam = obj->getCamera();

	    if (interrupt.wasInterrupted())
		return false;
	    if (!obj->writeSample(err, context))
	    {
		return err.error("Error creating transform for %s at time %g",
			obj->getFullPath(fullpath), context.time());
	    }
	    if (shape)
	    {
		if (!shape->writeSample(err, context))
		{
		    return err.error("Error creating shape for %s at time %g",
			    obj->getFullPath(fullpath), context.time());
		}
		if (shape->isTimeDependent())
		    dshapes.append(shape);
	    }
	    if (cam)
	    {
		if (!cam->writeSample(err, context))
		{
		    return err.error("Error creating camera for %s at time %g",
			    obj->getFullPath(fullpath), context.time());
		}
		if (cam->isTimeDependent())
		    dcameras.append(cam);
	    }
	}
	return true;
    }
    return false;
}


void ROP_AbcTree::setShutterParms(fpreal shutterOpen,
	    fpreal shutterClose, int samples)
{
	myShutterOpen = shutterOpen;
	myShutterClose = shutterClose;
	mySamples = samples;
}

bool
ROP_AbcTree::writeSample(ROP_AbcError &err, fpreal time)
{
    if (!myEnableMotionBlur)
    {
        return doWriteSample(err, time);
    }
    else
    {
        size_t numSubFrames = myOffsetFrames.size();
        for (unsigned int i = 0; i < numSubFrames; i++)
        {
            fpreal curTime = time + myOffsetFrames[i];
            if (!doWriteSample(err, curTime))
                return false;
        }
    }
    return true;
}

bool
ROP_AbcTree::doWriteSample(ROP_AbcError &err, fpreal time)
{
    UT_AutoInterrupt interrupt("Writing alembic frame", err.getInterrupt());

    if (interrupt.wasInterrupted())
	return false;

    myContext.setTime(time);
    if (myFirstFrame)
    {
	try
	{
	    myFirstFrame = false;
	    myDynamicShapes.resize(0);
	    if (!writeFirstFrame(err, myStaticObjects,
			myDynamicShapes, myDynamicCameras, myContext))
	    {
		return false;
	    }
	    if (!writeFirstFrame(err, myDynamicObjects,
			myDynamicShapes, myDynamicCameras, myContext))
	    {
		return false;
	    }
	}
	catch (const std::exception &e)
	{
	    err.error("Error building alembic tree for export: %s", e.what());
	    return false;
	}
    }
    else
    {
	try
	{
	    UT_WorkBuffer	fullpath;
	    for (UT_HashTable::traverser it = myDynamicObjects.begin();
			    !it.atEnd(); ++it)
	    {
		if (interrupt.wasInterrupted())
		    return false;
		ROP_AbcXform	*obj = it.thing().asPointer<ROP_AbcXform>();
		if (!obj->writeSample(err, myContext))
		{
		    return err.error(
			    "Error writing transform for %s at time %g",
			    obj->getFullPath(fullpath), time);
		}
	    }
	    for (exint i = 0; i < myDynamicShapes.entries(); ++i)
		myDynamicShapes(i)->writeSample(err, myContext);
	    for (exint i = 0; i < myDynamicCameras.entries(); ++i)
		myDynamicCameras(i)->writeSample(err, myContext);
	}
	catch (const std::exception &e)
	{
	    err.error("Error writing at time %g: %s", time, e.what());
	    return false;
	}
    }
    return true;
}
