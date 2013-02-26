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
 * NAME:	SOP_AlembicPrimitive.C (SOP Library, C++)
 *
 * COMMENTS:
 */

#include "SOP_AlembicPrimitive.h"
#include <UT/UT_WorkArgs.h>
#include <UT/UT_DSOVersion.h>
#include <PRM/PRM_Include.h>
#include <CMD/CMD_Manager.h>
#include <OP/OP_OperatorTable.h>
#include <GABC/GABC_GUPrim.h>

namespace
{
static PRM_Name		prm_groupName("group", "Group Name");
static PRM_Name		prm_frameName("frame", "Frame");
static PRM_Name		prm_fpsName("fps", "Frames Per Second");
static PRM_Name		prm_visibilityName("usevisibility", "Use Visibility");
static PRM_Default	prm_frameDefault(1, "$FF");
static PRM_Default	prm_fpsDefault(1, "$FPS");
static PRM_Default	prm_visibilityDefault(2);

static PRM_Name	prm_visibilityChoices[] = {
    PRM_Name("off",		"Ignore Alembic Visibility"),
    PRM_Name("on",		"Use Alembic Visibility"),
    PRM_Name("unchanged",	"Leave Visibility Unchanged"),
    PRM_Name()
};
static PRM_ChoiceList	prm_visibilityMenu(PRM_CHOICELIST_SINGLE,
				    prm_visibilityChoices);

enum
{
    VAR_ABCFRAME,
    VAR_ABCOBJECT,
    VAR_ABCFILENAME,
    VAR_ABCTYPE,
    VAR_ABCUSEVISIBLE,
};

}

PRM_Template	SOP_AlembicPrimitive::myTemplateList[] =
{
    PRM_Template(PRM_STRING,	1, &PRMgroupName, 0, &SOP_Node::primGroupMenu),
    PRM_Template(PRM_FLT,	1, &prm_frameName, &prm_frameDefault),
    PRM_Template(PRM_FLT,	1, &prm_fpsName, &prm_fpsDefault),
    PRM_Template(PRM_INT,	1, &prm_visibilityName, &prm_visibilityDefault,
				    &prm_visibilityMenu),
    PRM_Template()
};

CH_LocalVariable	SOP_AlembicPrimitive::myVariables[] =
{
    { "ABCFRAME",	VAR_ABCFRAME,		0 },
    { "ABCOBJECT",	VAR_ABCOBJECT,		CH_VARIABLE_STRVAL },
    { "ABCFILENAME",	VAR_ABCFILENAME,	CH_VARIABLE_STRVAL },
    { "ABCTYPE",	VAR_ABCTYPE,		CH_VARIABLE_STRVAL },
    { "ABCUSEVISIBILE",	VAR_ABCUSEVISIBLE,		0 },
    { 0, 0, 0 },
};

SOP_AlembicPrimitive::SOP_AlembicPrimitive(OP_Network *net,
	const char *name, OP_Operator *op)
    : SOP_Node(net, name, op)
    , myCurrPrim(NULL)
{
}

SOP_AlembicPrimitive::~SOP_AlembicPrimitive()
{
}

bool
SOP_AlembicPrimitive::updateParmsFlags()
{
    bool	changed = false;
    return changed;
}

bool
SOP_AlembicPrimitive::evalVariableValue(fpreal &v, int index, int thread)
{
    if (myCurrPrim)
    {
	switch (index)
	{
	    case VAR_ABCFRAME:
		v = myCurrPrim->frame();
		return true;
	    case VAR_ABCOBJECT:
	    case VAR_ABCFILENAME:
	    case VAR_ABCTYPE:
		v = 0;
		return true;
	    case VAR_ABCUSEVISIBLE:
		v = myCurrPrim->useVisibility();
		return true;
	}
    }
    return SOP_Node::evalVariableValue(v, index, thread);
}

bool
SOP_AlembicPrimitive::evalVariableValue(UT_String &v, int index, int thread)
{
    if (myCurrPrim)
    {
	UT_WorkBuffer		 wbuf;
	switch (index)
	{
	    case VAR_ABCFRAME:
		wbuf.sprintf("%g", myCurrPrim->frame());
		v.harden(wbuf.buffer());
		return true;
	    case VAR_ABCOBJECT:
		v = myCurrPrim->objectPath();
		return true;
	    case VAR_ABCFILENAME:
		v = myCurrPrim->filename();
		return true;
	    case VAR_ABCTYPE:
		v = GABCnodeType(myCurrPrim->abcNodeType());
		return true;
	    case VAR_ABCUSEVISIBLE:
		wbuf.sprintf("%d", myCurrPrim->useVisibility());
		v.harden(wbuf.buffer());
		return true;
	}
    }
    return SOP_Node::evalVariableValue(v, index, thread);
}

bool
SOP_AlembicPrimitive::setProperties(GABC_GEOPrim *prim, OP_Context &ctx)
{
    fpreal	t = ctx.getTime();
    fpreal	fps = SYSmax(1e-6, FPS(t));
    prim->setFrame(FRAME(t)/fps);
    switch (VISIBILITY(t))
    {
	case 0:
	    prim->setUseVisibility(false);
	    break;
	case 1:
	    prim->setUseVisibility(true);
	    break;
    }
    return error() < UT_ERROR_ABORT;
}

OP_ERROR
SOP_AlembicPrimitive::cookInputGroups(OP_Context &context, int alone)
{
    return cookInputPrimitiveGroups(context, myGroup, myDetailGroupPair, alone);
}

OP_ERROR
SOP_AlembicPrimitive::cookMySop(OP_Context &context)
{
    if (lockInputs(context) >= UT_ERROR_ABORT)
	return error();

    duplicateSource(0, context);

    // Set up loacal variables
    setCurGdh(0, myGdpHandle);
    setVariableOrder(3, 0, 2, 1);
    setupLocalVars();

    GA_PrimitiveTypeId	abctype = GABC_GUPrim::theTypeId();
    if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT)
    {
	for (GA_Iterator it(gdp->getPrimitiveRange(myGroup)); !it.atEnd(); ++it)
	{
	    GEO_Primitive	*p = gdp->getGEOPrimitive(*it);
	    if (p->getTypeId() != abctype)
		continue;

	    // Set up local variable
	    myCurPrimOff[0] = p->getMapOffset();
	    myCurrPrim = UTverify_cast<GABC_GEOPrim *>(p);
	    if (!setProperties(myCurrPrim, context))
		break;
	}
    }
    myCurrPrim = NULL;
    resetLocalVarRefs();

    unlockInputs();

    return error();
}

const char *
SOP_AlembicPrimitive::inputLabel(unsigned int idx) const
{
    UT_ASSERT(idx == 0);
    return "Alembic Geometry";
}


OP_Node *
SOP_AlembicPrimitive::myConstructor(OP_Network *net, const char *name,
	OP_Operator *op)
{
    return new SOP_AlembicPrimitive(net, name, op);
}

void
SOP_AlembicPrimitive::installSOP(OP_OperatorTable *table)
{
    OP_Operator	*abcPrimitive = new OP_Operator(
	    "alembicprimitive",
	    "Alembic Primitive",
	    SOP_AlembicPrimitive::myConstructor,
	    SOP_AlembicPrimitive::myTemplateList,
	    1, 1,
	    SOP_AlembicPrimitive::myVariables);
    abcPrimitive->setIconName("SOP_alembic");
    table->addOperator(abcPrimitive);
}

void
newSopOperator(OP_OperatorTable *table)
{
    SOP_AlembicPrimitive::installSOP(table);
}
