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

#include "ROP_AlembicOut.h"
#include "ROP_AbcOpBuilder.h"
#include "ROP_AbcArchive.h"
#include <ROP/ROP_Shared.h>

#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_SharedNames.h>
#include <SOP/SOP_Node.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_BundleList.h>
#include <OP/OP_Director.h>
#include <MGR/MGR_Node.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>
#include <GABC/GABC_OError.h>

#include <UT/UT_Interrupt.h>
#include <UT/UT_DSOVersion.h>

#if !defined(CUSTOM_ALEMBIC_TOKEN_PREFIX)
    #define CUSTOM_ALEMBIC_TOKEN_PREFIX	""
    #define CUSTOM_ALEMBIC_LABEL_PREFIX	""
#endif

using namespace GABC_NAMESPACE;

namespace
{
    class rop_AlembicOutError : public GABC_NAMESPACE::GABC_OError
    {
    public:
	rop_AlembicOutError(ROP_AlembicOut &node, UT_Interrupt *interrupt)
	    : GABC_OError(interrupt)
	    , myNode(node)
	{
	}
	virtual void	handleError(const char *msg)
				{ myNode.abcError(msg); }
	virtual void	handleWarning(const char *msg)
				{ myNode.abcWarning(msg); }
	virtual void	handleInfo(const char *msg)
				{ myNode.abcInfo(0, msg); }
    private:
	ROP_AlembicOut	&myNode;
    };

    static void
    buildBundleMenu(void *, PRM_Name *menu, int max,
		    const PRM_SpareData *spare, const PRM_Parm *)
    {
	OPgetDirector()->getBundles()->buildBundleMenu(
		    menu, max, spare ? spare->getValue("opfilter") : 0);
    }

    // Get the list of single primitive string attributes from a SOP.
    static void
    getAttribs(SOP_Node *node, OP_Context &context, UT_StringArray &vals)
    {
        const GU_Detail *ref = node->getCookedGeo(context);
        if(ref)
        {
            for(auto it = ref->getAttributeDict(GA_ATTRIB_PRIMITIVE).begin();
                    !it.atEnd();
                    ++it)
            {
                const GA_Attribute *atr = it.attrib();

                if(atr->getStorageClass() != GA_STORECLASS_STRING
                    || atr->getTupleSize() > 1
                    || atr->getScope() != GA_SCOPE_PUBLIC)
                {
                    continue;
                }

                const char *name = atr->getName();
                vals.append(name);
            }

            vals.sort();
        }
    }

    // Build the menu of likely choices for use with the "Build Hierarchy From
    // Attribute" option.
    static void
    buildAttribMenu(void *data,
            PRM_Name *menu_entries,
            int menu_size,
            const PRM_SpareData *,
            const PRM_Parm *)
    {
        ROP_AlembicOut *me = (ROP_AlembicOut *)data;

        me->clearErrors();

        OP_Context      context(CHgetEvalTime());
        UT_StringArray  vals;

        if(me->nInputs() > 0 && me->lockInput(0, context) < UT_ERROR_ABORT)
        {
            SOP_Node   *node = CAST_SOPNODE(me->getInput(0));
            if (node)
            {
                getAttribs(node, context, vals);
            }

    	    me->unlockInput(0);
        }
        else
        {
            SOP_Node   *node = me->getSOPInput(0);

            if (node)
            {
                getAttribs(node, context, vals);
            }
        }

        int i = 0;
        while(i < vals.entries() && i + 1 < menu_size)
        {
            menu_entries[i].setToken(vals(i));
            menu_entries[i].setLabel(vals(i));
            ++i;
        }
        menu_entries[i].setToken(0);
        menu_entries[i].setLabel(0);
    }

    static PRM_Name     separator1Name("_sep1", "");
    static PRM_Name     separator2Name("_sep2", "");
    static PRM_Name     separator3Name("_sep3", "");
    static PRM_Name	theFilenameName("filename", "Alembic File");
    static PRM_Name	theFormatName("format", "Format");
    static PRM_Name	theSingleSopModeName("use_sop_path", "Use SOP Path");
    static PRM_Name     theSOPPathName("sop_path", "SOP Path");
    static PRM_Name	theRootName("root", "Root Object");
    static PRM_Name	theObjectsName("objects", "Objects");
    static PRM_Name	theInitSim("initsim", "Initialize Simulation OPs");
    static PRM_Name	theRenderFullRange("render_full_range",
			    "Render Full Range (Override Frame-By-Frame)");
    static PRM_Name	theCollapseName("collapse", "Collapse Objects");
    static PRM_Name	theSaveHiddenName("save_hidden",
				"Save All Non-Displayed (Hidden) Objects");
    static PRM_Name	theUseInstancingName("use_instancing",
				"Use Alembic Instancing Where Possible");
    static PRM_Name	theFullBoundsName("full_bounds",
				"Full Bounding Box Tree");
    static PRM_Name	theDisplaySOPName("displaysop",
				"Use Display SOP");
    static PRM_Name	theSaveAttributesName("save_attributes",
				"Save Attributes");
    static PRM_Name     theBuildHierarchyFromPathName("build_from_path",
                                "Build Hierarchy From Attribute");
    static PRM_Name     thePathAttribName("path_attrib", "Path Attribute");
    static PRM_Name     thePackedAbcPriorityName("packed_priority",
                                "Packed Alembic Priority");
    static PRM_Name	thePartitionModeName("partition_mode",
				"Partition Mode");
    static PRM_Name	thePartitionAttributeName("partition_attribute",
				"Partition Attribute");
    static PRM_Name	theAttributePatternNames[GA_ATTRIB_OWNER_N] = {
	PRM_Name("vertexAttributes",	"Vertex Attributes"),
	PRM_Name("pointAttributes",	"Point Attributes"),
	PRM_Name("primitiveAttributes",	"Primitive Attributes"),
	PRM_Name("detailAttributes",	"Detail Attributes"),
    };
    static PRM_Name	theFaceSetModeName("facesets", "Face Sets");
    static PRM_Name	theSubdGroupName("subdgroup",
				"Subdivision Group");
    static PRM_Name	theVerboseName("verbose", "Verbosity");
    static PRM_Name	theMotionBlurName("motionBlur", "Use Motion Blur");
    static PRM_Name	theSampleName("samples", "Samples");
    static PRM_Name	theShutterName("shutter", "Shutter");

    static PRM_Default	theFilenameDefault(0, "$HIP/output.abc");
    static PRM_Default	theFormatDefault(0, "default");
    static PRM_Default	theRootDefault(0, "/obj");
    static PRM_Default	theStarDefault(0, "*");
    static PRM_Default	theCollapseDefault(0, "off");
    static PRM_Default	theFullBoundsDefault(0, "no");
    static PRM_Default	theDisplaySOPDefault(0, "no");
    static PRM_Default	theSaveAttributesDefault(1, "yes");
    static PRM_Default  theBuildHierarchyFromPathDefault(0, "no");
    static PRM_Default  thePackedAbcPriorityDefault(0, "hier");
    static PRM_Default	thePartitionModeDefault(0, "no");
    static PRM_Default	thePartitionAttributeDefault(0, "");
    static PRM_Default	theFaceSetModeDefault(1, "nonempty");
    static PRM_Default	theVerboseDefault(0);
    static PRM_Default	theMotionBlurDefault(0, "no");
    static PRM_Default	theSampleDefault(2);
    static PRM_Default	theShutterDefault[] = {0, 1};
    //static PRM_Default	theFaceSetDefault(0);

    static PRM_Name	theFormatChoices[] =
    {
	PRM_Name("default",	"Default Format"),
	PRM_Name("hdf5",	"HDF5"),
	PRM_Name("ogawa",	"Ogawa"),
	PRM_Name()
    };

    static PRM_Name	thePackedAbcPriorityChoices[] =
    {
        PRM_Name("hier",        "Hierarchy"),
        PRM_Name("xform",       "Transformation"),
        PRM_Name()
    };
    static bool
    mapPackedAbcPriority(const char *mode, int &value)
    {
        if (!strcmp(mode, "hier"))
        {
            value = ROP_AbcContext::PRIORITY_HIERARCHY;
        }
        else
        {
            UT_ASSERT(!strcmp(mode, "xform"));

            value = ROP_AbcContext::PRIORITY_TRANSFORM;
        }

        return true;
   }

    static PRM_Name	thePartitionModeChoices[] =
    {
	PRM_Name("no",		"No Geometry Partitioning"),
	PRM_Name("full",	"Use Attribute Value"),
	PRM_Name("shape",	"Use Shape Node Component Of Path Attribute Value"),
	PRM_Name("xform",	"Use Transform Node Component Of Path Attribute value"),
	PRM_Name("xformshape",	"Use Combination Of Transform/Shape Node"),
	PRM_Name(),	// Sentinal
    };
    static bool
    mapPartitionMode(const char *mode, int &value)
    {
	value = ROP_AbcContext::PATHMODE_FULLPATH;
	if (!strcmp(mode, "full"))
	    return true;
	if (!strcmp(mode, "shape"))
	{
	    value = ROP_AbcContext::PATHMODE_SHAPE;
	    return true;
	}
	if (!strcmp(mode, "xform"))
	{
	    value = ROP_AbcContext::PATHMODE_XFORM;
	    return true;
	}
	if (!strcmp(mode, "xformshape"))
	{
	    value = ROP_AbcContext::PATHMODE_XFORM_SHAPE;
	    return true;
	}
	return false;
    }

    static PRM_Name	thePartitionAttributeChoices[] =
    {
	PRM_Name("",		"No Geometry Partitions"),
	PRM_Name("name",	"Partition Based On 'name' Attribute"),
	PRM_Name("abcPath",	"Partition Based On 'abcPath' Attribute"),
	PRM_Name()	// Sentinal
    };

    static PRM_Name	theFaceSetModeChoices[] =
    {
	PRM_Name("no",		"No Face Sets"),
	PRM_Name("nonempty",	"Save Non-Empty Groups As Face Sets"),
	PRM_Name("all",		"Save All Groups As Face Sets"),
	PRM_Name()
    };
    static bool
    mapFaceSetMode(const char *mode, GABC_OOptions::FaceSetMode &value)
    {
	value = ROP_AbcContext::FACESET_DEFAULT;
	if (!strcmp(mode, "no"))
	{
	    value = ROP_AbcContext::FACESET_NONE;
	    return true;
	}
	if (!strcmp(mode, "nonempty"))
	{
	    value = ROP_AbcContext::FACESET_NON_EMPTY;
	    return true;
	}
	if (!strcmp(mode, "all"))
	{
	    value = ROP_AbcContext::FACESET_ALL_GROUPS;
	    return true;
	}
	return false;
    }

    static PRM_Name	theCollapseChoices[] =
    {
	PRM_Name("off",	"Do Not Collapse Identity Objects"),
	PRM_Name("on",  "Collapse Non-Animating Identity Objects"),
	PRM_Name("geo", "Collapse All Geometry Container Objects"),
	PRM_Name("all", "Collapse All Objects (Direct Hierarchy Placement)"),
	PRM_Name()
    };

    static PRM_ChoiceList	theObjectsMenu(PRM_CHOICELIST_REPLACE,
					buildBundleMenu);
    static PRM_ChoiceList       thePathAttribMenu(PRM_CHOICELIST_REPLACE,
                                        buildAttribMenu);
    static PRM_ChoiceList	theFormatMenu(PRM_CHOICELIST_SINGLE,
					theFormatChoices);
    static PRM_ChoiceList	thePackedAbcPriorityMenu(PRM_CHOICELIST_SINGLE,
					thePackedAbcPriorityChoices);
    static PRM_ChoiceList	thePartitionModeMenu(PRM_CHOICELIST_SINGLE,
					thePartitionModeChoices);
    static PRM_ChoiceList	thePartitionAttributeMenu(PRM_CHOICELIST_REPLACE,
					thePartitionAttributeChoices);
    static PRM_ChoiceList	theFaceSetModeMenu(PRM_CHOICELIST_SINGLE,
					theFaceSetModeChoices);
    static PRM_ChoiceList	theCollapseMenu(PRM_CHOICELIST_SINGLE,
					theCollapseChoices);

    static PRM_Range            theVerboseRange(PRM_RANGE_RESTRICTED, 0,
				        PRM_RANGE_UI, 3);

    // Make paths relative to /obj (for the bundle code)
    static PRM_SpareData	theObjectList(PRM_SpareArgs()
				    << PRM_SpareToken("opfilter", "!!OBJ!!")
				    << PRM_SpareToken("oprelative", "/obj")
				);

    static PRM_Template	theParameters[] = {
	PRM_Template(PRM_FILE, 1, &theFilenameName, &theFilenameDefault),
	PRM_Template(PRM_ORD, 1, &theFormatName, &theFormatDefault,
				    &theFormatMenu),
	PRM_Template(PRM_TOGGLE, 1, &ROPmkpath, PRMoneDefaults),
	PRM_Template(PRM_JOINED_TOGGLE, 1, &theSingleSopModeName),
	PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH, 1, &theSOPPathName,
                                    0, 0, 0, 0, &PRM_SpareData::sopPath),
	// Root object should be relative to ROP
	PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH,
				    1, &theRootName, &theRootDefault,
				    0, 0, 0, &PRM_SpareData::objPath),
	PRM_Template(PRM_STRING_OPLIST, PRM_TYPE_DYNAMIC_PATH_LIST,
				    1, &theObjectsName, &theStarDefault,
				    &theObjectsMenu, 0, 0,
				    &theObjectList),
	PRM_Template(PRM_TOGGLE, 1, &theInitSim),
	PRM_Template(PRM_TOGGLE, 1, &theRenderFullRange, PRMoneDefaults),

        PRM_Template(PRM_SEPARATOR, 1, &separator1Name),

	PRM_Template(PRM_TOGGLE, 1, &theBuildHierarchyFromPathName,
				    &theBuildHierarchyFromPathDefault),
        PRM_Template(PRM_STRING, 1, &thePathAttribName, 0, &thePathAttribMenu),
        PRM_Template(PRM_ORD, 1, &thePackedAbcPriorityName,
                                    &thePackedAbcPriorityDefault,
                                    &thePackedAbcPriorityMenu),
	PRM_Template(PRM_ORD, 1, &thePartitionModeName,
				    &thePartitionModeDefault,
				    &thePartitionModeMenu),
	PRM_Template(PRM_STRING, 1, &thePartitionAttributeName,
				    &thePartitionAttributeDefault,
				    &thePartitionAttributeMenu),

        PRM_Template(PRM_SEPARATOR, 1, &separator2Name),

	PRM_Template(PRM_ORD, 1, &theCollapseName, &theCollapseDefault,
				    &theCollapseMenu),
	PRM_Template(PRM_TOGGLE, 1, &theSaveHiddenName, PRMoneDefaults),
	PRM_Template(PRM_TOGGLE, 1, &theUseInstancingName, PRMoneDefaults),
	PRM_Template(PRM_TOGGLE, 1, &theFullBoundsName,
				    &theFullBoundsDefault),
	PRM_Template(PRM_TOGGLE, 1, &theDisplaySOPName,
				    &theDisplaySOPDefault),
	PRM_Template(PRM_TOGGLE, 1, &theSaveAttributesName,
				    &theSaveAttributesDefault),

        PRM_Template(PRM_SEPARATOR, 1, &separator3Name),

	PRM_Template(PRM_STRING, 1,
		&theAttributePatternNames[GA_ATTRIB_POINT],
		&theStarDefault),
	PRM_Template(PRM_STRING, 1,
		&theAttributePatternNames[GA_ATTRIB_VERTEX],
		&theStarDefault),
	PRM_Template(PRM_STRING, 1,
		&theAttributePatternNames[GA_ATTRIB_PRIMITIVE],
		&theStarDefault),
	PRM_Template(PRM_STRING, 1,
		&theAttributePatternNames[GA_ATTRIB_DETAIL],
		&theStarDefault),
	PRM_Template(PRM_ORD, 1, &theFaceSetModeName,
				    &theFaceSetModeDefault,
				    &theFaceSetModeMenu),
	PRM_Template(PRM_STRING, 1, &theSubdGroupName),
	PRM_Template(PRM_INT,	1, &theVerboseName, &theVerboseDefault,
				    0, &theVerboseRange),
        PRM_Template(PRM_TOGGLE, 1, &theMotionBlurName, &theMotionBlurDefault),
        PRM_Template(PRM_INT,	 1, &theSampleName, &theSampleDefault),
        PRM_Template(PRM_FLT,	 2, &theShutterName, theShutterDefault),
	PRM_Template()
    };

    static OP_TemplatePair *
    getTemplatePair()
    {
	static OP_TemplatePair	*pair = 0;
	if (!pair)
	{
	    OP_TemplatePair	*abc = new OP_TemplatePair(theParameters);
	    pair = new OP_TemplatePair(ROP_Node::getROPbaseTemplate(), abc);
	}
	return pair;
    }
    static OP_VariablePair *
    getVariablePair()
    {
	static OP_VariablePair	*pair = 0;
	if (!pair)
	{
	    pair = new OP_VariablePair(ROP_Node::myVariableList);
	}
	return pair;
    }

    OP_Node *
    createAlembicOut(OP_Network *net, const char *name, OP_Operator *op)
    {
	return new ROP_AlembicOut(net, name, op);
    }
}

ROP_AlembicOut::ROP_AlembicOut(OP_Network *net, const char *name, OP_Operator *op)
    : ROP_Node(net, name, op)
    , myArchive(NULL)
    , myContext(NULL)
    , myError(NULL)
    , myVerbose(0)
{
}

ROP_AlembicOut::~ROP_AlembicOut()
{
}

void
ROP_AlembicOut::close()
{
    delete myContext;	myContext = NULL;
    delete myError;	myError = NULL;
    delete myArchive;	myArchive = NULL;
}

int
ROP_AlembicOut::COLLAPSE(fpreal time)
{
    UT_String	str;
    evalString(str, "collapse", 0, time);
    if (str == "off")
	return ROP_AbcContext::COLLAPSE_NONE;
    if (str == "geo")
	return ROP_AbcContext::COLLAPSE_GEOMETRY;
    if (str == "all")
	return ROP_AbcContext::COLLAPSE_ALL;
    UT_ASSERT(str == "on");
    return ROP_AbcContext::COLLAPSE_IDENTITY;
}

void
ROP_AlembicOut::buildRenderDependencies(const ROP_RenderDepParms &p)
{
    if (RENDER_FULL_RANGE())
    {
	ROP_RenderDepParms	parms = p;
	parms.setFullAscendingFrameRange(*this);
	ROP_Node::buildRenderDependencies(parms);
    }
    else
    {
	ROP_Node::buildRenderDependencies(p);
    }
}

int
ROP_AlembicOut::startRender(int nframes, fpreal start, fpreal end)
{
    close();

    /// Trap errors
    myError = new rop_AlembicOutError(*this, UTgetInterrupt());
    myContext = new ROP_AbcContext();

    if (!executePreRenderScript(start))
	return 0;

    /// Evaluate parameters
    OP_Node    *input = getInput(0);
    OP_Node    *rootnode = NULL;
    OP_Node    *sop_parent = NULL;
    SOP_Node   *sop = NULL;
    UT_String   filename;
    UT_String   format;
    UT_String   objects;
    UT_String   root;
    UT_String   sop_path;
    fpreal      tdelta = (end - start);
    fpreal      tstep;
    fpreal      shutter_open = 0;
    fpreal      shutter_close = 0;
    int         mb_samples = 1;

    if (!input && USE_SOP_PATH(start))
    {
        SOP_PATH(sop_path, start);
        sop_path.trimBoundingSpace();
        if (sop_path.isstring())
        {
            sop = CAST_SOPNODE(findNode(sop_path));
            if (!sop)
            {
                abcError("Invalid SOP path: node either not found or not SOP");
            }
        }
    }
    else
    {
        sop = CAST_SOPNODE(input);
    }

    FILENAME(filename, start);
    FORMAT(format, start);
    if (sop)
    {
	sop_parent = sop->getCreator();
	rootnode = sop_parent ? sop_parent->getParent() : NULL;
	if (!rootnode)
	{
	    abcError("Invalid SOP configuration");
	    sop = NULL;
	}
	myContext->setSingletonSOP(sop);
    }
    ROOT(root, start);
    OBJECTS(objects, start);
    myVerbose = VERBOSE(start);
    myEndTime = end;
    myFirstFrame = true;

    filename.trimBoundingSpace();
    root.trimBoundingSpace();

    if (!filename.isstring())
	abcError("Require Alembic filename");

    if (!rootnode)
    {
	if (!root.isstring())
	    abcError("Require root for all objects");
	else if (!(rootnode = findNode(root)))
	    abcError("Can't find root node");
    }

    // Check for evaluation errors
    if (error() >= UT_ERROR_ABORT)
	return 0;

    if (nframes < 2 || SYSequalZero(tdelta))
	tstep = 1.0/(CHgetManager()->getSamplesPerSec());
    else
	tstep = tdelta / (nframes-1);

    UT_String			faceset_str;
    GABC_OOptions::FaceSetMode	faceset_mode;
    FACESET_MODE(faceset_str, start);
    if (mapFaceSetMode(faceset_str, faceset_mode))
	myContext->setFaceSetMode(faceset_mode);
    else
	abcWarning("Invalid value for faceset mode");

    myContext->setCollapseIdentity(COLLAPSE(start));
    myContext->setSaveAttributes(SAVE_ATTRIBUTES(start));
    myContext->setUseDisplaySOP(DISPLAYSOP(start));
    myContext->setFullBounds(FULL_BOUNDS(start));
    myContext->setUseInstancing(USE_INSTANCING(start));
    myContext->setSaveHidden(SAVE_HIDDEN(start));
    myContext->setFirstFrame(start / tstep);

    // Can only place objects directly into hierarchy from a SOP network.
    // Direct hierarchy placement and partitioning use the same code,
    // can only do one or the other. Now that direct hierarchy placement
    // is available, I don't see much use for partitioning at all, but
    // it remains for backwards compatability.
    if (sop)
    {
        myContext->setBuildFromPath(BUILD_HIERARCHY_FROM_PATH(start));
    }
    if (myContext->buildFromPath())
    {
        OP_Context          context(CHgetEvalTime());
        GA_Attribute       *attrib;
        const GU_Detail    *ref = sop->getCookedGeo(context);
        UT_String           packed_priority;
    	UT_String           path_attrib;
        int                 packed_priority_val;

        PATH_ATTRIBUTE(path_attrib, start);

        attrib = ref->getAttributeDict(GA_ATTRIB_PRIMITIVE)
                .find(GA_SCOPE_PUBLIC, path_attrib);
        if (!attrib)
        {
            abcError("Cannot find path attribute in primitive attributes.");
        }
        else if (attrib->getTupleSize() > 1
                || attrib->getStorageClass() != GA_STORECLASS_STRING)
        {
            abcError("Path attribute is not a string tuple size 1.");
        }

    	PACKED_ABC_PRIORITY(packed_priority, start);
        mapPackedAbcPriority(packed_priority, packed_priority_val);

    	myContext->setPathAttribute(path_attrib);
        myContext->serPackedAlembicPriority(packed_priority_val);
    }
    else
    {
        UT_String	partition_mode;
        UT_String	partition_attrib;
        int		partition_mode_val;

        PARTITION_MODE(partition_mode, start);
        myContext->clearPartitionAttribute();
        if (mapPartitionMode(partition_mode, partition_mode_val))
        {
            PARTITION_ATTRIBUTE(partition_attrib, start);
            myContext->setPartitionMode(partition_mode_val);
            myContext->setPartitionAttribute(partition_attrib);
        }
    }

    for (int i = 0; i < GA_ATTRIB_OWNER_N; ++i)
    {
	UT_String	pattern;
	evalString(pattern, theAttributePatternNames[i].getToken(), 0, start);
	myContext->setAttributePattern((GA_AttributeOwner)i, pattern);
    }

    try
    {
	UT_String	subdgroup;
	SUBDGROUP(subdgroup, start);
	myContext->setSubdGroup(subdgroup);

	if (MOTIONBLUR(start))
	{
	    mb_samples = SYSmax(SAMPLES(start), 1);
	    shutter_open = SHUTTEROPEN(start);
	    shutter_close = SHUTTERCLOSE(start);
	}
	myContext->setTimeSampling(nframes,
		start,
	        tstep,
	        mb_samples,
	        shutter_open,
	        shutter_close);

	myArchive = new ROP_AbcArchive();
	if (!myArchive->open(*myError, filename, format))
	{
	    close();
	    return 0;
	}

	if (INITSIM(start))
	{
	    initSimulationOPs();
	    OPgetDirector()->bumpSkipPlaybarBasedSimulationReset(-1);
	}

	if (sop)
	{
	    UT_ASSERT(rootnode && sop_parent);
	    ROP_AbcOpBuilder	builder(rootnode);
	    builder.addChild(*myError, sop_parent);
	    builder.buildTree(*myArchive, *myContext);
	}
	else
	{
	    // Now, build the tree
	    OP_Bundle	*bundle = getParmBundle("objects", 0, objects,
				OPgetDirector()->getManager("obj"), "!!OBJ!!");
	    if (bundle)
	    {
		UT_WorkBuffer	message;
		message.sprintf("Alembic file %s created with %d objects",
			(const char *)filename, bundle->entries());
		abcInfo(-1, message.buffer());
		ROP_AbcOpBuilder	builder(rootnode);
		for (exint i = 0; i < bundle->entries(); ++i)
		{
		    OP_Node	*node = bundle->getNode(i);
		    if (filterNode(node, start))
			builder.addChild(*myError, node);
		}
		//builder.ls();
		builder.buildTree(*myArchive, *myContext);
	    }
	}
	if (!myArchive->childCount())
	    abcWarning("No objects selected for writing");
    }
    catch (const std::exception &err)
    {
	UT_WorkBuffer	msg;
	msg.sprintf("Alembic exception: %s", err.what());
	abcError(msg.buffer());
    }

    if (error() >= UT_ERROR_ABORT)
    {
	close();
	return 0;
    }
    return 1;
}

bool
ROP_AlembicOut::filterNode(OP_Node *node, fpreal now)
{
    if (!node)
	return false;

    OBJ_Node	*obj = node->castToOBJNode();
    if (!obj)
    {
	UT_WorkBuffer	path;
	node->getFullPath(path);
	myError->error("%s is not an object", path.buffer());
	return false;
    }
    // We need to evaluate the display before isDisplayTimeDependent() will
    // give us valid results.  We need to do this here since we check time
    // dependency in ROP_AbcOpXform.C.  If this isn't done, hbatch may end up
    // with incorrect time dependency information.
    bool	disp = obj->getObjectDisplay(now);
    if (!myContext->saveHidden())
    {
	// If we're hidden and we aren't time dependent, then we can skip the
	// object
	if (!disp && !obj->isDisplayTimeDependent())
	    return false;
    }
    return true;
}

#if  0
static void
countObjects(const ROP_AbcObject *root, int &total, int &tdep, int &stdep)
{
    typedef ROP_AbcObject::ChildContainer	Container;
    total++;
    if (root->timeDependent()) tdep++;
    if (root->selfTimeDependent()) stdep++;
    const Container	&kids = root->getChildren();
    for (Container::const_iterator it = kids.begin(); it != kids.end(); ++it)
    {
	countObjects(it->second, total, tdep, stdep);
    }
}
#endif

ROP_RENDER_CODE
ROP_AlembicOut::renderFrame(fpreal time, UT_Interrupt *boss)
{
    if (!executePreFrameScript(time))
    {
	close();
	return ROP_ABORT_RENDER;
    }

    if (myArchive)
    {
	try
	{
	    int		start = 0;
	    //UT_StopWatch	timer; timer.start();
	    if (myFirstFrame)
	    {
		myFirstFrame = false;
		myContext->setTime(time, 0);
		if (!myArchive->firstFrame(*myError, *myContext))
		    return ROP_ABORT_RENDER;
		start = 1;
#if 0
		fprintf(stderr, "First frame: %g\n", timer.lap());
		int stdep = 0;
		int tdep = 0;
		int total = 0;
		countObjects(myArchive, total, tdep, stdep);
		fprintf(stderr, "Nodes: %d %d/%d\n", total, stdep, tdep);
#endif
	    }
	    for (int i = start; i < myContext->samplesPerFrame(); ++i)
	    {
		myContext->setTime(time, i);
		if (!myArchive->nextFrame(*myError, *myContext))
		    return ROP_ABORT_RENDER;
		//fprintf(stderr, "Next frame: %g\n", timer.lap());
	    }
	}
	catch (std::exception &err)
	{
	    UT_WorkBuffer	msg;
	    msg.sprintf("Alembic exception: %s", err.what());
	    abcError(msg.buffer());
	}
    }

    if (!executePostFrameScript(time))
    {
	close();
	return ROP_ABORT_RENDER;
    }

    return ROP_CONTINUE_RENDER;
}

ROP_RENDER_CODE
ROP_AlembicOut::endRender()
{
    close();

    if (!executePostRenderScript(myEndTime))
	return ROP_ABORT_RENDER;
    if (INITSIM(myEndTime))
	OPgetDirector()->bumpSkipPlaybarBasedSimulationReset(-1);
    return ROP_CONTINUE_RENDER;
}

bool
ROP_AlembicOut::updateParmsFlags()
{
    bool        build_hier = BUILD_HIERARCHY_FROM_PATH(0);
    bool	changed = ROP_Node::updateParmsFlags();
    bool	issop = CAST_SOPNODE(getInput(0)) != NULL;
    bool        use_sop_path = USE_SOP_PATH(0);
    bool        sop_mode = issop || use_sop_path;
    UT_String	mode;

    PARTITION_MODE(mode, 0);

    changed |= enableParm("use_sop_path", !issop);
    changed |= enableParm("sop_path", !issop && use_sop_path);
    changed |= enableParm("root", !sop_mode);
    changed |= enableParm("objects", !sop_mode);
    changed |= enableParm("build_from_path", sop_mode);
    changed |= enableParm("path_attrib", build_hier && sop_mode);
    changed |= enableParm("packed_priority", build_hier && sop_mode);
    changed |= enableParm("partition_mode", !build_hier || !sop_mode);
    changed |= enableParm("partition_attribute",
                ((!build_hier || !sop_mode ) && (mode != "no")));
    changed |= enableParm("shutter", MOTIONBLUR(0));
    changed |= enableParm("samples", MOTIONBLUR(0));

    return changed;
}

void
newDriverOperator(OP_OperatorTable *table)
{
    OP_Operator	*alembic_op = new OP_Operator(
        CUSTOM_ALEMBIC_TOKEN_PREFIX "alembic",		// Internal name
        CUSTOM_ALEMBIC_LABEL_PREFIX "Alembic",		// GUI name
	createAlembicOut,
	getTemplatePair(),
	0, 9999,
	getVariablePair(),
	OP_FLAG_GENERATOR);
    alembic_op->setIconName("ROP_alembic");
    table->addOperator(alembic_op);

    OP_Operator	*alembic_sop = new OP_Operator(
        CUSTOM_ALEMBIC_TOKEN_PREFIX "rop_alembic",
        CUSTOM_ALEMBIC_LABEL_PREFIX "ROP Alembic Output",
	createAlembicOut,
	getTemplatePair(),
	0, 1,
	getVariablePair(),
	OP_FLAG_GENERATOR|OP_FLAG_MANAGER);
    alembic_sop->setIconName("ROP_alembic");

    // Note:  This is reliant on the order of operator table construction and
    // may not be safe to do in all cases.
    OP_OperatorTable	*soptable = OP_Network::getOperatorTable(
					    SOP_TABLE_NAME,
					    SOP_SCRIPT_NAME);
    soptable->addOperator(alembic_sop);
}
