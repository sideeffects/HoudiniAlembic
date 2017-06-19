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

#ifndef __ROP_AbcNode__
#define __ROP_AbcNode__

#include "ROP_AbcArchive.h"

#include <UT/UT_Map.h>
#include <GABC/GABC_Util.h>

typedef GABC_NAMESPACE::GABC_Util::CollisionResolver CollisionResolver;

/// Class describing a node of data exported to an Alembic archive.  This
/// is subclassed for the various types of nodes found in an Alembic archive.
class ROP_AbcNode
{
public:
    ROP_AbcNode(const std::string &name) : myParent(nullptr), myName(name) {}
    virtual ~ROP_AbcNode()
    {
	for(auto &it : myChildren)
	    delete it.second;
    }

    /// Returns name of this node.
    const char *getName() const { return myName.c_str(); }

    /// Updates 'name' to a unique value if this node already has a child with
    /// that name.
    void makeCollisionFreeName(std::string &name) const;

    /// Adds a new child node.
    void addChild(ROP_AbcNode *child);

    /// Returns the parent node.
    ROP_AbcNode *getParent() const { return myParent; }

    /// Returns the OObject from the Alembic archive corresponding to this
    /// node.
    virtual OObject getOObject() = 0;

    /// Clears the current sample data.
    virtual void clearData()
    {
	for(auto &it : myChildren)
	    it.second->clearData();
    }

    /// Sets a new current Alembic archive.  All references to the previous
    /// Alembic archive are released.
    virtual void setArchive(const ROP_AbcArchivePtr &archive)
    {
	myArchive = archive;
	for(auto &it : myChildren)
	    it.second->setArchive(archive);
    }

    /// Exports the current sample data update the computed bounding box.
    virtual void update() = 0;

    /// Returns the last computed bounding box.
    const UT_BoundingBox &getBBox() const { return myBox; }

protected:
    /// this node's name
    const std::string myName;
    /// the current Alembic archive
    ROP_AbcArchivePtr myArchive;
    /// this node's parent
    ROP_AbcNode *myParent;
    /// this node's computed bounding box
    UT_BoundingBox myBox;
    /// this node's children
    UT_SortedMap<std::string, ROP_AbcNode *> myChildren;

private:
    CollisionResolver myResolver;
};

/// Class describing the root node of data exported to an Alembic archive.
class ROP_AbcNodeRoot : public ROP_AbcNode
{
public:
    ROP_AbcNodeRoot() : ROP_AbcNode("") {}

    virtual OObject getOObject() { return myArchive->getTop(); }
    virtual void update()
    {
	// The root node has no sample data, just update the computed bounding
	// box.
	myBox.initBounds();
	for(auto &it : myChildren)
	{
	    it.second->update();
	    myBox.enlargeBounds(it.second->getBBox());
	}
        myArchive->setBoundingBox(myBox);
    }
};

#endif
