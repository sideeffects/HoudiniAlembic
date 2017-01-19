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

#include <GT/GT_FaceSetMap.h>
#include <GT/GT_PrimPolygonMesh.h>
#include <GT/GT_PrimCurveMesh.h>

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
    myCached = nullptr;
}

OObject
ROP_AbcNodeShape::getOObject()
{
    return myWriter->getOObject();
}

static bool
ropCompare(const GT_AttributeListHandle &a, const GT_AttributeListHandle &b)
{
    int n = a->entries();
    if(b->entries() != n)
	return false;

    int nsegs = a->getSegments();
    if(b->getSegments() != nsegs)
	return false;

    for(int i = 0; i < n; ++i)
    {
	if(strcmp(a->getName(i), b->getName(i)))
	    return false;
	for(int j = 0; j < nsegs; ++j)
	{
	    if(!a->get(i, j)->isEqual(*b->get(i, j)))
		return false;
	}
    }

    return true;
}

static bool
ropCompare(const GT_FaceSetMapPtr &a, const GT_FaceSetMapPtr &b)
{
    if(!a)
	return !b;

    if(!b || (a->entries() != b->entries()))
	return false;

    for(auto it = a->begin(); it != a->end(); ++it)
    {
	GT_FaceSetPtr faceset = b->find(it.name().c_str());
	if(!faceset
	    || !it.faceSet()->extractMembers()->isEqual(*faceset->extractMembers()))
	    return false;
    }

    return true;
}

static bool
ropCompare(const GT_PrimitiveHandle &a, const GT_PrimitiveHandle &b)
{
    if (*a->getPrimitiveTransform() != *b->getPrimitiveTransform()
	|| !ropCompare(a->getPointAttributes(), b->getPointAttributes())
	|| !ropCompare(a->getVertexAttributes(), b->getVertexAttributes())
	|| !ropCompare(a->getUniformAttributes(), b->getUniformAttributes())
	|| !ropCompare(a->getDetailAttributes(), b->getDetailAttributes()))
	return false;

    switch(a->getPrimitiveType())
    {
	case GT_PRIM_POLYGON_MESH:
	case GT_PRIM_SUBDIVISION_MESH:
	    {
		if(!ropCompare(UTverify_cast<const GT_PrimPolygonMesh *>(a.get())->faceSetMap(),
			       UTverify_cast<const GT_PrimPolygonMesh *>(b.get())->faceSetMap()))
		    return false;
	    }
	    break;

	case GT_PRIM_CURVE_MESH:
	    {
		if(!ropCompare(UTverify_cast<const GT_PrimCurveMesh *>(a.get())->faceSetMap(),
			       UTverify_cast<const GT_PrimCurveMesh *>(b.get())->faceSetMap()))
		    return false;
	    }
	    break;
    }

    return true; 
}

void
ROP_AbcNodeShape::update()
{
    if(!myLocked)
	myBox.initBounds();

    exint nsamples = myArchive->getSampleCount();
    if(!myWriter && myPrim)
    {
	myCached = myPrim->harden();
	myCachedCount = 0;
	myCachedVisDeferred = (nsamples == 1);

	// write the first sample
	myWriter.reset(new GABC_OGTGeometry(myName));
	myWriter->start(myPrim, myParent->getOObject(),
			myArchive->getOOptions(), myArchive->getOError(),
			myCachedVisDeferred ?
			    Alembic::AbcGeom::kVisibilityDeferred :
			    Alembic::AbcGeom::kVisibilityHidden);
	myPrim->enlargeBounds(&myBox, 1);
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
	    // avoid writing multiple samples for static geometry
	    if(myCached && !myCachedVisDeferred)
		myCachedCount += hidden;
	    else
	    {
		if(myCached)
		{
		    myWriter->updateFromPrevious(myArchive->getOError(),
						 Alembic::AbcGeom::kVisibilityDeferred,
						 myCachedCount);
		    myCached = nullptr;
		}
		myWriter->updateFromPrevious(myArchive->getOError(),
					     Alembic::AbcGeom::kVisibilityHidden,
					     hidden);
	    }
	}

	if(myPrim)
	{
	    // write the current sample
	    if(myLocked)
	    {
		// avoid writing multiple samples for static geometry
		if(myCached && myCachedVisDeferred)
		    ++myCachedCount;
		else
		{
		    if(myCached)
		    {
			myWriter->updateFromPrevious(myArchive->getOError(),
						     Alembic::AbcGeom::kVisibilityHidden,
						     myCachedCount);
			myCached = nullptr;
		    }

		    myWriter->updateFromPrevious(myArchive->getOError(),
					Alembic::AbcGeom::kVisibilityDeferred);
		}
	    }
	    else
	    {
		// avoid writing multiple samples for static geometry
		if(myCached && myCachedVisDeferred && ropCompare(myPrim, myCached))
		    ++myCachedCount;
		else
		{
		    if(myCached)
		    {
			myWriter->updateFromPrevious(myArchive->getOError(),
						     myCachedVisDeferred ? Alembic::AbcGeom::kVisibilityDeferred : Alembic::AbcGeom::kVisibilityHidden,
						     myCachedCount);
			myCached = nullptr;
		    }

		    myWriter->update(myPrim, myArchive->getOOptions(),
				     myArchive->getOError(),
				     Alembic::AbcGeom::kVisibilityDeferred);
		}
		myPrim->enlargeBounds(&myBox, 1);
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
