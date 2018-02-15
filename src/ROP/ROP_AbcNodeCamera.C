/*
 * Copyright (c) 2018
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

#include "ROP_AbcNodeCamera.h"

typedef Alembic::Abc::Box3d Box3d;
typedef Alembic::Abc::CompoundPropertyWriterPtr CompoundPropertyWriterPtr;
typedef Alembic::Abc::DataType DataType;
typedef Alembic::Abc::ScalarPropertyWriterPtr ScalarPropertyWriterPtr;

typedef Alembic::AbcGeom::CameraSample CameraSample;
typedef Alembic::AbcGeom::FilmBackXformOp FilmBackXformOp;
typedef Alembic::AbcGeom::OCamera OCamera;

void
ROP_AbcNodeCamera::setArchive(const ROP_AbcArchivePtr &archive)
{
    myOCamera = OCamera();
    ROP_AbcNode::setArchive(archive);
    myIsValid = false;
    mySampleCount = 0;
}

OObject
ROP_AbcNodeCamera::getOObject()
{
    makeValid();
    return myOCamera;
}

void
ROP_AbcNodeCamera::update(const GABC_LayerOptions &layerOptions)
{
    makeValid();

    exint nsamples = myArchive->getSampleCount();
    for(; mySampleCount < nsamples; ++mySampleCount)
    {
	auto &schema = myOCamera.getSchema();
	bool cur = (mySampleCount + 1 == nsamples);

	if(!mySampleCount || cur)
	{
	    fpreal raspect = fpreal(myResY) / myResX;

	    CameraSample sample;
	    sample.setFocalLength(myFocalLength);
	    sample.setFStop(myFStop);
	    sample.setFocusDistance(myFocusDistance);

	    sample.setShutterOpen(0);
	    sample.setShutterClose(myShutterClose);

	    sample.setNearClippingPlane(myNearClippingPlane);
	    sample.setFarClippingPlane(myFarClippingPlane);

	    sample.setOverScanLeft(0);
	    sample.setOverScanRight(0);
	    sample.setOverScanTop(0);
	    sample.setOverScanBottom(0);

	    // We need to output the filmback fit and post projection transform
	    FilmBackXformOp winsize = FilmBackXformOp(Alembic::AbcGeom::kScaleFilmBackOperation, "winsize");
	    sample.addOp(winsize);
	    sample[0].setChannelValue(0, (1.0 / myWinSizeX));
	    sample[0].setChannelValue(1, (1.0 / myWinSizeY));

	    sample.setHorizontalAperture(myHorizontalAperture);
	    sample.setVerticalAperture(raspect * myVerticalAperture);
	    sample.setHorizontalFilmOffset(myHorizontalFilmOffset);
	    sample.setVerticalFilmOffset(raspect * myVerticalFilmOffset);
	    sample.setLensSqueezeRatio(myLensSqueezeRatio);

	    schema.set(sample);
	}
	else
	    schema.setFromPrevious();
    }
}

void
ROP_AbcNodeCamera::makeValid()
{
    if(myIsValid)
	return;

    myOCamera = OCamera(myParent->getOObject(), myName, myArchive->getTimeSampling());

    // XXX: consider merging with general user properties handling
    DataType dtype = DataType(Alembic::Abc::kFloat32POD);
    CompoundPropertyWriterPtr userPropWrtPtr = GetCompoundPropertyWriterPtr(myOCamera.getSchema().getUserProperties());
    ScalarPropertyWriterPtr resXWrtPtr = userPropWrtPtr->createScalarProperty("resx", Alembic::Abc::MetaData(), dtype, 0);
    if (resXWrtPtr)
    {
	Alembic::Util::float32_t sampleResX = myResX; 
	resXWrtPtr->setSample(&sampleResX);
    }
    else
	myArchive->getOError().warning("Failed to export camera resolution x");

    ScalarPropertyWriterPtr resYWrtPtr = userPropWrtPtr->createScalarProperty("resy", Alembic::Abc::MetaData(), dtype, 0);
    if (resYWrtPtr)
    {
	Alembic::Util::float32_t sampleResY = myResY;
	resYWrtPtr->setSample(&sampleResY);
    }
    else
	myArchive->getOError().warning("Failed to export camera resolution y");

    myIsValid = true;
}
