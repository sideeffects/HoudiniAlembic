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

typedef Alembic::Abc::Box3d Box3d;
typedef Alembic::Abc::OCompoundProperty OCompoundProperty;

typedef Alembic::AbcGeom::XformSample XformSample;

typedef GABC_NAMESPACE::GABC_Util GABC_Util;

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
    mySampleCount = 0;
    myVisibility = OVisibilityProperty();
    ROP_AbcNode::setArchive(archive);
    myUserProperties.clear();
    myIsValid = false;
    myHasCachedMatrix = false;
    myHasCachedVis = false;
    myHasCachedBounds = false;
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

    XformSample sample;
    UT_Matrix4D m = myPreMatrix * myMatrix;
    sample.setMatrix(GABC_Util::getM(m));

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
	// avoid writing multiple samples for static transforms
	if(myHasCachedMatrix && (m == myCachedMatrix))
	    ++myCachedMatrixCount;
	else
	{
	    if(myHasCachedMatrix)
	    {
		for(; myCachedMatrixCount; --myCachedMatrixCount)
		    myOXform.getSchema().setFromPrevious();
		myHasCachedMatrix = false;
	    }
	    else if(!i)
	    {
		myCachedMatrix = m;
		myCachedMatrixCount = 0;
		myHasCachedMatrix = true;
	    }
	    myOXform.getSchema().set(sample);
	}

	// avoid writing multiple samples for static visibility
	bool vis = (i + 1 == nsamples && myVisible);
	if(myHasCachedVis && (myCachedVisDeferred == vis))
	    ++myCachedVisCount;
	else
	{
	    if(myHasCachedVis)
	    {
		for(; myCachedVisCount; --myCachedVisCount)
		{
		    myVisibility.set(myCachedVisDeferred ?
			    Alembic::AbcGeom::kVisibilityDeferred :
			    Alembic::AbcGeom::kVisibilityHidden);
		}
		myHasCachedVis = false;
	    }
	    else if(!i)
	    {
		myCachedVisDeferred = vis;
		myCachedVisCount = 0;
		myHasCachedVis = true;
	    }
	    myVisibility.set(vis ?
				Alembic::AbcGeom::kVisibilityDeferred :
				Alembic::AbcGeom::kVisibilityHidden);
	}

	// update computed bounding box
	if(full_bounds)
	{
	    // avoid writing multiple samples for static bounds
	    if(myHasCachedBounds && (myCachedBounds == myBox))
		++myCachedBoundsCount;
	    else
	    {
		if(myHasCachedBounds)
		{
		    Box3d b = GABC_Util::getBox(myCachedBounds);
		    for(; myCachedBoundsCount; --myCachedBoundsCount)
			myOXform.getSchema().getChildBoundsProperty().set(b);
		    myHasCachedBounds = false;
		}
		else if(!i)
		{
		    myCachedBounds = myBox;
		    myCachedBoundsCount = 0;
		    myHasCachedBounds = true;
		}
		myOXform.getSchema().getChildBoundsProperty().set(b3);
	    }
	}
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
