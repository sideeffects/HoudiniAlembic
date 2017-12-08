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

#include "ROP_AlembicOut.h"

#include "ROP_AbcNodeInstance.h"

#include <GABC/GABC_IObject.h>
#include <GABC/GABC_PackedImpl.h>
#include <GABC/GABC_Util.h>
#include <GT/GT_Refine.h>
#include <GT/GT_RefineParms.h>
#include <GT/GT_GEODetail.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_PrimCollect.h>
#include <GT/GT_PrimInstance.h>
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
#include <UT/UT_SharedPtr.h>

#if !defined(CUSTOM_ALEMBIC_TOKEN_PREFIX)
    #define CUSTOM_ALEMBIC_TOKEN_PREFIX ""
    #define CUSTOM_ALEMBIC_LABEL_PREFIX ""
#endif

using namespace UT::Literal;

typedef GABC_NAMESPACE::GABC_IObject GABC_IObject;
typedef GABC_NAMESPACE::GABC_PackedImpl GABC_PackedImpl;
typedef GABC_NAMESPACE::GABC_NodeType GABC_NodeType;
typedef GABC_NAMESPACE::GABC_Util GABC_Util;

void
ROP_AlembicOut::rop_RefinedGeoAssignments::setMatrix(const UT_Matrix4D &m)
{
    if(!myMatrix.isEqual(m))
	myMatrix = m;
}

ROP_AbcNodeXform *
ROP_AlembicOut::rop_RefinedGeoAssignments::newXformNode(
    const std::string &name, const ROP_AbcArchivePtr &abc)
{
    std::string temp_name = name;
    myParent->makeCollisionFreeName(temp_name);
    ROP_AbcNodeXform *child = new ROP_AbcNodeXform(temp_name);
    child->setArchive(abc);
    myParent->addChild(child);
    return child;
}

ROP_AbcNodeShape *
ROP_AlembicOut::rop_RefinedGeoAssignments::newShapeNode(
    const std::string &name, const ROP_AbcArchivePtr &abc)
{
    std::string temp_name = name;
    myParent->makeCollisionFreeName(temp_name);
    ROP_AbcNodeShape *child = new ROP_AbcNodeShape(temp_name);
    child->setArchive(abc);
    myParent->addChild(child);
    return child;
}

void
ROP_AlembicOut::rop_RefinedGeoAssignments::refine(
    const GT_PrimitiveHandle &prim,
    PackedTransform packedtransform,
    exint facesetmode,
    bool subd,
    bool use_instancing,
    bool shape_nodes,
    bool save_hidden,
    const std::string &name,
    const ROP_AbcArchivePtr &abc)
{
    UT_ASSERT(!myLocked);

    GT_RefineParms rparms;
    rparms.setCoalesceFragments(false);
    rparms.setFastPolyCompacting(false);
    rparms.setShowUnusedPoints(true);
    rparms.setFaceSetMode(facesetmode);
    rparms.setPolysAsSubdivision(subd);
    rparms.setAlembicHoudiniAttribs(false);
    rparms.setAlembicSkipInvisible(!save_hidden);
    rparms.setSkipHidden(false);

    class rop_AbcRefiner : public GT_Refine
    {
	// helper to identify hidden primitives
	class rop_RefineHelper
	{
	public:
	    rop_RefineHelper(rop_AbcRefiner &refine, const GT_RefineParms &p)
		: myRefiner(refine), myParms(p) {}

	    virtual ~rop_RefineHelper() {}

	    virtual void refine(const GT_PrimitiveHandle &prim)
	    {
		prim->refine(myRefiner, &myParms);
	    }

	    void addPrimitive(const GT_PrimitiveHandle &prim)
	    {
		if(prim->getPrimitiveType() != GT_PRIM_DETAIL)
		{
		    refine(prim);
		    return;
		}

		const GT_GEODetail *detail =
				static_cast<const GT_GEODetail *>(prim.get());
		const GU_ConstDetailHandle &gdh = detail->getGeometry();

		GU_DetailHandleAutoReadLock rlock(gdh);
		const GU_Detail *gdp = rlock.getGdp();
		GA_OffsetList offsets[2];
		const GA_PrimitiveGroup *hidden_group = nullptr;

		if(gdp)
		{
		    hidden_group = gdp->findPrimitiveGroup(GU_HIDDEN_3D_PRIMS_GROUP);
		    if(hidden_group)
		    {
			GA_Range r = gdp->getPrimitiveRange();

			const GA_Range *range = detail->getRange().get();
			if(!range)
			    range = &r;

			for(GA_Iterator it(*range); !it.atEnd(); ++it)
			{
			    GA_Offset offset = *it;
			    int idx = (hidden_group->contains(offset) ? 1 : 0);
			    offsets[idx].append(offset);
			}
		    }
		}

		bool saved_vis = myRefiner.myVisible;
		if(!hidden_group || !offsets[1].entries())
		{
		    // all visible
		    refine(prim);
		}
		else if(!offsets[0].entries())
		{
		    // all hidden
		    if(myRefiner.mySaveHidden)
		    {
			myRefiner.myVisible = false;
			refine(prim);
		    }
		}
		else
		{
		    // some hidden
		    for(int i = 0; i < 2; ++i)
		    {
			if(i)
			{
			    if(!myRefiner.mySaveHidden)
				break;

			    myRefiner.myVisible = false;
			}

			GA_Range range(gdp->getPrimitiveMap(), offsets[i]);
			GT_PrimitiveHandle pr =
					GT_GEODetail::makeDetail(gdh, &range);
			pr->setPrimitiveTransform(prim->getPrimitiveTransform());
			refine(pr);
		    }
		}
		myRefiner.myVisible = saved_vis;
	    }

	protected:
	    rop_AbcRefiner &myRefiner;
	    const GT_RefineParms &myParms;
	};

	// helper to identify instances with hidden primitives
	class rop_RefineInstanceHelper : public rop_RefineHelper
	{
	public:
	    rop_RefineInstanceHelper(rop_AbcRefiner &refine,
				     const GT_RefineParms &p,
				     const GT_PrimInstance &inst)
		: rop_RefineHelper(refine, p), myInstance(inst) {}

	    virtual void refine(const GT_PrimitiveHandle &prim)
	    {
		GT_PrimInstance instance(prim,
					 myInstance.transforms(),
					 myInstance.packedPrimOffsets(),
					 myInstance.uniform(),
					 myInstance.detail(),
					 myInstance.sourceGeometry());
		instance.setPrimitiveTransform(myInstance.getPrimitiveTransform());
		instance.refine(myRefiner, &myParms);
	    }

	private:
	    const GT_PrimInstance &myInstance;
	};

    public:
	rop_AbcRefiner(ROP_AlembicOut::rop_RefinedGeoAssignments *assignments,
			const GT_RefineParms &rparms,
			PackedTransform packedtransform,
			bool subd,
			bool use_instancing,
			bool shape_nodes,
			bool save_hidden,
			const std::string &name,
			const ROP_AbcArchivePtr &abc)
	    : myAssignments(assignments)
	    , myParms(rparms)
	    , myName(name)
	    , myArchive(abc)
	    , myPackedCount(0)
	    , myPackedTransform(packedtransform)
	    , mySubd(subd)
	    , myUseInstancing(use_instancing)
	    , myShapeNodes(shape_nodes)
	    , mySaveHidden(true)
	    , myVisible(true)
	{
	}

	// We need the primitives generated in a consistent order
	virtual bool allowThreading() const { return false; }

	bool processInstance(const GT_PrimitiveHandle &prim)
	{
	    if(prim->getPrimitiveType() != GT_PRIM_INSTANCE)
		return false;

	    const GT_PrimInstance *inst = static_cast<const GT_PrimInstance *>(prim.get());
	    if(!myUseInstancing)
	    {
		inst->flattenInstances(*this, &myParms);
		return true;
	    }

	    GT_PrimitiveHandle geo = inst->geometry();
	    int type = geo->getPrimitiveType();

	    if(GABC_OGTGeometry::isPrimitiveSupported(geo))
	    {
		exint idx;

		auto it = myInstanceCount.find(type);
		if(it == myInstanceCount.end())
		{
		    idx = 0;
		    myInstanceCount.emplace(type, 1);
		}
		else
		    idx = it->second++;

		auto &rop_i = myAssignments->myInstances[std::make_tuple(myName, type, mySubd)];
		while(idx >= rop_i.entries())
		    rop_i.append(rop_Instance());
		auto &rop_inst = rop_i(idx);
		auto &rop_xforms = rop_inst.myXforms;
		GT_TransformArrayHandle xforms = inst->transforms();

		UT_Matrix4D prim_xform;
		prim->getPrimitiveTransform()->getMatrix(prim_xform, 0);
		exint n = xforms->entries();
		for(exint i = 0; i < n; ++i)
		{
		    GT_TransformHandle xform = xforms->get(i);
		    UT_ASSERT(xform->getSegments() == 1);
		    UT_Matrix4D m;
		    xform->getMatrix(m, 0);
		    m *= prim_xform;

		    if(myPackedTransform == ROP_ALEMBIC_PACKEDTRANSFORM_MERGE_WITH_PARENT_TRANSFORM)
		    {
			ROP_AbcNode *parent = myAssignments->myParent;
			if(!parent->getParent())
			    myAssignments->warnRoot(myArchive);
			else if(!static_cast<ROP_AbcNodeXform *>(parent)->setPreMatrix(m))
			{
			    myAssignments->warnChildren(myArchive);
			}

			m.identity();
		    }

		    if(i >= rop_xforms.entries())
		    {
			UT_WorkBuffer buf;
			buf.append(myName.c_str());
			buf.appendSprintf("_instance%" SYS_PRId64, i + 1);
			std::string name = buf.buffer();
			rop_xforms.append(
			    myAssignments->newXformNode(name, myArchive));
		    }

		    ROP_AbcNodeXform *node = rop_xforms(i);
		    node->setData(m, myVisible);

		    if(i)
		    {
			if(rop_inst.myInstances.entries() < i)
			{
			    std::string temp_name = myName;
			    node->makeCollisionFreeName(temp_name);
			    ROP_AbcNodeInstance *child = new ROP_AbcNodeInstance(temp_name, rop_inst.myShape);
			    child->setArchive(myArchive);
			    node->addChild(child);

			    rop_inst.myInstances.append(child);
			}
		    }
		    else if(myShapeNodes)
		    {
			if(!rop_inst.myShape)
			{
			    std::string temp_name = myName;
			    node->makeCollisionFreeName(temp_name);
			    ROP_AbcNodeShape *child = new ROP_AbcNodeShape(temp_name);
			    child->setArchive(myArchive);
			    node->addChild(child);

			    rop_inst.myShape = child;
			}

			rop_inst.myShape->setData(geo, true);
		    }
		}
	    }
	    else if(type == GT_GEO_PACKED || type == GT_PRIM_ALEMBIC_SHAPE)
	    {
		// transform before refining further
		const GT_GEOPrimPacked *packed =
			    static_cast<const GT_GEOPrimPacked *>(geo.get());

		bool saved_vis = myVisible;
		if(type == GT_PRIM_ALEMBIC_SHAPE)
		{
		    const GU_PrimPacked *pr =
			static_cast<const GU_PrimPacked *>(packed->getPrim());
		    const GABC_PackedImpl *impl =
			static_cast<const GABC_PackedImpl *>(pr->implementation());
		    myVisible &= (impl->intrinsicFullVisibility(pr) != 0);
		}

		// use the packed primitive's pivot as the geometry's origin
		UT_Matrix4D prim_xform;
		packed->getPrimitiveTransform()->getMatrix(prim_xform, 0);
		const GU_PrimPacked *p = packed->getPrim();
		UT_Vector3 pivot = p->getPos3(0);
		UT_Matrix4D packed_xform;
		p->getFullTransform4(packed_xform);
		UT_Matrix4D m(1);
		if(packed->transformed())
		    m = packed_xform;
		packed_xform.invert();
		pivot *= packed_xform;
		m.pretranslate(pivot.x(), pivot.y(), pivot.z());
		m *= prim_xform;

		GT_TransformArrayHandle xforms_copy =
			inst->transforms()->preMultiply(new GT_Transform(&m, 1));

		m.invert();
		GT_PrimitiveHandle geo_copy = geo->copyTransformed(new GT_Transform(&m, 1));
		GT_PrimInstance instance(geo_copy, xforms_copy,
					 inst->packedPrimOffsets(),
					 inst->uniform(),
					 inst->detail(),
					 inst->sourceGeometry());
		instance.refineCopyTransformFrom(*prim);

		instance.refine(*this, &myParms);
		myVisible = saved_vis;
	    }
	    else
	    {
		rop_RefineInstanceHelper helper(*this, myParms, *inst);
		helper.addPrimitive(geo);
	    }
	    return true;
	}

	bool processPacked(const GT_PrimitiveHandle &prim)
	{
	    if(myPackedTransform == ROP_ALEMBIC_PACKEDTRANSFORM_DEFORM_GEOMETRY
		|| (prim->getPrimitiveType() != GT_GEO_PACKED
		    && prim->getPrimitiveType() != GT_PRIM_ALEMBIC_SHAPE))
	    {
		return false;
	    }

	    const GT_GEOPrimPacked *packed =
			static_cast<const GT_GEOPrimPacked *>(prim.get());

	    // use the packed primitive's pivot as the geometry's origin
	    UT_Matrix4D prim_xform;
	    packed->getPrimitiveTransform()->getMatrix(prim_xform, 0);
	    const GU_PrimPacked *p = packed->getPrim();
	    UT_Vector3 pivot = p->getPos3(0);
	    UT_Matrix4D packed_xform;
	    p->getFullTransform4(packed_xform);
	    UT_Matrix4D m(1);
	    if(packed->transformed())
		m = packed_xform;
	    packed_xform.invert();
	    pivot *= packed_xform;
	    m.pretranslate(pivot.x(), pivot.y(), pivot.z());
	    prim_xform = m * prim_xform;

	    m.invert();
	    GT_PrimitiveHandle copy = prim->doSoftCopy();
	    copy->setPrimitiveTransform(new GT_Transform(&m, 1));
	    auto assignments = myAssignments;
	    if(myPackedTransform == ROP_ALEMBIC_PACKEDTRANSFORM_TRANSFORM_GEOMETRY)
	    {
		auto &rop_i = myAssignments->myTransformAndShapes[std::make_pair(myName, mySubd)];
		// create transform node
		exint idx = myPackedCount++;
		auto &children = myAssignments->myChildren;
		while(idx >= rop_i.entries())
		    rop_i.append(rop_TransformAndShape());

		auto &xform_and_shape = rop_i(idx);
		if(!xform_and_shape.myXform)
		{
		    UT_WorkBuffer buf;
		    buf.append(myName.c_str());
		    buf.appendSprintf("_packed%" SYS_PRId64, idx + 1);

		    xform_and_shape.myChildIndex = children.entries();
		    xform_and_shape.myXform =
			myAssignments->newXformNode(buf.buffer(), myArchive);

		    children.append(
			rop_RefinedGeoAssignments(xform_and_shape.myXform));
		}

		assignments = &children[xform_and_shape.myChildIndex];
		ROP_AbcNode *parent = assignments->myParent;
		static_cast<ROP_AbcNodeXform *>(parent)->setData(prim_xform, true);
	    }
	    else // if(myPackedTransform == ROP_ALEMBIC_PACKEDTRANSFORM_MERGE_WITH_PARENT_TRANSFORM)
	    {
		ROP_AbcNode *parent = myAssignments->myParent;
		if(!parent->getParent())
		    myAssignments->warnRoot(myArchive);
		else if(!static_cast<ROP_AbcNodeXform *>(parent)->setPreMatrix(prim_xform))
		{
		    myAssignments->warnChildren(myArchive);
		}
	    }

	    auto saved_assignments = myAssignments;
	    myAssignments = assignments;
	    copy->refine(*this, &myParms);
	    myAssignments = saved_assignments;
	    return true;
	}

	virtual void addPrimitive(const GT_PrimitiveHandle &prim)
	{
	    int type = prim->getPrimitiveType();

	    if(GABC_OGTGeometry::isPrimitiveSupported(prim))
	    {
		if(!myShapeNodes)
		    return;

		// get the shape node
		exint idx;

		auto it = myShapeCount.find(type);
		if(it == myShapeCount.end())
		{
		    idx = 0;
		    myShapeCount.emplace(type, 1);
		}
		else
		    idx = it->second++;

		auto &shapes = myAssignments->myShapes[std::make_tuple(myName, type, mySubd)];
		while(idx >= shapes.entries())
		{
		    shapes.append(
			myAssignments->newShapeNode(myName, myArchive));
		}

		shapes[idx]->setData(prim, myVisible);
	    }
	    else if(!processInstance(prim) && !processPacked(prim))
	    {
		if(type == GT_PRIM_ALEMBIC_SHAPE)
		{
		    const GT_GEOPrimPacked *packed =
			static_cast<const GT_GEOPrimPacked *>(prim.get());
		    const GU_PrimPacked *pr =
			static_cast<const GU_PrimPacked *>(packed->getPrim());
		    const GABC_PackedImpl *impl =
			static_cast<const GABC_PackedImpl *>(pr->implementation());

		    bool saved_vis = myVisible;
		    myVisible &= (impl->intrinsicFullVisibility(pr) != 0);
		    prim->refine(*this, &myParms);
		    myVisible = saved_vis;
		}
		else
		{
		    rop_RefineHelper helper(*this, myParms);
		    helper.addPrimitive(prim);
		}
	    }
	}

    private:
	ROP_AlembicOut::rop_RefinedGeoAssignments *myAssignments;
	const GT_RefineParms &myParms;
	const std::string &myName;
	const ROP_AbcArchivePtr &myArchive;
	UT_Map<int, exint> myShapeCount;
	UT_Map<int, exint> myInstanceCount;
	exint myPackedCount;
	PackedTransform myPackedTransform;
	bool mySubd;
	bool myUseInstancing;
	bool myShapeNodes;
	bool mySaveHidden;
	bool myVisible;
    } refiner(this, rparms, packedtransform, subd, use_instancing, shape_nodes,
	      save_hidden, name, abc);

    refiner.addPrimitive(prim);
}

void
ROP_AlembicOut::rop_RefinedGeoAssignments::setUserProperties(
    const UT_String &vals,
    const UT_String &meta,
    bool subd,
    const std::string &name)
{
    UT_ASSERT(!myLocked);
    for(auto &it : myShapes)
    {
	if(std::get<0>(it.first) == name && std::get<2>(it.first) == subd)
	{
	    auto &shapes = it.second;
	    for(exint i = 0; i < shapes.entries(); ++i)
		shapes(i)->setUserProperties(vals, meta);
	}
    }
    for(auto &it : myInstances)
    {
	if(std::get<0>(it.first) == name && std::get<2>(it.first) == subd)
	{
	    auto &insts = it.second;
	    for(exint i = 0; i < insts.entries(); ++i)
	    {
		// could be null with ROP_ALEMBIC_PACKEDTRANSFORM_MERGE_WITH_PARENT_TRANSFORM
		ROP_AbcNodeShape *shape = insts(i).myShape;
		if(shape)
		    shape->setUserProperties(vals, meta);
	    }
	}
    }
    for(exint i = 0; i < myChildren.entries(); ++i)
	myChildren(i).setUserProperties(vals, meta, subd, name);
}

void
ROP_AlembicOut::rop_RefinedGeoAssignments::setLocked(bool locked)
{
    myLocked = locked;

    for(auto &it : myShapes)
    {
	auto &shapes = it.second;
	for(exint i = 0; i < shapes.entries(); ++i)
	    shapes(i)->setLocked(locked);
    }
    for(auto &it : myInstances)
    {
	auto &insts = it.second;
	for(exint i = 0; i < insts.entries(); ++i)
	{
	    auto &inst = insts(i);
	    inst.myShape->setLocked(locked);
	    auto &xforms = inst.myXforms;
	    for(exint j = 0; j < xforms.entries(); ++j)
		xforms(j)->setLocked(locked);
	}
    }
    for(exint i = 0; i < myChildren.entries(); ++i)
    {
	auto &child = myChildren(i);
	static_cast<ROP_AbcNodeXform *>(child.getParent())->setLocked(locked);
	child.setLocked(locked);
    }
}

void
ROP_AlembicOut::rop_RefinedGeoAssignments::warnRoot(
    const ROP_AbcArchivePtr &abc)
{
    if(myWarnedRoot)
	return;

    abc->getOError().warning("Cannot push packed primitive transform to root node.");
    myWarnedRoot = true;
}

void
ROP_AlembicOut::rop_RefinedGeoAssignments::warnChildren(
    const ROP_AbcArchivePtr &abc)
{
    if(myWarnedChildren)
	return;

    UT_Array<const ROP_AbcNode *> ancestors;
    for(const ROP_AbcNode *node = myParent; node; node = node->getParent())
	ancestors.append(node);

    UT_WorkBuffer buf;
    for(exint i = ancestors.entries() - 2; i >= 0; --i)
    {
	buf.append('/');
	buf.append(ancestors(i)->getName());
    }

    abc->getOError().warning("Cannot push multiple packed primitive transforms to %s.", buf.buffer());
    myWarnedChildren = true;
}

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
    PackedTransform packedtransform;
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
	myRoot->clearData();
	myArchive->setCookTime(time, i);

	// update tree
	if(sop)
	{
	    if(!updateFromSop(geo, sop, packedtransform, facesetmode, use_instancing, shape_nodes, displaysop, save_hidden))
		return ROP_ABORT_RENDER;
	}
	else if(!updateFromHierarchy(packedtransform, facesetmode, use_instancing, shape_nodes, displaysop, save_hidden))
	    return ROP_ABORT_RENDER;

	myRoot->update();
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

void
ROP_AlembicOut::exportUserProperties(
    rop_RefinedGeoAssignments &r,
    bool subd,
    const GU_Detail &gdp,
    const GA_Range &range,
    const std::string &name,
    const GA_ROHandleS &vals,
    const GA_ROHandleS &meta)
{
    GA_Iterator it(range);
    if(it.atEnd())
	return;

    GA_Offset offset = *it;

    UT_String v;
    UT_String m;
    if(vals.isValid() && meta.isValid())
    {
	v = vals.get(offset);
	m = meta.get(offset);
    }

    if(!v.isstring() && !m.isstring())
    {
	const GA_Primitive *prim = gdp.getPrimitive(offset);
	if(GU_PrimPacked::isPackedPrimitive(*prim))
	{
	    const GU_PrimPacked *packed = static_cast<const GU_PrimPacked *>(prim);

	    auto &&alembic_def = GUgetFactory().lookupDefinition("AlembicRef"_sh);
	    if(alembic_def && packed->getTypeId() == alembic_def->getId())
	    {
		const GABC_PackedImpl *impl = static_cast<const GABC_PackedImpl *>(packed->implementation());
		const GABC_IObject &obj = impl->object();

		UT_WorkBuffer vals_buffer;
		UT_WorkBuffer meta_buffer;
		ropGetUserProperties(vals_buffer, meta_buffer, obj, impl->frame());

		v.harden(vals_buffer.buffer());
		m.harden(meta_buffer.buffer());
	    }
	}
    }

    if(v.isstring() && m.isstring())
	r.setUserProperties(v, m, subd, name);
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
    rop_RefinedGeoAssignments &assignments,
    PackedTransform packedtransform,
    exint facesetmode,
    bool use_instancing,
    bool shape_nodes,
    bool displaysop,
    bool save_hidden,
    OBJ_Geometry *geo,
    SOP_Node *sop,
    fpreal time)
{
    int locked = 0;
    OP_Node *creator = sop->getCreator();
    if(creator)
    {
	creator->evalParameterOrProperty(
			GABC_Util::theLockGeometryParameter, 0, 0, locked);
    }

    if(assignments.isLocked())
    {
	if(locked)
	    return;

	assignments.setLocked(false);
    }

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

    bool exported = false;
    for(int idx = 0; idx < 2; ++idx)
    {
	for(auto &it : partitions[idx])
	{
	    GT_PrimitiveHandle prim;
	    GA_Range range(gdp->getPrimitiveMap(), it.second);
	    if(it.second.size())
		prim = GT_GEODetail::makeDetail(gdh, &range);
	    if(!func && !idx)
	    {
		GA_OffsetList offsets;
		if(gdp->findUnusedPoints(&offsets))
		{
		    GT_PrimCollect *collect = new GT_PrimCollect();
		    if(prim)
			collect->appendPrimitive(prim);

		    GA_Range ptrange(gdp->getPointMap(), offsets);
		    collect->appendPrimitive(GT_GEODetail::makePointMesh(gdh, &ptrange));
		    prim = collect;
		}
	    }
	    if(prim)
	    {
		exported = true;

		bool subd = (idx == 1);
		assignments.refine(prim, packedtransform, facesetmode, subd,
				   use_instancing, shape_nodes, save_hidden,
				   it.first, myArchive);
		exportUserProperties(assignments, subd, *gdp, range, it.first,
				     up_vals, up_meta);
	    }
	}
    }

    if(locked && exported)
	assignments.setLocked(true);
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
ROP_AlembicOut::updateFromSop(
    OBJ_Geometry *geo,
    SOP_Node *sop,
    PackedTransform packedtransform,
    exint facesetmode,
    bool use_instancing,
    bool shape_nodes,
    bool displaysop,
    bool save_hidden)
{
    fpreal time = myArchive->getCookTime();
    OP_Context context(time);

    if(!BUILD_FROM_PATH(time))
    {
	if(!mySopAssignments)
	{
	    // insert a transform node
	    ROP_AbcNode *root = myRoot.get();
	    OBJ_Node *obj = CAST_OBJNODE(sop->getCreator());
	    if(obj)
	    {
		const UT_String &name = obj->getName();
		std::string s = name.c_str();
		ROP_AbcNodeXform *child = new ROP_AbcNodeXform(s);
		child->setArchive(myArchive);
		root->addChild(child);
		root = child;
	    }

	    mySopAssignments.reset(new rop_SopAssignments(root));
	}

	ROP_AbcNode *node = mySopAssignments->getParent();
	if(node != myRoot.get())
	{
	    UT_Matrix4D m(1);
	    OBJ_Node *obj = CAST_OBJNODE(sop->getCreator());
	    if(obj)
	    {
		if(obj->isSubNetwork(false))
		    static_cast<OBJ_SubNet *>(obj)->getSubnetTransform(context, m);
		else
		    obj->getLocalTransform(context, m); 
	    }

	    // make transform visible
	    static_cast<ROP_AbcNodeXform *>(node)->setData(m, true);
	}
	refineSop(*mySopAssignments, packedtransform, facesetmode,
		  use_instancing, shape_nodes, displaysop, save_hidden,
		  geo, sop, time);
	return true;
    }

    if(!mySopAssignments)
	mySopAssignments.reset(new rop_SopAssignments(myRoot.get()));

    GU_ConstDetailHandle gdh(ropGetCookedGeoHandle(sop, context, displaysop));
    GU_DetailHandleAutoReadLock rlock(gdh);
    const GU_Detail *gdp = rlock.getGdp();
    if(!gdp)
    {
	reportCookErrors(sop, time);
	return false;
    }

    UT_String path_attrib;
    PATH_ATTRIBUTE(path_attrib, time);
    myArchive->getOOptions().setPathAttribute(path_attrib);

    GA_ROHandleS path_handle(gdp, GA_ATTRIB_PRIMITIVE, path_attrib);

    // identify subd
    bool subd_all;
    const GA_PrimitiveGroup *grp = getSubdGroup(subd_all, geo, gdp, time);

    auto &&alembic_def = GUgetFactory().lookupDefinition("AlembicRef"_sh);
    UT_SortedMap<std::string, const GU_PrimPacked *> abc_prims;
    UT_SortedMap<std::string, GA_OffsetList> offsets[2];

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
		while(*s && *s == '/')
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
		const GABC_PackedImpl *impl = static_cast<const GABC_PackedImpl *>(packed->implementation());
		if(!ropIsShape(impl->nodeType()))
		    continue;
	    }
	}

	if(subd_all || (grp && grp->contains(offset)))
	    offsets[1][p].append(offset);
	else
	    offsets[0][p].append(offset);
    }

    GA_ROHandleS up_vals(gdp, GA_ATTRIB_PRIMITIVE,
			 GABC_Util::theUserPropsValsAttrib);
    GA_ROHandleS up_meta(gdp, GA_ATTRIB_PRIMITIVE,
			 GABC_Util::theUserPropsMetaAttrib);
    UT_WorkBuffer vals_buffer;
    UT_WorkBuffer meta_buffer;

    // handle transforms
    UT_Array<ROP_AbcNodeXform *> ancestors;
    UT_Array<GABC_IObject> objs;

    UT_Map<ROP_AbcNodeXform *, rop_RefinedGeoAssignments *> visited;
    UT_Map<ROP_AbcNodeXform *, UT_Matrix4D> xforms;
    UT_Map<ROP_AbcNodeXform *, GABC_IObject> xform_obj;
    UT_Map<ROP_AbcNodeXform *, const GU_PrimPacked *> xform_packed;
    for(auto &it : abc_prims)
    {
	// we only use the first transform primitive
	const GU_PrimPacked *packed = it.second;
	const GABC_PackedImpl *impl = static_cast<const GABC_PackedImpl *>(packed->implementation());

	const char *s = it.first.c_str();
	bool do_implicit = (impl->objectPath() == s);

	rop_SopAssignments *assignments = mySopAssignments.get();
	ancestors.clear();
	while(*s)
	{
	    // skip '/'
	    UT_ASSERT(*s == '/');
	    ++s;

	    const char *start = s;
	    while(*s && *s != '/')
		++s;

	    buf.strncpy(start, s - start);
	    if(*s)
	    {
		std::string name = buf.buffer();
		// create an xform node if needed
		rop_SopAssignments *a = assignments->getChild(name);
		if(!a)
		{
		    ROP_AbcNodeXform *xform =
			    assignments->newXformNode(name, myArchive);
		    assignments->addChild(name, rop_SopAssignments(xform));
		    a = assignments->getChild(name);
		}
		assignments = a;
		ROP_AbcNodeXform *node =
			static_cast<ROP_AbcNodeXform *>(assignments->getParent());
		// make xform node visible
		// the proper transform will be set later
		node->setData(UT_Matrix4D(1), true);
		visited.emplace(node, assignments);
		ancestors.append(node);
	    }
	}

	exint n = ancestors.entries();
	if(do_implicit)
	{
	    GABC_IObject obj = impl->object().getParent();

	    // fetch parent GABC_IObjects
	    objs.clear();
	    exint i = n - 1;
	    for(; i >= 0 && xforms.find(ancestors(i)) == xforms.end(); --i)
	    {
		objs.append(obj);
		obj = obj.getParent();
	    }
	    ++i;

	    UT_Matrix4D m;
	    for(exint j = objs.entries() - 1; i < n; ++i, --j)
	    {
		ROP_AbcNodeXform *node = ancestors(i);
		GEO_AnimationType atype;
		const GABC_IObject &iobj = objs(j);
		xform_obj.emplace(node, iobj);

		iobj.getWorldTransform(m, time, atype);
		xforms.emplace(node, m);

		UT_String upv, upm;
		if(ropGetUserProperties(vals_buffer, meta_buffer, iobj, time))
		{
		    upv = vals_buffer.buffer();
		    upm = meta_buffer.buffer();
		}

		node->setUserProperties(upv, upm);
	    }
	}

	if(impl->nodeType() == GABC_NodeType::GABC_XFORM)
	{
	    std::string name = buf.buffer();
	    // create an xform node if needed
	    rop_SopAssignments *a = assignments->getChild(name);
	    if(!a)
	    {
		ROP_AbcNodeXform *xform =
			assignments->newXformNode(name, myArchive);
		assignments->addChild(name, rop_SopAssignments(xform));
		a = assignments->getChild(name);
	    }
	    ROP_AbcNodeXform *node =
			static_cast<ROP_AbcNodeXform *>(a->getParent());
	    visited.emplace(node, a);
	    xform_packed.emplace(node, packed);

	    UT_Matrix4D m;
	    packed->getFullTransform4(m);
	    xforms.emplace(node, m);

	    UT_String upv, upm;
	    if(up_vals.isValid() && up_meta.isValid())
	    {
		GA_Offset offset = packed->getMapOffset();
		upv = up_vals.get(offset);
		upm = up_meta.get(offset);
	    }

	    if(!upv.isstring() && !upm.isstring())
	    {
		GABC_IObject iobj = impl->object();
		if(ropGetUserProperties(vals_buffer, meta_buffer, iobj, time))
		{
		    upv = vals_buffer.buffer();
		    upm = meta_buffer.buffer();
		}
	    }

	    node->setUserProperties(upv, upm);
	}
	else if(!do_implicit && n > 0 && ropIsShape(impl->nodeType()))
	{
	    UT_Matrix4D m;
	    packed->getFullTransform4(m);
	    xforms.emplace(ancestors(n - 1), m);
	}
    }

    for(auto &it : xforms)
    {
	ROP_AbcNodeXform *node = it.first;
	UT_Matrix4D m = it.second;

	ROP_AbcNode *parent = node->getParent();
	if(parent->getParent())
	{
	    auto it2 = xforms.find(static_cast<ROP_AbcNodeXform *>(parent));
	    if(it2 != xforms.end())
	    {
		UT_Matrix4D m2 = it2->second;
		m2.invert();
		m *= m2;
	    }
	}

	// make this robust against numerical issues
	auto a = visited.find(node)->second;
	a->setMatrix(m);
	node->setData(a->getMatrix(), true);
    }

    for(int idx = 0; idx < 2; ++idx)
    {
	bool subd = (idx == 1);
	for(auto &it : offsets[idx])
	{
	    const char *s = it.first.c_str();

	    rop_SopAssignments *assignments = mySopAssignments.get();
	    buf.clear();
	    while(*s)
	    {
		// skip '/'
		UT_ASSERT(*s == '/');
		++s;

		const char *start = s;
		while(*s && *s != '/')
		    ++s;

		buf.strncpy(start, s - start);
		if(*s)
		{
		    std::string name = buf.buffer();
		    // create an xform node if needed
		    rop_SopAssignments *a = assignments->getChild(name);
		    if(!a)
		    {
			ROP_AbcNodeXform *xform =
				assignments->newXformNode(name, myArchive);
			assignments->addChild(name, rop_SopAssignments(xform));
			a = assignments->getChild(name);
		    }
		    assignments = a;
		    ROP_AbcNodeXform *node = static_cast<ROP_AbcNodeXform *>(assignments->getParent());
		    if(visited.find(node) == visited.end())
		    {
			// make this robust against numerical issues
			visited.emplace(node, assignments);
			assignments->setMatrix(UT_Matrix4D(1));
			node->setData(assignments->getMatrix(), true);
		    }
		}
	    }

	    GA_Range r(gdp->getPrimitiveMap(), it.second);

	    UT_Matrix4D m(1);
	    std::string name = buf.buffer();
	    GT_PrimitiveHandle prim = GT_GEODetail::makeDetail(gdh, &r);
	    ancestors.clear();
	    for(ROP_AbcNode *parent = assignments->getParent();
		parent->getParent();
		parent = parent->getParent())
	    {
		ROP_AbcNodeXform *node = static_cast<ROP_AbcNodeXform *>(parent);
		auto it2 = xforms.find(node);
		if(it2 != xforms.end())
		{
		    // update the other ancestor transforms so we don't need to
		    // traverse up again.
		    UT_Matrix4D m2 = it2->second;
		    for(exint i = ancestors.entries() - 1; i >= 0; --i)
			xforms.emplace(ancestors(i), m2);
		    m = m2 * m;
		    break;
		}
		ancestors.append(node);
	    }
	    if(!m.isIdentity())
	    {
		m.invert();
		prim = prim->copyTransformed(new GT_Transform(&m, 1));
	    }

	    assignments->refine(prim, packedtransform, facesetmode, subd,
				use_instancing, shape_nodes, save_hidden,
				name, myArchive);
	    exportUserProperties(*assignments, subd, *gdp, r, name,
				 up_vals, up_meta);
	}
    }

    return true;
}

bool
ROP_AlembicOut::updateFromHierarchy(
    PackedTransform packedtransform,
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
		    myGeoAssignments.emplace(geo, rop_RefinedGeoAssignments(parent));
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

    OP_Context context(time);
    for(auto &it : myObjAssignments)
    {
	auto obj = it.first;

	UT_Matrix4D m;
	if(obj->isSubNetwork(false))
	    static_cast<OBJ_SubNet *>(obj)->getSubnetTransform(context, m);
	else
	    obj->getLocalTransform(context, m); 

	UT_String user_props;
	UT_String user_props_meta;
	bool has_vals = obj->hasParm("userProps");
	bool has_meta = obj->hasParm("userPropsMeta");
	if(has_vals && has_meta)
	{
	    obj->evalString(user_props, "userProps", 0, time);
	    obj->evalString(user_props_meta, "userPropsMeta", 0, time);
	    it.second->setUserProperties(user_props, user_props_meta);
	}

	it.second->setData(m, obj->getObjectDisplay(time));
    }

    const CH_Manager *chman = OPgetDirector()->getChannelManager();
    for(auto &it : myCamAssignments)
    {
	auto cam = it.first;

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

    // add data sources for the geometry
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
		refineSop(it->second, packedtransform, facesetmode,
			  use_instancing, shape_nodes, displaysop, save_hidden,
			  geo, sop, time);
	    }
	    else
	    {
		// all entries in myGeos should be in myGeoAssignments
		UT_ASSERT(0);
	    }
	}
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
