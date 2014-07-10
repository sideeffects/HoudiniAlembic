/*
 * Copyright (c) 2014
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

#ifndef __ROP_AbcGTCompoundShape__
#define __ROP_AbcGTCompoundShape__

#include "ROP_AbcGTShape.h"
#include "ROP_AbcPackedAbc.h"
#include <GABC/GABC_OError.h>

/// Houdini geometry can be composed of multiple simple shapes.
/// This class "splits" the houdini geometry into multiple simple shapes which
/// can be represented in Alembic.
class ROP_AbcGTCompoundShape
{
public:
    typedef GABC_NAMESPACE::GABC_OError	        GABC_OError;

    typedef Alembic::Abc::OObject	        OObject;
    typedef Alembic::AbcGeom::ObjectVisibility	ObjectVisibility;
    typedef Alembic::AbcGeom::OXform	        OXform;

    ROP_AbcGTCompoundShape(const std::string &name,
	    bool polygons_as_subd,
	    bool show_unused_points);
    ~ROP_AbcGTCompoundShape();

    bool	first(const GT_PrimitiveHandle &prim,
			const OObject &parent,
			GABC_OError &err,
			const ROP_AbcContext &ctx,
			bool create_container);

    bool	update(const GT_PrimitiveHandle &prim,
			GABC_OError &err,
			const ROP_AbcContext &ctx);

    OObject	getShape() const;
private:
    void	clear();
    bool        startMyShape(ROP_AbcGTShape *shape,
                        GT_PrimitiveHandle prim,
                        GABC_OError &err,
                        const ROP_AbcContext &ctx,
                        ObjectVisibility vis);

    const OObject              *myShapeParent;
    OXform                     *myContainer;
    ROP_AbcPackedAbc            myPackedAbc;
    UT_Array<ROP_AbcGTShape *>  myShapes;
    std::string                 myName;
    exint                       myElapsedFrames;
    bool                        myPolysAsSubd;
    bool                        myShowUnusedPoints;
};

#endif
