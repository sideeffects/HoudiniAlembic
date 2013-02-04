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
 * NAME:	SOP_AlembicPrimitive.h (SOP  Library, C++)
 *
 * COMMENTS:
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

private:
    GU_DetailGroupPair		 myDetailGroupPair;
    const GA_PrimitiveGroup	*myGroup;
    GABC_GEOPrim		*myCurrPrim;	// Current primitive
};

#endif
