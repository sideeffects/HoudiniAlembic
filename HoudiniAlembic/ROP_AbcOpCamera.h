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

#ifndef __ROP_AbcOpCamera__
#define __ROP_AbcOpCamera__

#include "ROP_AbcObject.h"
#include "ROP_AbcContext.h"

class OBJ_Camera;

/// An Alembic camera node as represented by a Houdini camera object.
class ROP_AbcOpCamera : public ROP_AbcObject
{
public:
    typedef Alembic::AbcGeom::CameraSample      CameraSample;
    typedef Alembic::AbcGeom::OCamera	        OCamera;
    typedef Alembic::AbcGeom::OObject	        OObject;

    ROP_AbcOpCamera(OBJ_Camera *node);
    virtual ~ROP_AbcOpCamera();

    /// @{
    /// Interface defined on ROP_AbcObject
    virtual const char *className() const	{ return "OpCamera"; }
    virtual bool	start(const OObject &parent,
				GABC_OError &err,
				const ROP_AbcContext &ctx,
				UT_BoundingBox &box);
    virtual bool	update(GABC_OError &err,
				const ROP_AbcContext &ctx,
				UT_BoundingBox &box);
    virtual bool	selfTimeDependent() const;
    virtual bool	getLastBounds(UT_BoundingBox &box) const;

    bool                fillSample(CameraSample &sample, 
                                    OBJ_Camera *cam, 
                                    const ROP_AbcContext &ctx,
                                    GABC_OError &err);
    /// @}

private:
    UT_BoundingBox	 myBox;
    OCamera		 myOCamera;
    int			 myCameraId;
    bool		 myTimeDependent;
};

#endif

