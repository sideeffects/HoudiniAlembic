/*
 * Copyright (c) 2017
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

class ROP_AlembicOut : public ROP_Node
{
public:
    static OP_Node *myConstructor(OP_Network *net, const char *name, OP_Operator *op);
    virtual bool updateParmsFlags();
    virtual void getDescriptiveParmName(UT_String &str) const
			{ str = "filename"; }
    /// We need to hint to the merge ROP that we can't be called one frame at a
    /// time.
    virtual void resolveObsoleteParms(PRM_ParmList *obsolete_parms);
    virtual void buildRenderDependencies(const ROP_RenderDepParms &p);

    SOP_Node *getSopNode(fpreal time) const;

protected:
    ROP_AlembicOut(OP_Network *net, const char *name, OP_Operator *entry);
    virtual ~ROP_AlembicOut();

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
    enum PackedMode
    {
	ROP_ALEMBIC_PACKEDMODE_TRANSFORMED_SHAPE,
	ROP_ALEMBIC_PACKEDMODE_TRANSFORM_AND_SHAPE,
	ROP_ALEMBIC_PACKEDMODE_TRANSFORMED_PARENT,
	ROP_ALEMBIC_PACKEDMODE_TRANSFORMED_PARENT_AND_SHAPE
    };

    enum PackedTransform
    {
	ROP_ALEMBIC_PACKEDTRANSFORM_DEFORM_GEOMETRY,
	ROP_ALEMBIC_PACKEDTRANSFORM_TRANSFORM_GEOMETRY,
	ROP_ALEMBIC_PACKEDTRANSFORM_MERGE_WITH_PARENT_TRANSFORM
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

    class rop_RefinedGeoAssignments
    {
	class rop_Instance
	{
	public:
	    rop_Instance() : myShape(nullptr) {}

	    ROP_AbcNodeShape *myShape;
	    UT_Array<ROP_AbcNodeXform *> myXforms;
	    UT_Array<ROP_AbcNodeInstance *> myInstances;
	};

	class rop_TransformAndShape
	{
	public:
	    rop_TransformAndShape() : myXform(nullptr), myChildIndex(-1) {}

	    ROP_AbcNodeXform *myXform;
	    exint myChildIndex;
	};

    public:
	rop_RefinedGeoAssignments(ROP_AbcNode *parent)
	    : myParent(parent), myMatrix(1), myLocked(false),
	      myWarnedRoot(false), myWarnedChildren(false) {}

	ROP_AbcNode *getParent() const { return myParent; }

	void refine(const GT_PrimitiveHandle &prim,
		    PackedTransform packedtransform, exint facesetmode,
		    bool subd, bool use_instancing, bool shape_nodes,
		    const std::string &name, const ROP_AbcArchivePtr &abc);

	void setUserProperties(const UT_String &vals, const UT_String &meta,
			       bool subd, const std::string &name);

	ROP_AbcNodeXform *newXformNode(const std::string &name,
				       const ROP_AbcArchivePtr &abc);

	void setMatrix(const UT_Matrix4D &m);
	const UT_Matrix4D &getMatrix() const { return myMatrix; }

	void setLocked(bool locked);
	bool isLocked() const { return myLocked; }

	void warnRoot(const ROP_AbcArchivePtr &abc);
	void warnChildren(const ROP_AbcArchivePtr &abc);

    private:
	ROP_AbcNodeShape *newShapeNode(const std::string &name,
				       const ROP_AbcArchivePtr &abc);

	ROP_AbcNode *myParent;
	UT_Matrix4D myMatrix;
	// use sorted maps so name collisions are resolved deterministicly
	UT_SortedMap<std::tuple<std::string, int, bool>, UT_Array<ROP_AbcNodeShape *> > myShapes;
	UT_SortedMap<std::tuple<std::string, int, bool>, UT_Array<rop_Instance> > myInstances;
	UT_SortedMap<std::pair<std::string, bool>, UT_Array<rop_TransformAndShape> > myTransformAndShapes;
	UT_Array<rop_RefinedGeoAssignments> myChildren;
	bool myLocked;
	bool myWarnedRoot;
	bool myWarnedChildren;
    };

    class rop_SopAssignments : public rop_RefinedGeoAssignments
    {
    public:
	rop_SopAssignments(ROP_AbcNode *parent) : rop_RefinedGeoAssignments(parent) {}

	void addChild(const std::string &name, const rop_SopAssignments &refinement)
	    { myChildren.emplace(name, refinement); }

	rop_SopAssignments *getChild(const std::string &name)
	    {
		auto it = myChildren.find(name);
		return it != myChildren.end() ? &it->second : nullptr;
	    }

    private:
	UT_Map<std::string, rop_SopAssignments> myChildren;
    };

    static void exportUserProperties(
			rop_RefinedGeoAssignments &r, bool subd,
			const GU_Detail &gdp, const GA_Range &range,
			const std::string &name,
			const GA_ROHandleS &vals, const GA_ROHandleS &meta);
    const GA_PrimitiveGroup *getSubdGroup(bool &subd_all, OBJ_Geometry *geo,
					  const GU_Detail *gdp, fpreal time);
    void refineSop(rop_RefinedGeoAssignments &refinement,
		   PackedTransform packedtransform, exint facesetmode,
		   bool use_instancing, bool shape_nodes, OBJ_Geometry *geo,
		   SOP_Node *sop, fpreal time);

    bool updateFromSop(OBJ_Geometry *geo, SOP_Node *sop,
		       PackedTransform packedtransform, exint facesetmode,
		       bool use_instancing, bool shape_nodes);
    bool updateFromHierarchy(PackedTransform packedtransform, exint facesetmode,
			     bool use_instancing, bool shape_nodes);

    // temporary storage when exporting to an Alembic archive
    ROP_AbcArchivePtr myArchive;
    UT_UniquePtr<rop_OError> myErrors;
    UT_UniquePtr<ROP_AbcNode> myRoot;
    fpreal myNFrames;
    fpreal myEndTime;
    bool myFullBounds;

    // temporary storage when exporting an OBJ hierarchy
    UT_Map<OBJ_Node *, ROP_AbcNodeXform *> myObjAssignments;
    UT_Map<OBJ_Camera *, ROP_AbcNodeCamera *> myCamAssignments;
    UT_Map<OBJ_Geometry *, rop_RefinedGeoAssignments> myGeoAssignments;
    // keys of myGeoAssignments for deterministic OBJ_Geometry traversal
    UT_Array<OBJ_Geometry *> myGeos;

    // temporary storage when exporting a SOP hierarchy
    UT_UniquePtr<rop_SopAssignments> mySopAssignments;
};

#endif
