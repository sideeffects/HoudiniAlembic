/*
 * Copyright (c) 2016
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

#ifndef __ROP_AbcNodeCamera__
#define __ROP_AbcNodeCamera__

#include "ROP_AbcNode.h"

#include <Alembic/AbcGeom/All.h>

#include <UT/UT_Matrix4.h>

typedef Alembic::AbcGeom::OCamera OCamera;

/// Class describing a camera exported to an Alembic archive.
class ROP_AbcNodeCamera : public ROP_AbcNode
{
public:
    ROP_AbcNodeCamera(const std::string &name, int resx, int resy)
	: ROP_AbcNode(name), myResX(resx), myResY(resy), myIsValid(false)
    {
	myBox.initBounds(0, 0, 0);
    }

    virtual OObject getOObject();
    virtual void setArchive(const ROP_AbcArchivePtr &archive);
    virtual void update();

    /// sets the current camera settings
    void setData(fpreal focal, fpreal fstop, fpreal focus, fpreal shutter,
		 fpreal hither, fpreal yon, fpreal winsizex, fpreal winsizey,
		 fpreal aperturex, fpreal aperturey,
		 fpreal offsetx, fpreal offsety, fpreal squeeze)
	    {
		myFocalLength = focal;
		myFStop = fstop;
		myFocusDistance = focus;
		myShutterClose = shutter;
		myNearClippingPlane = hither;
		myFarClippingPlane = yon;
		myWinSizeX = winsizex;
		myWinSizeY = winsizey;
		myHorizontalAperture = aperturex;
		myVerticalAperture = aperturey;
		myHorizontalFilmOffset = offsetx;
		myVerticalFilmOffset = offsety;
		myLensSqueezeRatio = squeeze;
	    }

private:
    void makeValid();

    OCamera myOCamera;
    int myResX;
    int myResY;
    fpreal myFocalLength;
    fpreal myFStop;
    fpreal myFocusDistance;
    fpreal myShutterClose;
    fpreal myNearClippingPlane;
    fpreal myFarClippingPlane;
    fpreal myWinSizeX;
    fpreal myWinSizeY;
    fpreal myHorizontalAperture;
    fpreal myVerticalAperture;
    fpreal myHorizontalFilmOffset;
    fpreal myVerticalFilmOffset;
    fpreal myLensSqueezeRatio;
    bool myIsValid;
};

#endif
