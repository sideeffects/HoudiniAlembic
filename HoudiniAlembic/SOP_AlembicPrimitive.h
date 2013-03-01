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

#ifndef __SOP_AlembicPrimitive__
#define __SOP_AlembicPrimitive__

#include <UT/UT_Version.h>
#include <UT/UT_Interrupt.h>
#include <SOP/SOP_Node.h>

class GABC_GEOPrim;

/// SOP to change intrinsic properties on Alembic primitives
class SOP_AlembicPrimitive : public SOP_Node
{
public:
    static OP_Node *myConstructor(OP_Network *net, const char *name,
		    OP_Operator *entry);
    static PRM_Template		myTemplateList[];
    static CH_LocalVariable	myVariables[];

    static void installSOP(OP_OperatorTable *table);

    /// Return the label for the given input
    virtual const char	*inputLabel(unsigned int idx) const;
    virtual bool	 evalVariableValue(fpreal &v, int index, int thread);
    virtual bool	 evalVariableValue(UT_String &v, int index, int thread);

protected:
    SOP_AlembicPrimitive(OP_Network *net, const char *name, OP_Operator *op);
    virtual ~SOP_AlembicPrimitive();

    virtual bool	updateParmsFlags();
    virtual OP_ERROR	cookMySop(OP_Context &context);
    virtual OP_ERROR	cookInputGroups(OP_Context &context,
				int alone = 0);

    bool	setProperties(GABC_GEOPrim *prim, OP_Context &ctx);

    fpreal	FRAME(fpreal t)
		    { return evalFloat("frame", 0, t); }
    fpreal	FPS(fpreal t)
		    { return evalFloat("fps", 0, t); }
    int		VISIBILITY(fpreal t)
		    { return evalFloat("usevisibility", 0, t); }

private:
    GU_DetailGroupPair		 myDetailGroupPair;
    const GA_PrimitiveGroup	*myGroup;
    GABC_GEOPrim		*myCurrPrim;	// Current primitive
};

#endif
