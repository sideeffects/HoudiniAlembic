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
ROP_AbcNodeXform::purgeObjects()
{
    myOXform = OXform();
    myVisibility = OVisibilityProperty();
    ROP_AbcNode::purgeObjects();
    myUserProperties.clear();
    mySampleCount = 0;
    myIsValid = false;
}

OObject
ROP_AbcNodeXform::getOObject(ROP_AbcArchive &archive, GABC_OError &err)
{
    UT_ASSERT(myLayerNodeType != GABC_LayerOptions::LayerType::DEFER);
    makeValid(archive, err);
    return myOXform;
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
    const GABC_LayerOptions &layerOptions, GABC_OError &err)
{
    Box3d bbox;

    myBox.initBounds();
    if(myLayerNodeType != GABC_LayerOptions::LayerType::PRUNE)
    {
	for(auto &it : myChildren)
	{
	    it.second->update(archive, layerOptions, err);
	    myBox.enlargeBounds(it.second->getBBox());
	}

	bbox = GABC_Util::getBox(myBox);
	myBox.transform(myMatrix);
    }

    // The defer node won't be exported.
    if(myLayerNodeType == GABC_LayerOptions::LayerType::DEFER)
	return;

    makeValid(archive, err);

    // The prune node doesn't need further implementation.
    if(myLayerNodeType == GABC_LayerOptions::LayerType::PRUNE)
	return;

    auto visibility = layerOptions.getVizType(myPath, myLayerNodeType);
    if(visibility == GABC_LayerOptions::VizType::DEFAULT)
	visibility = myVisible;

    bool full_bounds = archive.getOOptions().fullBounds();
    exint nsamples = archive.getSampleCount();
    for(; mySampleCount < nsamples; ++mySampleCount)
    {
	auto &schema = myOXform.getSchema();
	bool cur = (mySampleCount + 1 == nsamples);
	auto viz = visibility;

	if(!mySampleCount || cur)
	{
	    if(!cur)
		viz = GABC_LayerOptions::VizType::HIDDEN;

	    if(myLayerNodeType == GABC_LayerOptions::LayerType::FULL
		|| myLayerNodeType == GABC_LayerOptions::LayerType::REPLACE)
	    {
		XformSample sample;
		sample.setMatrix(GABC_Util::getM(myMatrix));
		schema.set(sample);
	    }
	}
	else
	{
	    schema.setFromPrevious();
	    viz = GABC_LayerOptions::VizType::HIDDEN;
	}

	myVisibility.set(GABC_LayerOptions::getOVisibility(viz));

	// update computed bounding box
	if(full_bounds)
	    schema.getChildBoundsProperty().set(bbox);

	// update user properties
	if(!myUserPropVals.empty() && !myUserPropMeta.empty())
	{
	    OCompoundProperty props = schema.getUserProperties();
	    myUserProperties.update(props, myUserPropVals, myUserPropMeta,
		archive, err, layerOptions, myLayerNodeType);
	}
    }
}

void
ROP_AbcNodeXform::makeValid(ROP_AbcArchive &archive, GABC_OError &err)
{
    UT_ASSERT(myLayerNodeType != GABC_LayerOptions::LayerType::DEFER);

    if(myIsValid)
	return;

    auto metadata = Alembic::Abc::MetaData();
    auto sparseFlag = GABC_LayerOptions::getSparseFlag(myLayerNodeType);
    GABC_LayerOptions::getMetadata(metadata, myLayerNodeType);

    myOXform = OXform(myParent->getOObject(archive, err),
	myName, archive.getTimeSampling(), metadata, sparseFlag);
    myVisibility = CreateVisibilityProperty(myOXform, archive.getTimeSampling());
    myIsValid = true;
}
