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
 * NAME:	SOP_AlembicGroup.C (SOP Library, C++)
 *
 * COMMENTS:
 */

#include "SOP_AlembicGroup.h"
#include <UT/UT_WorkArgs.h>
#include <UT/UT_DSOVersion.h>
#include <CMD/CMD_Manager.h>
#include <OP/OP_OperatorTable.h>
#include <GABC/GABC_GUPrim.h>

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
    GA_PrimitiveTypeId	abctype = GABC_GUPrim::theTypeId();
    StringSet		branches;
    for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
    {
	const GEO_Primitive	*p = gdp->getGEOPrimitive(*it);
	if (p->getTypeId() == abctype)
	{
	    const GABC_GEOPrim	*abc = UTverify_cast<const GABC_GEOPrim *>(p);
	    getAllPathComponents(abc->objectPath().c_str(), names, branches);
	}
    }
}

static int
selectAlembicNodes(void *data, int index,
	fpreal t, const PRM_Template *)
{
    SOP_AlembicGroup	*sop = (SOP_AlembicGroup *)(data);
    CMD_Manager		*mgr = CMDgetManager();
    OP_Context		 ctx(t);
    UT_WorkBuffer	 cmd;
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
    sop->evalString(objectpath, "objectPath", 0, t);	// Get curr selection
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
    sop->setString(result, CH_STRING_LITERAL, "objectPath", 0, t);
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

static PRM_Name prm_groupName("group", "Group Name");
static PRM_Name	prm_objectPathName("objectPath", "Object Path");
static PRM_Name	prm_pickObjectPathName("pickobjectPath", "Pick");
static PRM_Name	prm_shapePoly("typepoly", "Polygon Mesh Alembic Primitives");
static PRM_Name	prm_shapeSubd("typesubd", "Subdivision Mesh Alembic Primitives");
static PRM_Name	prm_shapeCurves("typecurves", "Curve Mesh Alembic Primitives");
static PRM_Name	prm_shapePoints("typepoints", "Point Mesh Alembic Primitives");
static PRM_Name	prm_shapeNuPatch("typenupatch", "NURBS Patch Alembic Primitives");
static PRM_Name	prm_shapeXform("typexform", "Transform Node Primitives");

}

PRM_Template	SOP_AlembicGroup::myTemplateList[] =
{
    PRM_Template(PRM_STRING,	1, &prm_groupName),
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

    UT_String	objectPath;
    UT_String	groupName;
    PathList	paths;
    bool	typeMatch[GABC_NUM_NODE_TYPES];
    fpreal	t = context.getTime();
    for (int i = 0; i < GABC_NUM_NODE_TYPES; ++i)
	typeMatch[i] = false;

    evalString(objectPath, "objectPath", 0, t);
    evalString(groupName, "group", 0, t);
    splitPathString(objectPath, paths);
    typeMatch[GABC_XFORM] = evalInt("typexform", 0, t) != 0;
    typeMatch[GABC_POLYMESH] = evalInt("typepoly", 0, t) != 0;
    typeMatch[GABC_SUBD] = evalInt("typesubd", 0, t) != 0;
    typeMatch[GABC_CURVES] = evalInt("typecurves", 0, t) != 0;
    typeMatch[GABC_POINTS] = evalInt("typepoints", 0, t) != 0;
    typeMatch[GABC_NUPATCH] = evalInt("typenupatch", 0, t) != 0;

    GA_PrimitiveGroup	*g = gdp->newPrimitiveGroup(groupName);
    if (g)
	selectPrimitives(g, paths, typeMatch);
    else
	addError(SOP_ERR_BADGROUP, (const char *)groupName);

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
    GA_PrimitiveTypeId	abctype = GABC_GUPrim::theTypeId();
    for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
    {
	const GEO_Primitive	*p = gdp->getGEOPrimitive(*it);
	if (p->getTypeId() != abctype)
	    continue;

	const GABC_GEOPrim	*abc = UTverify_cast<const GABC_GEOPrim *>(p);
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
	GABC_NodeType	ntype = abc->abcNodeType();
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
	    "alembicgroup",
	    "Alembic Group",
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
