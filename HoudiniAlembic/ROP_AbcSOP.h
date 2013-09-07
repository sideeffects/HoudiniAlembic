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

    OObject			 myParent;
    UT_Array<ROP_AbcGTShape *>	 myShapes;
    UT_BoundingBox		 myBox;
    int				 mySopId;
    bool			 myTimeDependent;
};

#endif
