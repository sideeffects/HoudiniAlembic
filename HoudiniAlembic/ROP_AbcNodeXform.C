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

#include "ROP_AbcNodeXform.h"

typedef Alembic::Abc::OCompoundProperty OCompoundProperty;

void
ROP_AbcNodeXform::clearData()
{
    if(!myLocked)
    {
	myPreMatrixSet = false;
	myVisible = false;
    }
    ROP_AbcNode::clearData();
}

void
ROP_AbcNodeXform::setArchive(const ROP_AbcArchivePtr &archive)
{
    myOXform = OXform();
    myVisibility = OVisibilityProperty();
    ROP_AbcNode::setArchive(archive);
    myUserProperties.clear();
    myBBoxCache.clear();
    myXformCache.clear();
    myVisibilityCache.clear();
    mySampleCount = 0;
    myIsValid = false;
}

OObject
ROP_AbcNodeXform::getOObject()
{
    makeValid();
    return myOXform;
}

void
ROP_AbcNodeXform::update()
{
    makeValid();

    UT_Matrix4D m = myPreMatrix * myMatrix;
    myBox.initBounds();
    for(auto &it : myChildren)
    {
	it.second->update();
	myBox.enlargeBounds(it.second->getBBox());
    }
    myBox.transform(m);
    Box3d b3 = GABC_Util::getBox(myBox);

    bool full_bounds = myArchive->getOOptions().fullBounds();
    exint nsamples = myArchive->getSampleCount();
    for(exint i = mySampleCount; i < nsamples; ++i)
    {
	myXformCache.set(myOXform.getSchema(), m);
	myVisibilityCache.set(myVisibility, (i + 1 == nsamples) && myVisible);

	// update computed bounding box
	if(full_bounds)
	    myBBoxCache.set(myOXform.getSchema().getChildBoundsProperty(), b3);
    }
    mySampleCount = nsamples;

    // update user properties
    if(myUserPropVals.isstring() && myUserPropMeta.isstring())
    {
	OCompoundProperty props = myOXform.getSchema().getUserProperties();
	myUserProperties.update(props, myUserPropVals, myUserPropMeta, myArchive);
    }
}

void
ROP_AbcNodeXform::makeValid()
{
    if(myIsValid)
	return;

    myOXform = OXform(myParent->getOObject(), myName, myArchive->getTimeSampling());
    myVisibility = CreateVisibilityProperty(myOXform, myArchive->getTimeSampling());
    myIsValid = true;
}

void
ROP_AbcNodeXform::setLocked(bool locked)
{
    myLocked = locked;
    if(myLocked)
	return;

    myPreMatrixSet = false;
    myVisible = false;
}

bool
ROP_AbcNodeXform::setPreMatrix(const UT_Matrix4D &m)
{
    if(!myPreMatrixSet)
    {
	myPreMatrix = m;
	myPreMatrixSet = true;
	return true;
    }
    return false;
}
