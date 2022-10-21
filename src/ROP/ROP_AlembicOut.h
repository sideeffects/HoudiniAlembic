/*
 * Copyright (c) 2022
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
#include "ROP_AbcHierarchy.h"
#include "ROP_AbcNodeCamera.h"
#include "ROP_AbcNodeInstance.h"
#include "ROP_AbcNodeShape.h"
#include "ROP_AbcNodeXform.h"

#include <GABC/GABC_LayerOptions.h>
#include <GA/GA_Handle.h>
#include <GA/GA_Range.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_UniquePtr.h>

class GA_PrimitiveGroup;
class OBJ_Camera;
class OBJ_Geometry;
class OBJ_Node;

class ROP_AlembicOut : public ROP_Node
{
public:
    static OP_Node *myConstructor(OP_Network *net, const char *name, OP_Operator *op);
    bool updateParmsFlags() override;
    /// We need to hint to the merge ROP that we can't be called one frame at a
    /// time.
    void resolveObsoleteParms(PRM_ParmList *obsolete_parms) override;
    void buildRenderDependencies(const ROP_RenderDepParms &p) override;

    SOP_Node *getSopNode(fpreal time);
    void getOutputNodesArray(UT_StringArray &array, fpreal time);
    void getAttrNamesByPattern(UT_SortedStringSet &names,
	const UT_String &pattern, fpreal time);
    void getFaceSetNamesByPattern(UT_SortedStringSet &names,
	const UT_String &pattern, fpreal time);
    void getUserPropNamesByPattern(UT_SortedStringSet &names,
	const UT_String &pattern, fpreal time);

protected:
    ROP_AlembicOut(OP_Network *net, const char *name, OP_Operator *entry);
    ~ROP_AlembicOut() override;

    void getDescriptiveParmName(UT_String &str) const override
			{ str = "filename"; }
    int startRender(int nframes, fpreal s, fpreal e) override;
    ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt *boss) override;
    ROP_RENDER_CODE endRender() override;

    void FILENAME(UT_String &str, fpreal time)
		{ getOutputOverrideEx(str, time, "filename", "mkpath"); }
    void FORMAT(UT_String &str, fpreal time) const
		{ evalString(str, "format", 0, time); }
    bool RENDER_FULL_RANGE() const
		{ return evalInt("render_full_range", 0, 0.0) != 0; }
    bool INITSIM(fpreal time) const
		{ return evalInt("initsim", 0, time) != 0; }
    bool USE_SOP_PATH(fpreal time) const
		{ return CAST_SOPNODE(getInput(0))
				|| evalInt("use_sop_path", 0, time) != 0; }
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
    bool OUTPUT_INDEXED_ARRAYS(fpreal time) const
		{ return evalInt("outputindexedarrays", 0, time) != 0; }
    void PRIM_TO_DETAIL_PATTERN(UT_String &str, fpreal time) const
		{ evalString(str, "prim_to_detail_pattern", 0, time); }
    bool FORCE_PRIM_TO_DETAIL(fpreal time) const
		{ return evalInt("force_prim_to_detail", 0, time); }
    void FACESET_MODE(UT_String &str, fpreal time) const
		{ evalString(str, "facesets", 0, time); }

    bool USE_LAYERING(fpreal time) const
		{ return evalInt("uselayering", 0, time) != 0; }
    bool FULL_ANCESTOR(fpreal time) const
		{ return evalInt("fullancestor", 0, time) != 0; }
    int  NUM_NODES(fpreal time) const
		{ return evalInt("numnodes", 0, time); }
    void NODE_PATH(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("nodepath#", &idx, str, 0, time); }
    void NODE_RULE(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("noderule#", &idx, str, 0, time); }
    int  NUM_VIZS(fpreal time) const
		{ return evalInt("numvizs", 0, time); }
    void VIZ_PATH(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("vizpath#", &idx, str, 0, time); }
    void VIZ_RULE(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("vizrule#", &idx, str, 0, time); }
    int  NUM_ATTRIBUTES(fpreal time) const
		{ return evalInt("numattrs", 0, time); }
    void ATTRIBUTE_PATH(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("attrpath#", &idx, str, 0, time); }
    void ATTRIBUTE_PATTERN(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("attrpattern#", &idx, str, 0, time); }
    void ATTRIBUTE_RULE(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("attrrule#", &idx, str, 0, time); }
    int  NUM_FACE_SETS(fpreal time) const
		{ return evalInt("numfacesets", 0, time); }
    void FACE_SET_PATH(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("facesetpath#", &idx, str, 0, time); }
    void FACE_SET_PATTERN(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("facesetpattern#", &idx, str, 0, time); }
    void FACE_SET_RULE(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("facesetrule#", &idx, str, 0, time); }
    int  NUM_USER_PROPS(fpreal time) const
		{ return evalInt("numuserprops", 0, time); }
    void USER_PROP_PATH(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("userproppath#", &idx, str, 0, time); }
    void USER_PROP_PATTERN(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("userproppattern#", &idx, str, 0, time); }
    void USER_PROP_RULE(UT_String &str, int idx, fpreal time) const
		{ evalStringInst("userproprule#", &idx, str, 0, time); }

    bool MOTIONBLUR(fpreal time) const
		{ return evalInt("motionBlur", 0, time) != 0; }
    int SAMPLES(fpreal time) const
		{ return evalInt("samples", 0, time); }
    fpreal SHUTTEROPEN(fpreal time) const
		{ return evalFloat("shutter", 0, time); }
    fpreal SHUTTERCLOSE(fpreal time) const
		{ return evalFloat("shutter", 1, time); }

private:
    class rop_OError : public GABC_OError
    {
    public:
	rop_OError() : GABC_OError(UTgetInterrupt()) {}

	void handleError(const char *msg) override
	    { myErrors.append(msg); }
	void handleWarning(const char *msg) override
	    { myWarnings.append(msg); }

	UT_StringArray myErrors;
	UT_StringArray myWarnings;
    };

    const GA_PrimitiveGroup *getSubdGroup(bool &subd_all, OBJ_Geometry *geo,
					  const GU_Detail *gdp, fpreal time);

    void partitionPrims(ROP_AbcHierarchy &assignments,
			OBJ_Geometry *geo, SOP_Node *sop,
			GU_ConstDetailHandle &gdh,
			const GU_Detail *gdp, GA_ROHandleS &path_handle,
			ROP_AbcHierarchySample &rootSpl,
			ROP_AbcHierarchySample *curSpl,
			ROP_AlembicPackedTransform packedtransform,
			exint facesetmode, bool use_instancing,
			bool shape_nodes, bool displaysop,
			bool save_hidden, fpreal time);

    bool updateFromSop(SOP_Node *sop,
		       ROP_AlembicPackedTransform packedtransform,
		       exint facesetmode, bool use_instancing,
		       bool shape_nodes, bool displaysop,
		       bool save_hidden, fpreal time);

    void refineSop(ROP_AbcHierarchy &assignments,
		   ROP_AlembicPackedTransform packedtransform,
		   exint facesetmode, bool use_instancing, bool shape_nodes,
		   bool displaysop, bool save_hidden, OBJ_Geometry *geo,
		   SOP_Node *sop, fpreal time);
    bool updateFromHierarchy(ROP_AlembicPackedTransform packedtransform,
			     exint facesetmode, bool use_instancing,
			     bool shape_nodes, bool displaysop,
			     bool save_hidden, fpreal time);

    void reportCookErrors(OP_Node *node, fpreal time);

    // modify the myRoot by taking the layerOptions then save the file.
    bool buildAlembicTree(fpreal time);
    void clearAlembicTree();
    void setLayerOptionsAndSave(fpreal time);

    // temporary storage when exporting to an Alembic archive
    UT_UniquePtr<ROP_AbcArchive> myArchive;
    UT_UniquePtr<rop_OError> myErrors;
    UT_UniquePtr<ROP_AbcNode> myRoot;
    fpreal myNFrames;
    fpreal myEndTime;
    bool myFullBounds;
    bool myFromSOP;

    // temporary storage when exporting an OBJ hierarchy
    UT_Map<OBJ_Node *, ROP_AbcNodeXform *> myObjAssignments;
    UT_Map<OBJ_Camera *, ROP_AbcNodeCamera *> myCamAssignments;
    UT_Map<OBJ_Geometry *, ROP_AbcHierarchy> myGeoAssignments;
    UT_Map<ROP_AbcNodeXform *, bool> myObjLocks;
    UT_Map<ROP_AbcHierarchy *, bool> myGeoLocks;
    // keys of myGeoAssignments for deterministic OBJ_Geometry traversal
    UT_Array<OBJ_Geometry *> myGeos;

    // temporary storage when exporting a SOP hierarchy
    UT_UniquePtr<ROP_AbcHierarchy> mySopAssignments;
};

#endif
