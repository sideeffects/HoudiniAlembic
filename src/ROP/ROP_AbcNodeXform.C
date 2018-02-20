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

#include "ROP_AbcNodeXform.h"

typedef Alembic::Abc::OCompoundProperty OCompoundProperty;

void
ROP_AbcNodeXform::setArchive(const ROP_AbcArchivePtr &archive)
{
    myOXform = OXform();
    myVisibility = OVisibilityProperty();
    ROP_AbcNode::setArchive(archive);
    myUserProperties.clear();
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
ROP_AbcNodeXform::preUpdate(bool locked)
{
    if(!locked)
    {
	myVisible = GABC_VisibilityType::GABC_VISIBLE_HIDDEN;
	myUserPropVals.clear();
	myUserPropMeta.clear();
    }
    ROP_AbcNode::preUpdate(locked);
}

void
ROP_AbcNodeXform::update(const GABC_LayerOptions &layerOptions)
{
    makeValid();

    // TODO: Remove this temporary mapping for the visibility.
    bool visible = (myVisible != GABC_VisibilityType::GABC_VISIBLE_HIDDEN);

    myBox.initBounds();
    for(auto &it : myChildren)
    {
	it.second->update(layerOptions);
	myBox.enlargeBounds(it.second->getBBox());
    }

    Box3d b3 = GABC_Util::getBox(myBox);
    myBox.transform(myMatrix);

    bool full_bounds = myArchive->getOOptions().fullBounds();
    exint nsamples = myArchive->getSampleCount();
    for(; mySampleCount < nsamples; ++mySampleCount)
    {
	auto &schema = myOXform.getSchema();
	bool cur = (mySampleCount + 1 == nsamples);

	if(!mySampleCount || cur)
	{
	    bool vis = (visible && cur);
	    XformSample sample;
	    sample.setMatrix(GABC_Util::getM(myMatrix));
	    schema.set(sample);
	    myVisibility.set(vis ? Alembic::AbcGeom::kVisibilityDeferred
				 : Alembic::AbcGeom::kVisibilityHidden);
	}
	else
	{
	    schema.setFromPrevious();
	    myVisibility.set(Alembic::AbcGeom::kVisibilityHidden);
	}

	// update computed bounding box
	if(full_bounds)
	    schema.getChildBoundsProperty().set(b3);

	// update user properties
	if(!myUserPropVals.empty() && !myUserPropMeta.empty())
	{
	    OCompoundProperty props = schema.getUserProperties();
	    myUserProperties.update(props, myUserPropVals, myUserPropMeta, myArchive);
	}
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
