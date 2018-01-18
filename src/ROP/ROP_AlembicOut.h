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

#ifndef __ROP_AlembicOut__
#define __ROP_AlembicOut__

#include <ROP/ROP_Node.h>
#include "ROP_AbcArchive.h"
#include "ROP_AbcNodeCamera.h"
#include "ROP_AbcNodeInstance.h"
#include "ROP_AbcNodeShape.h"
#include "ROP_AbcNodeXform.h"

#include <GA/GA_Handle.h>
#include <GA/GA_Range.h>
#include <UT/UT_Array.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_Map.h>
#include <UT/UT_UniquePtr.h>

class GA_PrimitiveGroup;
class OBJ_Camera;
class OBJ_Geometry;
class OBJ_Node;

class rop_AbcHierarchySample;

enum ROP_AlembicPackedTransform
{
    ROP_ALEMBIC_PACKEDTRANSFORM_DEFORM_GEOMETRY,
    ROP_ALEMBIC_PACKEDTRANSFORM_TRANSFORM_GEOMETRY,
    ROP_ALEMBIC_PACKEDTRANSFORM_MERGE_WITH_PARENT_TRANSFORM
};

class ROP_AlembicOut : public ROP_Node
{
public:
    static OP_Node *myConstructor(OP_Network *net, const char *name, OP_Operator *op);
    virtual bool updateParmsFlags();
    /// We need to hint to the merge ROP that we can't be called one frame at a
    /// time.
    virtual void resolveObsoleteParms(PRM_ParmList *obsolete_parms);
    virtual void buildRenderDependencies(const ROP_RenderDepParms &p);

    SOP_Node *getSopNode(fpreal time) const;

protected:
    ROP_AlembicOut(OP_Network *net, const char *name, OP_Operator *entry);
    virtual ~ROP_AlembicOut();

    virtual void getDescriptiveParmName(UT_String &str) const
			{ str = "filename"; }
    virtual int startRender(int nframes, fpreal s, fpreal e);
    virtual ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt *boss);
    virtual ROP_RENDER_CODE endRender();

    void FILENAME(UT_String &str, fpreal time)
		{ getOutputOverrideEx(str, time, "filename", "mkpath"); }
    void FORMAT(UT_String &str, fpreal time) const
		{ evalString(str, "format", 0, time); }
    bool RENDER_FULL_RANGE() const
		{ return evalInt("render_full_range", 0, 0.0) != 0; }
    bool INITSIM(fpreal time) const
		{ return evalInt("initsim", 0, time) != 0; }
    bool USE_SOP_PATH(fpreal time) const
		{ return evalInt("use_sop_path", 0, time) != 0; }
    void SOP_PATH(UT_String &str, fpreal time) const
		{ evalString(str, "sop_path", 0, time); }
    void SUBDGROUP(UT_String &str, fpreal time) const
		{ evalString(str, "subdgroup", 0, time); }
    bool BUILD_FROM_PATH(fpreal time) const
		{ return evalInt("build_from_path", 0, time) != 0; }
    void PATH_ATTRIBUTE(UT_String &sval, fpreal time) const
		{ evalString(sval, "path_attrib", 0, time); }
    void ROOT(UT_String &str, fpreal time) const
		{ evalString(str, "root", 0, time); }
    void OBJECTS(UT_String &str, fpreal time) const
		{ evalString(str, "objects", 0, time); }
    void COLLAPSE(UT_String &str, fpreal time) const
		{ evalString(str, "collapse", 0, time); }
    bool SAVE_HIDDEN(fpreal time) const
		{ return evalInt("save_hidden", 0, time) != 0; }
    bool DISPLAYSOP(fpreal time) const
		{ return evalInt("displaysop", 0, time) != 0; }
    void PARTITION_MODE(UT_String &sval, fpreal time) const
		{ evalString(sval, "partition_mode", 0, time); }
    void PARTITION_ATTRIBUTE(UT_String &str, fpreal time) const
		{ evalString(str, "partition_attribute", 0, time); }
    bool FULL_BOUNDS(fpreal time) const
		{ return evalInt("full_bounds", 0, time) != 0; }
    void PACKED_TRANSFORM(UT_String &str, fpreal time) const
		{ evalString(str, "packed_transform", 0, time); }
    bool USE_INSTANCING(fpreal time) const
		{ return evalInt("use_instancing", 0, time) != 0; }
    bool SHAPE_NODES(fpreal time) const
		{ return evalInt("shape_nodes", 0, time) != 0; }
    bool SAVE_ATTRIBUTES(fpreal time) const
		{ return evalInt("save_attributes", 0, time) != 0; }
    void PRIM_TO_DETAIL_PATTERN(UT_String &str, fpreal time) const
		{ evalString(str, "prim_to_detail_pattern", 0, time); }
    bool FORCE_PRIM_TO_DETAIL(fpreal time) const
		{ return evalInt("force_prim_to_detail", 0, time); }
    void UV_ATTRIBUTE(UT_String &str, fpreal time) const
		{ evalString(str, "uvAttributes", 0, time); }
    void FACESET_MODE(UT_String &str, fpreal time) const
		{ evalString(str, "facesets", 0, time); }
    bool MOTIONBLUR(fpreal time) const
		{ return evalInt("motionBlur", 0, time) != 0; }
    int SAMPLES(fpreal time) const
		{ return evalInt("samples", 0, time); }
    fpreal SHUTTEROPEN(fpreal time) const
		{ return evalFloat("shutter", 0, time); }
    fpreal SHUTTERCLOSE(fpreal time) const
		{ return evalFloat("shutter", 1, time); }

private:
    class Hierarchy
    {
	class Node
	{
	public:
	    Node(ROP_AbcNode *node=nullptr) : myAbcNode(node) {}

	    ROP_AbcNode *getAbcNode() const { return myAbcNode; }

	    UT_Map<std::string, Node> &getChildren() { return myChildren; }
	    const UT_Map<std::string, UT_Map<int, UT_Array<ROP_AbcNodeShape *> > > &getShapes() const { return myShapes; }
	    const UT_Map<std::string, UT_Map<int, UT_Array<exint> > > &getInstancedShapes() const { return myInstancedShapes; }

	    Node *setChild(const std::string &name,
			   const ROP_AbcArchivePtr &arch,
			   const UT_Matrix4D &m,
			   const std::string &up_vals,
			   const std::string &up_meta);

	    void setShape(const std::string &name, int type, exint i,
			  const ROP_AbcArchivePtr &arch,
			  const GT_PrimitiveHandle &prim, bool visible,
			  const std::string &up_vals,
			  const std::string &up_meta);

	    ROP_AbcNodeShape *
	    newInstanceSource(const std::string &name, int type, exint key2,
			      const ROP_AbcArchivePtr &arch,
			      const GT_PrimitiveHandle &prim,
			      const std::string &up_vals,
			      const std::string &up_meta);

	    void newInstanceRef(const std::string &name, int type, exint key2,
				const ROP_AbcArchivePtr &arch,
				ROP_AbcNodeShape *src);

	    void setVisibility(bool vis);

	private:
	    ROP_AbcNode *myAbcNode; // root or xform
	    UT_Map<std::string, Node> myChildren;
	    UT_Map<std::string, UT_Map<int, UT_Array<ROP_AbcNodeShape *> > > myShapes;
	    UT_Map<std::string, UT_Map<int, UT_Array<exint> > > myInstancedShapes;
	    // FIXME: add support for cameras
	};

    public:
	Hierarchy(ROP_AbcNode *root)
	    : myRoot(root)
	    , myNextInstanceId(0)
	    , myLocked(false) {}

	ROP_AbcNode *getRoot() const { return myRoot.getAbcNode(); }

	void update(ROP_AbcArchivePtr &arch,
		    const rop_AbcHierarchySample &src,
		    const UT_Map<std::string, UT_Map<int, UT_Array<GT_PrimitiveHandle> > > &instance_map);

	void setLocked(bool locked) { myLocked = locked; }
	bool getLocked() const { return myLocked; }

    private:
	Node myRoot;
	UT_Map<int, UT_Map<exint, ROP_AbcNodeShape *> > myInstancedShapes;
	exint myNextInstanceId;
	bool myLocked;
    };

    class rop_OError : public GABC_OError
    {
    public:
	rop_OError() : GABC_OError(UTgetInterrupt()) {}

	virtual void handleError(const char *msg)
	    { myErrors.append(msg); }
	virtual void handleWarning(const char *msg)
	    { myWarnings.append(msg); }

	UT_StringArray myErrors;
	UT_StringArray myWarnings;
    };

    const GA_PrimitiveGroup *getSubdGroup(bool &subd_all, OBJ_Geometry *geo,
					  const GU_Detail *gdp, fpreal time);
    void refineSop(Hierarchy &assignments,
		   ROP_AlembicPackedTransform packedtransform,
		   exint facesetmode, bool use_instancing, bool shape_nodes,
		   bool displaysop, bool save_hidden, OBJ_Geometry *geo,
		   SOP_Node *sop, fpreal time);

    bool updateFromSop2(OBJ_Geometry *geo, SOP_Node *sop,
		       ROP_AlembicPackedTransform packedtransform,
		       exint facesetmode, bool use_instancing,
		       bool shape_nodes, bool displaysop, bool save_hidden);
    bool updateFromSop(OBJ_Geometry *geo, SOP_Node *sop,
		       ROP_AlembicPackedTransform packedtransform,
		       exint facesetmode, bool use_instancing,
		       bool shape_nodes, bool displaysop, bool save_hidden);
    bool updateFromHierarchy(ROP_AlembicPackedTransform packedtransform,
			     exint facesetmode, bool use_instancing,
			     bool shape_nodes, bool displaysop,
			     bool save_hidden);
    void reportCookErrors(OP_Node *node, fpreal time);

    // temporary storage when exporting to an Alembic archive
    ROP_AbcArchivePtr myArchive;
    UT_UniquePtr<rop_OError> myErrors;
    UT_UniquePtr<ROP_AbcNode> myRoot;
    fpreal myNFrames;
    fpreal myEndTime;
    bool myFullBounds;
    bool myFromSOP;

    // temporary storage when exporting an OBJ hierarchy
    UT_Map<OBJ_Node *, ROP_AbcNodeXform *> myObjAssignments;
    UT_Map<OBJ_Camera *, ROP_AbcNodeCamera *> myCamAssignments;
    UT_Map<OBJ_Geometry *, Hierarchy> myGeoAssignments;
    // keys of myGeoAssignments for deterministic OBJ_Geometry traversal
    UT_Array<OBJ_Geometry *> myGeos;

    // temporary storage when exporting a SOP hierarchy
    UT_UniquePtr<Hierarchy> mySopAssignments;
};

#endif
