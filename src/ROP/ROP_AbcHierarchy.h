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

#ifndef __ROP_AbcHierarchy__
#define __ROP_AbcHierarchy__

#include "ROP_AbcHierarchySample.h"
#include "ROP_AbcNodeShape.h"
#include "ROP_AbcNodeXform.h"

class ROP_AbcHierarchy
{
    class Node
    {
    public:
	Node(ROP_AbcNode *node=nullptr) : myAbcNode(node) {}

	// ROP_AbcNodeRoot or ROP_AbcNodeXform this node represents
	ROP_AbcNode *getAbcNode() const { return myAbcNode; }

	// returns child xform nodes
	UT_Map<std::string, Node> &getChildren() { return myChildren; }

	// returns child non-instanced shape nodes
	const UT_Map<std::string, UT_Map<int, UT_Array<ROP_AbcNodeShape *> > >
	    &getShapes() const { return myShapes; }

	// returns child instanced shape nodes
	const UT_Map<std::string, UT_Map<int, UT_Array<exint> > >
	    &getInstancedShapes() const { return myInstancedShapes; }

	// returns the named child xform node creating one if none existed
	Node *setChild(const std::string &name,
		       GABC_OError &err,
		       const UT_Matrix4D &m,
		       const std::string &up_vals,
		       const std::string &up_meta);

	// returns the named child non-instanced shape node creating one if
	// none existed
	void setShape(const std::string &name, int type, exint i,
		      GABC_OError &err,
		      const GT_PrimitiveHandle &prim, bool visible,
		      const std::string &up_vals,
		      const std::string &up_meta,
		      const std::string &subd_grp);

	// adds a child shape node that can be the source of instanced shape
	// nodes
	ROP_AbcNodeShape *
	newInstanceSource(const std::string &name, int type, exint key2,
			  GABC_OError &err,
			  const GT_PrimitiveHandle &prim,
			  const std::string &up_vals,
			  const std::string &up_meta,
			  const std::string &subd_grp);

	// adds a child shape node that is an instance of an existing shape
	// node
	void newInstanceRef(const std::string &name, int type, exint key2,
			    GABC_OError &err,
			    ROP_AbcNodeShape *src);

	// sets the visibility of the xform node
	void setVisibility(bool vis);

    private:
	ROP_AbcNode *myAbcNode; // root or xform
	// map from node name to child xform
	UT_Map<std::string, Node> myChildren;
	// list of child non-instanced shape nodes by name and primitive type
	UT_Map<std::string, UT_Map<int, UT_Array<ROP_AbcNodeShape *> > > myShapes;
	// list of child instanced shape nodes by name and primitive type
	UT_Map<std::string, UT_Map<int, UT_Array<exint> > > myInstancedShapes;
	// FIXME: add support for cameras
    };

public:
    ROP_AbcHierarchy(ROP_AbcNode *root)
	: myRoot(root)
	, myNextInstanceId(0)
	, myLocked(false) {}

    // returns the hierarchy's root
    ROP_AbcNode *getRoot() const { return myRoot.getAbcNode(); }

    // writes a new sample
    void merge(GABC_OError &err,
	       const ROP_AbcHierarchySample &src,
	       const ROP_AbcHierarchySample::InstanceMap &instance_map);

    // get/set a flag indicating if the last written sample was locked
    void setLocked(bool locked) { myLocked = locked; }
    bool getLocked() const { return myLocked; }

private:
    // hierarchy's root
    Node myRoot;
    // map from primitive type and instance ID to the source shape node
    UT_Map<int, UT_Map<exint, ROP_AbcNodeShape *> > myInstancedShapes;
    // ID of next allocated instance
    exint myNextInstanceId;
    // flag indicating if the last written sample was locked
    bool myLocked;
};

#endif
