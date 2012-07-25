#include "ROP_AbcTree.h"
#include <UT/UT_Version.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_StopWatch.h>
#include <OP/OP_Director.h>
#include <OBJ/OBJ_Node.h>
#include <CMD/CMD_Manager.h>
#include <CMD/CMD_Args.h>
#include <stdarg.h>
#include "ROP_AbcError.h"

namespace
{
    class RenderContext
    {
    public:
	RenderContext(ROP_AbcError &err,
		    OP_Network *root,
		    const char *filename,
		    const UT_StringArray &objects,
		    fpreal tstart, fpreal tstep)
	    : myTree()
	    , myError(err)
	    , myRoot(root)
	    , myFilename(filename)
	    , myObjects(objects)
	    , myTStart(tstart)
	    , myTStep(tstep)
	    , myTimer()
	{
	}

	static OP_Network	*findRoot(OP_Node *node = NULL)
		{
		    OBJ_Node	*obj = node ? node->castToOBJNode() : NULL;
		    OP_Network	*net;
		    if (!obj)
			net = OPgetDirector()->getManager("obj");
		    else
			net = obj->getCreator();
		    UT_ASSERT(net->getChildTypeID() == OBJ_OPTYPE_ID);
		    return net;
		}

	void	addObject(OBJ_Node *obj)
		{
		    UT_WorkBuffer	fullpath;
		    obj->getFullPath(fullpath);
		    myError.info("Add %s to abc tree", fullpath.buffer());
		    if (!myTree.addObject(myError, obj))
			myError.error("Unable to export %s", fullpath.buffer());

		    // Recurse into subnets
		    int		nkids = obj->getNchildren();
		    for (int i = 0; i < nkids; ++i)
		    {
			OP_Node		*kid = obj->getChild(i);
			OBJ_Node	*kobj = kid->castToOBJNode();
			if (kobj)
			    addObject(kobj);
		    }
		}

	bool	startRender()
		{
		    myTimer.start();
		    if (myRoot && myRoot->getChildTypeID() != OBJ_OPTYPE_ID)
		    {
			myRoot = NULL;	// Not a valid object container
		    }
		    if (!myRoot)
		    {
			myRoot = OPgetDirector()->getManager("obj");
		    }
		    if (!myObjects.entries())
			return false;

		    if (!myTree.open(myError, myRoot, myFilename,
				myTStart, myTStep))
			return false;

		    for (int i = 0; i < myObjects.entries(); ++i)
		    {
			UT_WorkBuffer	 fullpath;
			const char	*path = myObjects(i);
			OP_Network	*op = static_cast<OP_Network *>(myRoot->findNode(path));
			if (!op)
			{
			    myError.error("Object %s not found", path);
			    continue;
			}
			op->getFullPath(fullpath);
			OBJ_Node	*obj = op->castToOBJNode();
			if (!obj)
			{
			    myError.error("Node %s is not an object",
				    fullpath.buffer());
			    continue;
			}

			addObject(obj);
		    }
		    if (!myTree.entries())
			myError.error("No nodes to save to '%s'", myFilename);
		    myError.warning("Alembic tree with %d static and %d dynamic nodes",
			    (int)myTree.staticEntries(),
			    (int)myTree.dynamicEntries());
		    return myTree.entries() > 0;
		}

	bool	renderFrame(fpreal now)
		{
		    fpreal	frame;
		    frame=OPgetDirector()->getChannelManager()->getSample(now);
		    myError.warning("--- Frame: %g (%.2f s) ---", frame, myTimer.lap());
		    return myTree.writeSample(myError, now);
		}
	bool	endRender()
		{
		    myTree.close();
		    return true;
		}


    private:
	ROP_AbcTree		 myTree;
	ROP_AbcError		&myError;
	UT_StopWatch		 myTimer;
	OP_Network		*myRoot;
	const char		*myFilename;
	const UT_StringArray	&myObjects;
	fpreal			 myTStart, myTStep;
    };
}

static bool
exportAlembic(
	ROP_AbcError &errors,
	OP_Network *root,
	const char *filename,
	const UT_StringArray &objects,
	int nframes,
	fpreal fstart, fpreal finc=1
    )
{
    CH_Manager		&chman = *OPgetDirector()->getChannelManager();
    fpreal		 tstart = chman.getTime(fstart);
    fpreal		 tstep = finc * chman.getSecsPerSample();

    RenderContext	ctx(errors, root, filename, objects, tstart, tstep);
    if (ctx.startRender())
    {
	for (int i = 0; i < nframes; ++i)
	{
	    if (!ctx.renderFrame(tstart + i*tstep))
	    {
		errors.error("Error rendering frame: %d", i);
		break;
	    }
	}
	ctx.endRender();
    }
    return errors.success();
}

// Usage: abcexport [-f fstart fend] [-i inc] [-c camera] abcfile object1 [object2 ...]
static void
cmd_AbcExport(CMD_Args &args)
{
    if (args.argc() < 3)
    {
	args.err() << "Invalid arguments\n";
	args.err() << "Usage: " << args(0)
		<< " [-f fstart fend] [-i inc] [-c camera] file.abc object1 [object2...]\n";
	return;
    }
    const char		*camera = args.found('c') ? args.argp('c') : NULL;
    OP_Network		*root = NULL;
    fpreal		 fstart, fend, finc = 1;

    if (args.found('f'))
    {
	fstart = args.fargp('f', 0);
	fend = args.fargp('f', 1);
	if (fend < fstart)
	    UTswap(fstart, fend);
    }
    else
    {
	fstart = CHgetEvalTime();
	fend = fstart;
    }
    finc = args.found('i') ? SYSmax(0.001, SYSabs(args.fargp('i'))) : 1;
    fend += finc*1.5;
    UT_ASSERT(fend - fstart > finc);
    int	nframes = (fend - fstart) / finc;
    if (camera)
    {
	root = RenderContext::findRoot(OPgetDirector()->findNode(camera));
    }

    UT_StringArray	 objects;
    for (int i = 2; i < args.argc(); ++i)
	objects.append(args(i));

    ROP_AbcError	err;
    exportAlembic(err, root, args(1), objects, nframes, fstart, finc);
}

void
CMDextendLibrary(CMD_Manager *cman)
{
    cman->installCommand("abcexport", "c:f::i:",	cmd_AbcExport);
}
