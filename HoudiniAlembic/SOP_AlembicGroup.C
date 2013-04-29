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

#include "SOP_AlembicGroup.h"
#include <UT/UT_WorkArgs.h>
#include <UT/UT_DSOVersion.h>
#include <CMD/CMD_Manager.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_SpareData.h>
#include <GU/GU_PrimSelection.h>
#include <GABC/GABC_GUPrim.h>

#if !defined(CUSTOM_ALEMBIC_TOKEN_PREFIX)
    #define CUSTOM_ALEMBIC_TOKEN_PREFIX	""
    #define CUSTOM_ALEMBIC_LABEL_PREFIX	""
#endif

using namespace GABC_NAMESPACE;

namespace
{
typedef SOP_AlembicGroup::PathList	PathList;
typedef UT_Set<std::string>		StringSet;

static void
getAllPathComponents(const char *rawpath, PathList &names, StringSet &branches)
{
    UT_String		path(rawpath);
    UT_WorkArgs		args;
    UT_WorkBuffer	wbuf;
    path.tokenize(args, '/');
    for (int i = 0; i < args.getArgc(); ++i)
    {
	const char	*component = args.getArg(i);
	if (UTisstring(component) || wbuf.length())
	{
	    wbuf.append("/");
	    wbuf.append(component);
	    std::string	result = wbuf.buffer();
	    if (!branches.count(result))
	    {
		names.push_back(result);
		branches.insert(result);
	    }
	}
    }
}

static void
getAlembicPrimitivePaths(const GU_Detail *gdp, PathList &names)
{
#if !defined(GABC_PACKED)
    GA_PrimitiveTypeId	abctype = GABC_GUPrim::theTypeId();
#else
    const GA_PrimitiveTypeId	&abctype = GABC_PackedImpl::typeId();
#endif
    StringSet		branches;
    for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
    {
	const GEO_Primitive	*p = gdp->getGEOPrimitive(*it);
	if (p->getTypeId() == abctype)
	{
#if !defined(GABC_PACKED)
	    const GABC_GEOPrim	*abc = UTverify_cast<const GABC_GEOPrim *>(p);
#else
	    const GU_PrimPacked	*pack = UTverify_cast<const GU_PrimPacked *>(p);
	    const GABC_PackedImpl *abc = UTverify_cast<const GABC_PackedImpl *>(pack->implementation());
#endif
	    getAllPathComponents(abc->objectPath().c_str(), names, branches);
	}
    }
}

static int
selectAlembicNodes(void *data, int index,
	fpreal t, const PRM_Template *tplate)
{
    const char	*token = tplate->getToken();
    if (strncmp(token, "pickobjectPath", 14) != 0)
    {
	fprintf(stderr, "Invalid parameter callback\n");
	return 0;
    }
    int			 inst = SYSatoi(token+14);
    SOP_AlembicGroup	*sop = (SOP_AlembicGroup *)(data);
    CMD_Manager		*mgr = CMDgetManager();
    OP_Context		 ctx(t);
    UT_WorkBuffer	 cmd;
    UT_WorkBuffer	 parmname;
    UT_String		 objectpath;
    PathList		 abcobjects;
    GU_DetailHandle	 gdh;
    const GU_Detail	*gdp;

    SOP_Node	*input = UTverify_cast<SOP_Node *>(sop->getInput(0));
    if (input)
	gdh = input->getCookedGeoHandle(ctx);
    gdp = gdh.readLock();
    if (!gdp)
    {
	cmd.sprintf("message No input geometry found");
	mgr->execute(cmd.buffer());
	return 0;
    }

    cmd.strcpy("treechooser");
    parmname.sprintf("objectPath%d", inst);
    sop->evalString(objectpath, parmname.buffer(), 0, t); // Get curr selection
    getAlembicPrimitivePaths(gdh.readLock(), abcobjects);
    if (objectpath.isstring())
    {
	cmd.strcat(" -s ");
	cmd.protectedStrcat(objectpath);
    }
    for (exint i = 0; i < abcobjects.size(); ++i)
    {
	cmd.strcat(" ");
	cmd.protectedStrcat(abcobjects[i].c_str());
    }

    UT_OStrStream	 os;
    mgr->execute(cmd.buffer(), 0, &os);
    os << ends;
    UT_String	result(os.str());
    result.trimBoundingSpace();
    sop->setString(result, CH_STRING_LITERAL, parmname.buffer(), 0, t);
    os.rdbuf()->freeze(0);

    return 0;
}

void
splitPathString(UT_String &str, PathList &paths)
{
    UT_WorkArgs		args;
    UT_WorkBuffer	tmp;
    paths.clear();
    str.parse(args);
    for (exint i = 0; i < args.getArgc(); ++i)
    {
	const char	*path = args.getArg(i);
	if (UTisstring(path))
	    paths.push_back(path);
    }
}

static void
buildPrimSelection(GU_Detail *gdp, const std::vector<std::string> &group_names)
{
    GU_PrimSelection		*primSelection;

    delete gdp->selection();
    gdp->selection(0);
    if (group_names.size() == 1)
	primSelection = new GU_PrimSelection(*gdp, group_names[0].c_str(), 0,
					     "__sopprimgroup");
    else
    {
	primSelection = new GU_PrimSelection(*gdp, "_gu_pmselection_", 0,
					     "__sopprimgroup");
	for (int i = 0; i < group_names.size(); i++)
	{
	    const GA_PrimitiveGroup *group = gdp->findPrimitiveGroup(
						    group_names[i].c_str());
	    if (group)
		primSelection->operator|=(*group);
	}
    }
    gdp->selection(primSelection);
}

static PRM_Name prm_groupName("group#", "Group Name");
static PRM_Name	prm_objectPathName("objectPath#", "Object Path");
static PRM_Name	prm_pickObjectPathName("pickobjectPath#", "Pick");
static PRM_Name	prm_shapePoly("typepoly#", "Polygon Mesh Alembic Primitives");
static PRM_Name	prm_shapeSubd("typesubd#", "Subdivision Mesh Alembic Primitives");
static PRM_Name	prm_shapeCurves("typecurves#", "Curve Mesh Alembic Primitives");
static PRM_Name	prm_shapePoints("typepoints#", "Point Mesh Alembic Primitives");
static PRM_Name	prm_shapeNuPatch("typenupatch#", "NURBS Patch Alembic Primitives");
static PRM_Name	prm_shapeXform("typexform#", "Transform Node Primitives");

static PRM_Name	prm_groupCount("ngroups", "Number Of Groups");

static PRM_Template	groupTemplate[] =
{
    PRM_Template(PRM_STRING,	1, &prm_groupName, &PRMgroupDefault),
    PRM_Template(PRM_STRING, PRM_TYPE_JOIN_PAIR, 1, &prm_objectPathName),
    PRM_Template(PRM_CALLBACK,	1, &prm_pickObjectPathName,
		0, 0, 0, selectAlembicNodes),
    PRM_Template(PRM_TOGGLE,	1, &prm_shapePoly, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE,	1, &prm_shapeSubd, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE,	1, &prm_shapeCurves, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE,	1, &prm_shapePoints, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE,	1, &prm_shapeNuPatch, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE,	1, &prm_shapeXform, PRMoneDefaults),

    PRM_Template()
};

}

PRM_Template	SOP_AlembicGroup::myTemplateList[] =
{
    PRM_Template(PRM_MULTITYPE_LIST, groupTemplate, 2,
	    &prm_groupCount, PRMoneDefaults, 0,
	    &PRM_SpareData::multiStartOffsetZero),

    PRM_Template()	// Sentinal
};

SOP_AlembicGroup::SOP_AlembicGroup(OP_Network *net,
	const char *name, OP_Operator *op)
    : SOP_Node(net, name, op)
{
}

SOP_AlembicGroup::~SOP_AlembicGroup()
{
}

bool
SOP_AlembicGroup::updateParmsFlags()
{
    bool	changed = false;
    return changed;
}

OP_ERROR
SOP_AlembicGroup::cookMySop(OP_Context &context)
{
    if (lockInputs(context) >= UT_ERROR_ABORT)
	return error();

    duplicateSource(0, context);

    fpreal			t = context.getTime();
    int				ngroups = evalInt("ngroups", 0, t);
    std::vector<std::string>	groupNames;
    UT_Set<std::string>		groupNameSet;
    for (int inst = 0; inst < ngroups; ++inst)
    {
	UT_String	str;
	evalStringInst("group#", &inst, str, 0, t);
	std::string	name = str.toStdString();
	groupNames.push_back(name);
	if (groupNameSet.count(name) == 1)
	{
	    UT_WorkBuffer	err;
	    err.sprintf("Warning: multiple groups named: %s", name.c_str());
	    addWarning(SOP_MESSAGE, err.buffer());
	}
	groupNameSet.insert(name);
    }

    for (int inst = 0; inst < ngroups; ++inst)
    {
	std::string	groupName = groupNames[inst];
	UT_String	objectPath;
	PathList	paths;
	bool	typeMatch[GABC_NUM_NODE_TYPES];
	for (int i = 0; i < GABC_NUM_NODE_TYPES; ++i)
	    typeMatch[i] = false;

	evalStringInst("objectPath#", &inst, objectPath, 0, t);
	splitPathString(objectPath, paths);
	typeMatch[GABC_XFORM] = evalIntInst("typexform#", &inst, 0, t) != 0;
	typeMatch[GABC_POLYMESH] = evalIntInst("typepoly#", &inst, 0, t) != 0;
	typeMatch[GABC_SUBD] = evalIntInst("typesubd#", &inst, 0, t) != 0;
	typeMatch[GABC_CURVES] = evalIntInst("typecurves#", &inst, 0, t) != 0;
	typeMatch[GABC_POINTS] = evalIntInst("typepoints#", &inst, 0, t) != 0;
	typeMatch[GABC_NUPATCH] = evalIntInst("typenupatch#", &inst, 0, t) != 0;

	GA_PrimitiveGroup	*g = gdp->newPrimitiveGroup(groupName.c_str());
	if (g)
	{
	    selectPrimitives(g, paths, typeMatch);
	}
	else
	    addError(SOP_ERR_BADGROUP, groupName.c_str());
    }
    buildPrimSelection(gdp, groupNames);

    unlockInputs();

    return error();
}

const char *
SOP_AlembicGroup::inputLabel(unsigned int idx) const
{
    UT_ASSERT(idx == 0);
    return "Alembic Geometry";
}

void
SOP_AlembicGroup::selectPrimitives(GA_PrimitiveGroup *group,
			const PathList &objectPaths,
			const bool *typeMatch)
{
    UT_ASSERT(group);
    if (!group)
	return;
#if !defined(GABC_PACKED)
    GA_PrimitiveTypeId	abctype = GABC_GUPrim::theTypeId();
#else
    const GA_PrimitiveTypeId	&abctype = GABC_PackedImpl::typeId();
#endif
    for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
    {
	const GEO_Primitive	*p = gdp->getGEOPrimitive(*it);
	if (p->getTypeId() != abctype)
	    continue;

#if !defined(GABC_PACKED)
	const GABC_GEOPrim	*abc = UTverify_cast<const GABC_GEOPrim *>(p);
#else
	const GU_PrimPacked	*pack = UTverify_cast<const GU_PrimPacked *>(p);
	const GABC_PackedImpl	*abc = UTverify_cast<const GABC_PackedImpl *>(pack->implementation());
#endif
	if (objectPaths.size())
	{
	    const char	*path = abc->objectPath().c_str();
	    exint	 pathlen = strlen(path);
	    bool	 found = false;
	    for (exint i = 0; i < objectPaths.size(); ++i)
	    {
		const std::string	&match = objectPaths[i];
		exint			 matchlen = match.size();
		if (matchlen <= pathlen)
		{
		    if (!strncmp(path, match.c_str(), matchlen) &&
			    (path[matchlen] == 0 || path[matchlen] == '/'))
		    {
			found = true;
			break;
		    }
		}
	    }
	    if (!found)
		continue;
	}
#if !defined(GABC_PACKED)
	GABC_NodeType	ntype = abc->abcNodeType();
#else
	GABC_NodeType	ntype = abc->nodeType();
#endif
	if (ntype >= 0)
	{
	    UT_ASSERT(ntype < GABC_NUM_NODE_TYPES);
	    if (!typeMatch[ntype])
		continue;
	}
	// We passed all the filters, so we can add to the group
	group->addOffset(*it);
    }
}


OP_Node *
SOP_AlembicGroup::myConstructor(OP_Network *net, const char *name,
	OP_Operator *op)
{
    return new SOP_AlembicGroup(net, name, op);
}

void
SOP_AlembicGroup::installSOP(OP_OperatorTable *table)
{
    OP_Operator	*abcGroup = new OP_Operator(
        CUSTOM_ALEMBIC_TOKEN_PREFIX "alembicgroup",
        CUSTOM_ALEMBIC_LABEL_PREFIX "Alembic Group",
	SOP_AlembicGroup::myConstructor,
	SOP_AlembicGroup::myTemplateList,
	1, 1,
	0);
    abcGroup->setIconName("SOP_alembic");
    table->addOperator(abcGroup);
}

void
newSopOperator(OP_OperatorTable *table)
{
    SOP_AlembicGroup::installSOP(table);
}
