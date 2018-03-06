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

#include "ROP_AbcRefiner.h"

#include <GABC/GABC_OGTGeometry.h>
#include <GABC/GABC_PackedImpl.h>
#include <GT/GT_GEODetail.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_PrimInstance.h>
#include <GU/GU_PackedDisk.h>
#include <GU/GU_PackedDiskSequence.h>
#include <GU/GU_PackedFragment.h>
#include <GU/GU_PackedGeometry.h>

using namespace UT::Literal;

typedef GABC_NAMESPACE::GABC_OGTGeometry GABC_OGTGeometry;
typedef GABC_NAMESPACE::GABC_PackedImpl GABC_PackedImpl;

namespace
{

static bool
ropGetInstanceKey(std::string &key, bool subd, const GU_PrimPacked *packed)
{
    GA_PrimitiveTypeId type_id = packed->getTypeId();
    if(type_id == GU_PackedFragment::typeId())
    {
	auto impl = static_cast<const GU_PackedFragment *>(packed->implementation());
	auto attrib = impl->attribute();
	auto name = impl->name();
	UT_WorkBuffer buf;
	buf.sprintf("f:%d:%d:%d:%s%s", int(subd), int(impl->geometryId()),
		    int(attrib.length()), attrib.c_str(),
		    name.c_str());
	key = buf.buffer();
	return true;
    }

    if(type_id == GU_PackedGeometry::typeId())
    {
	auto impl = static_cast<const GU_PackedGeometry *>(packed->implementation());
	UT_WorkBuffer buf;
	buf.sprintf("g:%d:%d", int(subd), int(impl->geometryId()));
	key = buf.buffer();
	return true;
    }

    if(type_id == GU_PackedDisk::typeId())
    {
	auto impl = static_cast<const GU_PackedDisk *>(packed->implementation());
	UT_WorkBuffer buf;
	buf.sprintf("d:%d:%s", int(subd), impl->filename().c_str());
	key = buf.buffer();
	return true;
    }

    if(type_id == GU_PackedDiskSequence::typeId())
    {
	auto impl = static_cast<const GU_PackedDiskSequence *>(packed->implementation());
	UT_WorkBuffer buf;
	buf.sprintf("ds:%d:%d:%g:",
		    int(subd),
		    int(impl->wrapMode()),
		    impl->index());
	UT_StringArray files;
	impl->filenames(files);
	for(exint i = 0; i < files.entries(); ++i)
	{
	    buf.appendSprintf("%d:%ds%s",
			      int(subd),
			      int(files(i).length()),
			      files(i).c_str());
	}
	key = buf.buffer();
	return true;
    }

    auto &&alembic_def = GUgetFactory().lookupDefinition("AlembicRef"_sh);
    if(alembic_def && type_id == alembic_def->getId())
    {
	auto impl = static_cast<const GABC_PackedImpl *>(packed->implementation());
	auto filenames = impl->intrinsicFilenamesJSON(packed);
	auto sourcepath = impl->intrinsicSourcePath(packed);
	auto point_attr = impl->intrinsicPoint(packed);
	auto vertex_attr = impl->intrinsicVertex(packed);
	auto prim_attr = impl->intrinsicPrimitive(packed);
	auto detail_attr = impl->intrinsicDetail(packed);
	auto faceset_attr = impl->intrinsicFaceSet(packed);

	UT_WorkBuffer buf;
	buf.sprintf("a:%d:%s:%ds%s%g:%ds%s%ds%s%ds%s%ds%s%s",
		int(subd),
		filenames.c_str(),
		int(sourcepath.length()), sourcepath.c_str(),
		impl->frame(),
		int(point_attr.length()), point_attr.c_str(),
		int(vertex_attr.length()), vertex_attr.c_str(),
		int(prim_attr.length()), prim_attr.c_str(),
		int(detail_attr.length()), detail_attr.c_str(),
		faceset_attr.c_str());

	key = buf.buffer();
	return true;
    }

    return false;
}

// helper to identify hidden primitives
class rop_RefineHelper
{
public:
    rop_RefineHelper(ROP_AbcRefiner &refine, const GT_RefineParms &p)
	: myRefiner(refine), myParms(p) {}

    virtual ~rop_RefineHelper() {}

    virtual void refine(const GT_PrimitiveHandle &prim)
    {
	prim->refine(myRefiner, &myParms);
    }

    void addPrimitive(const GT_PrimitiveHandle &prim);

protected:
    ROP_AbcRefiner &myRefiner;
    const GT_RefineParms &myParms;
};

// helper to identify instances with hidden primitives
class rop_RefineInstanceHelper : public rop_RefineHelper
{
public:
    rop_RefineInstanceHelper(ROP_AbcRefiner &refine,
			     const GT_RefineParms &p,
			     const GT_PrimInstance &inst)
	: rop_RefineHelper(refine, p), myInstance(inst) {}

    virtual void refine(const GT_PrimitiveHandle &prim);

private:
    const GT_PrimInstance &myInstance;
};


void
rop_RefineHelper::addPrimitive(const GT_PrimitiveHandle &prim)
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

    bool saved_vis = myRefiner.getVisible();
    if(!hidden_group || !offsets[1].entries())
    {
	// all visible
	refine(prim);
    }
    else if(!offsets[0].entries())
    {
	// all hidden
	if(myRefiner.getSaveHidden())
	{
	    myRefiner.setVisible(false);
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
		if(!myRefiner.getSaveHidden())
		    break;

		myRefiner.setVisible(false);
	    }

	    GA_Range range(gdp->getPrimitiveMap(), offsets[i]);
	    GT_PrimitiveHandle pr =
			    GT_GEODetail::makeDetail(gdh, &range);
	    pr->setPrimitiveTransform(prim->getPrimitiveTransform());
	    refine(pr);
	}
    }
    myRefiner.setVisible(saved_vis);
}

void
rop_RefineInstanceHelper::refine(const GT_PrimitiveHandle &prim)
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

}

ROP_AbcRefiner::ROP_AbcRefiner(
    ROP_AbcHierarchySample::InstanceMap &instance_map,
    GABC_OError &err,
    ROP_AlembicPackedTransform packedtransform,
    exint facesetmode,
    bool use_instancing,
    bool shape_nodes,
    bool save_hidden)
    : myInstanceMap(instance_map)
    , myRoot(nullptr)
    , myError(err)
    , myPackedTransform(packedtransform)
    , myUseInstancing(use_instancing)
    , myShapeNodes(shape_nodes)
    , mySaveHidden(save_hidden)
    , myVisible(true)
    , mySubd(false)
{
    myParms.setCoalesceFragments(false);
    myParms.setFastPolyCompacting(false);
    myParms.setShowUnusedPoints(true);
    myParms.setFaceSetMode(facesetmode);
    myParms.setAlembicHoudiniAttribs(false);
    myParms.setSkipHidden(false);
    myParms.setAlembicSkipInvisible(!save_hidden);
}

void
ROP_AbcRefiner::addPrimitive(const GT_PrimitiveHandle &prim)
{
    if(GABC_OGTGeometry::isPrimitiveSupported(prim))
	appendShape(prim);
    else if(!processInstance(prim) && !processPacked(prim))
    {
	bool saved_vis = myVisible;
	if(prim->getPrimitiveType() == GT_PRIM_ALEMBIC_SHAPE)
	{
	    const GT_GEOPrimPacked *packed =
		static_cast<const GT_GEOPrimPacked *>(prim.get());
	    const GU_PrimPacked *pr =
		static_cast<const GU_PrimPacked *>(packed->getPrim());
	    const GABC_PackedImpl *impl =
		static_cast<const GABC_PackedImpl *>(pr->implementation());

	    myVisible &= (impl->intrinsicFullVisibility(pr) != 0);
	}
	rop_RefineHelper helper(*this, myParms);
	helper.addPrimitive(prim);
	myVisible = saved_vis;
    }
}

void
ROP_AbcRefiner::addPartition(
    ROP_AbcHierarchySample &root,
    const std::string &name,
    bool subd,
    const GT_PrimitiveHandle &prim,
    const std::string &up_vals,
    const std::string &up_meta,
    const std::string &subd_grp)
{
    myRoot = &root;
    myName = name;
    myUserPropsVals = up_vals;
    myUserPropsMeta = up_meta;
    mySubdGroup = subd_grp;
    myParms.setPolysAsSubdivision(subd);
    addPrimitive(prim);
}

void
ROP_AbcRefiner::appendShape(const GT_PrimitiveHandle &prim)
{
    if(!myShapeNodes)
	return;

    if(myInstanceKey.empty())
    {
	myRoot->appendShape(myName, prim, myVisible,
			    myUserPropsVals, myUserPropsMeta, mySubdGroup);
	return;
    }

    int type = prim->getPrimitiveType();
    auto it = myInstanceShapeIndex.find(myInstanceKey);
    exint idx;
    if(it != myInstanceShapeIndex.end())
    {
	// use defined instance
	idx = it->second[type]++;
    }
    else
    {
	// defining a new instance
	auto &list = myInstanceMap[myInstanceKey][type];
	idx = list.entries();
	list.append(prim);
    }
    myRoot->appendInstancedShape(myName, prim->getPrimitiveType(),
				 myInstanceKey, idx, myVisible,
				 myUserPropsVals, myUserPropsMeta, mySubdGroup);
}

bool
ROP_AbcRefiner::processInstance(const GT_PrimitiveHandle &prim)
{
    if(prim->getPrimitiveType() != GT_PRIM_INSTANCE)
	return false;

    const GT_PrimInstance *inst = static_cast<const GT_PrimInstance *>(prim.get());
    if(!myUseInstancing
       || myPackedTransform == ROP_ALEMBIC_PACKEDTRANSFORM_DEFORM_GEOMETRY)
    {
	inst->flattenInstances(*this, &myParms);
	return true;
    }

    GT_PrimitiveHandle geo = inst->geometry();
    int type = geo->getPrimitiveType();
    if(GABC_OGTGeometry::isPrimitiveSupported(geo))
    {
	UT_Matrix4D prim_xform;
	prim->getPrimitiveTransform()->getMatrix(prim_xform, 0);

	GT_TransformArrayHandle xforms = inst->transforms();
	exint n = xforms->entries();
	UT_WorkBuffer buf;
	for(exint i = 0; i < n; ++i)
	{
	    GT_TransformHandle xform = xforms->get(i);
	    UT_ASSERT(xform->getSegments() == 1);
	    UT_Matrix4D m;
	    xform->getMatrix(m, 0);
	    m *= prim_xform;

	    auto saved_root = myRoot;
	    auto new_root = myRoot;
	    if(myPackedTransform == ROP_ALEMBIC_PACKEDTRANSFORM_TRANSFORM_GEOMETRY)
	    {
		buf.sprintf("%s_instance%" SYS_PRId64,
			    myName.c_str(),
			    saved_root->getNextInstanceId(myName));
		new_root = new_root->getChildXform(buf.toStdString());
		new_root->setXform(m);
	    }
	    else // if(myPackedTransform == ROP_ALEMBIC_PACKEDTRANSFORM_MERGE_WITH_PARENT_TRANSFORM)
	    {
		if(!new_root->getParent())
		    new_root->warnRoot(myError);
		else if(!new_root->setPreXform(m))
		    new_root->warnChildren(myError);
	    }

	    myRoot = new_root;
	    appendShape(geo);
	    myRoot = saved_root;

	    if(!myInstanceKey.empty())
		myInstanceShapeIndex[myInstanceKey].clear();
	}
    }
    else if(type == GT_GEO_PACKED || type == GT_PRIM_ALEMBIC_SHAPE)
    {
	// transform before refining further
	const GT_GEOPrimPacked *packed =
		    static_cast<const GT_GEOPrimPacked *>(geo.get());
	const GU_PrimPacked *pr =
	    static_cast<const GU_PrimPacked *>(packed->getPrim());

	bool saved_vis = myVisible;
	if(type == GT_PRIM_ALEMBIC_SHAPE)
	{
	    const GABC_PackedImpl *impl =
		static_cast<const GABC_PackedImpl *>(pr->implementation());
	    myVisible &= (impl->intrinsicFullVisibility(pr) != 0);
	}

	std::string saved_key = myInstanceKey;
	ropGetInstanceKey(myInstanceKey, mySubd, pr);

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
	myInstanceKey = saved_key;
	myVisible = saved_vis;
    }
    else
    {
	rop_RefineInstanceHelper helper(*this, myParms, *inst);
	helper.addPrimitive(geo);
    }

    return true;
}

bool
ROP_AbcRefiner::processPacked(const GT_PrimitiveHandle &prim)
{
    int type = prim->getPrimitiveType();
    if(myPackedTransform == ROP_ALEMBIC_PACKEDTRANSFORM_DEFORM_GEOMETRY
	|| (type != GT_GEO_PACKED && type != GT_PRIM_ALEMBIC_SHAPE))
    {
	return false;
    }

    const GT_GEOPrimPacked *packed =
		static_cast<const GT_GEOPrimPacked *>(prim.get());
    const GU_PrimPacked *pr =
	static_cast<const GU_PrimPacked *>(packed->getPrim());

    std::string saved_key = myInstanceKey;
    ropGetInstanceKey(myInstanceKey, mySubd, pr);

    bool saved_vis = myVisible;
    if(type == GT_PRIM_ALEMBIC_SHAPE)
    {
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
    prim_xform = m * prim_xform;

    m.invert();
    GT_PrimitiveHandle copy = prim->doSoftCopy();
    copy->setPrimitiveTransform(new GT_Transform(&m, 1));
    auto saved_root = myRoot;
    auto new_root = myRoot;
    if(myPackedTransform == ROP_ALEMBIC_PACKEDTRANSFORM_TRANSFORM_GEOMETRY)
    {
	UT_WorkBuffer buf;
	buf.sprintf("%s_packed%" SYS_PRId64,
		    myName.c_str(), saved_root->getNextPackedId(myName));
	new_root = new_root->getChildXform(buf.toStdString());
	new_root->setXform(prim_xform);
    }
    else // if(myPackedTransform == ROP_ALEMBIC_PACKEDTRANSFORM_MERGE_WITH_PARENT_TRANSFORM)
    {
	if(!new_root->getParent())
	    new_root->warnRoot(myError);
	else if(!new_root->setPreXform(prim_xform))
	    new_root->warnChildren(myError);
    }

    myRoot = new_root;
    copy->refine(*this, &myParms);
    if(!myInstanceKey.empty())
	myInstanceShapeIndex[myInstanceKey].clear();
    myRoot = saved_root;
    myInstanceKey = saved_key;
    myVisible = saved_vis;
    return true;
}
