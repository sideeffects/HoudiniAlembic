/*
 * Copyright (c) 2013
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

#ifndef __ROP_AbcObject__
#define __ROP_AbcObject__

#include <GABC/GABC_Include.h>
#include <Alembic/Abc/OObject.h>
#include <UT/UT_StringMap.h>
#include <UT/UT_BoundingBox.h>

class GABC_OError;
class ROP_AbcContext;
class UT_WorkBuffer;

/// Base class for all Alembic output objects
class ROP_AbcObject
{
public:
    typedef UT_StringMap<ROP_AbcObject *>	ChildContainer;
    typedef Alembic::Abc::OObject		OObject;

    ROP_AbcObject();
    virtual ~ROP_AbcObject();

    /// Return the class name
    virtual const char	*className() const = 0;

    /// Return the name of the node
    const std::string	&name() const { return myName; }
    /// Names should only be set when added to the parent object.  This
    /// guarantees that the names are unique.
    void		 setName(const std::string &name) { myName = name; }

    /// Write the first sample to the Alembic file.  Please call
    /// updateTimeDependentKids() after processing children
    virtual bool		 start(const OObject &parent,
					    GABC_OError &err,
					    const ROP_AbcContext &context,
					    UT_BoundingBox &bounds) = 0;
    /// Write a subsequent sample to the Alembic file
    virtual bool		 update(GABC_OError &err,
					    const ROP_AbcContext &context,
					    UT_BoundingBox &bounds) = 0;
    /// Return whether the node is time dependent.  This is only valid after
    /// the call to start().
    virtual bool		 selfTimeDependent() const = 0;

    /// Return the last computed box (for non-time dependent code)
    virtual bool		 getLastBounds(UT_BoundingBox &box) const = 0;

    /// Return whether any children (or grandchildren) are time dependent
    bool		 childrenTimeDependent() const
				{ return myTimeDependentKids; }

    /// Returns whether I or any of my children are time dependent
    bool	timeDependent() const
			{ return myTimeDependentKids || selfTimeDependent(); }

    exint	childCount() const	{ return myKids.size(); }

    /// Adding a child may possibly rename the child if there's a duplicate name
    /// Returns true if the child was added
    /// @note This will set the name on the child
    bool	addChild(const char *name, ROP_AbcObject *kid,
			    bool rename_duplicate=true);

    /// Access to the children
    const ChildContainer	&getChildren() const	{ return myKids; }

protected:
    /// Clear out children
    void	deleteChildren();

    /// Call updateTimeDependentKids() after processing kids in start()
    void	updateTimeDependentKids();

    /// Process all children, writing their first sample.
    /// The @c myobject parameter should be the object associated with this
    /// node (not the parent as passed into start()).
    /// On return, the kidbox will be the union of the bounds for all children
    bool	startChildren(const OObject &myobject,
				GABC_OError &err,
				const ROP_AbcContext &context,
				UT_BoundingBox &kidbox);
    /// Process all children for subsequent samples
    bool	updateChildren(GABC_OError &err,
				const ROP_AbcContext &context,
				UT_BoundingBox &kidbox);
    /// Called by addChild() when there are duplicate names
    virtual const char	*renameChild(const char *base,
				    ROP_AbcObject *kid,
				    UT_WorkBuffer &storage,
				    int retries) const;
private:
    ChildContainer	myKids;
    std::string		myName;
    bool		myTimeDependentKids;
};

#endif
