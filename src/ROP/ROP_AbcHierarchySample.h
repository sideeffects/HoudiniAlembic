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

#ifndef __ROP_AbcHierarchySample__
#define __ROP_AbcHierarchySample__

#include <UT/UT_Map.h>
#include <UT/UT_Array.h>
#include <UT/UT_Matrix4.h>
#include <GT/GT_Primitive.h>

#include "ROP_AbcArchive.h"

enum ROP_AlembicPackedTransform
{
    ROP_ALEMBIC_PACKEDTRANSFORM_DEFORM_GEOMETRY,
    ROP_ALEMBIC_PACKEDTRANSFORM_TRANSFORM_GEOMETRY,
    ROP_ALEMBIC_PACKEDTRANSFORM_MERGE_WITH_PARENT_TRANSFORM
};

// class representing next set of Alembic samples to be exported
class ROP_AbcHierarchySample
{
public:
    typedef UT_Map<std::string, UT_Map<int, UT_Array<GT_PrimitiveHandle> > > InstanceMap;

    ROP_AbcHierarchySample(ROP_AbcHierarchySample *parent,
			   const std::string &name)
	: myParent(parent)
	, myName(name)
	, myXform(1)
	, myPreXform(1)
	, myWarnedRoot(false)
	, myWarnedChildren(false)
	, mySetPreXform(false) {}

    // returns the parent xform or root node
    // returns nullptr if this node is a root node
    ROP_AbcHierarchySample *getParent() const { return myParent; }

    // returns the name of this node
    const std::string &getName() const { return myName; }

    // returns all child xforms by name
    UT_SortedMap<std::string, ROP_AbcHierarchySample> &getChildren()
    {
	return myChildren;
    }

    // returns all child xforms by name
    const UT_SortedMap<std::string, ROP_AbcHierarchySample> &getChildren() const
    {
	return myChildren;
    }

    // returns a list of all child non-instanced shapes (along with visibility,
    // and user properties) by name and primitive type
    const UT_SortedMap<std::string, UT_SortedMap<int, UT_Array<std::tuple<GT_PrimitiveHandle, bool, std::string, std::string> > > > &getShapes() const
    {
	return myShapes;
    }

    // returns a list of all child instanced shapes (along with visibility,
    // and user properties) by name and primitive type
    const UT_Map<std::string, UT_Map<int, UT_Array<std::tuple<std::string, exint, bool, std::string, std::string> > > > &getInstancedShapes() const
    {
	return myInstancedShapes;
    }

    // returns the named child xform creating one if none existed
    ROP_AbcHierarchySample *getChildXform(const std::string &name);
    // get/set the node's xform
    void setXform(const UT_Matrix4D &m) { myXform = m; }
    const UT_Matrix4D &getXform() const { return myXform; }
    // get/set the xform pushed from a child node
    bool setPreXform(const UT_Matrix4D &m);
    const UT_Matrix4D &getPreXform() const { return myPreXform; }

    // set user properties for this node
    void setUserProperties(const char *vals, const char *meta)
    {
	myUserPropsVals = vals;
	myUserPropsMeta = meta;
    }

    // get user properties for this node
    std::string getUserPropsVals() const { return myUserPropsVals; }
    std::string getUserPropsMeta() const { return myUserPropsMeta; }

    // adds a child non-instanced shape node
    void appendShape(const std::string &name, const GT_PrimitiveHandle &prim, bool visible, const std::string &userprops, const std::string &userpropsmeta);
    // adds a child instanced shape node
    void appendInstancedShape(const std::string &name, int type, const std::string &key, exint idx, bool visible, const std::string &up_vals, const std::string &up_meta);
    // gets (and increments) the ID for the next child instance transform
    exint getNextInstanceId(const std::string &name);
    // gets (and increments) the ID for the next child packed transform
    exint getNextPackedId(const std::string &name);

    // warn user about problems pushing xforms to parent nodes
    void warnRoot(GABC_OError &err);
    void warnChildren(GABC_OError &err);

private:
    ROP_AbcHierarchySample *myParent;
    const std::string myName;

    // all child xforms by name
    UT_SortedMap<std::string, ROP_AbcHierarchySample> myChildren;
    // list of all child non-instanced shapes (along with visibility, and user
    // properties) by name and primitive type
    UT_SortedMap<std::string, UT_SortedMap<int, UT_Array<std::tuple<GT_PrimitiveHandle, bool, std::string, std::string> > > > myShapes;
    // list of all child instanced shapes (along with visibility, and user
    // properties) by name and primitive type
    UT_Map<std::string, UT_Map<int, UT_Array<std::tuple<std::string, exint, bool, std::string, std::string> > > > myInstancedShapes;
    // number of written instance child xform nodes
    UT_Map<std::string, exint> myInstanceCount;
    // number of written packed child xform nodes
    UT_Map<std::string, exint> myPackedCount;

    // xform of this node
    UT_Matrix4D myXform;
    // xform pushed from a child node
    UT_Matrix4D myPreXform;
    // user properties
    std::string myUserPropsVals;
    std::string myUserPropsMeta;

    // true if we have warned the user about attempting to push a transform to
    // a root node
    bool myWarnedRoot;
    // true if we have warned the user about multiple children attempting to
    // push xforms to the same parent
    bool myWarnedChildren;
    // true if a child node pushed an xform
    bool mySetPreXform;
};

#endif
