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
 * NAME:	ROP_AbcOpXform.h
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcOpXform__
#define __ROP_AbcOpXform__

#include "ROP_AbcObject.h"
#include "ROP_AbcContext.h"

class OBJ_Node;

/// Transform node defined by a Houdini object node
class ROP_AbcOpXform : public ROP_AbcObject
{
public:
    typedef Alembic::AbcGeom::OXform	OXform;
    typedef Alembic::Abc::OObject	OObject;

    ROP_AbcOpXform(OBJ_Node *node, const ROP_AbcContext &ctx);
    virtual ~ROP_AbcOpXform();

    /// @{
    /// Interface defined on ROP_AbcObject
    virtual const char	*className() const	{ return "OpXform"; }
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
    UT_Matrix4D		 myMatrix;
    UT_BoundingBox	 myBox;
    OXform		 myOXform;
    int			 myNodeId;
    bool		 myTimeDependent;
    bool		 myIdentity;
    bool		 myGeometryContainer;
};

#endif

