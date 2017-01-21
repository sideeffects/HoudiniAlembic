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

#ifndef __ROP_AbcNodeXform__
#define __ROP_AbcNodeXform__

#include "ROP_AbcNode.h"

#include <Alembic/AbcGeom/All.h>

#include <GABC/GABC_OGTGeometry.h>
#include <UT/UT_Matrix4.h>

#include "ROP_AbcUserProperties.h"

typedef Alembic::AbcGeom::OVisibilityProperty OVisibilityProperty;
typedef Alembic::AbcGeom::OXform OXform;
typedef Alembic::AbcGeom::XformSample XformSample;
typedef Alembic::AbcGeom::OXformSchema OXformSchema;

typedef GABC_NAMESPACE::GABC_Util GABC_Util;
typedef GABC_NAMESPACE::GABC_OGTGeometry::VisibilityCache VisibilityCache;

class ROP_AbcXformCache
{
public:
    ROP_AbcXformCache() : myCount(-1) {}
    void clear() { myCount = -1; }
    void set(OXformSchema &schema, const UT_Matrix4D &xform)
    {
	if((myCount >= 0) && (myXform == xform))
	    ++myCount;
	else
	{
	    for(exint i = 0; i < myCount; ++i)
		setSample(schema);
	    myXform = xform;
	    myCount = 0;
	    setSample(schema);
	}
    }

private:
    void setSample(OXformSchema &schema)
    {
	XformSample sample;
	sample.setMatrix(GABC_Util::getM(myXform));
	schema.set(sample);
    }

    UT_Matrix4D myXform;
    exint myCount;
};

/// Class describing a transform exported to an Alembic archive.
class ROP_AbcNodeXform : public ROP_AbcNode
{
public:
    ROP_AbcNodeXform(const std::string &name)
	: ROP_AbcNode(name), myMatrix(1), myPreMatrix(1), myPreMatrixSet(false),
	  myIsValid(false), myVisible(false), myLocked(false) {}

    virtual OObject getOObject();
    virtual void clearData();
    virtual void setArchive(const ROP_AbcArchivePtr &archive);
    virtual void update();

    /// When locked update() uses the previously written sample instead of the
    /// current sample.
    void setLocked(bool locked);
    /// Sets the current user properties.
    void setUserProperties(const UT_String &vals, const UT_String &meta)
	    { myUserPropVals = vals; myUserPropMeta = meta; }
    /// Sets the current transform.
    void setData(const UT_Matrix4D &m, bool visible)
	    { myMatrix = m; myVisible = visible; }
    /// Sets an option pretransform.
    bool setPreMatrix(const UT_Matrix4D &m);
    /// Gets the current transform.
    const UT_Matrix4D &getMatrix() const { return myMatrix; }

private:
    void makeValid();

    OXform myOXform;
    OVisibilityProperty myVisibility;
    ROP_AbcBBoxCache myBBoxCache;
    ROP_AbcXformCache myXformCache;
    VisibilityCache myVisibilityCache;

    UT_StringHolder myUserPropVals;
    UT_StringHolder myUserPropMeta;
    ROP_AbcUserProperties myUserProperties;

    UT_Matrix4D myMatrix;
    UT_Matrix4D myPreMatrix;
    exint mySampleCount;
    bool myPreMatrixSet;
    bool myIsValid;
    bool myVisible;
    bool myLocked;
};

#endif
