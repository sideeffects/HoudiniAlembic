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

