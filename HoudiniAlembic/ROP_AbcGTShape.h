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
 * NAME:	ROP_AbcGTShape.h
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcGTShape__
#define __ROP_AbcGTShape__

#include "ROP_AbcObject.h"
#include "ROP_AbcContext.h"
#include <GT/GT_Handles.h>

class GABC_OGTGeometry;

/// A shape node representation for a single GT primitive
class ROP_AbcGTShape : public ROP_AbcObject
{
public:
    ROP_AbcGTShape(const std::string &name);
    virtual ~ROP_AbcGTShape();

    static bool	isPrimitiveSupported(const GT_PrimitiveHandle &prim);

    /// Write the first frame to the archive
    bool	firstFrame(const GT_PrimitiveHandle &prim,
			    const OObject &parent,
			    GABC_OError &err,
			    const ROP_AbcContext &ctx);
    /// Write the next frame to the archvive
    bool	nextFrame(const GT_PrimitiveHandle &prim,
			    GABC_OError &err,
			    const ROP_AbcContext &ctx);

private:
    /// @{
    /// Interface defined on ROP_AbcObject.
    ///
    /// For GTShapes, these methods are not implemented.  Time dependence is
    /// known only by the parent.
    virtual const char	*className() const	{ return "OGTShape"; }
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
    // Use std::string since the name is shared by the ABCGTGeometry and
    // std::string has COW semantics.
    std::string		 myName;
    GABC_OGTGeometry	*myShape;
};

#endif

