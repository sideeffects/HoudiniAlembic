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
 * NAME:	SOP_AlembicGroup.h (SOP  Library, C++)
 *
 * COMMENTS:
 */

#ifndef __SOP_AlembicGroup__
#define __SOP_AlembicGroup__

#include <UT/UT_Version.h>
#include <UT/UT_Interrupt.h>
#include <SOP/SOP_Node.h>

/// SOP to select Alembic primitives based on various criteria
class SOP_AlembicGroup : public SOP_Node
{
public:
    typedef std::vector<std::string>	PathList;

    static OP_Node *myConstructor(OP_Network *net, const char *name,
		    OP_Operator *entry);
    static PRM_Template myTemplateList[];

    static void installSOP(OP_OperatorTable *table);

    /// Return the label for the given input
    virtual const char	*inputLabel(unsigned int idx) const;

protected:
    SOP_AlembicGroup(OP_Network *net, const char *name, OP_Operator *op);
    virtual ~SOP_AlembicGroup();

    virtual bool	updateParmsFlags();
    virtual OP_ERROR	cookMySop(OP_Context &context);

    void	selectPrimitives(GA_PrimitiveGroup *group,
			const PathList &objectPaths,
			const bool *typeMatch);
private:
};

#endif
