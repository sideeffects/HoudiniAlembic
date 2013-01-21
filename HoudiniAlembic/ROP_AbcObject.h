/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Side Effects Software Inc
 *	123 Front Street West, Suite 1401
 *	Toronto, Ontario
 *	Canada   M5J 2M2
 *	416-504-9876
 *
 * NAME:	ROP_AbcObject.h ( ROP Library, C++)
 *
 * COMMENTS:
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
