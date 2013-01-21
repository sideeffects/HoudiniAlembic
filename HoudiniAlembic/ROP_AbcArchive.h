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
 * NAME:	ROP_AbcArchive.h
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcArchive__
#define __ROP_AbcArchive__

#include "ROP_AbcObject.h"
#include "ROP_AbcContext.h"

class GABC_OError;
class OBJ_Node;

/// Output archive for Houdini
class ROP_AbcArchive : public ROP_AbcObject
{
public:
    typedef Alembic::Abc::OArchive	OArchive;

    ROP_AbcArchive();
    virtual ~ROP_AbcArchive();

    /// Open a file for writing
    bool	open(GABC_OError &err, const char *file);

    /// Close the archive
    void	close();

    /// Write the first frame to the archive
    bool	firstFrame(GABC_OError &err, const ROP_AbcContext &ctx);
    /// Write the next frame to the archvive
    bool	nextFrame(GABC_OError &err, const ROP_AbcContext &ctx);

protected:
    /// @{
    /// Interface defined on ROP_AbcObject
    virtual const char	*className() const	{ return "OArchive"; }
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
    OArchive		 myArchive;
    UT_BoundingBox	 myBox;
    bool		 myTimeDependent;
};

#endif
