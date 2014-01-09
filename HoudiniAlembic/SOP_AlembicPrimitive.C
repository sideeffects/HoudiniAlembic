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

#include "SOP_AlembicPrimitive.h"
#include <UT/UT_WorkArgs.h>
#include <UT/UT_DSOVersion.h>
#include <PRM/PRM_Include.h>
#include <CMD/CMD_Manager.h>
#include <OP/OP_OperatorTable.h>
#include <GU/GU_PrimPacked.h>

#if !defined(CUSTOM_ALEMBIC_TOKEN_PREFIX)
    #define CUSTOM_ALEMBIC_TOKEN_PREFIX	""
    #define CUSTOM_ALEMBIC_LABEL_PREFIX	""
#endif

using namespace GABC_NAMESPACE;

namespace
{
static PRM_Name		prm_groupName("group", "Group Name");
static PRM_Name		prm_frameName("frame", "Frame");
static PRM_Name		prm_fpsName("fps", "Frames Per Second");
static PRM_Name		prm_visibilityName("usevisibility", "Use Visibility");
static PRM_Name		prm_lodName("viewportlod", "Display As");
static PRM_Default	prm_frameDefault(1, "$FF");
static PRM_Default	prm_fpsDefault(1, "$FPS");
static PRM_Default	prm_visibilityDefault(2);

static PRM_Name	prm_lodChoices[] = {
    PRM_Name("unchanged",	"Leave Unchanged"),
    PRM_Name("full",		"Full Geometry"),
    PRM_Name("points",		"Point Cloud"),
    PRM_Name("box",		"Bounding Box"),
    PRM_Name("centroid",	"Centroid"),
    PRM_Name("hidden",		"Hidden"),
    PRM_Name()
};
static PRM_ChoiceList	prm_lodMenu(PRM_CHOICELIST_SINGLE, prm_lodChoices);
static PRM_Default	prm_lodDefault(0, "unchanged");

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
    VAR_ABCVIEWPORTLOD,
};

}

PRM_Template	SOP_AlembicPrimitive::myTemplateList[] =
{
    PRM_Template(PRM_STRING,	1, &PRMgroupName, 0, &SOP_Node::primGroupMenu),
    PRM_Template(PRM_FLT,	1, &prm_frameName, &prm_frameDefault),
    PRM_Template(PRM_FLT,	1, &prm_fpsName, &prm_fpsDefault),
    PRM_Template(PRM_INT,	1, &prm_visibilityName, &prm_visibilityDefault,
				    &prm_visibilityMenu),
    PRM_Template(PRM_ORD,	1, &prm_lodName, &prm_lodDefault, &prm_lodMenu),
    PRM_Template()
};

CH_LocalVariable	SOP_AlembicPrimitive::myVariables[] =
{
    { "ABCFRAME",	VAR_ABCFRAME,		0 },
    { "ABCOBJECT",	VAR_ABCOBJECT,		CH_VARIABLE_STRVAL },
    { "ABCFILENAME",	VAR_ABCFILENAME,	CH_VARIABLE_STRVAL },
    { "ABCTYPE",	VAR_ABCTYPE,		CH_VARIABLE_STRVAL },
    { "ABCUSEVISIBILE",	VAR_ABCUSEVISIBLE,	0 },
    { "ABCVIEWPORTLOD",	VAR_ABCVIEWPORTLOD,	CH_VARIABLE_STRVAL },
    { 0, 0, 0 },
};

SOP_AlembicPrimitive::SOP_AlembicPrimitive(OP_Network *net,
	const char *name, OP_Operator *op)
    : SOP_Node(net, name, op)
    , myCurrPrim(NULL)
    , myCurrAbc(NULL)
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
    if (myCurrAbc)
    {
	UT_ASSERT(myCurrPrim);
	switch (index)
	{
	    case VAR_ABCFRAME:
		v = myCurrAbc->frame();
		return true;
	    case VAR_ABCOBJECT:
	    case VAR_ABCFILENAME:
	    case VAR_ABCTYPE:
		v = 0;
		return true;
	    case VAR_ABCUSEVISIBLE:
		v = myCurrAbc->useVisibility();
		return true;
	    case VAR_ABCVIEWPORTLOD:
		v = myCurrPrim->viewportLOD();
		return true;
	}
    }
    return SOP_Node::evalVariableValue(v, index, thread);
}

bool
SOP_AlembicPrimitive::evalVariableValue(UT_String &v, int index, int thread)
{
    if (myCurrAbc)
    {
	UT_ASSERT(myCurrPrim);
	UT_WorkBuffer		 wbuf;
	switch (index)
	{
	    case VAR_ABCFRAME:
		wbuf.sprintf("%g", myCurrAbc->frame());
		v.harden(wbuf.buffer());
		return true;
	    case VAR_ABCOBJECT:
		v = myCurrAbc->objectPath();
		return true;
	    case VAR_ABCFILENAME:
		v = myCurrAbc->filename();
		return true;
	    case VAR_ABCTYPE:
		v = GABCnodeType(myCurrAbc->nodeType());
		return true;
	    case VAR_ABCUSEVISIBLE:
		wbuf.sprintf("%d", myCurrAbc->useVisibility());
		v.harden(wbuf.buffer());
		return true;
	    case VAR_ABCVIEWPORTLOD:
		wbuf.sprintf("%s", GEOviewportLOD(myCurrPrim->viewportLOD()));
		v.harden(wbuf.buffer());
		return true;
	}
    }
    return SOP_Node::evalVariableValue(v, index, thread);
}

bool
SOP_AlembicPrimitive::setProperties(GU_PrimPacked *pack,
	GABC_PackedImpl *prim, OP_Context &ctx)
{
    fpreal	t = ctx.getTime();
    fpreal	fps = SYSmax(1e-6, FPS(t));
    UT_String	lod;
    prim->setFrame(FRAME(t)/fps);
    switch (VISIBILITY(t))
    {
	case 0:
	    prim->setUseVisibility(false);
	    break;
	case 1:
	    prim->setUseVisibility(true);
	    break;
	// case 2: unchanged
    }
    VIEWPORTLOD(lod, t);
    if (lod != "unchanged")
    {
	pack->setViewportLOD(GEOviewportLOD(lod));
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

    const GA_PrimitiveTypeId	&abctype = GABC_PackedImpl::typeId();
    if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT)
    {
	for (GA_Iterator it(gdp->getPrimitiveRange(myGroup)); !it.atEnd(); ++it)
	{
	    GEO_Primitive	*p = gdp->getGEOPrimitive(*it);
	    if (p->getTypeId() != abctype)
		continue;

	    // Set up local variable
	    myCurPrimOff[0] = p->getMapOffset();
	    myCurrPrim = UTverify_cast<GU_PrimPacked *>(p);
	    myCurrAbc = UTverify_cast<GABC_PackedImpl *>(myCurrPrim->implementation());
	    if (!setProperties(myCurrPrim, myCurrAbc, context))
		break;
	}
    }
    myCurrPrim = NULL;
    myCurrAbc = NULL;
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
        CUSTOM_ALEMBIC_TOKEN_PREFIX "alembicprimitive",
        CUSTOM_ALEMBIC_LABEL_PREFIX "Alembic Primitive",
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
