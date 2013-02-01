//-*****************************************************************************
//
// Copyright (c) 2009-2012,
//  Sony Pictures Imageworks Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic, nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

#include "SOP_AlembicIn.h"
#include <GABC/GABC_GEOWalker.h>

#include <UT/UT_DSOVersion.h>
#include <UT/UT_WorkArgs.h>
#include <GU/GU_Detail.h>
#include <SOP/SOP_Guide.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_Director.h>

namespace
{
    typedef GABC_Util::PathList		PathList;

    static int
    selectAlembicNodes(void *data, int index,
	    fpreal t, const PRM_Template *)
    {
	SOP_AlembicIn2	*sop = (SOP_AlembicIn2 *)(data);
	UT_WorkBuffer	cmd;
	UT_String	filename;
	UT_String	objectpath;
	cmd.strcpy("treechooser");
	sop->evalString(filename, "fileName", 0, t);
	sop->evalString(objectpath, "objectPath", 0, t);
	const PathList	&abcobjects = GABC_Util::getObjectList((const char *)filename);
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

	CMD_Manager	*mgr = CMDgetManager();
	UT_OStrStream	 os;
	mgr->execute(cmd.buffer(), 0, &os);
	os << ends;
	UT_String	result(os.str());
	result.trimBoundingSpace();
	sop->setString(result, CH_STRING_LITERAL, "objectPath", 0, t);
	os.rdbuf()->freeze(0);

	return 0;
    }
}

SOP_AlembicIn2::Parms::Parms()
    : myLoadMode(GABC_GEOWalker::LOAD_ABC_PRIMITIVES)
    , myBuildAbcXform(false)
    , myBoundMode(GABC_GEOWalker::BOX_CULL_IGNORE)
    , myPointMode(GABC_GEOWalker::ABCPRIM_UNIQUE_POINT)
    , myFilename()
    , myObjectPath()
    , myObjectPattern()
    , mySubdGroupName()
    , myIncludeXform(true)
    , myBuildLocator(false)
    , myGroupMode(GABC_GEOWalker::ABC_GROUP_SHAPE_NODE)
    , myAnimationFilter(GABC_GEOWalker::ABC_AFILTER_ALL)
    , myPathAttribute()
    , myFilenameAttribute()
    , myNameMapPtr()
{
    myBoundBox.makeInvalid();
}

SOP_AlembicIn2::Parms::Parms(const SOP_AlembicIn2::Parms &src)
    : myLoadMode(GABC_GEOWalker::LOAD_ABC_PRIMITIVES)
    , myBoundMode(GABC_GEOWalker::BOX_CULL_IGNORE)
    , myPointMode(GABC_GEOWalker::ABCPRIM_UNIQUE_POINT)
    , myBuildAbcXform(false)
    , myFilename()
    , myObjectPath()
    , myObjectPattern()
    , mySubdGroupName()
    , myIncludeXform(true)
    , myBuildLocator(false)
    , myGroupMode(GABC_GEOWalker::ABC_GROUP_SHAPE_NODE)
    , myAnimationFilter(GABC_GEOWalker::ABC_AFILTER_ALL)
    , myPathAttribute()
    , myFilenameAttribute()
    , myNameMapPtr()
{
    *this = src;
}


SOP_AlembicIn2::Parms &
SOP_AlembicIn2::Parms::operator=(const SOP_AlembicIn2::Parms &src)
{
    myFilename = src.myFilename;
    myLoadMode = src.myLoadMode;
    myBoundMode = src.myBoundMode;
    myPointMode = src.myPointMode;
    myBoundBox = src.myBoundBox;
    myBuildAbcXform = src.myBuildAbcXform;
    myIncludeXform = src.myIncludeXform;
    myBuildLocator = src.myBuildLocator;
    myGroupMode = src.myGroupMode;
    myAnimationFilter = src.myAnimationFilter;
    myNameMapPtr = src.myNameMapPtr;
    myObjectPath.harden(src.myObjectPath);
    myObjectPattern.harden(src.myObjectPattern);
    mySubdGroupName.harden(src.mySubdGroupName);
    for (int i = 0; i < GA_ATTRIB_OWNER_N; ++i)
	myAttributePatterns[i].harden(src.myAttributePatterns[i]);
    myPathAttribute.harden(src.myPathAttribute);
    myFilenameAttribute.harden(src.myFilenameAttribute);
    return *this;
}

bool
SOP_AlembicIn2::Parms::needsPathAttributeUpdate(const SOP_AlembicIn2::Parms &parms)
{
    return myPathAttribute != parms.myPathAttribute ||
	myFilenameAttribute != parms.myFilenameAttribute;
}

bool
SOP_AlembicIn2::Parms::needsNewGeometry(const SOP_AlembicIn2::Parms &src)
{
    if (myLoadMode != src.myLoadMode)
	return true;
    if (myBoundMode != src.myBoundMode)
	return true;
    if (myLoadMode == GABC_GEOWalker::LOAD_ABC_PRIMITIVES
	    && myPointMode != src.myPointMode)
    {
	return true;
    }
    if (myBoundMode != GABC_GEOWalker::BOX_CULL_IGNORE)
    {
	if (myBoundBox != src.myBoundBox)
	    return true;
    }
    if (myBuildAbcXform != src.myBuildAbcXform)
	return true;
    if (myFilename != src.myFilename)
	return true;
    if (myGroupMode != src.myGroupMode)
	return true;
    if (myAnimationFilter != src.myAnimationFilter)
	return true;
    if (myObjectPath != src.myObjectPath ||
	    src.myObjectPattern != src.myObjectPattern)
	return true;
    if (src.mySubdGroupName != src.mySubdGroupName)
	return true;
    for (int i = 0; i < GA_ATTRIB_OWNER_N; ++i)
    {
	if (myAttributePatterns[i] != src.myAttributePatterns[i])
	    return true;
    }
    // myIncludeXform can change
    if (myBuildLocator != src.myBuildLocator)
	return true;
    // myPathAttribute can change
    // myFilenameAttribute can change
    if (myNameMapPtr && src.myNameMapPtr)
    {
	if (*myNameMapPtr != *src.myNameMapPtr)
	    return true;
    }
    else if (myNameMapPtr || src.myNameMapPtr)
    {
	return true;
    }
    return false;
}

//-*****************************************************************************

OP_Node *
SOP_AlembicIn2::myConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
    return new SOP_AlembicIn2(net, name, op);
}

//-*****************************************************************************

static PRM_Name prm_abcAttribName("abcName#", "Alembic Name #");
static PRM_Name prm_hAttribName("hName#", "Houdini Name #");
static PRM_Template prm_AttributeRemapTemplate[] = {
    PRM_Template(PRM_STRING, 1, &prm_abcAttribName),
    PRM_Template(PRM_STRING, 1, &prm_hAttribName),
    PRM_Template()
};

static PRM_Name prm_reloadbutton("reload", "Reload Geometry");
static PRM_Name prm_filenameName("fileName", "File Name");
static PRM_Name prm_frameName("frame", "Frame");
static PRM_Name prm_fpsName("fps", "Frames Per Second");
static PRM_Name prm_objectPathName("objectPath", "Object Path");
static PRM_Name	prm_pickObjectPathName("pickobjectPath", "Pick");
static PRM_Name prm_includeXformName("includeXform", "Transform Geometry To World Space");
static PRM_Name prm_groupnames("groupnames", "Primitive Groups");
static PRM_Name prm_animationfilter("animationfilter", "Animating Objects");
static PRM_Name prm_boxcull("boxcull", "Box Culling");
static PRM_Name prm_addfile("addfile", "Add Filename Attribute");
static PRM_Name prm_fileattrib("fileattrib", "Filename Attribute");
static PRM_Name prm_addpath("addpath", "Add Path Attribute");
static PRM_Name prm_pathattrib("pathattrib", "Path Attribute");
static PRM_Name prm_remapAttribName("remapAttributes", "Remap Attributes");

static PRM_Name prm_loadmodeName("loadmode", "Load As");
static PRM_Name prm_pointModeName("pointmode", "Points");
static PRM_Name prm_abcxformName("abcxform", "Create Primitives For Transform Nodes");

static PRM_Default prm_frameDefault(1, "$FF");
static PRM_Default prm_objectPathDefault(0, "");
static PRM_Default prm_fpsDefault(24, "$FPS");
static PRM_Default prm_includeXformDefault(true);
static PRM_Default prm_pathattribDefault(0, "abcPath");
static PRM_Default prm_fileattribDefault(0, "abcFileName");

static PRM_ChoiceList	prm_objectPathMenu(PRM_CHOICELIST_TOGGLE,
        "__import__('_alembic_hom_extensions').alembicGetObjectPathListForMenu"
                "(hou.pwd().evalParm('fileName'))[:16380]", CH_PYTHON_SCRIPT);

static PRM_Name	loadModeOptions[] = {
    PRM_Name("alembic",	"Alembic Delayed Load Primitives"),
    PRM_Name("houdini",	"Load Houdini Geometry"),
    PRM_Name("hpoints", "Houdini Point Cloud"),
    PRM_Name("hboxes",  "Bounding Boxes"),
    PRM_Name()
};
static PRM_Default prm_loadmodeDefault(0, "alembic");
static PRM_ChoiceList menu_loadmode(PRM_CHOICELIST_SINGLE, loadModeOptions);

static PRM_Name pointModeOptions[] = {
    PRM_Name("shared",	"Shared Point"),
    PRM_Name("unique",	"Unique Points"),
    PRM_Name()
};
static PRM_Default prm_pointModeDefault(1, "unique");
static PRM_ChoiceList menu_pointMode(PRM_CHOICELIST_SINGLE, pointModeOptions);

static PRM_Name boxCullOptions[] = {
    PRM_Name("none",		"No Spatial Filtering"),
    PRM_Name("inside",		"Load Objects Entirely Inside Box"),
    PRM_Name("anyinside",	"Load Objects With Any Part In Box"),
    PRM_Name("outside",		"Load Object Outside Box"),
    PRM_Name("anyoutside",	"Load Objects With Any Part Outside Box"),
    PRM_Name()
};
static PRM_Default	prm_boxcullDefault(0, "none");
static PRM_ChoiceList	menu_boxcull(PRM_CHOICELIST_SINGLE, boxCullOptions);

static PRM_Name	boxcullSource("boxsource", "Use First Input To Specify Box");
static PRM_Name	boxcullSize("boxsize", "Box Size");
static PRM_Range boxcullSizeRange(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 10);
static PRM_Name	boxcullCenter("boxcenter", "Box Center");

static PRM_Name groupNameOptions[] = {
    PRM_Name("none",	"No Groups"),
    PRM_Name("shape",	"Name Group By Shape Node Path" ),
    PRM_Name("xform",	"Name Group By Transform Node Path" ),
    PRM_Name( 0 )
};
static PRM_Default prm_groupnamesDefault(0, "none");
static PRM_ChoiceList menu_groupnames(PRM_CHOICELIST_SINGLE, groupNameOptions);

static PRM_Name animationFilterOptions[] = {
    PRM_Name("all",		"Include All Primitives"),
    PRM_Name("static",		"Only Static Primitives"),
    PRM_Name("animating",	"Only Animating Primitives"),
    PRM_Name( 0 )
};
static PRM_Default prm_animationfilterDefault(1, "all");
static PRM_ChoiceList menu_animationfilter(PRM_CHOICELIST_SINGLE, animationFilterOptions);

static PRM_Name prm_loadLocatorName("loadLocator", "Load Maya Locator");
static PRM_Name prm_objecPatternName("objectPattern", "Object Pattern");
static PRM_Name prm_subdgroupName("subdgroup", "Subdivision Group");
static PRM_Name prm_pointAttributesName("pointAttributes", "Point Attributes");
static PRM_Name prm_vertexAttributesName("vertexAttributes", "Vertex Attributes");
static PRM_Name prm_primitiveAttributesName("primitiveAttributes", "Primitive Attributes");
static PRM_Name prm_detailAttributesName("detailAttributes", "Detail Attributes");
static PRM_Default prm_starDefault(0, "*");
static PRM_Default prm_loadLocatorDefault(false);

// The order here must match the order of the GA_AttributeOwner enum
static PRM_Name	theAttributePatternNames[GA_ATTRIB_OWNER_N] = {
    PRM_Name("vertexAttributes",	"Vertex Attributes"),
    PRM_Name("pointAttributes",		"Point Attributes"),
    PRM_Name("primitiveAttributes",	"Primitive Attributes"),
    PRM_Name("detailAttributes",	"Detail Attributes"),
};

static PRM_SpareData	theAbcPattern(
	PRM_SpareData::getFileChooserPatternToken(), "*.abc",
	NULL
);

static PRM_Default	mainSwitcher[] =
{
    PRM_Default(7, "Geometry"),
    PRM_Default(8, "Selection"),
    PRM_Default(9, "Attributes"),
};

PRM_Template SOP_AlembicIn2::myTemplateList[] =
{
    PRM_Template(PRM_CALLBACK, 1, &prm_reloadbutton,
	    0, 0, 0, SOP_AlembicIn2::reloadGeo),
    PRM_Template(PRM_FILE,  1, &prm_filenameName, 0, 0, 0, 0, &theAbcPattern),
    PRM_Template(PRM_FLT_J, 1, &prm_frameName, &prm_frameDefault),
    PRM_Template(PRM_FLT_J, 1, &prm_fpsName, &prm_fpsDefault),

    // Define 3 tabs of folders
    PRM_Template(PRM_SWITCHER, 3, &PRMswitcherName, mainSwitcher),

    // Geometry tab
    PRM_Template(PRM_ORD, 1, &prm_loadmodeName, &prm_loadmodeDefault,
	    &menu_loadmode),
    PRM_Template(PRM_ORD, 1, &prm_pointModeName, &prm_pointModeDefault,
	    &menu_pointMode),
    PRM_Template(PRM_TOGGLE, 1, &prm_abcxformName, PRMzeroDefaults),
    PRM_Template(PRM_TOGGLE, 1, &prm_includeXformName, &prm_includeXformDefault),
    PRM_Template(PRM_TOGGLE, 1, &prm_loadLocatorName, &prm_loadLocatorDefault),
    PRM_Template(PRM_ORD, 1, &prm_groupnames, &prm_groupnamesDefault,
	    &menu_groupnames),
    PRM_Template(PRM_STRING, 1, &prm_subdgroupName),

    // Selection tab
    PRM_Template(PRM_STRING, PRM_TYPE_JOIN_PAIR, 1, &prm_objectPathName, &prm_objectPathDefault,
            &prm_objectPathMenu, NULL),
    PRM_Template(PRM_CALLBACK, 1, &prm_pickObjectPathName,
	    0, 0, 0, selectAlembicNodes),
    PRM_Template(PRM_STRING, 1, &prm_objecPatternName, &prm_starDefault),
    PRM_Template(PRM_ORD, 1, &prm_animationfilter, &prm_animationfilterDefault,
	    &menu_animationfilter),
    PRM_Template(PRM_ORD, 1, &prm_boxcull, &prm_boxcullDefault,
	    &menu_boxcull),
    PRM_Template(PRM_TOGGLE, 1, &boxcullSource, PRMzeroDefaults),
    PRM_Template(PRM_XYZ_J,  3, &boxcullSize, PRMoneDefaults,
	    NULL, &boxcullSizeRange),
    PRM_Template(PRM_XYZ_J,  3, &boxcullCenter, PRMzeroDefaults),

    // Attribute tab
    PRM_Template(PRM_STRING, 1, &theAttributePatternNames[GA_ATTRIB_POINT],
		&prm_starDefault),
    PRM_Template(PRM_STRING, 1, &theAttributePatternNames[GA_ATTRIB_VERTEX],
		&prm_starDefault),
    PRM_Template(PRM_STRING, 1, &theAttributePatternNames[GA_ATTRIB_PRIMITIVE],
		&prm_starDefault),
    PRM_Template(PRM_STRING, 1, &theAttributePatternNames[GA_ATTRIB_DETAIL],
		&prm_starDefault),
    PRM_Template(PRM_TOGGLE, 1, &prm_addpath, PRMzeroDefaults),
    PRM_Template(PRM_STRING, 1, &prm_pathattrib, &prm_pathattribDefault),
    PRM_Template(PRM_TOGGLE, 1, &prm_addfile, PRMzeroDefaults),
    PRM_Template(PRM_STRING, 1, &prm_fileattrib, &prm_fileattribDefault),
    PRM_Template(PRM_MULTITYPE_LIST, prm_AttributeRemapTemplate, 2,
            &prm_remapAttribName, PRMzeroDefaults, 0,
            &PRM_SpareData::multiStartOffsetOne),
    PRM_Template()
};

//-*****************************************************************************

SOP_AlembicIn2::SOP_AlembicIn2(OP_Network *net, const char *name,
        OP_Operator *op)
    : SOP_Node(net, name, op)
    , myLastParms()
    , myTopologyConstant(false)
    , myEntireSceneIsConstant(false)
    , myConstantUniqueId(-1)
{
    mySopFlags.setNeedGuide1(1);
}

SOP_AlembicIn2::~SOP_AlembicIn2()
{
    clearEventHandler();
}

//-*****************************************************************************

int
SOP_AlembicIn2::reloadGeo(void *data, int index, float time, const PRM_Template *tplate)
{
    SOP_AlembicIn2 *me = (SOP_AlembicIn2 *) data;
    GABC_Util::clearCache();
    me->unloadData();
    return 1;
}

//-*****************************************************************************

GABC_GEOWalker::BoxCullMode
SOP_AlembicIn2::getCullingBox(UT_BoundingBox &box, OP_Context &ctx)
{
    UT_String			boxcull;
    GABC_GEOWalker::BoxCullMode	mode = GABC_GEOWalker::BOX_CULL_IGNORE;
    fpreal			now = ctx.getTime();

    evalString(boxcull, "boxcull", 0, now);
    if (boxcull == "inside")
	mode = GABC_GEOWalker::BOX_CULL_INSIDE;
    else if (boxcull == "anyinside")
	mode = GABC_GEOWalker::BOX_CULL_ANY_INSIDE;
    else if (boxcull == "outside")
	mode = GABC_GEOWalker::BOX_CULL_OUTSIDE;
    else if (boxcull == "anyoutside")
	mode = GABC_GEOWalker::BOX_CULL_ANY_OUTSIDE;
    if (mode != GABC_GEOWalker::BOX_CULL_IGNORE)
    {
	bool	useinput = (nInputs() != 0 && evalInt("boxsource", 0, now) != 0);
	if (useinput)
	{
	    if (lockInputs(ctx) >= UT_ERROR_ABORT)
		mode = GABC_GEOWalker::BOX_CULL_IGNORE;
	    const GU_Detail	*src = inputGeo(0, ctx);
	    src->getBBox(&box);
	    unlockInputs();
	}
	else
	{
	    fpreal	size[3];
	    fpreal	center[3];
	    evalFloats("boxsize", size, ctx.getTime());
	    evalFloats("boxcenter", center, ctx.getTime());
	    box.initBounds(center[0]-size[0]*.5,
			    center[1]-size[1]*.5,
			    center[2]-size[2]*.5);
	    box.enlargeBounds(center[0]+size[0]*.5,
			    center[1]+size[1]*.5,
			    center[2]+size[2]*.5);
	}
    }
    return mode;
}

//-*****************************************************************************

bool
SOP_AlembicIn2::updateParmsFlags()
{
    bool	changed = false;
    bool	hasbox = (nInputs() > 0);
    bool	enablebox = !hasbox || (evalInt("boxsource", 0, 0) == 0);
    UT_String	boxcull;
    bool	loadAbc;

    evalString(boxcull, "boxcull", 0, 0);
    if (boxcull == "none")
	enablebox = false;
    loadAbc = evalInt("loadmode", 0, 0) == 0;

    changed |= enableParm("pathattrib", evalInt("addpath", 0, 0));
    changed |= enableParm("fileattrib", evalInt("addfile", 0, 0));
    changed |= enableParm("abcxform", loadAbc);
    changed |= enableParm("pointmode", loadAbc);
    changed |= setVisibleState("boxsource", hasbox && boxcull != "none");
    changed |= setVisibleState("boxsize", enablebox);
    changed |= setVisibleState("boxcenter", enablebox);

    return changed;
}

//-*****************************************************************************
void
SOP_AlembicIn2::evaluateParms(Parms &parms, OP_Context &context)
{
    fpreal	now = context.getTime();
    UT_String	sval;

    evalString(sval, "fileName", 0, now);
    parms.myFilename = (const char *)sval;

    parms.myBuildAbcXform = false;
    switch (evalInt("loadmode", 0, now))
    {
	case 0:
	default:
	    parms.myLoadMode = GABC_GEOWalker::LOAD_ABC_PRIMITIVES;
	    parms.myBuildAbcXform = (evalInt("abcxform", 0, now) != 0);
	    break;
	case 1:
	    parms.myLoadMode = GABC_GEOWalker::LOAD_HOUDINI_PRIMITIVES;
	    break;
	case 2:
	    parms.myLoadMode = GABC_GEOWalker::LOAD_HOUDINI_POINTS;
	    break;
	case 3:
	    parms.myLoadMode = GABC_GEOWalker::LOAD_HOUDINI_BOXES;
	    break;
    }
    parms.myBuildLocator = evalInt("loadLocator", 0, now) != 0;
    parms.myBoundMode = getCullingBox(parms.myBoundBox, context);
    switch (evalInt("pointmode", 0, now))
    {
	case 0:
	    parms.myPointMode = GABC_GEOWalker::ABCPRIM_SHARED_POINT;
	    break;
	case 1:
	default:
	    parms.myPointMode = GABC_GEOWalker::ABCPRIM_UNIQUE_POINT;
	    break;
    }

    evalString(parms.myObjectPath, "objectPath", 0, now);
    evalString(parms.myObjectPattern, "objectPattern", 0, now);
    evalString(parms.mySubdGroupName, "subdgroup", 0, now);
    for (int i = 0; i < GA_ATTRIB_OWNER_N; ++i)
    {
	evalString(parms.myAttributePatterns[i],
		theAttributePatternNames[i].getToken(), 0, now);
    }
    parms.myIncludeXform = evalInt("includeXform", 0, now) != 0;
    if (evalInt("addpath", 0, now))
	evalString(parms.myPathAttribute, "pathattrib", 0, now);
    if (evalInt("addfile", 0, now))
	evalString(parms.myFilenameAttribute, "fileattrib", 0, now);

    evalString(sval, "groupnames", 0, now);
    if (sval == "none")
	parms.myGroupMode = GABC_GEOWalker::ABC_GROUP_NONE;
    else if (sval == "xform")
	parms.myGroupMode = GABC_GEOWalker::ABC_GROUP_XFORM_NODE;
    else
	parms.myGroupMode = GABC_GEOWalker::ABC_GROUP_SHAPE_NODE;

    evalString(sval, "animationfilter", 0, now);
    if (sval == "all")
	parms.myAnimationFilter = GABC_GEOWalker::ABC_AFILTER_ALL;
    else if (sval == "static")
	parms.myAnimationFilter = GABC_GEOWalker::ABC_AFILTER_STATIC;
    else
	parms.myAnimationFilter = GABC_GEOWalker::ABC_AFILTER_ANIMATING;


    int	nmapSize = evalInt("remapAttributes", 0, now);
    parms.myNameMapPtr = new GABC_NameMap();
    for (int i = 1; i <= nmapSize; ++i)
    {
	UT_String	abcName, hName;
	evalStringInst("abcName#", &i, abcName, 0, now);
	evalStringInst("hName#", &i, hName, 0, now);
	if (abcName.isstring() && hName.isstring())
	    parms.myNameMapPtr->addMap(abcName, hName);
    }
}

//-*****************************************************************************
void
SOP_AlembicIn2::setupEventHandler(const std::string &filename)
{
    clearEventHandler();
    myEventHandler.reset(new EventHandler(this));
    if (!GABC_Util::addEventHandler(filename, myEventHandler))
	myEventHandler.clear();
}

void
SOP_AlembicIn2::clearEventHandler()
{
    if (myEventHandler)
    {
	EventHandler	*h = UTverify_cast<EventHandler*>(myEventHandler.get());
	h->setSOP(NULL);	// Clear out SOP ptr so no callbacks happen
    }
}

//-*****************************************************************************

void
SOP_AlembicIn2::archiveClearEvent()
{
    // Clear out the lasst-cook parameters
    myLastParms = Parms();
    myConstantUniqueId = -1;
    forceRecook();
}

//-*****************************************************************************
void
SOP_AlembicIn2::setPathAttributes(GABC_GEOWalker &walk, const Parms &parms)
{
    if (parms.myPathAttribute.isstring())
    {
	GA_RWAttributeRef	a = gdp->addStringTuple(GA_ATTRIB_PRIMITIVE,
					parms.myPathAttribute, 1);
	if (a.isValid())
	    walk.setPathAttribute(a);
    }
    if (parms.myFilenameAttribute.isstring())
    {
	GA_RWAttributeRef	aref = gdp->addStringTuple(GA_ATTRIB_DETAIL,
				    parms.myFilenameAttribute, 1);
	GA_RWHandleS	h(aref.getAttribute());
	if (h.isValid())
	    h.set(GA_Offset(0), parms.myFilename.c_str());
	else
	    addWarning(SOP_MESSAGE, "Error adding filename attribute");
    }
}

OP_ERROR
SOP_AlembicIn2::cookMySop(OP_Context &context)
{
    Parms	parms;
    fpreal	now = context.getTime();

    evaluateParms(parms, context);
    if (gdp->getUniqueId() != myConstantUniqueId ||
	    myLastParms.needsNewGeometry(parms))
    {
	myTopologyConstant = false;
	myEntireSceneIsConstant = false;
    }
    // The constant unique id is used to detect if the geometry has been
    // flushed out of the cache.
    myConstantUniqueId = gdp->getUniqueId();

    if (myEntireSceneIsConstant && !myLastParms.needsPathAttributeUpdate(parms))
	return error();

    GABC_GEOWalker	walk(*gdp);

    walk.setObjectPattern(parms.myObjectPattern);
    for (int i = 0; i < GA_ATTRIB_OWNER_N; ++i)
    {
	walk.setAttributePattern((GA_AttributeOwner)i,
		parms.myAttributePatterns[i]);
    }
    if (parms.myAnimationFilter == GABC_GEOWalker::ABC_AFILTER_STATIC)
    {
	// When we only load static geometry, we don't need to evaluate the
	// frame number.  And thus, we aren't marked as time-dependent on the
	// first cook.
	walk.setFrame(1, 24);
    }
    else
    {
	walk.setFrame(evalFloat("frame", 0, now), evalFloat("fps", 0, now));
    }
    walk.setIncludeXform(parms.myIncludeXform);
    walk.setBuildLocator(parms.myBuildLocator);
    walk.setLoadMode(parms.myLoadMode);
    walk.setBuildAbcXform(parms.myBuildAbcXform);
    walk.setNameMapPtr(parms.myNameMapPtr);
    if (myLastParms.myPathAttribute != parms.myPathAttribute)
    {
	if (myLastParms.myPathAttribute.isstring())
	{
	    gdp->destroyAttribute(GA_ATTRIB_PRIMITIVE,
		    myLastParms.myPathAttribute);
	}
	walk.setPathAttributeChanged(true);
    }
    if (myLastParms.myFilenameAttribute != parms.myFilenameAttribute)
    {
	if (myLastParms.myFilenameAttribute.isstring())
	{
	    gdp->destroyAttribute(GA_ATTRIB_DETAIL,
		    myLastParms.myFilenameAttribute);
	}
    }
    walk.setGroupMode(parms.myGroupMode);
    walk.setAnimationFilter(parms.myAnimationFilter);
    walk.setBounds(parms.myBoundMode, parms.myBoundBox);

    bool	needwalk = true;
    walk.setReusePrimitives(myTopologyConstant);
    if (!walk.reusePrimitives())
    {
	gdp->clearAndDestroy();
	setPathAttributes(walk, parms);
    }
    else
    {
	gdp->destroyInternalNormalAttribute();
	setPathAttributes(walk, parms);
	if (walk.buildAbcPrim())
	{
	    walk.updateAbcPrims();
	    needwalk =  false;
	}
    }
    if (parms.mySubdGroupName.isstring())
    {
	GA_PrimitiveGroup	*g;
	g = gdp->findPrimitiveGroup(parms.mySubdGroupName);
	if (!g)
	    g = gdp->newPrimitiveGroup(parms.mySubdGroupName);
	walk.setSubdGroup(g);
    }
    if (parms.myLoadMode == GABC_GEOWalker::LOAD_ABC_PRIMITIVES)
    {
	GA_Offset	shared = GA_INVALID_OFFSET;
	if (parms.myPointMode == GABC_GEOWalker::ABCPRIM_SHARED_POINT)
	{
	    UT_ASSERT(gdp->getNumPoints() == 0 || gdp->getNumPoints() == 1);
	    if (gdp->getNumPoints() == 0)
		shared = gdp->appendPointOffset();
	    else
		shared = gdp->pointOffset(GA_Index(0));
	}
	walk.setPointMode(parms.myPointMode, shared);
    }

    if (needwalk)
    {
	// So we don't get events during our cook
	clearEventHandler();
	if (parms.myObjectPath.isstring())
	{
	    UT_WorkArgs	args;
	    parms.myObjectPath.parse(args);
	    if (args.getArgc())
	    {
		UT_StringArray	olist;
		for (int i = 0; i < args.getArgc(); ++i)
		    olist.append(args(i));
		if (!GABC_Util::walk(parms.myFilename, walk, olist))
		    addError(SOP_MESSAGE, "Error evaluating objects in file");
	    }
	}
	else
	{
	    if (!GABC_Util::walk(parms.myFilename, walk))
		addError(SOP_MESSAGE, "Error evaluating Alembic file");
	}
	setupEventHandler(parms.myFilename);
    }

    if (error() < UT_ERROR_ABORT)
    {
	myEntireSceneIsConstant = walk.isConstant();
	myTopologyConstant = walk.topologyConstant();
    }

    myLastParms = parms;

    return error();
}

OP_ERROR
SOP_AlembicIn2::cookMyGuide1(OP_Context &ctx)
{
    UT_BoundingBox	box;

    myGuide1->stashAll();
    if (getCullingBox(box, ctx) != GABC_GEOWalker::BOX_CULL_IGNORE)
    {
	myGuide1->cube(box.xmin(), box.xmax(), box.ymin(), box.ymax(),
		    box.zmin(), box.zmax());
    }
    return error();
}

//-*****************************************************************************
void
SOP_AlembicIn2::syncNodeVersion(const char *old_version,
				const char *current_version,
				bool *node_deleted)
{
    // Check to see if we're loading from before 12.5.0, if so, we don't want
    // to create Alembic primitives.
    if (UT_String::compareVersionString(old_version, "12.5.0") < 0)
    {
	setInt("loadmode", 0, 0, 1);
    }
    SOP_Node::syncNodeVersion(old_version, current_version, node_deleted);
}

//-*****************************************************************************

void
SOP_AlembicIn2::installSOP(OP_OperatorTable *table)
{
    OP_Operator *alembic_op = new OP_Operator(
        "alembic",			// Internal name
        "Alembic",			// GUI name
        SOP_AlembicIn2::myConstructor,	// Op Constructr
        SOP_AlembicIn2::myTemplateList,	// Parameter Definition
        0, 1,				// Min/Max # of Inputs
        0,				// Local variables
	OP_FLAG_GENERATOR);		// Generator flag
    alembic_op->setIconName("SOP_alembic");
    table->addOperator(alembic_op);
}

const char *
SOP_AlembicIn2::inputLabel(unsigned int idx) const
{
    switch (idx)
    {
	case 0:
	    return "Bounding Source";
	default:
	    break;
    }
    return "Error";
}

void
newSopOperator(OP_OperatorTable *table)
{
    SOP_AlembicIn2::installSOP(table);
}
