#include "ROP_AlembicOut.h"
#include "ROP_AbcError.h"
#include "ROP_AbcTree.h"
#include <ROP/ROP_Shared.h>

#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_SharedNames.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_BundleList.h>
#include <OP/OP_Director.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_SpareData.h>

#include <UT/UT_Interrupt.h>
#include <UT/UT_DSOVersion.h>


namespace
{
    class rop_AlembicOutError : public ROP_AbcError
    {
    public:
	rop_AlembicOutError(ROP_AlembicOut &node, UT_Interrupt *interrupt)
	    : ROP_AbcError(interrupt)
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

    static PRM_Name	theFilenameName("filename", "Alembic File");
    static PRM_Name	theRootName("root", "Root Object");
    static PRM_Name	theObjectsName("objects", "Objects");
    static PRM_Name	theCollapseName("collapse", "Collapse identity transforms");
    static PRM_Name	theSaveAttributesName("save_attributes",
				"Save Attributes");
    static PRM_Name	thePartitionAttributeName("partition_attribute",
				"Partition Attribute");
    static PRM_Name	theVerboseName("verbose", "Verbosity");

    static PRM_Default	theFilenameDefault(0, "$HIP/output.abc");
    static PRM_Default	theRootDefault(0, "/obj");
    static PRM_Default	theObjectsDefault(0, "*");
    static PRM_Default	theCollapseDefault(1, "yes");
    static PRM_Default	theSaveAttributesDefault(1, "yes");
    static PRM_Default	thePartitionAttributeDefault(0, "");
    static PRM_Default	theVerboseDefault(0);

    static PRM_Name	thePartitionAttributeChoices[] = {
	PRM_Name("",		"No geometry partitions"),
	PRM_Name("name",	"Partition based on the 'name' attribute"),
	PRM_Name()	// Sentinal
    };

    static PRM_ChoiceList	theObjectsMenu(PRM_CHOICELIST_REPLACE,
					buildBundleMenu);
    static PRM_ChoiceList	thePartitionAttributeMenu(PRM_CHOICELIST_REPLACE,
					thePartitionAttributeChoices);

    static PRM_Range	theVerboseRange(PRM_RANGE_RESTRICTED, 0,
				    PRM_RANGE_UI, 3);

    // Make paths relative to /obj (for the bundle code)
    static PRM_SpareData	theObjectList(
				    "opfilter",		"!!OBJ!!",
				    "oprelative",	"/obj",
				    NULL);

    static PRM_Name	theSampleName("samples", "Samples");
    static PRM_Name	theMotionBlurName("motionBlur", "Use Motion Blur");
    static PRM_Name	theShutterName("shutter", "Shutter");
    static PRM_Default	theMotionBlurDefault(0, "no");
    static PRM_Default	theSampleDefault(2);
    static PRM_Default	theShutterDefault[] = {0, 1};

    static PRM_Template	theParameters[] = {
	PRM_Template(PRM_FILE,	1, &theFilenameName, &theFilenameDefault),
	PRM_Template(PRM_TOGGLE,1, &ROPmkpath, PRMzeroDefaults),
	// Root object should be relative to ROP
	PRM_Template(PRM_STRING, PRM_TYPE_DYNAMIC_PATH,
				    1, &theRootName, &theRootDefault,
				    0, 0, 0, &PRM_SpareData::objPath),
	PRM_Template(PRM_STRING_OPLIST, PRM_TYPE_DYNAMIC_PATH_LIST,
				    1, &theObjectsName, &theObjectsDefault,
				    &theObjectsMenu, 0, 0,
				    &theObjectList),
	PRM_Template(PRM_TOGGLE, 1, &theCollapseName, &theCollapseDefault),
	PRM_Template(PRM_TOGGLE, 1, &theSaveAttributesName,
				    &theSaveAttributesDefault),
	PRM_Template(PRM_STRING, 1, &thePartitionAttributeName,
				    &thePartitionAttributeDefault,
				    &thePartitionAttributeMenu),
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
    , myTree(NULL)
    , myError(NULL)
    , myVerbose(0)
{
}

ROP_AlembicOut::~ROP_AlembicOut()
{
    delete myTree;
}

void
ROP_AlembicOut::close()
{
    delete myTree;
    delete myError;
    myTree = 0;
    myError = 0;
}

int
ROP_AlembicOut::startRender(int nframes, fpreal start, fpreal end)
{
    close();
    if (!executePreRenderScript(start))
	return 0;

    UT_String	 filename;
    UT_String	 root;
    UT_String	 objects;
    fpreal	 tdelta = (end - start);
    fpreal	 tstep;
    OP_Node	*rootnode = NULL;

    FILENAME(filename, start);
    ROOT(root, start);
    OBJECTS(objects, start);
    myVerbose = VERBOSE(start);
    myEndTime = end;

    filename.trimBoundingSpace();
    root.trimBoundingSpace();

    if (!filename.isstring())
	abcError("Require Alembic filename");
    if (!root.isstring())
	abcError("Require root for all objects");
    else if (!(rootnode = findNode(root)))
	abcError("Can't find root node");

    // Check for evaluation errors
    if (error() >= UT_ERROR_ABORT)
	return 0;

    if (nframes < 2 || tdelta < 1e-6)
	tstep = 1.0/(OPgetDirector()->getChannelManager()->getSamplesPerSec());
    else
	tstep = tdelta / (nframes-1);

    UT_String	partition;
    myError = new rop_AlembicOutError(*this, UTgetInterrupt());
    myTree = new ROP_AbcTree();
    myTree->setCollapseNodes(COLLAPSE(start));
    myTree->setSaveAttributes(SAVE_ATTRIBUTES(start));
    PARTITION_ATTRIBUTE(partition, start);
    myTree->setPartitionAttribute(partition);

    bool enableMotionBlur = MOTIONBLUR(start);
    myTree->setMotionBlur(enableMotionBlur);
    if (enableMotionBlur)
    {
        myTree->setShutterParms(SHUTTEROPEN(start),
		SHUTTERCLOSE(start), SAMPLES(start));
    }

    if (!myTree->open(*myError, rootnode, filename, start, tstep))
    {
	close();
	return 0;
    }

    OP_Bundle	*bundle = getParmBundle("objects", 0, objects,
			OPgetDirector()->getManager("obj"), "!!OBJ!!");
    if (bundle)
    {
	UT_WorkBuffer	message;
	message.sprintf("Alembic file %s created with %d objects",
		(const char *)filename, bundle->entries());
	abcInfo(-1, message.buffer());
	for (exint i = 0; i < bundle->entries(); ++i)
	{
	    addObject(bundle->getNode(i), start);
	}
    }
    else
    {
	abcWarning("No objects found in bundle");
    }

    if (error() >= UT_ERROR_ABORT)
    {
	close();
	return 0;
    }
    return 1;
}

void
ROP_AlembicOut::addObject(OP_Node *node, fpreal now)
{
    if (!node)
	return;

    OBJ_Node	*obj = node->castToOBJNode();
    if (!obj)
    {
	UT_WorkBuffer	path;
	node->getFullPath(path);
	myError->error("%s is not an object", path.buffer());
	return;
    }
#if 0
    // We likely want to output all objects (including bones)
    if (!obj->isObjectRenderable())
    {
	UT_WorkBuffer	path;
	node->getFullPath(path);
	myError->info("Skipping non-renderable object: %s", path.buffer());
	return;
    }
#endif
    if (!obj->getObjectDisplay(now))
    {
	return;
    }

    if (!myTree->addObject(*myError, obj))
	return;
}

ROP_RENDER_CODE
ROP_AlembicOut::renderFrame(fpreal time, UT_Interrupt *boss)
{
    if (!executePreFrameScript(time))
    {
	close();
	return ROP_ABORT_RENDER;
    }

    if (!myTree->writeSample(*myError, time))
    {
	close();
	return ROP_ABORT_RENDER;
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
    return ROP_CONTINUE_RENDER;
}

bool
ROP_AlembicOut::updateParmsFlags()
{
    bool	 changed = ROP_Node::updateParmsFlags();

    changed |= enableParm("shutter", MOTIONBLUR(0));
    changed |= enableParm("samples", MOTIONBLUR(0));

    return changed;
}

void
newDriverOperator(OP_OperatorTable *table)
{
    OP_Operator	*alembic_op = new OP_Operator(
		"alembic",
		"Alembic",
		createAlembicOut,
		getTemplatePair(),
		0, 1,
		getVariablePair(),
		OP_FLAG_GENERATOR);
    alembic_op->setIconName("ROP_alembic");

    table->addOperator(alembic_op);
}
