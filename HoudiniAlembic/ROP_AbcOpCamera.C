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
 * NAME:	ROP_AbcOpCamera.h
 *
 * COMMENTS:
 */

#include "ROP_AbcOpCamera.h"
#include <OBJ/OBJ_Camera.h>
#include <CH/CH_Channel.h>
#include <OP/OP_Director.h>
#include <GABC/GABC_OError.h>
#include "ROP_AbcContext.h"

namespace
{
    typedef Alembic::AbcGeom::OCamera		OCamera;
    typedef Alembic::AbcGeom::CameraSample	CameraSample;

    static bool
    fillSample(CameraSample &sample, OBJ_Camera *cam, const ROP_AbcContext &ctx)
    {
	const CH_Manager	*chman = OPgetDirector()->getChannelManager();
	fpreal	now = ctx.cookTime();
	fpreal	winx = cam->WINX(now);
	fpreal	winy = cam->WINY(now);
	fpreal	winsizex = cam->WINSIZEX(now);
	fpreal	winsizey = cam->WINSIZEY(now);
	fpreal	focal = cam->FOCAL(now);
	fpreal	aperture = cam->APERTURE(now);
	fpreal	aspect = cam->ASPECT(now);
	fpreal	resx = cam->RESX(now);
	fpreal	resy = cam->RESY(now);
	fpreal	fstop = cam->FSTOP(now);
	fpreal	focus = cam->FOCUS(now);
	fpreal	hither = cam->getNEAR(now);
	fpreal	yon = cam->getFAR(now);
	fpreal	shutter = cam->SHUTTER(now);

	// Compute resolution aspect
	fpreal	raspect = resy/resx;

	// Alembic stores value in cm. (not mm.)
	aperture *= 0.1;

	sample.setFocalLength(focal);

	sample.setHorizontalAperture(aperture);
	sample.setVerticalAperture(aperture * raspect);
	sample.setHorizontalFilmOffset(winx * aperture / aspect);
	sample.setVerticalFilmOffset(winy * aperture * raspect / aspect);
	sample.setLensSqueezeRatio(aspect);

	sample.setOverScanLeft(0);
	sample.setOverScanRight(0);
	sample.setOverScanTop(0);
	sample.setOverScanBottom(0);

	sample.setFStop(fstop);
	sample.setFocusDistance(focus);

	sample.setShutterOpen(0);
	sample.setShutterClose(chman->getTimeDelta(shutter));

	sample.setNearClippingPlane(hither);
	sample.setFarClippingPlane(yon);

	// We need to output the filmback fit and post projection transform
	Alembic::AbcGeom::FilmBackXformOp	winsize(
		Alembic::AbcGeom::kScaleFilmBackOperation, "winsize");
	sample.addOp(winsize);
	sample[0].setChannelValue(0, winsizex);
	sample[0].setChannelValue(1, winsizey);
	return true;
    }

    static const char	*theChannelNames[] = {
	"winx",
	"winy",
	"winsizex",
	"winsizey",
	"focal",
	"aperture",
	"aspect",
	"resx",
	"resy",
	"fstop",
	"focus",
	"near",
	"far",
	"shutter",
    };
    #define NUM_CHANNELS	(sizeof(theChannelNames)/sizeof(const char *))

    static bool
    checkTimeDependent(OBJ_Camera *cam)
    {
	// check if there are any animated parameters
	for (int i = 0; i < NUM_CHANNELS; ++i)
	{
	    const CH_Channel	*chp = cam->getChannel(theChannelNames[i]);
	    if (chp && chp->isTimeDependent())
		return true;
	}
	return false;
    }

    OBJ_Camera *
    getCamera(int id)
    {
	OP_Node	*node = OP_Node::lookupNode(id);
	return UTverify_cast<OBJ_Camera *>(node);
    }

    void
    nodeFullPath(UT_WorkBuffer &buf, int id)
    {
	OBJ_Camera	*o = getCamera(id);
	if (!o)
	    buf.sprintf("Camera with unique id %d was deleted", id);
	else
	    o->getFullPath(buf);
    }
}

ROP_AbcOpCamera::ROP_AbcOpCamera(OBJ_Camera *node)
    : myCameraId(node ? node->getUniqueId() : -1)
    , myOCamera()
    , myTimeDependent(false)
{
}

ROP_AbcOpCamera::~ROP_AbcOpCamera()
{
}

bool
ROP_AbcOpCamera::start(const OObject &parent,
	GABC_OError &err, const ROP_AbcContext &ctx, UT_BoundingBox &box)
{
    OBJ_Camera	*cam = getCamera(myCameraId);
    myOCamera = OCamera(parent, name(), ctx.timeSampling());
    if (!cam || !myOCamera)
    {
	UT_WorkBuffer	fullpath;
	nodeFullPath(fullpath, myCameraId);
	return err.error("Unable to create camera for %s", fullpath.buffer());
    }

    UT_ASSERT(!childCount());

    myTimeDependent = checkTimeDependent(cam);

    // Now, just do an update
    return update(err, ctx, box);
}

bool
ROP_AbcOpCamera::update(GABC_OError &err,
	const ROP_AbcContext &ctx, UT_BoundingBox &box)
{
    OBJ_Camera	*cam = getCamera(myCameraId);
    if (!cam)
	return false;
    CameraSample	sample;
    fillSample(sample, cam, ctx);
    myOCamera.getSchema().set(sample);
    box.initBounds(0, 0, 0);

    return true;
}

bool
ROP_AbcOpCamera::selfTimeDependent() const
{
    return myTimeDependent;
}

bool
ROP_AbcOpCamera::getLastBounds(UT_BoundingBox &box) const
{
    box.initBounds(0, 0, 0);
    return true;
}
