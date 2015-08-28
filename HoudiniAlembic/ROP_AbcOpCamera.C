/*
 * Copyright (c) 2015
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

#include "ROP_AbcOpCamera.h"
#include <OBJ/OBJ_Camera.h>
#include <CH/CH_Channel.h>
#include <OP/OP_Director.h>
#include <GABC/GABC_OError.h>
#include "ROP_AbcContext.h"

using namespace GABC_NAMESPACE;

namespace
{
    typedef Alembic::Abc::DataType		     DataType;

    typedef Alembic::Abc::BasePropertyWriterPtr      BasePropertyWriterPtr;
    typedef Alembic::Abc::CompoundPropertyWriterPtr  CompoundPropertyWriterPtr;
    typedef Alembic::Abc::ScalarPropertyWriterPtr    ScalarPropertyWriterPtr; 

    typedef Alembic::AbcGeom::CameraSample           CameraSample;
    typedef Alembic::AbcGeom::FilmBackXformOp        FilmBackXformOp;
    typedef Alembic::AbcGeom::OCamera                OCamera;

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

    static OBJ_Camera *
    getCamera(int id)
    {
	OP_Node	*node = OP_Node::lookupNode(id);
	return UTverify_cast<OBJ_Camera *>(node);
    }

    static void
    cameraNodeFullPath(UT_WorkBuffer &buf, int id)
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
    myOCamera = OCamera(parent, getName(), ctx.timeSampling());
    if (!cam || !myOCamera)
    {
	UT_WorkBuffer	fullpath;
	cameraNodeFullPath(fullpath, myCameraId);
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
    fillSample(sample, cam, ctx, err);
    myOCamera.getSchema().set(sample);
    box.initBounds(0, 0, 0);

    return true;
}

bool
ROP_AbcOpCamera::fillSample(CameraSample &sample, OBJ_Camera *cam, const ROP_AbcContext &ctx, GABC_OError &err)
{
    const CH_Manager   *chman = OPgetDirector()->getChannelManager();
    FilmBackXformOp     winsize;
    fpreal              now = ctx.cookTime();
    fpreal              winx = cam->WINX(now);
    fpreal              winy = cam->WINY(now);
    fpreal              winsizex = cam->WINSIZEX(now);
    fpreal              winsizey = cam->WINSIZEY(now);
    fpreal              focal = cam->FOCAL(now);
    fpreal              aperture = cam->APERTURE(now);
    fpreal              aspect = cam->ASPECT(now);
    fpreal              resx = cam->RESX(now);
    fpreal              resy = cam->RESY(now);
    fpreal              fstop = cam->FSTOP(now);
    fpreal              focus = cam->FOCUS(now);
    fpreal              hither = cam->getNEAR(now);
    fpreal              yon = cam->getFAR(now);
    fpreal              shutter = cam->SHUTTER(now);

    // Compute resolution aspect
    fpreal		raspect = resy/resx;

    // Alembic stores value in cm. (not mm.)
    aperture *= 0.1;

    sample.setFocalLength(focal);
    sample.setFStop(fstop);
    sample.setFocusDistance(focus);

    // For Alembic camera, shutter open/close time are stored in seconds.
    // For Houdini camera, shutter time is stored in frames.  
    sample.setShutterOpen(0);
    sample.setShutterClose(chman->getTimeDelta(shutter));

    sample.setNearClippingPlane(hither);
    sample.setFarClippingPlane(yon);

    sample.setOverScanLeft(0);
    sample.setOverScanRight(0);
    sample.setOverScanTop(0);
    sample.setOverScanBottom(0);

    // We need to output the filmback fit and post projection transform
    winsize = FilmBackXformOp(Alembic::AbcGeom::kScaleFilmBackOperation, "winsize");
    sample.addOp(winsize);
    sample[0].setChannelValue(0, (1.0 / winsizex));
    sample[0].setChannelValue(1, (1.0 / winsizey));

    sample.setHorizontalAperture(aperture / aspect);
    sample.setVerticalAperture(aperture * raspect / aspect);
    sample.setHorizontalFilmOffset(winx * aperture / aspect);
    sample.setVerticalFilmOffset(winy * aperture * raspect / aspect);
    sample.setLensSqueezeRatio(aspect);

    // Write out Houdini camera resolution as the camera's user properties
    DataType dtype = DataType(Alembic::Abc::kFloat32POD);
    CompoundPropertyWriterPtr userPropWrtPtr = GetCompoundPropertyWriterPtr(myOCamera.getSchema().getUserProperties());

    ScalarPropertyWriterPtr resXWrtPtr = NULL;
    BasePropertyWriterPtr resXProp = userPropWrtPtr->getProperty("resx");
    if (!resXProp)
    	resXWrtPtr = userPropWrtPtr->createScalarProperty("resx", Alembic::Abc::MetaData(), dtype, now);
    else
    	resXWrtPtr = resXProp->asScalarPtr();

    if (resXWrtPtr)
    {
    	Alembic::Util::float32_t sampleResX = resx;
    	resXWrtPtr->setSample(&sampleResX);
    }
    else
    {
    	err.warning("Failed to export camera resolution x");
    }

    ScalarPropertyWriterPtr resYWrtPtr = NULL;
    BasePropertyWriterPtr resYProp = userPropWrtPtr->getProperty("resy");
    if (!resYProp)
        resYWrtPtr = userPropWrtPtr->createScalarProperty("resy", Alembic::Abc::MetaData(), dtype, now);
    else
    	resYWrtPtr = resYProp->asScalarPtr();

    if (resYWrtPtr)
    {
    	Alembic::Util::float32_t sampleResY = resy;
    	resYWrtPtr->setSample(&sampleResY);
    }
    else
    {
    	err.warning("Failed to export camera resolution y");
    }

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
