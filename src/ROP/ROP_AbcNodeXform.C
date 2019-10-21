/*
 * Copyright (c) 2019
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

void
ROP_AbcNodeXform::purgeObjects()
{
    myOXform = OXform();
    myOObject = OObject();
    mySchema = OCompoundProperty();
    myVisibility = OVisibilityProperty();
    myChildBounds = OBox3dProperty();
    myUserProps = OCompoundProperty();
    ROP_AbcNode::purgeObjects();
    myUserProperties.clear();
    mySampleCount = 0;
    myIsValid = false;
    myIsSparse = false;
    myVizValid = false;
}

OObject
ROP_AbcNodeXform::getOObject(ROP_AbcArchive &archive, GABC_OError &err)
{
    UT_ASSERT(myLayerNodeType != GABC_LayerOptions::LayerType::NONE);
    makeValid(archive, err);
    return myOObject;
}

void
ROP_AbcNodeXform::getUserPropNames(UT_SortedStringSet &names,
    GABC_OError &err) const
{
    ROP_AbcUserProperties::getTokens(
	names, myUserPropVals, myUserPropMeta, err);
}

void
ROP_AbcNodeXform::clearData(bool locked)
{
    if(!locked)
    {
	myVisible = GABC_LayerOptions::VizType::HIDDEN;
	myUserPropVals.clear();
	myUserPropMeta.clear();
    }
    ROP_AbcNode::clearData(locked);
}

void
ROP_AbcNodeXform::update(ROP_AbcArchive &archive,
    bool displayed, UT_BoundingBox &box,
    const GABC_LayerOptions &layerOptions, GABC_OError &err)
{
    Box3d abcBox;

    auto vizLayerOption = layerOptions.getVizType(myPath, myLayerNodeType);
    if(vizLayerOption > GABC_LayerOptions::VizType::DEFAULT)
	myVisible = vizLayerOption;

    switch(myVisible)
    {
	case GABC_LayerOptions::VizType::HIDDEN:
	    displayed = false;
	    break;

	case GABC_LayerOptions::VizType::VISIBLE:
	    displayed = true;
	    break;

	default:
	    break;
    }

    box.initBounds();
    if(myLayerNodeType != GABC_LayerOptions::LayerType::PRUNE)
    {
	for(auto &it : myChildren)
	{
	    UT_BoundingBox childBox;
	    it.second->update(archive, displayed, childBox, layerOptions, err);
	    box.enlargeBounds(childBox);
	}

	abcBox = GABC_Util::getBox(box);
	box.transform(myMatrix);
    }

    // The defer node won't be exported.
    if(myLayerNodeType == GABC_LayerOptions::LayerType::NONE)
	return;

    makeValid(archive, err);

    if(!myVizValid)
    {
        if(vizLayerOption != GABC_LayerOptions::VizType::NONE)
	{
	    myVisibility =
		Alembic::AbcGeom::CreateVisibilityProperty(myOObject,
					 archive.getTimeSampling());
	}
	myVizValid = true;
    }

    // The prune node doesn't need further implementation.
    if(myLayerNodeType == GABC_LayerOptions::LayerType::PRUNE)
	return;

    bool full_bounds = archive.getOOptions().fullBounds();
    exint nsamples = archive.getSampleCount();
    for(; mySampleCount < nsamples; ++mySampleCount)
    {
	bool cur = (mySampleCount + 1 == nsamples);
	auto viz = myVisible;

	if(!mySampleCount || cur)
	{
	    if(!cur)
		viz = GABC_LayerOptions::VizType::HIDDEN;

	    if(!myIsSparse)
	    {
		// guard against matrix roundoff errors producing an
		// animated transform
		if(!mySampleCount || !myCachedMatrix.isEqual(myMatrix))
		    myCachedMatrix = myMatrix;

		XformSample sample;
		sample.setMatrix(GABC_Util::getM(myCachedMatrix));
		myOXform.getSchema().set(sample);
	    }
	}
	else
	{
	    viz = GABC_LayerOptions::VizType::HIDDEN;

	    if(!myIsSparse)
		myOXform.getSchema().setFromPrevious();
	}

	// update visibility property
	if(myVisibility.valid())
	    myVisibility.set(GABC_LayerOptions::getOVisibility(viz));

	// update computed bounding box
	if(full_bounds)
	{
	    if(!myChildBounds)
	    {
		if(myIsSparse)
		{
		    myChildBounds =
			OBox3dProperty(mySchema, ".childBnds",
				       archive.getTimeSampling());
		}
		else
		{
		    myChildBounds =
			myOXform.getSchema().getChildBoundsProperty();
		}
	    }
	    myChildBounds.set(abcBox);
	}

	// update user properties
	if(!myUserPropVals.empty() && !myUserPropMeta.empty())
	{
	    if(!myUserProps)
	    {
		if(myIsSparse)
		{
		    myUserProps =
			OCompoundProperty(mySchema, ".userProperties");
		}
		else
		    myUserProps = myOXform.getSchema().getUserProperties();
	    }
	    myUserProperties.update(myUserProps, myUserPropVals, myUserPropMeta,
		archive, err, layerOptions, myLayerNodeType);
	}
    }
}

void
ROP_AbcNodeXform::makeValid(ROP_AbcArchive &archive, GABC_OError &err)
{
    UT_ASSERT(myLayerNodeType != GABC_LayerOptions::LayerType::NONE);

    if(myIsValid)
	return;

    UT_ASSERT(myLayerNodeType != GABC_LayerOptions::LayerType::NONE);
    if(myLayerNodeType == GABC_LayerOptions::LayerType::SPARSE)
    {
	myOObject = OObject(myParent->getOObject(archive, err), myName);
	mySchema = OCompoundProperty(myOObject.getPtr()->getProperties(), ".xform");
	myIsSparse = true;
    }
    else
    {
	auto sparseFlag = GABC_LayerOptions::getSparseFlag(myLayerNodeType);
	auto metadata = Alembic::Abc::MetaData();
	GABC_LayerOptions::getMetadata(metadata, myLayerNodeType);

	myOXform = OXform(myParent->getOObject(archive, err),
	    myName, archive.getTimeSampling(), metadata, sparseFlag);
	myOObject = myOXform;
    }

    myIsValid = true;
}
