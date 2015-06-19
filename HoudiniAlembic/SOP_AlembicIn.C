/*
 * Copyright (c) 2015
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

#include "SOP_AlembicIn.h"
#include <GABC/GABC_GEOWalker.h>

#include <UT/UT_DSOVersion.h>
#include <UT/UT_StringStream.h>
#include <UT/UT_WorkArgs.h>
#include <UT/UT_WorkBuffer.h>
#include <GU/GU_Detail.h>
#include <PRM/PRM_Shared.h>
#include <SOP/SOP_Guide.h>
#include <SOP/SOP_ObjectAppearance.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_Director.h>
#include <OP/OP_NodeInfoParms.h>

#if !defined(CUSTOM_ALEMBIC_TOKEN_PREFIX)
    #define CUSTOM_ALEMBIC_TOKEN_PREFIX	""
    #define CUSTOM_ALEMBIC_LABEL_PREFIX	""
#endif

using namespace GABC_NAMESPACE;

namespace
{
    typedef GABC_Util::PathList		PathList;

    class SOP_AlembicInErr : public GABC_NAMESPACE::GABC_IError
    {
    public:
        SOP_AlembicInErr(SOP_AlembicIn2 &node, UT_Interrupt *interrupt)
            : GABC_IError(interrupt)
            , myNode(node)
        {}
        virtual void	handleError(const char *msg)
                                { myNode.abcError(msg); }
        virtual void	handleWarning(const char *msg)
                                { myNode.abcWarning(msg); }
        virtual void	handleInfo(const char *msg)
                                { myNode.abcInfo(msg); }
    private:
        SOP_AlembicIn2  &myNode;
    };

    static int
    selectAlembicNodes(void *data, int index,
	    fpreal t, const PRM_Template *tplate)
    {
	SOP_AlembicIn2	*sop = (SOP_AlembicIn2 *)(data);
	UT_WorkBuffer	cmd;
	UT_String	filename;
	UT_String	objectpath;
	UT_String	parm_name = tplate->getToken();

	// Strip the "pick" part of the parm name away to leave the name of
	// the object path parm we want to modify.
	parm_name.eraseHead(4);
	cmd.strcpy("treechooser");
	sop->evalString(filename, "fileName", 0, t);
	sop->evalString(objectpath, parm_name, 0, t);
	const PathList	&abcobjects =
	    GABC_Util::getObjectList((const char *)filename);

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
	UT_OStringStream	 os;
	mgr->execute(cmd.buffer(), 0, &os);
	UT_String	result(os.str().buffer());
	result.trimBoundingSpace();
	sop->setString(result, CH_STRING_LITERAL, parm_name, 0, t);

	return 0;
    }

    static int
    stringComparator(const UT_StringHolder *s1, const UT_StringHolder *s2)
    {
        return strcmp(s1->buffer(), s2->buffer());
    }

    // Remove duplicates and redundant strings. Used to clean up
    // paths list.
    //
    // Ex:  {
    //          aaa/bbb/ccc
    //          aaa/bbb
    //          aaa/bbb
    //          aaa/ddd/eee/fff
    //          aaa/ddd/eee/ggg
    //      }
    //
    //      would become:
    //
    //      {
    //          aaa/bbb
    //          aaa/ddd/eee
    //      }
    //
    static void
    removeDuplicates(UT_StringArray &strings)
    {
        if (strings.entries() < 2)
            return;

        UT_StringHolder     str1, str2;
        int i = 1;

        strings.sort(stringComparator);

        while (i < strings.entries())
        {
            str1 = strings(i - 1);
            str2 = strings(i);

            if (str2.startsWith(str1))
            {
                strings.remove(i);
            }
            else
            {
                ++i;
            }
        }
    }
}

SOP_AlembicIn2::Parms::Parms()
    : myLoadMode(GABC_GEOWalker::LOAD_ABC_PRIMITIVES)
    , myBoundMode(GABC_GEOWalker::BOX_CULL_IGNORE)
    , myPointMode(GABC_GEOWalker::ABCPRIM_CENTROID_POINT)
    , myBuildAbcShape(true)
    , myBuildAbcXform(false)
    , myFilename()
    , myObjectPath()
    , myObjectPattern()
    , myExcludeObjectPath()
    , mySubdGroupName()
    , myIncludeXform(true)
    , myMissingFileError(true)
    , myUseVisibility(true)
    , myBuildLocator(false)
    , myLoadUserProps(GABC_GEOWalker::UP_LOAD_NONE)
    , myGroupMode(GABC_GEOWalker::ABC_GROUP_SHAPE_NODE)
    , myAnimationFilter(GABC_GEOWalker::ABC_AFILTER_ALL)
    , myGeometryFilter(GABC_GEOWalker::ABC_GFILTER_ALL)
    , myPolySoup(GABC_GEOWalker::ABC_POLYSOUP_POLYMESH)
    , myViewportLOD(GEO_VIEWPORT_FULL)
    , myPathAttribute("")
    , myFilenameAttribute("")
    , myNameMapPtr()
{
    myBoundBox.makeInvalid();
}

SOP_AlembicIn2::Parms::Parms(const SOP_AlembicIn2::Parms &src)
    : myLoadMode(GABC_GEOWalker::LOAD_ABC_PRIMITIVES)
    , myBoundMode(GABC_GEOWalker::BOX_CULL_IGNORE)
    , myPointMode(GABC_GEOWalker::ABCPRIM_CENTROID_POINT)
    , myBuildAbcShape(true)
    , myBuildAbcXform(false)
    , myFilename()
    , myObjectPath()
    , myObjectPattern()
    , myExcludeObjectPath()
    , mySubdGroupName()
    , myIncludeXform(true)
    , myMissingFileError(true)
    , myUseVisibility(true)
    , myBuildLocator(false)
    , myLoadUserProps(GABC_GEOWalker::UP_LOAD_NONE)
    , myGroupMode(GABC_GEOWalker::ABC_GROUP_SHAPE_NODE)
    , myAnimationFilter(GABC_GEOWalker::ABC_AFILTER_ALL)
    , myGeometryFilter(GABC_GEOWalker::ABC_GFILTER_ALL)
    , myPolySoup(GABC_GEOWalker::ABC_POLYSOUP_POLYMESH)
    , myViewportLOD(GEO_VIEWPORT_FULL)
    , myPathAttribute("")
    , myFilenameAttribute("")
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
    myBuildAbcShape = src.myBuildAbcShape;
    myBuildAbcXform = src.myBuildAbcXform;
    myIncludeXform = src.myIncludeXform;
    myMissingFileError = src.myMissingFileError;
    myUseVisibility = src.myUseVisibility;
    myBuildLocator = src.myBuildLocator;
    myLoadUserProps = src.myLoadUserProps;
    myGroupMode = src.myGroupMode;
    myAnimationFilter = src.myAnimationFilter;
    myGeometryFilter = src.myGeometryFilter;
    myPolySoup = src.myPolySoup;
    myViewportLOD = src.myViewportLOD;
    myNameMapPtr = src.myNameMapPtr;
    myObjectPath.harden(src.myObjectPath);
    myObjectPattern.harden(src.myObjectPattern);
    myExcludeObjectPath.harden(src.myExcludeObjectPath);
    mySubdGroupName.harden(src.mySubdGroupName);
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
    if (myLoadMode == GABC_GEOWalker::LOAD_ABC_PRIMITIVES &&
	myPointMode != src.myPointMode)
    {
	return true;
    }
    if (myBoundMode != GABC_GEOWalker::BOX_CULL_IGNORE)
    {
	if (myBoundBox != src.myBoundBox)
	    return true;
    }
    if (myBuildAbcShape != src.myBuildAbcShape)
	return true;
    if (myBuildAbcXform != src.myBuildAbcXform)
	return true;
    if (myFilename != src.myFilename)
	return true;
    if (myGroupMode != src.myGroupMode)
	return true;
    if (myAnimationFilter != src.myAnimationFilter)
	return true;
    if (myGeometryFilter != src.myGeometryFilter)
        return true;
    if (myPolySoup != src.myPolySoup)
	return true;
    if (myViewportLOD != src.myViewportLOD)
	return true;
    if (myObjectPath != src.myObjectPath)
	return true;
    if (myObjectPattern != src.myObjectPattern)
	return true;
    if (myExcludeObjectPath != src.myExcludeObjectPath)
	return true;
    if (mySubdGroupName != src.mySubdGroupName)
	return true;
    if (myIncludeXform != src.myIncludeXform)
	return true;
    if (myMissingFileError != src.myMissingFileError)
	return true;
    if (myUseVisibility != src.myUseVisibility)
	return true;
    if (myBuildLocator != src.myBuildLocator)
	return true;
    if (myLoadUserProps != src.myLoadUserProps)
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
static PRM_Name prm_missingFileName("missingfile", "Missing File");
static PRM_Name prm_fpsName("fps", "Frames Per Second");
static PRM_Name prm_objectPathName("objectPath", "Object Path");
static PRM_Name	prm_pickObjectPathName("pickobjectPath", "Pick");
static PRM_Name prm_objectExcludeName("objectExclude", "Object Exclude");
static PRM_Name	prm_pickObjectExcludeName("pickobjectExclude", "Pick");
static PRM_Name prm_includeXformName("includeXform", "Transform Geometry To World Space");
static PRM_Name prm_useVisibilityName("usevisibility", "Use Visibility");
static PRM_Name prm_userPropsName("loadUserProps", "User Properties");
static PRM_Name prm_groupnames("groupnames", "Primitive Groups");
static PRM_Name prm_animationfilter("animationfilter", "Animating Objects");
static PRM_Name prm_geometryfilterPolygon("polygonFilter", "Load Polygons");
static PRM_Name prm_geometryfilterCurve("curveFilter", "Load Curves");
static PRM_Name prm_geometryfilterNURBS("NURBSFilter", "Load NURBS");
static PRM_Name prm_geometryfilterPoints("pointsFilter", "Load Points");
static PRM_Name prm_geometryfilterSubd("subdFilter", "Load Subdivision Surfaces");
static PRM_Name prm_loadLocatorName("loadLocator", "Load Maya Locator");
static PRM_Name prm_polysoup("polysoup", "Poly Soup Primitives");
static PRM_Name prm_viewportlod("viewportlod", "Display As");
static PRM_Name prm_boxcull("boxcull", "Box Culling");
static PRM_Name prm_addfile("addfile", "Add Filename Attribute");
static PRM_Name prm_fileattrib("fileattrib", "Filename Attribute");
static PRM_Name prm_addpath("addpath", "Add Path Attribute");
static PRM_Name prm_pathattrib("pathattrib", "Path Attribute");
static PRM_Name prm_remapAttribName("remapAttributes", "Remap Attributes");

static PRM_Name prm_loadmodeName("loadmode", "Load As");
static PRM_Name prm_pointModeName("pointmode", "Points");
static PRM_Name prm_abcxformName("abcxform", "Create Primitives For");

static PRM_Default prm_frameDefault(1, "$FF");
static PRM_Default prm_objectPathDefault(0, "");
static PRM_Default prm_fpsDefault(24, "$FPS");
static PRM_Default prm_includeXformDefault(true);
static PRM_Default prm_useVisibilityDefault(true);
static PRM_Default prm_loadLocatorDefault(false);
static PRM_Default prm_pathattribDefault(0, "path");
static PRM_Default prm_fileattribDefault(0, "abcFileName");
static PRM_Default prm_geometryfilterDefault(true);

static PRM_ChoiceList	prm_objectPathMenu(PRM_CHOICELIST_TOGGLE,
        "__import__('_alembic_hom_extensions').alembicGetObjectPathListForMenu"
                "(hou.pwd().evalParm('fileName'))[:16380]", CH_PYTHON_SCRIPT);

static PRM_Name	loadModeOptions[] = {
    PRM_Name("alembic",	"Alembic Delayed Load Primitives"),
    PRM_Name("houdini",	"Load Houdini Geometry (deprecated)"),
    PRM_Name("hpoints", "Houdini Point Cloud"),
    PRM_Name("hboxes",  "Bounding Boxes"),
    PRM_Name()
};
static PRM_Default prm_loadmodeDefault(0, "alembic");
static PRM_ChoiceList menu_loadmode(PRM_CHOICELIST_SINGLE, loadModeOptions);

static PRM_Name	missingFileOptions[] = {
    PRM_Name("error",	"Report Error"),
    PRM_Name("empty",	"No Geometry"),
    PRM_Name(0),
};
static PRM_Default missingFileDefault(0, "error");
static PRM_ChoiceList menu_missingFile(PRM_CHOICELIST_SINGLE, missingFileOptions);

static PRM_Name pointModeOptions[] = {
    PRM_Name("shared",	"Shared Point"),
    PRM_Name("unique",	"Unique Points At Origin"),
    PRM_Name("centroid", "Unique Points At Centroid"),
    PRM_Name()
};
static PRM_Default prm_pointModeDefault(2, "centroid");
static PRM_ChoiceList menu_pointMode(PRM_CHOICELIST_SINGLE, pointModeOptions);

static PRM_Name	abcxformOptions[] = {
    PRM_Name("off",     "Shape Nodes Only"),
    PRM_Name("on",      "Shape and Transform Nodes"),
    PRM_Name("xform",   "Transform Nodes Only"),
    PRM_Name()
};
static PRM_Default prm_abcxformDefault(0, "off");
static PRM_ChoiceList menu_abcxform(PRM_CHOICELIST_SINGLE, abcxformOptions);

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

static PRM_Name userPropsOptions[] = {
    PRM_Name("none",	"Do Not Load"),
    PRM_Name("data",	"Load Values Only" ),
    PRM_Name("both",	"Load Values and Metadata" ),
    PRM_Name( 0 )
};
static PRM_Default prm_userPropsDefault(0, "none");
static PRM_ChoiceList menu_userProps(PRM_CHOICELIST_SINGLE, userPropsOptions);

static PRM_Name groupNameOptions[] = {
    PRM_Name("none",	"No Groups"),
    PRM_Name("shape",	"Name Group Using Shape Node Full Path"),
    PRM_Name("xform",	"Name Group Using Transform Node Full Path"),
    PRM_Name("basename","Name Group Using Shape Node Name"),
    PRM_Name("xformbase","Name Group Using Transform Node Name"),
    PRM_Name( 0 )
};
static PRM_Default prm_groupnamesDefault(0, "none");
static PRM_ChoiceList menu_groupnames(PRM_CHOICELIST_SINGLE, groupNameOptions);

static PRM_Name animationFilterOptions[] = {
    PRM_Name("all",		"Include All Primitives"),
    PRM_Name("static",		"Only Static Primitives"),
    PRM_Name("deforming",       "Only Deforming Primitives"),
    PRM_Name("transforming",    "Only Transforming Primitives"),
    PRM_Name("animating",       "Only Deforming or Transforming Primitives"),
    PRM_Name( 0 )
};
static PRM_Default prm_animationfilterDefault(1, "all");
static PRM_ChoiceList menu_animationfilter(PRM_CHOICELIST_SINGLE, animationFilterOptions);

static PRM_Name polysoupOptions[] = {
    PRM_Name("none",		"No Poly Soup Primitives"),
    PRM_Name("polymesh",	"Use Poly Soups For Polygon Meshes"),
    PRM_Name("subd",		"Use Poly Soups Wherever Possible"),
    PRM_Name()
};
static PRM_Default prm_polysoupDefault(1, "polymesh");
static PRM_ChoiceList menu_polysoup(PRM_CHOICELIST_SINGLE, polysoupOptions);
static PRM_Default prm_viewportlodDefault(0, "full");

static PRM_Name prm_objecPatternName("objectPattern", "Object Pattern");
static PRM_Name prm_subdgroupName("subdgroup", "Subdivision Group");
static PRM_Default prm_starDefault(0, "*");

static PRM_SpareData theTreeButtonSpareData(
    PRM_SpareArgs() <<
    PRM_SpareToken(PRM_SpareData::getButtonIconToken(), "BUTTONS_tree")
);

// The order here must match the order of the GA_AttributeOwner enum
static PRM_Name	theAttributePatternNames[GA_ATTRIB_OWNER_N] = {
    PRM_Name("vertexAttributes",	"Vertex Attributes"),
    PRM_Name("pointAttributes",		"Point Attributes"),
    PRM_Name("primitiveAttributes",	"Primitive Attributes"),
    PRM_Name("detailAttributes",	"Detail Attributes"),
};

static PRM_SpareData	theAbcPattern(
    PRM_SpareToken(PRM_SpareData::getFileChooserPatternToken(), "*.abc")
);

static PRM_Default	mainSwitcher[] =
{
    PRM_Default(9, "Geometry"),
    PRM_Default(16, "Selection"),
    PRM_Default(10, "Attributes"),
};

static PRM_SpareData *
sopCreateObjectPathSpareData(bool clear_path, bool clear_exclude)
{
    static const char	*theBaseScript =
	"import soputils\n"
	"kwargs['objectPath'] = kwargs['node'].parmTuple('objectPath')\n"
	"kwargs['objectExclude'] = kwargs['node'].parmTuple('objectExclude')\n"
	"kwargs['clearObjectPath'] = %s\n"
	"kwargs['clearObjectExclude'] = %s\n"
	"soputils.selectAlembicPaths(kwargs)";
    UT_WorkBuffer	 script;

    script.sprintf(theBaseScript,
		   clear_path ? "True" : "False",
		   clear_exclude ? "True" : "False");

    return new PRM_SpareData(
	PRM_SpareArgs() <<
	PRM_SpareToken(PRM_SpareData::getScriptActionToken(),
		script.buffer()) <<
	PRM_SpareToken(PRM_SpareData::getScriptActionHelpToken(),
		"Select Alembic objects from an available viewport.") <<
	PRM_SpareToken(PRM_SpareData::getScriptActionIconToken(),
		"BUTTONS_reselect")
    );
}

PRM_Template SOP_AlembicIn2::myTemplateList[] =
{
    PRM_Template(PRM_CALLBACK, 1, &prm_reloadbutton,
	    0, 0, 0, SOP_AlembicIn2::reloadGeo),
    PRM_Template(PRM_FILE,  1, &prm_filenameName, 0, 0, 0, 0, &theAbcPattern),
    PRM_Template(PRM_FLT_J, 1, &prm_frameName, &prm_frameDefault),
    PRM_Template(PRM_FLT_J, 1, &prm_fpsName, &prm_fpsDefault),
    PRM_Template(PRM_ORD,   1, &prm_missingFileName, &missingFileDefault,
	    &menu_missingFile),

    // Define 3 tabs of folders
    PRM_Template(PRM_SWITCHER, 3, &PRMswitcherName, mainSwitcher),

    // Geometry tab 
    // Currently there are 9 elements (9 PRM_Template() calls below) in this tab, 
    // which matches PRM_Default(9, "Geometry") defined in mainSwitcher
    PRM_Template(PRM_ORD, 1, &prm_loadmodeName, &prm_loadmodeDefault,
	    &menu_loadmode),
    PRM_Template(PRM_ORD, 1, &prm_viewportlod, &prm_viewportlodDefault,
	    &PRMviewportLODMenu),
    PRM_Template(PRM_ORD, 1, &prm_pointModeName, &prm_pointModeDefault,
	    &menu_pointMode),
    PRM_Template(PRM_ORD, 1, &prm_abcxformName, &prm_abcxformDefault,
            &menu_abcxform),
    PRM_Template(PRM_ORD, 1, &prm_polysoup, &prm_polysoupDefault,
	    &menu_polysoup),
    PRM_Template(PRM_TOGGLE, 1, &prm_includeXformName,
	    &prm_includeXformDefault),
    PRM_Template(PRM_TOGGLE, 1, &prm_useVisibilityName,
	    &prm_useVisibilityDefault),
    PRM_Template(PRM_ORD, 1, &prm_groupnames, &prm_groupnamesDefault,
	    &menu_groupnames),
    PRM_Template(PRM_STRING, 1, &prm_subdgroupName),

    // Selection tab
    // Currently there are 16 elements (16 PRM_Template() calls below) in this tab, 
    // which matches PRM_Default(16, "Selection") defined in mainSwitcher
    PRM_Template(PRM_STRING, PRM_TYPE_JOIN_PAIR, 1, &prm_objectPathName,
	    &prm_objectPathDefault, &prm_objectPathMenu, NULL, NULL,
	    sopCreateObjectPathSpareData(true, true)),
    PRM_Template(PRM_CALLBACK, PRM_TYPE_NO_LABEL, 1, &prm_pickObjectPathName,
	    0, 0, 0, selectAlembicNodes, &theTreeButtonSpareData),
    PRM_Template(PRM_STRING, PRM_TYPE_JOIN_PAIR, 1, &prm_objectExcludeName,
	    &prm_objectPathDefault, &prm_objectPathMenu, NULL, NULL,
	    sopCreateObjectPathSpareData(false, true)),
    PRM_Template(PRM_CALLBACK, PRM_TYPE_NO_LABEL, 1, &prm_pickObjectExcludeName,
	    0, 0, 0, selectAlembicNodes, &theTreeButtonSpareData),
    PRM_Template(PRM_STRING, 1, &prm_objecPatternName, &prm_starDefault),
    PRM_Template(PRM_ORD, 1, &prm_animationfilter, &prm_animationfilterDefault,
	    &menu_animationfilter),
    PRM_Template(PRM_TOGGLE, 1, &prm_geometryfilterPolygon, &prm_geometryfilterDefault),
    PRM_Template(PRM_TOGGLE, 1, &prm_geometryfilterCurve, &prm_geometryfilterDefault),
    PRM_Template(PRM_TOGGLE, 1, &prm_geometryfilterNURBS, &prm_geometryfilterDefault),
    PRM_Template(PRM_TOGGLE, 1, &prm_geometryfilterPoints, &prm_geometryfilterDefault),
    PRM_Template(PRM_TOGGLE, 1, &prm_geometryfilterSubd, &prm_geometryfilterDefault),
    PRM_Template(PRM_TOGGLE, 1, &prm_loadLocatorName, &prm_loadLocatorDefault),
    PRM_Template(PRM_ORD, 1, &prm_boxcull, &prm_boxcullDefault,
	    &menu_boxcull),
    PRM_Template(PRM_TOGGLE, 1, &boxcullSource, PRMzeroDefaults),
    PRM_Template(PRM_XYZ_J,  3, &boxcullSize, PRMoneDefaults,
	    NULL, &boxcullSizeRange),
    PRM_Template(PRM_XYZ_J,  3, &boxcullCenter, PRMzeroDefaults),

    // Attribute tab
    // Currently there are 10 elements (10 PRM_Template() calls below) in this tab, 
    // which matches PRM_Default(10, "Attributes") defined in mainSwitcher
    PRM_Template(PRM_STRING, 1, &theAttributePatternNames[GA_ATTRIB_POINT],
		&prm_starDefault),
    PRM_Template(PRM_STRING, 1, &theAttributePatternNames[GA_ATTRIB_VERTEX],
		&prm_starDefault),
    PRM_Template(PRM_STRING, 1, &theAttributePatternNames[GA_ATTRIB_PRIMITIVE],
		&prm_starDefault),
    PRM_Template(PRM_STRING, 1, &theAttributePatternNames[GA_ATTRIB_DETAIL],
		&prm_starDefault),
    PRM_Template(PRM_ORD, 1, &prm_userPropsName, &prm_userPropsDefault,
            &menu_userProps),
    PRM_Template(PRM_TOGGLE, 1, &prm_addpath, PRMoneDefaults),
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
    , myComputedFrameRange(false)
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
    int		loadmode;

    evalString(boxcull, "boxcull", 0, 0);
    if (boxcull == "none")
	enablebox = false;
    loadmode = evalInt("loadmode", 0, 0);

    changed |= enableParm("pathattrib", evalInt("addpath", 0, 0));
    changed |= enableParm("fileattrib", evalInt("addfile", 0, 0));
    changed |= enableParm("abcxform", loadmode == 0);
    changed |= enableParm("pointmode", loadmode == 0);
    changed |= enableParm("subdgroup", loadmode == 1);
    changed |= enableParm("viewportlod", loadmode == 0);
    changed |= enableParm("polysoup", loadmode == 1);
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

    switch (evalInt("loadmode", 0, now))
    {
	case 0:
	default:
	    parms.myLoadMode = GABC_GEOWalker::LOAD_ABC_PRIMITIVES;
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

    switch (evalInt("abcxform", 0, now))
    {
        case 0:
        default:
            parms.myBuildAbcShape = true;
            parms.myBuildAbcXform = false;
            break;
        case 1:
            parms.myBuildAbcShape = true;
            parms.myBuildAbcXform = true;
            break;
        case 2:
            parms.myBuildAbcShape = false;
            parms.myBuildAbcXform = true;
            break;
    }

    parms.myBoundMode = getCullingBox(parms.myBoundBox, context);
    switch (evalInt("pointmode", 0, now))
    {
	case 0:
	    parms.myPointMode = GABC_GEOWalker::ABCPRIM_SHARED_POINT;
	    break;
	case 1:
	    parms.myPointMode = GABC_GEOWalker::ABCPRIM_UNIQUE_POINT;
	    break;
	case 2:
	default:
	    parms.myPointMode = GABC_GEOWalker::ABCPRIM_CENTROID_POINT;
	    break;
    }

    evalString(parms.myObjectPath, "objectPath", 0, now);
    evalString(parms.myObjectPattern, "objectPattern", 0, now);
    evalString(parms.myExcludeObjectPath, "objectExclude", 0, now);
    evalString(parms.mySubdGroupName, "subdgroup", 0, now);
    parms.myIncludeXform = evalInt("includeXform", 0, now) != 0;
    parms.myMissingFileError = evalInt("missingfile", 0, now) == 0;
    parms.myUseVisibility = evalInt("usevisibility", 0, now) != 0;
    parms.myBuildLocator = evalInt("loadLocator", 0, now) != 0;
    if (evalInt("addpath", 0, now))
	evalString(parms.myPathAttribute, "pathattrib", 0, now);
    if (evalInt("addfile", 0, now))
	evalString(parms.myFilenameAttribute, "fileattrib", 0, now);

    evalString(sval, "loadUserProps", 0, now);
    if (sval == "none")
	parms.myLoadUserProps = GABC_GEOWalker::UP_LOAD_NONE;
    else if (sval == "data")
	parms.myLoadUserProps = GABC_GEOWalker::UP_LOAD_DATA;
    else
    {
        UT_ASSERT(sval == "both");
	parms.myLoadUserProps = GABC_GEOWalker::UP_LOAD_ALL;
    }

    evalString(sval, "groupnames", 0, now);
    if (sval == "none")
	parms.myGroupMode = GABC_GEOWalker::ABC_GROUP_NONE;
    else if (sval == "xform")
	parms.myGroupMode = GABC_GEOWalker::ABC_GROUP_XFORM_NODE;
    else if (sval == "shape")
	parms.myGroupMode = GABC_GEOWalker::ABC_GROUP_SHAPE_NODE;
    else if (sval == "basename")
	parms.myGroupMode = GABC_GEOWalker::ABC_GROUP_SHAPE_BASENAME;
    else
	parms.myGroupMode = GABC_GEOWalker::ABC_GROUP_XFORM_BASENAME;

    evalString(sval, "animationfilter", 0, now);
    if (sval == "static")
	parms.myAnimationFilter = GABC_GEOWalker::ABC_AFILTER_STATIC;
    else if (sval == "animating")
	parms.myAnimationFilter = GABC_GEOWalker::ABC_AFILTER_ANIMATING;
    else if (sval == "deforming")
        parms.myAnimationFilter = GABC_GEOWalker::ABC_AFILTER_DEFORMING;
    else if (sval == "transforming")
        parms.myAnimationFilter = GABC_GEOWalker::ABC_AFILTER_TRANSFORMING;
    else
        parms.myAnimationFilter = GABC_GEOWalker::ABC_AFILTER_ALL;

    int geometryFilterBytes = 0x00;
    if (evalInt("polygonFilter", 0, now) != 0)
        geometryFilterBytes |= GABC_GEOWalker::ABC_GFILTER_POLYMESH;
    if (evalInt("curveFilter", 0, now) != 0)
        geometryFilterBytes |= GABC_GEOWalker::ABC_GFILTER_CURVES;
    if (evalInt("NURBSFilter", 0, now) != 0)
        geometryFilterBytes |= GABC_GEOWalker::ABC_GFILTER_NUPATCH;
    if (evalInt("pointsFilter", 0, now) != 0)
        geometryFilterBytes |= GABC_GEOWalker::ABC_GFILTER_POINTS;
    if (evalInt("subdFilter", 0, now) != 0)
        geometryFilterBytes |= GABC_GEOWalker::ABC_GFILTER_SUBD;
    parms.myGeometryFilter = geometryFilterBytes;

    evalString(sval, "polysoup", 0, now);
    if (sval == "none")
	parms.myPolySoup = GABC_GEOWalker::ABC_POLYSOUP_NONE;
    else if (sval == "polymesh")
	parms.myPolySoup = GABC_GEOWalker::ABC_POLYSOUP_POLYMESH;
    else if (sval == "subd")
	parms.myPolySoup = GABC_GEOWalker::ABC_POLYSOUP_SUBD;
    else
	UT_ASSERT(0 && "Bad value for polysoup");

    evalString(sval, "viewportlod", 0, now);
    parms.myViewportLOD = GEOviewportLOD(sval);
    if (parms.myViewportLOD < 0)
    {
	// TODO: Add warning about impossible case
	parms.myViewportLOD = GEO_VIEWPORT_FULL;
    }

    int	nmapSize = evalInt("remapAttributes", 0, now);
    parms.myNameMapPtr = new GEO_PackedNameMap();
    for (int i = 1; i <= nmapSize; ++i)
    {
	UT_String	abcName, hName;
	evalStringInst("abcName#", &i, abcName, 0, now);
	evalStringInst("hName#", &i, hName, 0, now);
	if (abcName.isstring() && hName.isstring())
	    parms.myNameMapPtr->addMap(abcName, hName);
    }
    for (int i = 0; i < GA_ATTRIB_OWNER_N; ++i)
    {
	UT_String	pattern;
	evalString(pattern, theAttributePatternNames[i].getToken(), 0, now);
	parms.myNameMapPtr->setPattern((GA_AttributeOwner)i, pattern);
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

static const char	*theHoudiniGeometryWarning =
    "Loading Houdini Geometry isn't always as accurate as unpacking Alembic primitives.";

void
SOP_AlembicIn2::getNodeSpecificInfoText(OP_Context &context,
    OP_NodeInfoParms &iparms)
{
    // Get parent's text.
    SOP_Node::getNodeSpecificInfoText(context, iparms);

    // If the scene is animated and we have computed valid start and end frames,
    // display this information.
    if (!myEntireSceneIsConstant && myStartFrame != myEndFrame && myComputedFrameRange)
    {
	fpreal fps = evalFloat("fps", 0, context.getTime());

        iparms.appendSprintf("Frame range: %g to %g (%g fps)\n", myStartFrame, myEndFrame, fps);
	iparms.appendSprintf("Time range: %.2fs to %.2fs", myStartFrame/fps, myEndFrame/fps);
    }
}

OP_ERROR
SOP_AlembicIn2::cookMySop(OP_Context &context)
{
    Parms	parms;
    fpreal	now = context.getTime();

    evaluateParms(parms, context);
    if (parms.myLoadMode == GABC_GEOWalker::LOAD_HOUDINI_PRIMITIVES)
    {
	addWarning(SOP_MESSAGE, theHoudiniGeometryWarning);
    }
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
    {
	myComputedFrameRange = false;
	return error();
    }

    SOP_AlembicInErr    error_handler(*this, UTgetInterrupt());
    GABC_GEOWalker	walk(*gdp, error_handler);

    walk.setObjectPattern(parms.myObjectPattern);
    walk.setExcludeObjects(parms.myExcludeObjectPath);
    if (parms.myAnimationFilter == GABC_GEOWalker::ABC_AFILTER_STATIC)
    {
	// When we only load static geometry, we don't need to evaluate the
	// frame number.  And thus, we aren't marked as time-dependent on the
	// first cook.
	walk.setFrame(1, 24);
    }
    else
    {
	double	fps = evalFloat("fps", 0, now);
	if (SYSequalZero(fps))
	{
	    addWarning(SOP_MESSAGE, "FPS evaluates to 0");
	    fps = 1;
	}
	walk.setFrame(evalFloat("frame", 0, now), fps);
    }
    walk.setIncludeXform(parms.myIncludeXform);
    walk.setUseVisibility(parms.myUseVisibility);
    walk.setBuildLocator(parms.myBuildLocator);
    walk.setLoadMode(parms.myLoadMode);
    walk.setBuildAbcShape(parms.myBuildAbcShape);
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
    walk.setUserProps(parms.myLoadUserProps);
    walk.setGroupMode(parms.myGroupMode);
    walk.setAnimationFilter(parms.myAnimationFilter);
    walk.setGeometryFilter(parms.myGeometryFilter);
    walk.setPolySoup(parms.myPolySoup);
    walk.setViewportLOD(parms.myViewportLOD);
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
    // Delete previous subdivision group
    if (myLastParms.mySubdGroupName != parms.mySubdGroupName)
	gdp->destroyPrimitiveGroup(myLastParms.mySubdGroupName);
    // Create new subd primtiive group
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
	    UT_String	opath(parms.myObjectPath);
	    opath.parse(args);
	    if (args.getArgc())
	    {
		UT_StringArray	olist;
		for (int i = 0; i < args.getArgc(); ++i)
		    olist.append(args(i));

		removeDuplicates(olist);

		if (!GABC_Util::walk(parms.myFilename, walk, olist))
		{
		    if (parms.myMissingFileError || !walk.badArchive())
		    {
			addError(SOP_MESSAGE,
				"Error evaluating objects in file");
		    }
		    else
		    {
			addWarning(SOP_MESSAGE,
				"Invalid or missing Alembic archive");
		    }
		}
	    }
	}
	else
	{
	    if (!GABC_Util::walk(parms.myFilename, walk))
	    {
		UT_WorkBuffer msg;
		if (parms.myMissingFileError || !walk.badArchive())
		{
		    msg.sprintf("Error evaluating Alembic file (%s)",
				parms.myFilename.c_str());
		    addError(SOP_MESSAGE, msg.buffer());
		}
		else
		{
		    msg.sprintf("Invalid or missing Alembic file (%s)",
				parms.myFilename.c_str());
		    addWarning(SOP_MESSAGE, msg.buffer());
		}
	    }
	}
	setupEventHandler(parms.myFilename);
    }

    if (error() < UT_ERROR_ABORT)
    {
	myEntireSceneIsConstant = walk.isConstant();
	myTopologyConstant = walk.topologyConstant();
    }
    else
    {
	myComputedFrameRange = false;
    }

    myLastParms = parms;

    if (!myEntireSceneIsConstant && walk.computedValidTimeRange())
    {
	fpreal fps = evalFloat("fps", 0, now);
	myStartFrame = walk.getStartTime() * fps;
	myEndFrame = walk.getEndTime() * fps;
        myComputedFrameRange = true;
    }

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

namespace
{

class abcFileAppearance : public SOP_ObjectAppearance
{
public:
    abcFileAppearance(SOP_AlembicIn2 *o)
	: myObject(o)
    {
    }

    virtual bool	isGeometricObject() const { return false; }
    
    virtual bool	applyEdits()		{ return true; }
    
    virtual bool 	viewportLOD(GA_Offset, GEO_ViewportLOD &lod) const
    {
	UT_String	lod_name;
	myObject->evalString(lod_name, "viewportlod", 0, CHgetEvalTime());
	lod = GEOviewportLOD(lod_name);
	return lod != GEO_VIEWPORT_INVALID_MODE;
    }
    
    using SOP_ObjectAppearance::setViewportLOD;
    virtual bool	setViewportLOD(const char *, GEO_ViewportLOD lod)
    {
	UT_String lod_name(GEOviewportLOD(lod));
	if (!lod_name.isstring())
	    return false;
	myObject->setString(lod_name, CH_STRING_LITERAL, "viewportlod", 0,
			    CHgetEvalTime());
	return true;
    }

private:
    SOP_AlembicIn2	*myObject;
};

}

SOP_ObjectAppearancePtr
SOP_AlembicIn2::getObjectAppearance()
{
    return SOP_ObjectAppearancePtr(new abcFileAppearance(this));
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
	setInt("polysoup", 0, 0, 0);
    }
    SOP_Node::syncNodeVersion(old_version, current_version, node_deleted);
}

//-*****************************************************************************

void
SOP_AlembicIn2::installSOP(OP_OperatorTable *table)
{
    OP_Operator *alembic_op = new OP_Operator(
        CUSTOM_ALEMBIC_TOKEN_PREFIX "alembic",		// Internal name
        CUSTOM_ALEMBIC_LABEL_PREFIX "Alembic",		// GUI name
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
