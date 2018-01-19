/*
 * Copyright (c) 2018
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

#include "ROP_AlembicOut.h"

#include "ROP_AbcNodeInstance.h"
#include "ROP_AbcHierarchySample.h"
#include "ROP_AbcRefiner.h"

#include <GABC/GABC_IObject.h>
#include <GABC/GABC_PackedImpl.h>
#include <GABC/GABC_Util.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_GEODetail.h>
#include <GT/GT_PrimCollect.h>
#include <GU/GU_PrimPacked.h>
#include <OBJ/OBJ_Camera.h>
#include <OBJ/OBJ_Geometry.h>
#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_SubNet.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_BundleList.h>
#include <OP/OP_Director.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <ROP/ROP_Error.h>
#include <ROP/ROP_Shared.h>
#include <SOP/SOP_Node.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_JSONWriter.h>

#if !defined(CUSTOM_ALEMBIC_TOKEN_PREFIX)
    #define CUSTOM_ALEMBIC_TOKEN_PREFIX ""
    #define CUSTOM_ALEMBIC_LABEL_PREFIX ""
#endif

using namespace UT::Literal;

typedef GABC_NAMESPACE::GABC_IObject GABC_IObject;
typedef GABC_NAMESPACE::GABC_PackedImpl GABC_PackedImpl;
typedef GABC_NAMESPACE::GABC_NodeType GABC_NodeType;
typedef GABC_NAMESPACE::GABC_Util GABC_Util;

static PRM_Name theFilenameName("filename", "Alembic File");
static PRM_Name theFormatName("format", "Format");
static PRM_Name theRenderFullRange("render_full_range", "Render Full Range (Override Frame-By-Frame)");
static PRM_Name theInitSim("initsim", "Initialize Simulation OPs");
static PRM_Name theSingleSopModeName("use_sop_path", "Use SOP Path");
static PRM_Name theSOPPathName("sop_path", "SOP Path");
static PRM_Name theSubdGroupName("subdgroup", "Subdivision Group");
static PRM_Name theBuildHierarchyFromPathName("build_from_path", "Build Hierarchy From Attribute");
static PRM_Name thePathAttribName("path_attrib", "Path Attribute");
static PRM_Name theRootName("root", "Root Object"); 
static PRM_Name theObjectsName("objects", "Objects");
static PRM_Name theCollapseName("collapse", "Collapse Objects");
static PRM_Name theSaveHiddenName("save_hidden", "Save All Non-Displayed (Hidden) Objects");
static PRM_Name theDisplaySOPName("displaysop", "Use Display SOP");
static PRM_Name thePartitionModeName("partition_mode", "Partition Mode");
static PRM_Name thePartitionAttributeName("partition_attribute", "Partition Attribute");
static PRM_Name theFullBoundsName("full_bounds", "Full Bounding Box Tree");
static PRM_Name thePackedTransformName("packed_transform", "Packed Transform");
static PRM_Name theUseInstancingName("use_instancing", "Use Instancing Where Possible");
static PRM_Name theShapeNodesName("shape_nodes", "Create Shape Nodes");
static PRM_Name theSaveAttributesName("save_attributes", "Save Attributes");
static PRM_Name thePointAttributesName("pointAttributes", "Point Attributes");
static PRM_Name theVertexAttributesName("vertexAttributes", "Vertex Attributes");
static PRM_Name thePrimitiveAttributesName("primitiveAttributes", "Primitive Attributes");
static PRM_Name theDetailAttributesName("detailAttributes", "Detail Attributes");
static PRM_Name thePromoteUniformPatternName("prim_to_detail_pattern", "Primitive To Detail");
static PRM_Name theForcePromoteUniformName("force_prim_to_detail", "Force Conversion of Matching Primitive Attributes to Detail");
static PRM_Name theUVAttribPatternName("uvAttributes", "Additional UV Attributes");
static PRM_Name theFaceSetModeName("facesets", "Face Sets");
static PRM_Name theMotionBlurName("motionBlur", "Use Motion Blur");
static PRM_Name theSampleName("samples", "Samples");
static PRM_Name theShutterName("shutter", "Shutter");

static PRM_Default theFilenameDefault(0, "$HIP/output.abc");
static PRM_Default theFormatDefault(0, "default");
static PRM_Default theRootDefault(0, "/obj");
static PRM_Default theStarDefault(0, "*");
static PRM_Default thePathAttribDefault(0, "path");
static PRM_Default theCollapseDefault(0, "off");
static PRM_Default thePackedTransformDefault(0, "deform");
static PRM_Default theFaceSetModeDefault(1, "nonempty");
static PRM_Default theSampleDefault(2);
static PRM_Default theShutterDefault[] = {0, 1};

static PRM_SpareData theAbcPattern(
	PRM_SpareToken(PRM_SpareData::getFileChooserPatternToken(), "*.abc")
    );

static PRM_Default mainSwitcher[] =
{
    PRM_Default(13, "Hierarchy"),
    PRM_Default(12, "Geometry"),
    PRM_Default(3, "Motion Blur"),
};

static PRM_Name theFormatChoices[] =
{
    PRM_Name("default",	"Default Format"),
    PRM_Name("hdf5",	"HDF5"),
    PRM_Name("ogawa",	"Ogawa"),
    PRM_Name()
};

static PRM_Name theCollapseChoices[] =
{
    PRM_Name("off", "Do Not Collapse Identity Objects"),
    PRM_Name("geo", "Collapse Non-Animating Identity Geometry Objects"),
    PRM_Name("on",  "Collapse Non-Animating Identity Objects"),
    PRM_Name()
};

static PRM_Name thePartitionModeChoices[] =
{
    PRM_Name("no",         "No Geometry Partitioning"),
    PRM_Name("full",       "Use Attribute Value"),
    PRM_Name("shape",      "Use Shape Node Component Of Path Attribute Value"),
    PRM_Name("xform",      "Use Transform Node Component Of Path Attribute value"),
    PRM_Name("xformshape", "Use Combination Of Transform/Shape Node"),
    PRM_Name(), // Sentinal
};

static PRM_Name thePartitionAttributeChoices[] =
{
    PRM_Name("",        "No Geometry Partitions"),
    PRM_Name("name",    "Partition Based On 'name' Attribute"),
    PRM_Name("abcPath", "Partition Based On 'abcPath' Attribute"),
    PRM_Name() // Sentinal
};

static PRM_Name thePackedTransformChoices[] =
{
    PRM_Name("deform",		"Deform Geometry"),
    PRM_Name("transform",	"Transform Geometry"),
    PRM_Name("transformparent",	"Merge With Parent Transform"),
    PRM_Name()
};

static PRM_Name theFaceSetModeChoices[] =
{
    PRM_Name("no",	    "No Face Sets"),
    PRM_Name("nonempty",    "Save Non-Empty Groups As Face Sets"),
    PRM_Name("all",	    "Save All Groups As Face Sets"),
    PRM_Name()
};

static void
buildBundleMenu(void *, PRM_Name *menu, int max,
		const PRM_SpareData *spare, const PRM_Parm *)
{
    OPgetDirector()->getBundles()->buildBundleMenu(menu, max,
				spare ? spare->getValue("opfilter") : 0);
}

static const GU_Detail *
getSopGeo(ROP_AlembicOut *me)
{
    fpreal time = CHgetEvalTime();
    SOP_Node *sop = me->getSopNode(time);

    if(!sop)
	return nullptr;

    OP_Context context(time);
    GU_DetailHandle *gdh = (GU_DetailHandle *)sop->getCookedData(context);
    if(!gdh)
	return nullptr;

    return sop->getLastGeo();
}

static void
buildGroupMenu(void *data, PRM_Name *menu_entries, int menu_size,
		const PRM_SpareData *, const PRM_Parm *)
{
    int i = 0;
    const GU_Detail *gdp = getSopGeo(static_cast<ROP_AlembicOut *>(data));
    if(gdp)
    {
	UT_StringArray vals;
	auto &table = gdp->getElementGroupTable(GA_ATTRIB_PRIMITIVE);
	for(auto it = table.beginTraverse(); !it.atEnd(); ++it)
	{
	    auto grp = it.group();

	    if(!grp->isInternal())
		vals.append(grp->getName());
	}

	vals.sort();
	while(i < vals.entries() && i + 1 < menu_size)
	{
	    menu_entries[i].setToken(vals(i));
	    menu_entries[i].setLabel(vals(i));
	    ++i;
	}
    }

    menu_entries[i].setToken(0);
    menu_entries[i].setLabel(0);
}

static void
buildAttribMenu(void *data, PRM_Name *menu_entries, int menu_size,
		const PRM_SpareData *, const PRM_Parm *)
{
    int i = 0;
    const GU_Detail *gdp = getSopGeo(static_cast<ROP_AlembicOut *>(data));
    if(gdp)
    {
	UT_StringArray vals;
	auto &dict = gdp->getAttributeDict(GA_ATTRIB_PRIMITIVE);
	for(auto it = dict.begin(); !it.atEnd(); ++it)
	{
	    const GA_Attribute *atr = it.attrib();

	    if(atr->getStorageClass() == GA_STORECLASS_STRING &&
	       atr->getTupleSize() == 1 &&
	       atr->getScope() == GA_SCOPE_PUBLIC)
	    {
		vals.append(atr->getName());
	    }
	}

	vals.sort();
	while(i < vals.entries() && i + 1 < menu_size)
	{
	    menu_entries[i].setToken(vals(i));
	    menu_entries[i].setLabel(vals(i));
	    ++i;
	}
    }

    menu_entries[i].setToken(0);
    menu_entries[i].setLabel(0);
}

static PRM_ChoiceList theFormatMenu(PRM_CHOICELIST_SINGLE, theFormatChoices);
static PRM_ChoiceList theSubdGroupMenu(PRM_CHOICELIST_REPLACE, buildGroupMenu);
static PRM_ChoiceList thePathAttribMenu(PRM_CHOICELIST_REPLACE, buildAttribMenu);
static PRM_ChoiceList theObjectsMenu(PRM_CHOICELIST_REPLACE, buildBundleMenu);
static PRM_ChoiceList theCollapseMenu(PRM_CHOICELIST_SINGLE, theCollapseChoices);
static PRM_ChoiceList thePartitionModeMenu(PRM_CHOICELIST_SINGLE, thePartitionModeChoices);
static PRM_ChoiceList thePartitionAttributeMenu(PRM_CHOICELIST_REPLACE, thePartitionAttributeChoices);
static PRM_ChoiceList thePackedTransformMenu(PRM_CHOICELIST_SINGLE, thePackedTransformChoices);
static PRM_ChoiceList theFaceSetModeMenu(PRM_CHOICELIST_SINGLE, theFaceSetModeChoices);

// Make paths relative to /obj (for the bundle code)
static PRM_SpareData theObjectList(PRM_SpareArgs()
		<< PRM_SpareToken("opfilter", "!!OBJ!!")
		<< PRM_SpareToken("oprelative", "/obj"));

static PRM_Template theParameters[] =
{
    PRM_Template(PRM_FILE, 1, &theFilenameName, &theFilenameDefault,
		    0, 0, 0, &theAbcPattern),
    PRM_Template(PRM_ORD, 1, &theFormatName, &theFormatDefault, &theFormatMenu),
    PRM_Template(PRM_TOGGLE, 1, &ROPmkpath, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE, 1, &theRenderFullRange, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE, 1, &theInitSim),
    PRM_Template(PRM_SWITCHER, 3, &PRMswitcherName, mainSwitcher),
    PRM_Template(PRM_TOGGLE, 1, &theSingleSopModeName),
    PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH, 1, &theSOPPathName,
		    0, 0, 0, 0, &PRM_SpareData::sopPath),
    PRM_Template(PRM_STRING, 1, &theSubdGroupName, 0, &theSubdGroupMenu),
    PRM_Template(PRM_TOGGLE, 1, &theBuildHierarchyFromPathName),
    PRM_Template(PRM_STRING, 1, &thePathAttribName, &thePathAttribDefault,
		    &thePathAttribMenu),
    PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH,
		    1, &theRootName, &theRootDefault, 0, 0, 0,
		    &PRM_SpareData::objPath),
    PRM_Template(PRM_STRING_OPLIST, PRM_TYPE_DYNAMIC_PATH_LIST,
		    1, &theObjectsName, &theStarDefault, &theObjectsMenu, 0, 0,
		    &theObjectList),
    PRM_Template(PRM_ORD, 1, &theCollapseName, &theCollapseDefault,
		    &theCollapseMenu),
    PRM_Template(PRM_TOGGLE, 1, &theSaveHiddenName, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE, 1, &theDisplaySOPName),
    PRM_Template(PRM_ORD, 1, &thePartitionModeName, 0, &thePartitionModeMenu),
    PRM_Template(PRM_STRING, 1, &thePartitionAttributeName, 0,
		    &thePartitionAttributeMenu),
    PRM_Template(PRM_TOGGLE, 1, &theFullBoundsName),
    PRM_Template(PRM_ORD, 1, &thePackedTransformName,
		    &thePackedTransformDefault, &thePackedTransformMenu),
    PRM_Template(PRM_TOGGLE, 1, &theUseInstancingName, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE, 1, &theShapeNodesName, PRMoneDefaults),
    PRM_Template(PRM_TOGGLE, 1, &theSaveAttributesName, PRMoneDefaults),
    PRM_Template(PRM_STRING, 1, &thePointAttributesName, &theStarDefault),
    PRM_Template(PRM_STRING, 1, &theVertexAttributesName, &theStarDefault),
    PRM_Template(PRM_STRING, 1, &thePrimitiveAttributesName, &theStarDefault),
    PRM_Template(PRM_STRING, 1, &theDetailAttributesName, &theStarDefault),
    PRM_Template(PRM_STRING, 1, &thePromoteUniformPatternName),
    PRM_Template(PRM_TOGGLE, 1, &theForcePromoteUniformName),
    PRM_Template(PRM_STRING, 1, &theUVAttribPatternName),
    PRM_Template(PRM_ORD, 1, &theFaceSetModeName, &theFaceSetModeDefault,
		    &theFaceSetModeMenu),
    PRM_Template(PRM_TOGGLE, 1, &theMotionBlurName),
    PRM_Template(PRM_INT, 1, &theSampleName, &theSampleDefault),
    PRM_Template(PRM_FLT, 2, &theShutterName, theShutterDefault),
    PRM_Template(),
};

static PRM_Name separator1Name("_sep1", "");
static PRM_Name thePackedAbcPriorityName("packed_priority", "Packed Alembic Priority");
static PRM_Name separator2Name("_sep2", "");
static PRM_Name separator3Name("_sep3", "");
static PRM_Name theVerboseName("verbose", "Verbosity");
static PRM_Name thePackedModeName("packed_mode", "Packed Mode");

static PRM_Default thePackedAbcPriorityDefault(0, "hier");
static PRM_Default thePackedModeDefault(0, "transformedshape");

static PRM_Name thePackedAbcPriorityChoices[] =
{
    PRM_Name("hier", "Hierarchy"),
    PRM_Name("xform", "Transformation"),
    PRM_Name()
};

static PRM_Name thePackedModeChoices[] =
{
    PRM_Name("transformedshape",		"Transformed Shape"),
    PRM_Name("transformandshape",		"Transform And Shape"),
    PRM_Name("transformedparent",		"Transformed Parent"),
    PRM_Name("transformedparentandshape",	"Transformed Parent And Shape"),
    PRM_Name()
};

static PRM_ChoiceList thePackedAbcPriorityMenu(PRM_CHOICELIST_SINGLE, thePackedAbcPriorityChoices);
static PRM_ChoiceList thePackedModeMenu(PRM_CHOICELIST_SINGLE, thePackedModeChoices);

static PRM_Range theVerboseRange(PRM_RANGE_RESTRICTED, 0, PRM_RANGE_UI, 3);

static PRM_Template theObsoleteParameters[] =
{
    PRM_Template(PRM_SEPARATOR, 1, &separator1Name),
    PRM_Template(PRM_ORD, 1, &thePackedAbcPriorityName,
		    &thePackedAbcPriorityDefault, &thePackedAbcPriorityMenu),
    PRM_Template(PRM_SEPARATOR, 1, &separator2Name),
    PRM_Template(PRM_SEPARATOR, 1, &separator3Name),
    PRM_Template(PRM_INT,   1, &theVerboseName, 0, 0, &theVerboseRange),
    PRM_Template(PRM_ORD, 1, &thePackedModeName, &thePackedModeDefault,
		    &thePackedModeMenu),
    PRM_Template(),
};

ROP_AlembicOut::ROP_AlembicOut(OP_Network *net, const char *name, OP_Operator *entry)
    : ROP_Node(net, name, entry)
{
}

ROP_AlembicOut::~ROP_AlembicOut()
{
}

OP_Node *
ROP_AlembicOut::myConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
    return new ROP_AlembicOut(net, name, op);
}

bool
ROP_AlembicOut::updateParmsFlags()
{
    bool issop = CAST_SOPNODE(getInput(0)) != NULL;
    bool use_path = (!issop && USE_SOP_PATH(0));
    bool sop_mode = (issop || use_path);
    bool build_from_path = (sop_mode && BUILD_FROM_PATH(0));
    bool partition_mode = (!sop_mode || !build_from_path);
    bool partition_attrib = partition_mode;
    if(partition_attrib)
    {
	UT_String str;
	PARTITION_MODE(str, 0);
	partition_attrib = (str != "no");
    }
    bool shape_nodes = SHAPE_NODES(0);
    bool save_attributes = (shape_nodes && SAVE_ATTRIBUTES(0));
    bool enable_prim_to_detail = save_attributes;
    if(enable_prim_to_detail)
    {
	UT_String p2d_pattern;
	PRIM_TO_DETAIL_PATTERN(p2d_pattern, 0);
	enable_prim_to_detail = p2d_pattern.isstring();
    }
    bool motionblur = MOTIONBLUR(0);

    bool changed = ROP_Node::updateParmsFlags();

    changed |= enableParm("use_sop_path", !issop);
    changed |= enableParm("sop_path", use_path);
    changed |= enableParm("subdgroup", sop_mode);
    changed |= enableParm("build_from_path", sop_mode);
    changed |= enableParm("path_attrib", build_from_path);
    changed |= enableParm("root", !sop_mode);
    changed |= enableParm("objects", !sop_mode);
    changed |= enableParm("collapse", !sop_mode);
    changed |= enableParm("partition_mode", partition_mode);
    changed |= enableParm("partition_attribute", partition_attrib);
    changed |= enableParm("save_attributes", shape_nodes);
    changed |= enableParm("vertexAttributes", save_attributes);
    changed |= enableParm("pointAttributes", save_attributes);
    changed |= enableParm("primitiveAttributes", save_attributes);
    changed |= enableParm("detailAttributes", save_attributes);
    changed |= enableParm("prim_to_detail_pattern", save_attributes);
    changed |= enableParm("force_prim_to_detail", enable_prim_to_detail);
    changed |= enableParm("uvAttributes", save_attributes);
    changed |= enableParm("facesets", shape_nodes);
    changed |= enableParm("shutter", motionblur);
    changed |= enableParm("samples", motionblur);
    return changed;
}

void
ROP_AlembicOut::resolveObsoleteParms(PRM_ParmList *obsolete_parms)
{
    if(!obsolete_parms)
	return;

    int mode = obsolete_parms->evalInt(thePackedModeName.getToken(), 0, 0.0f);
    switch(mode)
    {
	case 1:
	    setString("transform", CH_STRING_LITERAL, thePackedTransformName.getToken(), 0, 0.0f);
	    break;

	case 2:
	    setString("transformparent", CH_STRING_LITERAL, thePackedTransformName.getToken(), 0, 0.0f);
	    setInt("shape_nodes", 0, 0.0f, false);
	    break;

	case 3:
	    setString("transformparent", CH_STRING_LITERAL, thePackedTransformName.getToken(), 0, 0.0f);
	    break;
    }

    ROP_Node::resolveObsoleteParms(obsolete_parms);
}

void
ROP_AlembicOut::buildRenderDependencies(const ROP_RenderDepParms &p)
{
    if(RENDER_FULL_RANGE())
    {
	ROP_RenderDepParms parms = p;
	parms.setFullAscendingFrameRange(*this);
	ROP_Node::buildRenderDependencies(parms);
    }
    else
	ROP_Node::buildRenderDependencies(p);
}

SOP_Node *
ROP_AlembicOut::getSopNode(fpreal time) const
{
    SOP_Node *sop = CAST_SOPNODE(getInput(0));
    if(!sop && USE_SOP_PATH(time))
    {
	UT_String sop_path;
	SOP_PATH(sop_path, time);
	sop_path.trimBoundingSpace();
	if(sop_path.isstring())
	    sop = CAST_SOPNODE(findNode(sop_path));
    }
    return sop;
}

int
ROP_AlembicOut::startRender(int nframes, fpreal tstart, fpreal tend)
{
    if (!executePreRenderScript(tstart))
	return 0;

    if (INITSIM(tstart))
    {
	initSimulationOPs();
	OPgetDirector()->bumpSkipPlaybarBasedSimulationReset(-1);
    }

    UT_ASSERT(!myArchive);
    myErrors.reset(new rop_OError());
    myRoot.reset(new ROP_AbcNodeRoot());

    myNFrames = nframes;
    myEndTime = tend;
    myFullBounds = FULL_BOUNDS(tstart);

    return 1;
}

ROP_RENDER_CODE
ROP_AlembicOut::renderFrame(fpreal time, UT_Interrupt *boss)
{
    UT_String filename;
    FILENAME(filename, time);
    filename.trimBoundingSpace();

    if(!myArchive || (myArchive->getFileName() != filename))
    {
	// close the previous archive
	const ROP_AbcArchivePtr dummy;
	myRoot->setArchive(dummy);
	myArchive.reset();
    }

    if (!executePreFrameScript(time))
	return ROP_ABORT_RENDER;

    if(!myArchive)
    {
	UT_String format;
	FORMAT(format, time);

	myArchive = UTmakeShared<ROP_AbcArchive>(filename, format != "hdf5", *myErrors.get());
	if(!myArchive || !myArchive->isValid())
	{
	    myArchive.reset();
	    return ROP_ABORT_RENDER;
	}

	int mb_samples = 1;
	fpreal shutter_open = 0;
	fpreal shutter_close = 0;
	if(MOTIONBLUR(time))
	{
	    mb_samples = SYSmax(SAMPLES(time), 1);
	    shutter_open = SHUTTEROPEN(time);
	    shutter_close = SHUTTERCLOSE(time);
	}

	auto &options = myArchive->getOOptions();
	options.setFullBounds(myFullBounds);
	myArchive->setTimeSampling(myNFrames, time, myEndTime, mb_samples,
				   shutter_open, shutter_close);
	if(SAVE_ATTRIBUTES(time))
	{
	    UT_String pattern;
	    evalString(pattern, thePointAttributesName.getToken(), 0, time);
	    options.setAttributePattern(GA_ATTRIB_POINT, pattern);
	    evalString(pattern, theVertexAttributesName.getToken(), 0, time);
	    options.setAttributePattern(GA_ATTRIB_VERTEX, pattern);
	    evalString(pattern, thePrimitiveAttributesName.getToken(), 0, time);
	    options.setAttributePattern(GA_ATTRIB_PRIMITIVE, pattern);
	    evalString(pattern, theDetailAttributesName.getToken(), 0, time);
	    options.setAttributePattern(GA_ATTRIB_DETAIL, pattern);

	    UT_String p2d_pattern;
	    PRIM_TO_DETAIL_PATTERN(p2d_pattern, time);
	    options.setPrimToDetailPattern(p2d_pattern);
	    options.setForcePrimToDetail(FORCE_PRIM_TO_DETAIL(time));

	    UT_String uv_pattern;
	    UV_ATTRIBUTE(uv_pattern, time);
	    options.setUVAttribPattern(uv_pattern);
	}

	myRoot->setArchive(myArchive);
    }

    OBJ_Geometry *geo = nullptr;
    SOP_Node *sop = getSopNode(time);
    myFromSOP = (sop != nullptr);
    if(sop)
    {
	OBJ_Node *obj = CAST_OBJNODE(sop->getCreator());
	if(obj)
	    geo = obj->castToOBJGeometry();
    }

    UT_String packed;
    PACKED_TRANSFORM(packed, time);
    ROP_AlembicPackedTransform packedtransform;
    if(packed == "deform")
	packedtransform = ROP_ALEMBIC_PACKEDTRANSFORM_DEFORM_GEOMETRY;
    else if(packed == "transformparent")
	packedtransform = ROP_ALEMBIC_PACKEDTRANSFORM_MERGE_WITH_PARENT_TRANSFORM;
    else // if(packed == "transform")
	packedtransform = ROP_ALEMBIC_PACKEDTRANSFORM_TRANSFORM_GEOMETRY;

    UT_String faceset;
    FACESET_MODE(faceset, time);
    exint facesetmode;
    if(faceset == "all")
	facesetmode = GT_RefineParms::FACESET_ALL_GROUPS;
    else if(faceset == "nonempty")
	facesetmode = GT_RefineParms::FACESET_NON_EMPTY;
    else // if(faceset == "no")
	facesetmode = GT_RefineParms::FACESET_NONE;

    bool shape_nodes = SHAPE_NODES(time);
    bool use_instancing = USE_INSTANCING(time);
    bool displaysop = DISPLAYSOP(time);
    bool save_hidden = SAVE_HIDDEN(time);

    exint n = myArchive->getSamplesPerFrame();
    for(exint i = 0; i < n; ++i)
    {
	myArchive->setCookTime(time, i);

	// update tree
	if(sop)
	{
	    if(!updateFromSop(geo, sop, packedtransform, facesetmode, use_instancing, shape_nodes, displaysop, save_hidden))
		return ROP_ABORT_RENDER;
	}
	else if(!updateFromHierarchy(packedtransform, facesetmode, use_instancing, shape_nodes, displaysop, save_hidden))
	    return ROP_ABORT_RENDER;
    }

    if (!executePostFrameScript(time))
	return ROP_ABORT_RENDER;

    --myNFrames;
    return ROP_CONTINUE_RENDER;
}

ROP_RENDER_CODE
ROP_AlembicOut::endRender()
{
    // report errors
    rop_OError *err = myErrors.get();
    for(exint i = 0; i < err->myErrors.entries(); ++i)
	addError(ROP_MESSAGE, err->myErrors(i));

    for(exint i = 0; i < err->myWarnings.entries(); ++i)
	addWarning(ROP_MESSAGE, err->myWarnings(i));

    myArchive.reset();
    myRoot.reset(nullptr);
    myErrors.reset(nullptr);

    myObjAssignments.clear();
    myCamAssignments.clear();
    myGeoAssignments.clear();
    myGeos.clear();
    mySopAssignments.reset(nullptr);

    if (!executePostRenderScript(myEndTime))
	return ROP_ABORT_RENDER;

    if (INITSIM(myEndTime))
	OPgetDirector()->bumpSkipPlaybarBasedSimulationReset(-1);

    return ROP_CONTINUE_RENDER;
}

static bool
rop_abcfilter(OBJ_Node *obj, bool save_hidden, fpreal time)
{
    // ignore hidden objects
    if(!save_hidden &&
       !obj->isDisplayTimeDependent() && !obj->getObjectDisplay(time))
    {
	return false;
    }

    if(obj->getObjectType() == OBJ_CAMERA)
	return obj->getName() != "ipr_camera";

    return obj->getObjectType() == OBJ_SUBNET || obj->castToOBJGeometry();
}

static bool
rop_isStaticIdentity(OBJ_Node *obj, fpreal time)
{
    // check if it is static
    if(obj->isDisplayTimeDependent())
	return false;

    OP_Context context(time);
    UT_Matrix4D m;
    obj->getLocalTransform(context, m);
    return m.isIdentity();
}

static bool
ropIsToggleEnabled(OP_Node *node, const char *name, fpreal time)
{
    int value;
    if(node->evalParameterOrProperty(name, 0, time, value))
	return value != 0;
    return false;
}

static bool
ropGetUserProperties(
    UT_WorkBuffer &vals,
    UT_WorkBuffer &meta,
    const GABC_IObject &obj,
    fpreal time)
{
    vals.clear();
    meta.clear();
    UT_UniquePtr<UT_JSONWriter> vals_writer(UT_JSONWriter::allocWriter(vals));
    UT_UniquePtr<UT_JSONWriter> meta_writer(UT_JSONWriter::allocWriter(meta));
    return GABC_Util::importUserPropertyDictionary(vals_writer.get(), meta_writer.get(), obj, time);
}

typedef const char *rop_PartitionFunc(char *);

static const char *
ropPartitionFull(char *s)
{
    const char *start = s;
    while(*s)
    {
	if(*s == '/')
	    *s = '_';
	++s;
    }
    return start;
}

static const char *
ropPartitionShape(char *s)
{
    const char *start = s;
    while(*s)
    {
	if(*s == '/')
	    start = &s[1];
	++s;
    }
    return start;
}

static const char *
ropPartitionXform(char *s)
{
    const char *start = s;
    char *end = nullptr;
    while(*s)
    {
	if(*s == '/')
	{
	    if(end)
		start = &end[1];
	    end = s;
	}
	++s;
    }
    if(end)
	*end = 0;
    return start;
}

static const char *
ropPartitionXformShape(char *s)
{
    const char *start = s;
    const char *end = nullptr;
    while(*s)
    {
	if(*s == '/')
	{
	    *s = '_';
	    if(end)
		start = &end[1];
	    end = s;
	}
	++s;
    }
    return start;
}

const GA_PrimitiveGroup *
ROP_AlembicOut::getSubdGroup(
    bool &subd_all, OBJ_Geometry *geo, const GU_Detail *gdp, fpreal time)
{
    bool subd = (geo && (ropIsToggleEnabled(geo, "vm_rendersubd", time)
			|| ropIsToggleEnabled(geo, "ri_rendersubd", time)));

    UT_String subdgroup;
    if(subd)
	geo->evalParameterOrProperty("vm_subdgroup", 0, time, subdgroup);
    else if(myFromSOP)
	SUBDGROUP(subdgroup, time);

    const GA_PrimitiveGroup *grp = nullptr;
    subd_all = false;
    if(subdgroup.isstring())
	grp = gdp->findPrimitiveGroup(subdgroup);
    else if(subd)
	subd_all = true;
    myArchive->getOOptions().setSubdGroup(subdgroup);
    return grp;
}

static GU_ConstDetailHandle
ropGetCookedGeoHandle(SOP_Node *sop, OP_Context &context, bool displaysop)
{
    if(!sop)
	return GU_ConstDetailHandle();

    int val;
    OBJ_Node *creator = CAST_OBJNODE(sop->getCreator());
    if(creator)
    {
	val = creator->isCookingRender();
	creator->setCookingRender(displaysop ? 0 : 1);
    }
    GU_ConstDetailHandle gdh(sop->getCookedGeoHandle(context));
    if(creator)
	creator->setCookingRender(val);
    return gdh;
}

void
ROP_AlembicOut::refineSop(
    ROP_AbcHierarchy &assignments,
    ROP_AlembicPackedTransform packedtransform,
    exint facesetmode,
    bool use_instancing,
    bool shape_nodes,
    bool displaysop,
    bool save_hidden,
    OBJ_Geometry *geo,
    SOP_Node *sop,
    fpreal time)
{
    OP_Context context(time);
    GU_ConstDetailHandle gdh(ropGetCookedGeoHandle(sop, context, displaysop));
    GU_DetailHandleAutoReadLock rlock(gdh);
    const GU_Detail *gdp = rlock.getGdp();
    if(!gdp)
    {
	reportCookErrors(sop, time);
	return;
    }

    GA_ROHandleS up_vals(gdp, GA_ATTRIB_PRIMITIVE,
			 GABC_Util::theUserPropsValsAttrib);
    GA_ROHandleS up_meta(gdp, GA_ATTRIB_PRIMITIVE,
			 GABC_Util::theUserPropsMetaAttrib);
    bool update_user_props = (up_vals.isValid() && up_meta.isValid());

    // identify subd
    bool subd_all;
    const GA_PrimitiveGroup *grp = getSubdGroup(subd_all, geo, gdp, time);

    UT_String partition_mode;
    PARTITION_MODE(partition_mode, time);

    rop_PartitionFunc *func = nullptr;
    if(partition_mode == "full")
	func = ropPartitionFull;
    else if(partition_mode == "shape")
	func = ropPartitionShape;
    else if(partition_mode == "xform")
	func = ropPartitionXform;
    else if(partition_mode == "xformshape")
	func = ropPartitionXformShape;

    GA_ROHandleS partition_attrib;
    if(func)
    {
	UT_String attrib_name;
	PARTITION_ATTRIBUTE(attrib_name, time);
	partition_attrib.bind(gdp, GA_ATTRIB_PRIMITIVE, attrib_name);
    }

    UT_SortedMap<std::string, GA_OffsetList> partitions[2];

    std::string name = sop->getName().c_str();
    UT_WorkBuffer buf;
    UT_WorkBuffer part_name;
    for(GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
    {
	GA_Offset offset = *it;

	std::string p;
	if(partition_attrib.isValid())
	{
	    // map primitive to named partition
	    part_name.clear();
	    if(func && partition_attrib.isValid())
	    {
		buf.strcpy(partition_attrib.get(offset));

		UT_WorkBuffer::AutoLock lock(buf);
		part_name.append(func(lock.string()));
	    }

	    if(!part_name.length())
		part_name.append(name);

	    p = part_name.buffer();
	}
	else
	    p = name;

	int idx = (subd_all || (grp && grp->contains(offset))) ? 1 : 0;
	partitions[idx][p].append(offset);
    }

    if(!func)
    {
	// ensured an entry for name exists so we can add the unshared points
	// in the following loop
	partitions[0][name];
    }

    ROP_AbcHierarchySample root(nullptr, "");
    UT_Map<std::string, UT_Map<int, UT_Array<GT_PrimitiveHandle> > > instance_map;

    ROP_AbcRefiner refiner(instance_map, myArchive, packedtransform,
			   facesetmode, use_instancing, shape_nodes,
			   save_hidden);

    for(int idx = 0; idx < 2; ++idx)
    {
	for(auto &it : partitions[idx])
	{
	    auto &cname = it.first;
	    auto &offsets = it.second;

	    GT_PrimitiveHandle prim;
	    GA_Range range(gdp->getPrimitiveMap(), offsets);
	    if(it.second.size())
		prim = GT_GEODetail::makeDetail(gdh, &range);
	    if(!func && !idx)
	    {
		GA_OffsetList pts;
		if(gdp->findUnusedPoints(&pts))
		{
		    GT_PrimCollect *collect = new GT_PrimCollect();
		    if(prim)
			collect->appendPrimitive(prim);

		    GA_Range ptrange(gdp->getPointMap(), pts);
		    collect->appendPrimitive(GT_GEODetail::makePointMesh(gdh, &ptrange));
		    prim = collect;
		}
	    }
	    if(prim)
	    {
		bool subd = (idx == 1);

		std::string userprops;
		std::string userpropsmeta;
		if(update_user_props)
		{
		    userprops = up_vals.get(offsets(0));
		    userpropsmeta = up_meta.get(offsets(0));
		}

		refiner.addPartition(root, cname, subd, prim, userprops, userpropsmeta);
	    }
	}
    }

    assignments.update(myArchive, root, instance_map);
}

static bool
ropIsShape(GABC_NodeType type)
{
    switch(type)
    {
	case GABC_NodeType::GABC_POLYMESH:
	case GABC_NodeType::GABC_SUBD:
	case GABC_NodeType::GABC_CURVES:
	case GABC_NodeType::GABC_POINTS:
	case GABC_NodeType::GABC_NUPATCH:
	    return true;

	default:
	    return false;
    }
}

bool
ROP_AlembicOut::updateFromSop2(
    OBJ_Geometry *geo,
    SOP_Node *sop,
    ROP_AlembicPackedTransform packedtransform,
    exint facesetmode,
    bool use_instancing,
    bool shape_nodes,
    bool displaysop,
    bool save_hidden)
{
    fpreal time = myArchive->getCookTime();
    OP_Context context(time);
    OP_Node *creator = sop->getCreator();

    GU_ConstDetailHandle gdh(ropGetCookedGeoHandle(sop, context, displaysop));
    GU_DetailHandleAutoReadLock rlock(gdh);
    const GU_Detail *gdp = rlock.getGdp();
    if(!gdp)
    {
	reportCookErrors(sop, time);
	return false;
    }

    ROP_AbcHierarchySample root(nullptr, "");
    ROP_AbcHierarchySample *assignments = &root;

    GA_ROHandleS path_handle;
    bool build_from_path = BUILD_FROM_PATH(time);
    if(build_from_path)
    {
	UT_String path_attrib;
	PATH_ATTRIBUTE(path_attrib, time);
	myArchive->getOOptions().setPathAttribute(path_attrib);
	path_handle.bind(gdp, GA_ATTRIB_PRIMITIVE, path_attrib);
    }
    else if(creator)
    {
	OBJ_Node *obj = CAST_OBJNODE(creator);
	if(obj)
	{
	    UT_Matrix4D m(1);
	    if(obj->isSubNetwork(false))
		static_cast<OBJ_SubNet *>(obj)->getSubnetTransform(context, m);
	    else
		obj->getLocalTransform(context, m); 
	    auto child = root.getChildXform(obj->getName().toStdString());
	    child->setXform(m);
	    assignments = child;
	}
    }

    // identify subd
    bool subd_all;
    const GA_PrimitiveGroup *grp = getSubdGroup(subd_all, geo, gdp, time);

    auto &&alembic_def = GUgetFactory().lookupDefinition("AlembicRef"_sh);
    UT_SortedMap<std::string, const GU_PrimPacked *> abc_prims;
    UT_Map<std::tuple<std::string, bool>, GA_OffsetList> offsets;

    // partition geometry
    const UT_String &name = sop->getName();
    UT_WorkBuffer buf;
    for(GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
    {
	GA_Offset offset = *it;

	// normalise path
	buf.clear();
	const char *s = path_handle.isValid() ? path_handle.get(offset) : nullptr;
	if(s)
	{
	    while(*s)
	    {
		// skip initial path separators
		while(*s == '/')
		    ++s;

		// parse next token
		const char *start = s;
		while(*s && *s != '/')
		    ++s;
		buf.append('/');
		buf.append(start, s - start);

		// skip trailing path separators
		while(*s && *s == '/')
		    ++s;
	    }
	}
	// replace invalid names
	if(buf.length() < 2)
	{
	    buf.clear();
	    buf.append('/');
	    buf.append(name);
	}

	std::string p = buf.buffer();

	const GA_Primitive *prim = gdp->getPrimitive(offset);
	if(GU_PrimPacked::isPackedPrimitive(*prim))
	{
	    const GU_PrimPacked *packed = static_cast<const GU_PrimPacked *>(prim);
	    if(alembic_def && packed->getTypeId() == alembic_def->getId())
	    {
		abc_prims.emplace(p, packed);

		// only refine shape nodes
		if(!ropIsShape(static_cast<const GABC_PackedImpl *>(packed->implementation())->nodeType()))
		    continue;
	    }
	}

	bool subd = (subd_all || (grp && grp->contains(offset)));
	offsets[std::make_tuple(p, subd)].append(offset);
    }

    // explicit and implicit xforms
    UT_Map<ROP_AbcHierarchySample *, GABC_IObject> implicit_nodes;
    UT_Map<ROP_AbcHierarchySample *, UT_Matrix4D> xforms;
    UT_Array<ROP_AbcHierarchySample *> ancestors;
    UT_WorkBuffer up_vals;
    UT_WorkBuffer up_meta;
    for(auto &it : abc_prims)
    {
	// we only use the first transform primitive
	const GU_PrimPacked *packed = it.second;
	const GABC_PackedImpl *impl = static_cast<const GABC_PackedImpl *>(packed->implementation());

	const char *s = it.first.c_str();
	bool do_implicit = (impl->objectPath() == s);

	// walk path
	ancestors.clear();
	ROP_AbcHierarchySample *parent = assignments;
	while(*s)
	{
	    // skip '/'
	    UT_ASSERT(*s == '/');
	    while(*s == '/')
		++s;

	    const char *start = s;
	    while(*s && *s != '/')
		++s;

	    buf.strncpy(start, s - start);
	    if(*s)
	    {
		ROP_AbcHierarchySample *child =
			parent->getChildXform(buf.toStdString());
		ancestors.append(child);
		parent = child;
	    }
	}

	if(do_implicit)
	{
	    GABC_IObject obj = impl->object().getParent();
	    exint n = ancestors.entries();
	    for(exint i = n - 1; i >= 0; --i, obj = obj.getParent())
	    {
		auto ancestor = ancestors(i);
		auto it2 = implicit_nodes.find(ancestor);
		if(it2 != implicit_nodes.end())
		    break;

		implicit_nodes.emplace(ancestor, obj);
	    }
	}

	if(impl->nodeType() == GABC_NodeType::GABC_XFORM)
	{
	    UT_Matrix4D m;
	    packed->getFullTransform4(m);
	    ROP_AbcHierarchySample *child =
		    parent->getChildXform(buf.toStdString());
	    xforms.emplace(child, m);

	    GABC_IObject obj = impl->object();
	    if(ropGetUserProperties(up_vals, up_meta, obj, time))
		child->setUserProperties(up_vals.buffer(), up_meta.buffer());
	}
    }

    // set transforms
    UT_Array<ROP_AbcHierarchySample *> work;
    UT_Array<UT_Matrix4D> work_xform;
    work.append(assignments);
    work_xform.append(UT_Matrix4D(1));
    while(work.entries())
    {
	// pop
	ROP_AbcHierarchySample *node = work.last();
	work.removeLast();
	UT_Matrix4D xform = work_xform.last();
	work_xform.removeLast();

	auto it2 = xforms.find(node);
	if(it2 != xforms.end())
	{
	    const UT_Matrix4D &m = it2->second;
	    // set transform
	    xform.invert();
	    node->setXform(m * xform);
	    xform = m;
	}
	else
	{
	    UT_Matrix4D m;
	    auto it3 = implicit_nodes.find(node);
	    if(it3 != implicit_nodes.end())
	    {
		auto &obj = it3->second;

		GEO_AnimationType atype;
		obj.getWorldTransform(m, time, atype);
		// set transform
		xform.invert();
		node->setXform(m * xform);
		xform = m;

		// set user properties
		if(ropGetUserProperties(up_vals, up_meta, obj, time))
		    node->setUserProperties(up_vals.buffer(), up_meta.buffer());
	    }
	    else
		m = xform;
	    xforms.emplace(node, m);
	}

	auto &children = node->getChildren();
	for(auto &it3 : children)
	{
	    work.append(&it3.second);
	    work_xform.append(xform);
	}
    }

    GA_ROHandleS up_vals_attr(gdp, GA_ATTRIB_PRIMITIVE, GABC_Util::theUserPropsValsAttrib);
    GA_ROHandleS up_meta_attr(gdp, GA_ATTRIB_PRIMITIVE, GABC_Util::theUserPropsMetaAttrib);
    bool update_user_props = (up_vals_attr.isValid() && up_meta_attr.isValid());
    // build GT prims for each partition
    UT_SortedMap<std::tuple<std::string, bool>, std::tuple<GT_PrimitiveHandle, std::string, std::string> > gt_part;
    for(auto &it : offsets)
    {
	std::string userprops;
	std::string userpropsmeta;

	if(update_user_props)
	{
	    // grab user from first primitive
	    userprops = up_vals_attr.get(it.second(0));
	    userpropsmeta = up_meta_attr.get(it.second(0));
	}
	else if(it.second.entries() == 1)
	{
	    const GA_Primitive *prim = gdp->getPrimitive(it.second(0));
	    if(GU_PrimPacked::isPackedPrimitive(*prim))
	    {
		const GU_PrimPacked *packed = static_cast<const GU_PrimPacked *>(prim);
		if(alembic_def && packed->getTypeId() == alembic_def->getId())
		{
		    // grab user from the packed alembic primitive
		    const GABC_PackedImpl *impl = static_cast<const GABC_PackedImpl *>(packed->implementation());
		    GABC_IObject obj = impl->object();
		    if(ropGetUserProperties(up_vals, up_meta, obj, time))
		    {
			userprops = up_vals.buffer();
			userpropsmeta = up_meta.buffer();
		    }
		}
	    }
	}

	GA_Range r(gdp->getPrimitiveMap(), it.second);
	gt_part.emplace(it.first, std::make_tuple(GT_GEODetail::makeDetail(gdh, &r), userprops, userpropsmeta));
    }

    // unused points
    if(!build_from_path)
    {
	GA_OffsetList pts;
	if(gdp->findUnusedPoints(&pts))
	{
	    GA_Range r(gdp->getPointMap(), pts);
	    GT_PrimitiveHandle prim = GT_GEODetail::makePointMesh(gdh, &r);

	    buf.clear();
	    buf.append('/');
	    buf.append(name);
	    std::string p = buf.buffer();
	    auto key = std::make_tuple(p, false);
	    auto it = gt_part.find(key);
	    if(it == gt_part.end())
		gt_part.emplace(key, std::make_tuple(prim, std::string(), std::string()));
	    else
	    {
		GT_PrimCollect *collect = new GT_PrimCollect();
		collect->appendPrimitive(std::get<0>(it->second));
		collect->appendPrimitive(prim);
		std::get<0>(it->second) = collect;
	    }
	}
    }

    UT_Map<std::string, UT_Map<int, UT_Array<GT_PrimitiveHandle> > > instance_map;
    ROP_AbcRefiner refiner(instance_map, myArchive, packedtransform,
			   facesetmode, use_instancing, shape_nodes,
			   save_hidden);
    // build refined hierarchy
    for(auto &it : gt_part)
    {
	auto &key = it.first;
	auto &name = std::get<0>(key);
	bool subd = std::get<1>(key);

	auto prim = std::get<0>(it.second);
	auto &userprops = std::get<1>(it.second);
	auto &userpropsmeta = std::get<2>(it.second);

	// walk path
	UT_Matrix4D m(1);
	ROP_AbcHierarchySample *parent = assignments;
	std::string cname;
	const char *s = name.c_str();
	while(*s)
	{
	    // skip '/'
	    UT_ASSERT(*s == '/');
	    while(*s == '/')
		++s;

	    const char *start = s;
	    while(*s && *s != '/')
		++s;

	    buf.strncpy(start, s - start);
	    cname = buf.buffer();
	    if(*s)
	    {
		ROP_AbcHierarchySample *child = parent->getChildXform(cname);
		auto it2 = xforms.find(child);
		if(it2 != xforms.end())
		    m = it2->second;
		else
		    xforms.emplace(child, m);

		parent = child;
	    }
	}

	if(!m.isIdentity())
	{
	    m.invert();
	    prim = prim->copyTransformed(new GT_Transform(&m, 1));
	}

	refiner.addPartition(*parent, cname, subd, prim, userprops, userpropsmeta);
    }

    mySopAssignments->update(myArchive, root, instance_map);
    return true;
}

// recursively call preUpdate on hierarchy
static void
ropPreUpdate(ROP_AbcNode *node, bool locked)
{
    UT_Array<ROP_AbcNode *> work;
    work.append(node);
    while(work.entries())
    {
	node = work.last();
	work.removeLast();

	node->preUpdate(locked);
	for(auto &it : node->getChildren())
	    work.append(it.second);
    }
}

// recursively call postUpdate on hierarchy
static void
ropPostUpdate(ROP_AbcNode *node, bool locked)
{
    UT_Array<ROP_AbcNode *> work;
    work.append(node);
    while(work.entries())
    {
	node = work.last();
	work.removeLast();

	node->postUpdate(locked);
	for(auto &it : node->getChildren())
	    work.append(it.second);
    }
}

bool
ROP_AlembicOut::updateFromSop(
    OBJ_Geometry *geo,
    SOP_Node *sop,
    ROP_AlembicPackedTransform packedtransform,
    exint facesetmode,
    bool use_instancing,
    bool shape_nodes,
    bool displaysop,
    bool save_hidden)
{
    if(!mySopAssignments)
	mySopAssignments.reset(new ROP_AbcHierarchy(myRoot.get()));

    OP_Node *creator = sop->getCreator();
    int locked = 0;
    if(creator)
    {
	creator->evalParameterOrProperty(
			GABC_Util::theLockGeometryParameter, 0, 0, locked);
    }

    // pre-update
    bool retval = true;
    ropPreUpdate(mySopAssignments->getRoot(), locked != 0);
    if(!mySopAssignments->getLocked() || !locked)
    {
	retval = updateFromSop2(geo, sop, packedtransform, facesetmode,
				use_instancing, shape_nodes, displaysop,
				save_hidden);
	if(retval)
	    myRoot->update();
    } 

    // post-update
    ropPostUpdate(mySopAssignments->getRoot(), locked != 0);
    return retval;
}

bool
ROP_AlembicOut::updateFromHierarchy(
    ROP_AlembicPackedTransform packedtransform,
    exint facesetmode,
    bool use_instancing,
    bool shape_nodes,
    bool displaysop,
    bool save_hidden)
{
    fpreal time = myArchive->getCookTime();

    UT_String root;
    ROOT(root, time);
    root.trimBoundingSpace();

    if(!root.isstring())
    {
	addError(ROP_MESSAGE, "Require root for all objects");
	return false;
    }

    OP_Node *rootnode = findNode(root);
    if(!rootnode)
    {
	addError(ROP_MESSAGE, "Can't find root node");
	return false;
    }

    UT_String collapse;
    COLLAPSE(collapse, time);
    bool collapse_geo = (collapse == "geo");
    bool collapse_on = (collapse == "on");

    UT_String objects;
    OBJECTS(objects, time);

    UT_Array<OBJ_Node *> work;
    OP_Bundle *bundle =
		getParmBundle("objects", 0, objects,
			    OPgetDirector()->getManager("obj"), "!!OBJ!!");
    if(bundle)
    {
	UT_Set<OBJ_Node *> visited;
	for(exint i = 0; i < bundle->entries(); ++i)
	{
	    OBJ_Node *obj = bundle->getNode(i)->castToOBJNode();
	    if(obj && visited.find(obj) == visited.end())
	    {
		visited.insert(obj);
		work.append(obj);
	    }
	}
    }
    UT_Array<OBJ_Node *> ancestors;
    for(exint w = 0; w < work.entries(); ++w)
    {
	OBJ_Node *obj = work(w);
	if(!obj)
	    continue;

	UT_WorkBuffer buf;
	obj->getFullPath(buf);
	OBJ_Camera *cam = obj->castToOBJCamera();
	OBJ_Geometry *geo = nullptr;
	if(obj->getObjectType() == OBJ_GEOMETRY)
	    geo = obj->castToOBJGeometry();

	ancestors.clear();
	for(;;)
	{
	    if(!obj || rootnode == obj || myObjAssignments.find(obj) != myObjAssignments.end())
	    {
		// reached root or an assigned node
		ROP_AbcNode *parent = nullptr;
		if(!obj || rootnode == obj)
		    parent = myRoot.get();
		else
		    parent = myObjAssignments.find(obj)->second;

		exint n = ancestors.entries();
		for(exint i = n - 1; i >= 0; --i)
		{
		    obj = ancestors(i);

		    std::string name(obj->getName());

		    // handle name collisions
		    parent->makeCollisionFreeName(name);

		    ROP_AbcNodeXform *child = new ROP_AbcNodeXform(name);
		    child->setArchive(myArchive);
		    parent->addChild(child);

		    myObjAssignments.emplace(obj, child);
		    parent = child;
		}

		if(geo && myGeoAssignments.find(geo) == myGeoAssignments.end())
		{
		    myGeoAssignments.emplace(geo, ROP_AbcHierarchy(parent));
		    myGeos.append(geo);
		}
		if(cam && myCamAssignments.find(cam) == myCamAssignments.end())
		{
		    std::string name("cameraProperties");

		    // handle name collisions
		    parent->makeCollisionFreeName(name);

		    ROP_AbcNodeCamera *child = new ROP_AbcNodeCamera(name, cam->RESX(time), cam->RESY(time));
		    child->setArchive(myArchive);
		    parent->addChild(child);
		    myCamAssignments.emplace(cam, child);
		}
		break;
	    }

	    if(!rop_abcfilter(obj, save_hidden, time))
		break;

	    // skip collapsed objects
	    int abc_collapse = 0;
	    obj->evalParameterOrProperty("abc_collapse", 0, time, abc_collapse);
	    if(!abc_collapse && ((!collapse_on && (!collapse_geo || !obj->castToOBJGeometry())) || !rop_isStaticIdentity(obj, time)))
		ancestors.append(obj);

	    // continue towards root
	    OP_Node *parent = obj->getInput(0);
	    if(!parent || parent->getParent() != obj->getParent())
	    {
		parent = obj->getParent();
		if(!parent)
		    break;
	    }

	    obj = parent->castToOBJNode();
	    if(!obj && parent != rootnode)
		break;
	}
    }

    // add data sources for the geometry
    UT_Set<OBJ_Geometry *> locked_geos;
    for(exint i = 0; i < myGeos.entries(); ++i)
    {
	OBJ_Geometry *geo = myGeos(i);
	// get cooked geometry
	SOP_Node *sop = displaysop ? geo->getDisplaySopPtr() : geo->getRenderSopPtr();
	if(sop)
	{
	    auto it = myGeoAssignments.find(geo);
	    if(it != myGeoAssignments.end())
	    {
		int locked = 0;
		geo->evalParameterOrProperty(
			GABC_Util::theLockGeometryParameter, 0, 0, locked);

		if(locked)
		    locked_geos.insert(geo);

		// do pre-update
		ropPreUpdate(it->second.getRoot(), locked != 0);

		if(!it->second.getLocked() || !locked)
		{
		    refineSop(it->second, packedtransform, facesetmode,
			      use_instancing, shape_nodes, displaysop,
			      save_hidden, geo, sop, time);
		}
		it->second.setLocked(locked != 0);
	    }
	    else
	    {
		// all entries in myGeos should be in myGeoAssignments
		UT_ASSERT(0);
	    }
	}
    }

    const CH_Manager *chman = OPgetDirector()->getChannelManager();
    for(auto &it : myCamAssignments)
    {
	auto cam = it.first;
	it.second->preUpdate(false);

	fpreal aspect = cam->ASPECT(time);
	// Alembic stores value in cm. (not mm.)
	fpreal aperture = 0.1 * cam->APERTURE(time) / aspect;

	// FIXME: consider setting the resolution here too
	it.second->setData(cam->FOCAL(time),
			   cam->FSTOP(time),
			   cam->FOCUS(time),
			   chman->getTimeDelta(cam->SHUTTER(time)),
			   cam->getNEAR(time),
			   cam->getFAR(time),
			   cam->WINSIZEX(time),
			   cam->WINSIZEY(time),
			   aperture,
			   aperture,
			   cam->WINX(time) * aperture,
			   cam->WINY(time) * aperture,
			   aspect);
    }

    OP_Context context(time);
    for(auto &it : myObjAssignments)
    {
	auto obj = it.first;
	it.second->preUpdate(false);

	UT_Matrix4D m;
	if(obj->isSubNetwork(false))
	    static_cast<OBJ_SubNet *>(obj)->getSubnetTransform(context, m);
	else
	    obj->getLocalTransform(context, m); 

	UT_String up_vals;
	UT_String up_meta;
	bool has_vals = obj->hasParm("userProps");
	bool has_meta = obj->hasParm("userPropsMeta");
	if(has_vals && has_meta)
	{
	    obj->evalString(up_vals, "userProps", 0, time);
	    obj->evalString(up_meta, "userPropsMeta", 0, time);
	    it.second->setUserProperties(up_vals.toStdString(),
					 up_meta.toStdString());
	}

	it.second->setData(m);
	it.second->setVisibility(obj->getObjectDisplay(time));
    }

    myRoot->update();

    // do post-update
    for(auto &it : myObjAssignments)
	it.second->postUpdate(false);
    for(auto &it : myCamAssignments)
	it.second->postUpdate(false);
    for(exint i = 0; i < myGeos.entries(); ++i)
    {
	OBJ_Geometry *geo = myGeos(i);
	auto it = myGeoAssignments.find(geo);
	if(it != myGeoAssignments.end())
	    ropPostUpdate(it->second.getRoot(), locked_geos.contains(geo));
    }

    return true;
}

void
ROP_AlembicOut::reportCookErrors(OP_Node *node, fpreal time)
{
    UT_WorkBuffer buf;
    node->getFullPath(buf);
    myArchive->getOError().error("Error cooking %s at time %g.",
				 buf.buffer(), time);
    UT_String errors;
    node->getErrorMessages(errors);
    addError(ROP_COOK_ERROR, (const char *)errors);
}

void
newDriverOperator(OP_OperatorTable *table)
{
    OP_TemplatePair pair(theParameters);
    OP_TemplatePair templatepair(ROP_Node::getROPbaseTemplate(), &pair);
    OP_VariablePair vp(ROP_Node::myVariableList);

    OP_Operator	*alembic_op = new OP_Operator(
        CUSTOM_ALEMBIC_TOKEN_PREFIX "alembic",		// Internal name
        CUSTOM_ALEMBIC_LABEL_PREFIX "Alembic",		// GUI name
	ROP_AlembicOut::myConstructor,
	&templatepair, 0, 9999, &vp,
	OP_FLAG_UNORDERED | OP_FLAG_GENERATOR);
    alembic_op->setObsoleteTemplates(theObsoleteParameters);
    alembic_op->setIconName("ROP_alembic");
    table->addOperator(alembic_op);
}

void
newSopOperator(OP_OperatorTable *table)
{
    OP_TemplatePair pair(theParameters);
    OP_TemplatePair templatepair(ROP_Node::getROPbaseTemplate(), &pair);
    OP_VariablePair vp(ROP_Node::myVariableList);

    OP_Operator	*alembic_sop = new OP_Operator(
        CUSTOM_ALEMBIC_TOKEN_PREFIX "rop_alembic",
        CUSTOM_ALEMBIC_LABEL_PREFIX "ROP Alembic Output",
	ROP_AlembicOut::myConstructor,
	&templatepair, 0, 1, &vp,
	OP_FLAG_GENERATOR | OP_FLAG_MANAGER);
    alembic_sop->setObsoleteTemplates(theObsoleteParameters);
    alembic_sop->setIconName("ROP_alembic");
    table->addOperator(alembic_sop);
}
