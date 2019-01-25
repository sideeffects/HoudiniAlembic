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

#include "ROP_AbcNodeShape.h"
#include <GT/GT_PrimCurveMesh.h>
#include <GT/GT_PrimPolygonMesh.h>

typedef Alembic::Abc::OCompoundProperty OCompoundProperty;

void
ROP_AbcNodeShape::purgeObjects()
{
    ROP_AbcNode::purgeObjects();
    myWriter.reset(nullptr);
    mySampleCount = 0;
    myUserProperties.clear();
}

OObject
ROP_AbcNodeShape::getOObject(ROP_AbcArchive &, GABC_OError &)
{
    return myWriter->getOObject();
}

static void
ropAddAttrNames(UT_SortedStringSet &names, const GT_AttributeList *lst)
{
    if(lst != nullptr)
    {
	const UT_StringArray &curNames = lst->getNames();

	for(auto it = curNames.begin(); it != curNames.end(); ++it)
	{
	    if(!GABC_OGTGeometry::getLayerSkip().contains(it.item()))
		names.insert(it.item());
	}
    }
}

void
ROP_AbcNodeShape::getAttrNames(UT_SortedStringSet &names) const
{
    if(myPrim)
    {
	ropAddAttrNames(names, myPrim->getPointAttributes().get());
	ropAddAttrNames(names, myPrim->getVertexAttributes().get());
	ropAddAttrNames(names, myPrim->getUniformAttributes().get());
	ropAddAttrNames(names, myPrim->getDetailAttributes().get());
    }
}

void
ROP_AbcNodeShape::getFaceSetNames(UT_SortedStringSet &names) const
{
    if(myPrim)
    {
	GT_FaceSetMapPtr facesets;

	switch(myPrim->getPrimitiveType())
	{
	    case GT_PRIM_POLYGON_MESH:
	    case GT_PRIM_SUBDIVISION_MESH:
	    {
		const GT_PrimPolygonMesh *p;
		p = UTverify_cast<const GT_PrimPolygonMesh *>(myPrim.get());
		facesets = p->faceSetMap();
		break;
	    }
	    case GT_PRIM_CURVE_MESH:
	    {
		const GT_PrimCurveMesh *p;
		p = UTverify_cast<const GT_PrimCurveMesh *>(myPrim.get());
		facesets = p->faceSetMap();
		break;
	    }
	    default:
		break;
	}

	if(!facesets)
	    return;

	for(GT_FaceSetMap::iterator it = facesets->begin();
	    !it.atEnd(); ++it)
	{
	    auto &&name = it.name();
	    // skip the group used to specific subdivision surfaces
	    if(::strcmp(mySubdGrp.c_str(), name.c_str()) == 0)
		continue;
	    // skip hidden group
	    if(GA_Names::_3d_hidden_primitives == name)
		continue;

	    names.insert(name);
	}
    }
}

void
ROP_AbcNodeShape::getUserPropNames(UT_SortedStringSet &names,
    GABC_OError &err) const
{
    ROP_AbcUserProperties::getTokens(
	names, myUserPropVals, myUserPropMeta, err);
}

void
ROP_AbcNodeShape::clearData(bool locked)
{
    myLocked = locked;
    if(!locked)
	clear();
    ROP_AbcNode::clearData(locked);
}

void
ROP_AbcNodeShape::updateLocked(bool locked)
{
    if(!locked)
	clear();
    else if(!myLocked)
    {
	if(myPrim)
	    myPrim = myPrim->harden();
    }
    ROP_AbcNode::updateLocked(locked);
}

void
ROP_AbcNodeShape::update(ROP_AbcArchive &archive,
    bool displayed, UT_BoundingBox &box,
    const GABC_LayerOptions &layerOptions, GABC_OError &err)
{
    exint nsamples = archive.getSampleCount();
    archive.getOOptions().setSubdGroup(mySubdGrp.c_str());

    auto vizLayerOption = layerOptions.getVizType(myPath, myLayerNodeType);
    if(vizLayerOption > GABC_LayerOptions::VizType::DEFAULT)
	myVisible = vizLayerOption;

    for(; mySampleCount < nsamples; ++mySampleCount)
    {
	auto viz = myVisible;

	// The defer node won't be exported.
	if(myLayerNodeType != GABC_LayerOptions::LayerType::NONE)
	{
	    if(!myWriter)
	    {
		if(myPrim)
		{
		    // write the first sample
		    mySampleCount = 0;
		    if(nsamples != 1)
			viz = GABC_LayerOptions::VizType::HIDDEN;

		    myWriter.reset(new GABC_OGTGeometry(myName, myLayerNodeType));
		    // transforms for exported geometry
		    myCachedXform = myPrim->getPrimitiveTransform();
		    myWriter->start(myPrim, myParent->getOObject(archive, err),
				    archive.getOOptions(), layerOptions, err,
				    GABC_LayerOptions::getOVisibility(viz),
				    vizLayerOption != GABC_LayerOptions::VizType::NONE);
		}
	    }
	    else
	    {
		if(mySampleCount + 1 != nsamples)
		{
		    viz = GABC_LayerOptions::VizType::HIDDEN;
		}
		else if(!myPrim)
		{
		    viz = GABC_LayerOptions::VizType::HIDDEN;
		    myVisible = GABC_LayerOptions::VizType::HIDDEN;
		}

		if(!myPrim || myLocked)
		{
		    myWriter->updateFromPrevious(err,
				    GABC_LayerOptions::getOVisibility(viz));
		}
		else
		{
		    // guard against matrix roundoff errors producing
		    // animated geometry
		    GT_TransformHandle xform = myPrim->getPrimitiveTransform();
		    bool similar = false;
		    if(xform && myCachedXform)
		    {
			int nsegs0 = xform->getSegments();
			int nsegs1 = myCachedXform->getSegments();
			if(nsegs0 == nsegs1)
			{
			    similar = true;
			    UT_Matrix4D m0, m1;
			    for(int i = 0; i < nsegs0; ++i)
			    {
				xform->getMatrix(m0, i);
				myCachedXform->getMatrix(m1, i);
				if(!m0.isEqual(m1))
				{
				    similar = false;
				    break;
				}
			    }
			}
		    }
		    if(similar)
			myPrim->setPrimitiveTransform(myCachedXform);
		    else
			myCachedXform = xform;

		    myWriter->update(myPrim, archive.getOOptions(), layerOptions,
				     err, GABC_LayerOptions::getOVisibility(viz));
		}
	    }

	    // update user properties
	    if(myWriter && !myUserPropVals.empty() && !myUserPropMeta.empty())
	    {
		OCompoundProperty props = myWriter->getUserProperties();
		myUserProperties.update(props, myUserPropVals, myUserPropMeta,
		    archive, err, layerOptions, myLayerNodeType);
	    }
	}
    }

    box.initBounds();

    if(myLayerNodeType != GABC_LayerOptions::LayerType::PRUNE)
    {
	switch(myVisible)
	{
	    case GABC_LayerOptions::VizType::VISIBLE:
		enlargeBounds(box);
		break;

	    case GABC_LayerOptions::VizType::DEFER:
		if(displayed)
		    enlargeBounds(box);
		break;

	    default:
		break;
	}
    }
}

void
ROP_AbcNodeShape::setData(
    const GT_PrimitiveHandle &prim, bool visible, const std::string &subd)
{
    myPrim = prim;
    myBounds.initBounds();
    setVisibility(visible);
    mySubdGrp = subd;
}

void
ROP_AbcNodeShape::clear()
{
    myPrim = nullptr;
    myBounds.initBounds();
    myVisible = GABC_LayerOptions::VizType::HIDDEN;
    myUserPropVals.clear();
    myUserPropMeta.clear();
}

void
ROP_AbcNodeShape::enlargeBounds(UT_BoundingBox &box)
{
    if(!myBounds.isValid())
    {
	myPrim->enlargeBounds(&myBounds, 1);

	GT_TransformHandle xform = myPrim->getPrimitiveTransform();
	if(xform && !xform->isIdentity())
	{
	    UT_Matrix4D m;
	    xform->getMatrix(m, 0);
	    myBounds.transform(m);
	}
    }
    box.enlargeBounds(myBounds);
}
