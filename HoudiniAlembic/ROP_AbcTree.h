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
 * NAME:	ROP_AbcTree.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcTree__
#define __ROP_AbcTree__

#include <GABC/GABC_Util.h>
#include <Alembic/Abc/OArchive.h>
#include <UT/UT_HashTable.h>
#include "ROP_AbcError.h"
#include "ROP_AbcContext.h"

class ROP_AbcCamera;
class ROP_AbcShape;
class ROP_AbcXform;
class OBJ_Node;
class OP_Node;

class ROP_AbcTree
{
public:
     ROP_AbcTree();
    ~ROP_AbcTree();

    bool	open(ROP_AbcError &err,
			OP_Node *root,
			const char *filename,
			fpreal tstart,
			fpreal tstep);
    void	close();

    /// Add an object to the archive
    ROP_AbcXform	*addObject(ROP_AbcError &err, OBJ_Node *obj);

    /// Return number of objects in the tree
    exint		 entries() const
			 {
			     return myStaticObjects.entries() +
				    myDynamicObjects.entries();
			 }
    /// Number of non-animating entries
    exint		 staticEntries() const
			 {
			     return myStaticObjects.entries();
			 }
    /// Number of animating entries
    exint		 dynamicEntries() const
			 {
			     return myDynamicObjects.entries();
			 }

    /// Enable motion blur
    void		setMotionBlur(bool f)	{ myEnableMotionBlur = f; }
    void		setShutterParms(fpreal shutterOpen,
					fpreal shutterClose, int samples);

    /// Write out a sample
    bool		 writeSample(ROP_AbcError &err, fpreal time_sample);

    /// @{
    /// Flag to collapse hierarchy.  This will cause any non-animating
    /// transforms with an identity transform to be collapsed.
    bool		 collapseNodes() const	{ return myCollapseNodes; }
    void		 setCollapseNodes(bool f)
			    { myCollapseNodes = f; }
    /// @}

    /// Set save attribute option
    void		setSaveAttributes(bool v)
			    { myContext.setSaveAttributes(v); }
    /// Set partition attribute name
    void		setPartitionAttribute(const char *s)
			    { myContext.setPartitionAttribute(s); }

private:
    bool		 doWriteSample(ROP_AbcError &err, fpreal time_sample);
    ROP_AbcXform	*doAddObject(ROP_AbcError &err, OBJ_Node *obj);
    ROP_AbcXform	*findXform(OP_Node *node) const;
    ROP_AbcXform	*addXform(ROP_AbcError &err,
				OBJ_Node *obj,
				ROP_AbcXform *parent);

    Alembic::Abc::OArchive		 myArchive;
    Alembic::Abc::TimeSamplingPtr	 myTimeRange;	// Range of times
    std::vector< float >		 myOffsetFrames;
    OP_Node				*myRoot;
    UT_HashTable			 myStaticObjects;
    UT_HashTable			 myDynamicObjects;
    UT_PtrArray<ROP_AbcShape *>		 myDynamicShapes;
    UT_PtrArray<ROP_AbcCamera *>	 myDynamicCameras;
    ROP_AbcContext			 myContext;
    fpreal				 myStartTime;
    fpreal				 myTimeStep;
    fpreal				 myShutterOpen;
    fpreal				 myShutterClose;
    int					 mySamples;
    bool				 myFirstFrame;
    bool				 myCollapseNodes;
    bool				 myEnableMotionBlur;
};

#endif
