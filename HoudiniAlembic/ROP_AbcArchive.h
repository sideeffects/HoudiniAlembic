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

#ifndef __ROP_AbcArchive__
#define __ROP_AbcArchive__

#include "ROP_AbcObject.h"
#include "ROP_AbcContext.h"
#include <GABC/GABC_OError.h>

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
