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
 * NAME:	ROP_AbcSOP.h
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcSOP__
#define __ROP_AbcSOP__

#include "ROP_AbcObject.h"
#include "ROP_AbcContext.h"
#include <UT/UT_Array.h>
#include <GT/GT_Handles.h>

class SOP_Node;
class ROP_AbcGTShape;

/// Geometry represented by a SOP node.  This may result in multiple shape
/// nodes (since Houdini can have a heterogenous mix of primitive types).
class ROP_AbcSOP : public ROP_AbcObject
{
public:
    typedef Alembic::Abc::OObject		OObject;

    ROP_AbcSOP(SOP_Node *node);
    virtual ~ROP_AbcSOP();

    /// @{
    /// Interface defined on ROP_AbcObject
    virtual const char	*className() const	{ return "SOP"; }
    virtual bool	start(const OObject &parent,
				GABC_OError &err,
				const ROP_AbcContext &ctx,
				UT_BoundingBox &box);
    virtual bool	update(GABC_OError &err,
				const ROP_AbcContext &ctx,
				UT_BoundingBox &box);
    virtual bool	selfTimeDependent() const;
    virtual bool	getLastBounds(UT_BoundingBox &box) const;
    /// @}

private:
    void		clear();
    void		writeShapes(GABC_OError &err,
				const ROP_AbcContext &ctx,
				const UT_Array<const GT_PrimitiveHandle> &prims,
				UT_BoundingBox &box);

    OObject			 myParent;
    UT_Array<ROP_AbcGTShape *>	 myShapes;
    UT_BoundingBox		 myBox;
    int				 mySopId;
    bool			 myTimeDependent;
};

#endif
