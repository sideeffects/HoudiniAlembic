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

#ifndef __ROP_AbcRefiner__
#define __ROP_AbcRefiner__

#include "ROP_AbcHierarchySample.h"

#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>

class ROP_AbcRefiner : public GT_Refine
{
public:
    ROP_AbcRefiner(UT_Map<std::string, UT_Map<int, UT_Array<GT_PrimitiveHandle> > > &instance_map,
		   const ROP_AbcArchivePtr &abc,
		   ROP_AlembicPackedTransform packedtransform,
		   exint facesetmode,
		   bool use_instancing,
		   bool shape_nodes,
		   bool save_hidden);

    // true if non-visible geometry should be kept
    bool getSaveHidden() const { return mySaveHidden; }

    // true if the refined geometry will be visible
    void setVisible(bool vis) { myVisible = vis; }
    bool getVisible() const { return myVisible; }

    // We need the primitives generated in a consistent order
    virtual bool allowThreading() const { return false; }

    virtual void addPrimitive(const GT_PrimitiveHandle &prim);

    // adds geometry to 'root' with the provided name
    void addPartition(ROP_AbcHierarchySample &root, const std::string &name,
		      bool subd, const GT_PrimitiveHandle &prim,
		      const std::string &up_vals, const std::string &up_meta);

private:
    void appendShape(const GT_PrimitiveHandle &prim);
    bool processInstance(const GT_PrimitiveHandle &prim);
    bool processPacked(const GT_PrimitiveHandle &prim);

    GT_RefineParms myParms;
    // shapes for a given instance source
    UT_Map<std::string, UT_Map<int, UT_Array<GT_PrimitiveHandle> > > &myInstanceMap;
    ROP_AbcHierarchySample *myRoot;
    std::string myName;
    std::string myUserPropsVals;
    std::string myUserPropsMeta;

    // name of current instance being defined
    std::string myInstanceKey;
    // when exporting an instance, number of shapes exported so far
    UT_Map<std::string, UT_Map<int, exint> > myInstanceShapeIndex;

    const ROP_AbcArchivePtr &myArchive;

    // refinement options
    ROP_AlembicPackedTransform myPackedTransform;
    bool myUseInstancing;
    bool myShapeNodes;
    bool mySaveHidden;
    bool myVisible;
};

#endif
