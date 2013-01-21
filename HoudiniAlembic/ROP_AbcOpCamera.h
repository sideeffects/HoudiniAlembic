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
 * NAME:	ROP_AbcOpCamera.h
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcOpCamera__
#define __ROP_AbcOpCamera__

#include "ROP_AbcObject.h"
#include "ROP_AbcContext.h"

class OBJ_Camera;

/// Camera properties defined by a Houdini object node
class ROP_AbcOpCamera : public ROP_AbcObject
{
public:
    typedef Alembic::AbcGeom::OCamera	OCamera;
    typedef Alembic::AbcGeom::OObject	OObject;

    ROP_AbcOpCamera(OBJ_Camera *node);
    virtual ~ROP_AbcOpCamera();

    /// @{
    /// Interface defined on ROP_AbcObject
    virtual const char	*className() const	{ return "OpCamera"; }
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
    UT_BoundingBox	 myBox;
    OCamera		 myOCamera;
    int			 myCameraId;
    bool		 myTimeDependent;
};

#endif

