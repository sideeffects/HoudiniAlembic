/*
 * Copyright (c) 2017
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

#include "ROP_AbcNodeShape.h"

typedef Alembic::Abc::OCompoundProperty OCompoundProperty;

void
ROP_AbcNodeShape::clearData()
{
    if(!myLocked)
	myPrim = nullptr;
    ROP_AbcNode::clearData();
}

void
ROP_AbcNodeShape::setArchive(const ROP_AbcArchivePtr &archive)
{
    ROP_AbcNode::setArchive(archive);
    myWriter.reset(nullptr);
    mySampleCount = 0;
    myUserProperties.clear();
}

OObject
ROP_AbcNodeShape::getOObject()
{
    return myWriter->getOObject();
}

static void
ropEnlargeBounds(UT_BoundingBox &box, const GT_PrimitiveHandle &prim)
{
    GT_TransformHandle xform = prim->getPrimitiveTransform();
    if(!xform || xform->isIdentity())
    {
	prim->enlargeBounds(&box, 1);
	return;
    }

    UT_Matrix4D m;
    xform->getMatrix(m, 0);

    UT_BoundingBox tmp;
    prim->enlargeBounds(&tmp, 1);
    tmp.transform(m);
    box.enlargeBounds(tmp);
}

void
ROP_AbcNodeShape::update()
{
    if(!myLocked)
	myBox.initBounds();

    exint nsamples = myArchive->getSampleCount();
    if(!myWriter && myPrim)
    {
	// write the first sample
	myWriter.reset(new GABC_OGTGeometry(myName));
	myWriter->start(myPrim, myParent->getOObject(),
			myArchive->getOOptions(), myArchive->getOError(),
			(nsamples == 1) ?
			    Alembic::AbcGeom::kVisibilityDeferred :
			    Alembic::AbcGeom::kVisibilityHidden);
	ropEnlargeBounds(myBox, myPrim);
	mySampleCount = 1;
    }

    if(!myWriter)
	return;

    if(mySampleCount < nsamples)
    {
	exint hidden = nsamples - mySampleCount;
	if(myPrim)
	    --hidden;

	if(hidden)
	{
	    myWriter->updateFromPrevious(myArchive->getOError(),
					 Alembic::AbcGeom::kVisibilityHidden,
					 hidden);
	}

	if(myPrim)
	{
	    // write the current sample
	    if(myLocked)
	    {
		myWriter->updateFromPrevious(myArchive->getOError(),
					Alembic::AbcGeom::kVisibilityDeferred);
	    }
	    else
	    {
		myWriter->update(myPrim, myArchive->getOOptions(),
				 myArchive->getOError(),
				 Alembic::AbcGeom::kVisibilityDeferred);
		ropEnlargeBounds(myBox, myPrim);
	    }
	}
	mySampleCount = nsamples;
    }

    // update user properties
    if(myUserPropVals.isstring() && myUserPropMeta.isstring())
    {
	OCompoundProperty props = myWriter->getUserProperties();
	myUserProperties.update(props, myUserPropVals, myUserPropMeta, myArchive);
    }
}

void
ROP_AbcNodeShape::setLocked(bool locked)
{   
    myLocked = locked;
    if(locked)
	return;

    myUserPropVals.clear();
    myUserPropMeta.clear();
    myPrim = nullptr;
}
