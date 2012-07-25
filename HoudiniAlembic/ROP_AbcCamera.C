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
 * NAME:	ROP_AbcCamera.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#include "ROP_AbcCamera.h"
#include <OBJ/OBJ_Camera.h>
#include <OP/OP_Director.h>
#include "ROP_AbcError.h"
#include "ROP_AbcContext.h"

// The camera shape node is a child of the camera's transform node
static const char	*theCamName = "camProperties";

ROP_AbcCamera::ROP_AbcCamera(OBJ_Camera *obj,
	Alembic::AbcGeom::OObject parent,
	Alembic::AbcGeom::TimeSamplingPtr timeSampling,
	bool time_dep)
    : myObject(obj)
    , myCamera(NULL)
    , myTimeDependent(time_dep)
{
    myCamera = new Alembic::AbcGeom::OCamera(parent, theCamName, timeSampling);
}

ROP_AbcCamera::~ROP_AbcCamera()
{
    delete myCamera;
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

bool
ROP_AbcCamera::isCameraTimeDependent(OBJ_Camera *cam)
{
    for (int i = 0; i < NUM_CHANNELS; ++i)
    {
	if (cam->getChannel(theChannelNames[i]))
	    return true;
    }
    return false;
}

OBJ_Camera *
ROP_AbcCamera::getObject() const
{
    return myObject;
}

const char *
ROP_AbcCamera::getFullPath(UT_WorkBuffer &path) const
{
    myObject->getFullPath(path);
    return path.buffer();
}


bool
ROP_AbcCamera::writeSample(ROP_AbcError &err, ROP_AbcContext &context)
{
    fpreal		 now = context.time();
    fpreal		 winx = myObject->WINX(now);
    fpreal		 winy = myObject->WINY(now);
    fpreal		 winsizex = myObject->WINSIZEX(now);
    fpreal		 winsizey = myObject->WINSIZEY(now);
    fpreal		 focal = myObject->FOCAL(now);
    fpreal		 aperture = myObject->APERTURE(now);
    fpreal		 aspect = myObject->ASPECT(now);
    fpreal		 resx = myObject->RESX(now);
    fpreal		 resy = myObject->RESY(now);
    fpreal		 fstop = myObject->FSTOP(now);
    fpreal		 focus = myObject->FOCUS(now);
    fpreal		 hither = myObject->getNEAR(now);
    fpreal		 yon = myObject->getFAR(now);
    fpreal		 shutter = myObject->SHUTTER(now);
    const CH_Manager	*chman = OPgetDirector()->getChannelManager();

    err.info("Writing camera sample: %g", now);

    // Compute resolution aspect
    fpreal	raspect = resy/resx;

    // Alembic stores value in cm. (not mm.)
    aperture *= 0.1;

    mySample.setFocalLength( focal );
    mySample.setHorizontalAperture( aperture );
    mySample.setVerticalAperture( aperture * raspect);
    mySample.setHorizontalFilmOffset( winx * aperture / aspect );
    mySample.setVerticalFilmOffset( winy * aperture * raspect / aspect);
    mySample.setLensSqueezeRatio( aspect );

    mySample.setOverScanLeft( 0 );
    mySample.setOverScanRight( 0 );
    mySample.setOverScanTop( 0 );
    mySample.setOverScanBottom( 0 );

    mySample.setFStop( fstop );
    mySample.setFocusDistance( focus );

    mySample.setShutterOpen( 0 );
    mySample.setShutterClose( chman->getTimeDelta(shutter) );

    mySample.setNearClippingPlane( hither );
    mySample.setFarClippingPlane( yon );

    if (myCamera->getSchema().getNumSamples() == 0)
    {
	// We need to output the filmback fit and post projection transform
	Alembic::AbcGeom::FilmBackXformOp	winsize(
		Alembic::AbcGeom::kScaleFilmBackOperation, "winsize");
	mySample.addOp(winsize);
    }
    mySample[0].setChannelValue(0, winsizex);
    mySample[0].setChannelValue(1, winsizey);

    myCamera->getSchema().set(mySample);
    return true;
}
