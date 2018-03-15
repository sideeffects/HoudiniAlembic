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

#ifndef __ROP_AbcNode__
#define __ROP_AbcNode__

#include "ROP_AbcArchive.h"

#include <UT/UT_Map.h>
#include <UT/UT_StringSet.h>
#include <GABC/GABC_LayerOptions.h>
#include <GABC/GABC_Util.h>

typedef GABC_NAMESPACE::GABC_LayerOptions GABC_LayerOptions;
typedef GABC_NAMESPACE::GABC_Util::CollisionResolver CollisionResolver;

/// Class describing a node of data exported to an Alembic archive.  This
/// is subclassed for the various types of nodes found in an Alembic archive.
class ROP_AbcNode
{
public:

    enum NodeType
    {
    	ROOT,
	XFORM,
	SHAPE,
	CAMERA,
	INSTANCE
    };

    ROP_AbcNode(const std::string &name) : myParent(nullptr), myName(name),
	myPath(name), myVisible(GABC_LayerOptions::VizType::HIDDEN) {}
    virtual ~ROP_AbcNode()
    {
	for(auto &it : myChildren)
	    delete it.second;
    }

    /// Returns name of this node.
    const char *getName() const { return myName.c_str(); }
    const char *getPath() const { return myPath.c_str(); };

    /// Sets the layer node type of this node.
    void setLayerNodeType(GABC_LayerOptions::LayerType type)
	{ myLayerNodeType = type; }
    GABC_LayerOptions::LayerType getLayerNodeType() const
	{ return myLayerNodeType; }

    /// Sets the tag of the visibility of this node.
    void setVisibility(GABC_LayerOptions::VizType vis) 
    { 
	// The node should always holds a valid viztype, the
	// DEFAULT only used in layerOption, then guide the 
	// program to obtain the real viztype from the node.
	UT_ASSERT(vis != GABC_LayerOptions::VizType::DEFAULT);
	myVisible = vis;
    }
    void setVisibility(bool vis)
    {
	myVisible = vis ? GABC_LayerOptions::VizType::DEFER
			: GABC_LayerOptions::VizType::HIDDEN;
    }

    /// Updates 'name' to a unique value if this node already has a child with
    /// that name.
    void makeCollisionFreeName(std::string &name, GABC_OError &err) const;

    // Returns the children of this node.
    const UT_SortedMap<std::string, ROP_AbcNode *> &getChildren() const { return myChildren; }

    /// Adds a new child node.
    void addChild(ROP_AbcNode *child);

    /// Returns the parent node.
    ROP_AbcNode *getParent() const { return myParent; }

    /// Returns the OObject from the Alembic archive corresponding to this
    /// node.
    virtual OObject getOObject(ROP_AbcArchive &archive, GABC_OError &err) = 0;
    virtual NodeType getNodeType() const = 0;

    /// Sets a new current Alembic archive.  All references to the previous
    /// Alembic archive are released.
    virtual void purgeObjects()
    {
	for(auto &it : myChildren)
	    it.second->purgeObjects();
    }

    /// Appends the choices inside the given string set.
    virtual void getAttrNames(UT_SortedStringSet &) const {};
    virtual void getUserPropNames(UT_SortedStringSet &, GABC_OError &) const {};

    /// Hook to prepare node for calls to setData().
    virtual void clearData(bool locked) {}
    /// Hook to clean up node after calls to update().
    virtual void updateLocked(bool locked) {}
    /// Exports the current sample data update the computed bounding box.
    virtual void update(ROP_AbcArchive &archive,
	const GABC_LayerOptions &layerOptions, GABC_OError &err) = 0;

    /// Returns the last computed bounding box.
    const UT_BoundingBox &getBBox() const { return myBox; }

protected:
    /// this node's name
    const std::string myName;
    /// this node's full path
    std::string myPath;
    /// this node's parent
    ROP_AbcNode *myParent;
    /// this node's computed bounding box
    UT_BoundingBox myBox;
    /// this node's children
    UT_SortedMap<std::string, ROP_AbcNode *> myChildren;
    /// this node's layer node type
    GABC_LayerOptions::LayerType myLayerNodeType;
    /// this node's visibility tag
    GABC_LayerOptions::VizType myVisible;

private:
    CollisionResolver myResolver;
};

/// Class describing the root node of data exported to an Alembic archive.
class ROP_AbcNodeRoot : public ROP_AbcNode
{
public:
    ROP_AbcNodeRoot() : ROP_AbcNode("") {}

    virtual OObject getOObject(ROP_AbcArchive &archive, GABC_OError &err);
    virtual NodeType getNodeType() const { return NodeType::ROOT; }
    virtual void update(ROP_AbcArchive &archive,
	const GABC_LayerOptions &layerOptions, GABC_OError &err);
};

#endif
