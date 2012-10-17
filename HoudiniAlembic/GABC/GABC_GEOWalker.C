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
 * NAME:	GABC_GEOWalker.h ( GABC Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_GEOWalker.h"
#include "GABC_GUPrim.h"
#include <UT/UT_Interrupt.h>
#include <Alembic/AbcGeom/All.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PrimPart.h>
#include <GU/GU_PrimNURBSurf.h>
#include <GA/GA_Handle.h>

namespace {
    typedef Imath::M44d				M44d;
    typedef Imath::V3d				V3d;
    typedef Alembic::Abc::IObject		IObject;
    typedef Alembic::Abc::ISampleSelector	ISampleSelector;
    typedef Alembic::Abc::ObjectHeader		ObjectHeader;
    typedef Alembic::Abc::Int32ArraySamplePtr	Int32ArraySamplePtr;
    typedef Alembic::Abc::FloatArraySamplePtr	FloatArraySamplePtr;
    typedef Alembic::Abc::P3fArraySamplePtr	P3fArraySamplePtr;
    typedef Alembic::Abc::ICompoundProperty	ICompoundProperty;
    typedef Alembic::Abc::PropertyHeader	PropertyHeader;

    typedef Alembic::AbcGeom::IXform		IXform;
    typedef Alembic::AbcGeom::IXformSchema	IXformSchema;
    typedef Alembic::AbcGeom::XformSample	XformSample;

    typedef Alembic::AbcGeom::ISubD		ISubD;
    typedef Alembic::AbcGeom::ISubDSchema	ISubDSchema;
    typedef Alembic::AbcGeom::IPolyMesh		IPolyMesh;
    typedef Alembic::AbcGeom::IPolyMeshSchema	IPolyMeshSchema;
    typedef Alembic::AbcGeom::ICurves		ICurves;
    typedef Alembic::AbcGeom::ICurvesSchema	ICurvesSchema;
    typedef Alembic::AbcGeom::IPoints		IPoints;
    typedef Alembic::AbcGeom::IPointsSchema	IPointsSchema;
    typedef Alembic::AbcGeom::INuPatch		INuPatch;
    typedef Alembic::AbcGeom::INuPatchSchema	INuPatchSchema;
    typedef INuPatchSchema::Sample		INuPatchSample;

    M44d	identity44d(1, 0, 0, 0,
			    0, 1, 0, 0,
			    0, 0, 1, 0,
			    0, 0, 0, 1);

    class PushTransform
    {
    public:
	PushTransform(GABC_GEOWalker &walk, IXform &xform)
	    : myWalk(walk)
	{
	    IXformSchema	&xs = xform.getSchema();
	    XformSample		 sample = xs.getValue(walk.timeSample());
	    if (!xs.isConstant())
		walk.setNonConstant();
	    walk.pushTransform(sample.getMatrix(), xs.isConstant(), myPop,
		    xs.getInheritsXforms());
	}
	~PushTransform()
	{
	    myWalk.popTransform(myPop);
	}
    private:
	GABC_GEOWalker			&myWalk;
	GABC_GEOWalker::TransformState	 myPop;
    };

#if 0
    void
    updateTopology(GABC_GEOWalker &walk,
		    Alembic::AbcGeom::MeshTopologyVariance top)
    {
	if (top == Alembic::AbcGeom::kHeterogenousTopology)
	{
	    walk.setNonConstantTopology();
	}
    }
#endif

    static GA_AttributeOwner
    getGAOwner(Alembic::AbcGeom::GeometryScope scope)
    {
	switch (scope)
	{
	    case Alembic::AbcGeom::kConstantScope:
		return GA_ATTRIB_DETAIL;
	    case Alembic::AbcGeom::kUnknownScope:
	    case Alembic::AbcGeom::kUniformScope:
		return GA_ATTRIB_PRIMITIVE;
	    case Alembic::AbcGeom::kFacevaryingScope:
		return GA_ATTRIB_VERTEX;
	    case Alembic::AbcGeom::kVertexScope:
	    case Alembic::AbcGeom::kVaryingScope:
		return GA_ATTRIB_POINT;
	}
	UT_ASSERT(0);
	return GA_ATTRIB_OWNER_N;
    }

    static GA_Storage
    getGAStorage(const Alembic::AbcGeom::DataType &dtype)
    {
	switch (dtype.getPod())
	{
	    case Alembic::AbcGeom::kBooleanPOD:
	    case Alembic::AbcGeom::kUint8POD:
		return GA_STORE_UINT8;
	    case Alembic::AbcGeom::kInt8POD:
		return GA_STORE_INT8;
	    case Alembic::AbcGeom::kUint16POD:
	    case Alembic::AbcGeom::kInt16POD:
		return GA_STORE_INT16;
	    case Alembic::AbcGeom::kUint32POD:
	    case Alembic::AbcGeom::kInt32POD:
		return GA_STORE_INT32;
	    case Alembic::AbcGeom::kUint64POD:
	    case Alembic::AbcGeom::kInt64POD:
		return GA_STORE_INT64;
	    case Alembic::AbcGeom::kFloat16POD:
		return GA_STORE_REAL16;
	    case Alembic::AbcGeom::kFloat32POD:
		return GA_STORE_REAL32;
	    case Alembic::AbcGeom::kFloat64POD:
		return GA_STORE_REAL64;
	    case Alembic::AbcGeom::kStringPOD:
		return GA_STORE_STRING;

	    case Alembic::AbcGeom::kWstringPOD:
	    case Alembic::AbcGeom::kNumPlainOldDataTypes:
	    case Alembic::AbcGeom::kUnknownPOD:
		break;
	}
	return GA_STORE_INVALID;
    }

    static inline int
    getGATupleSize(const Alembic::AbcGeom::DataType &dtype)
    {
	return dtype.getExtent();
    }

    static inline GA_TypeInfo
    getGATypeInfo(const char *interp)
    {
	if (UTisstring(interp))
	{
	    if (!strcmp(interp, "point"))
		return GA_TYPE_POINT;
	    if (!strcmp(interp, "vector"))
		return GA_TYPE_VECTOR;
	    if (!strcmp(interp, "matrix"))
		return GA_TYPE_TRANSFORM;
	    if (!strcmp(interp, "normal"))
		return GA_TYPE_NORMAL;
	    if (!strcmp(interp, "quat"))
		return GA_TYPE_QUATERNION;
	}
	return GA_TYPE_VOID;
    }

    static GA_Defaults	theZeroDefaults(0);
    static GA_Defaults	theWidthDefaults(0.1);
    static GA_Defaults	theColorDefaults(1.0);

    static const GA_Defaults &
    getDefaults(const char *name, int tsize, const char *interp)
    {
	if (!strcmp(name, "width") && tsize == 1)
	    return theWidthDefaults;
	if (!strcmp(interp, "color"))
	    return theColorDefaults;
	return theZeroDefaults;
    }

    static GA_RWAttributeRef
    findAttribute(GU_Detail &gdp, GA_AttributeOwner owner,
	    const char *name, int tsize, GA_Storage storage, const char *interp)
    {
	GA_RWAttributeRef	attrib;
	switch (GAstorageClass(storage))
	{
	    case GA_STORECLASS_REAL:
		attrib = gdp.findFloatTuple(owner, name, tsize);
		break;
	    case GA_STORECLASS_INT:
		attrib = gdp.findIntTuple(owner, name, tsize);
		break;
	    case GA_STORECLASS_STRING:
		attrib = gdp.findStringTuple(owner, name, tsize);
		break;
	    default:
		break;
	}
	if (!attrib.isValid())
	{
	    if (!strcmp(name, "uv") && tsize == 2)
		tsize = 3;	// Adjust for "Houdini" uv coordinates

	    attrib = gdp.addTuple(storage, owner, name, tsize,
				getDefaults(name, tsize, interp));
	}
	return attrib;
    }

    template <typename T>
    static void
    setStringAttribute(GU_Detail &gdp, GA_RWAttributeRef &attrib,
		    const T &array, int extent, exint start, exint end,
		    bool extend_array)
    {
	GA_RWHandleS		 h(attrib.getAttribute());
	int			 tsize = SYSmin(extent, attrib.getTupleSize());
	const std::string	*data = (const std::string *)array->getData();
	UT_ASSERT(h.isValid());
	for (exint i = start; i < end; ++i)
	{
	    for (int j = 0; j < tsize; ++j)
		h.set(GA_Offset(i), j, data[j].c_str());
	    if (!extend_array)
		data += extent;
	}
    }

    template <typename T, typename GA_T, typename ABC_T>
    static void
    setNumericAttribute(GU_Detail &gdp, GA_RWAttributeRef &attrib,
		    const T &array, int extent, exint start, exint end,
		    bool extend_array)
    {
	GA_RWHandleT<GA_T>	 h(attrib.getAttribute());
	int			 tsize = SYSmin(extent, attrib.getTupleSize());
	const GA_AIFTuple	*tuple = attrib.getAIFTuple();
	const ABC_T		*data = (const ABC_T *)array->getData();
	if (!data)
	    return;
	UT_ASSERT(h.isValid());
	for (exint i = start; i < end; ++i)
	{
	    for (int j = 0; j < tsize; ++j)
	    {
		if (tuple)
		    tuple->set(attrib.getAttribute(), GA_Offset(i), (double)data[j], j);
		else h.set(GA_Offset(i), j, data[j]);
	    }
	    if (!extend_array)
		data += extent;
	}
    }

    static GA_TypeInfo
    isReallyVector(const char *name, int tsize)
    {
	// Alembic stores all sorts of things as vectors.  For example "uv"
	// coordinates.  We don't actually want "uv" coordinates transformed as
	// vectors, so we strip out the type information here.
	if (!strcmp(name, "uv") && tsize > 1 && tsize < 4)
	    return GA_TYPE_VOID;
	return GA_TYPE_VECTOR;
    }

    /// Template argument @T is expected to be a
    ///  boost::shared_ptr<TypedArraySample<TRAITS>>
    template <typename T>
    static void
    setAttribute(GU_Detail &gdp,
		    GA_AttributeOwner owner,
		    const char *name,
		    const T &array,
		    exint start, exint len)
    {
	if (!*array)
	    return;
	GA_RWAttributeRef	attrib;
	GA_Storage	 store = getGAStorage(array->getDataType());
	int		 tsize = getGATupleSize(array->getDataType());
	const char	*interp= T::element_type::traits_type::interpretation();
	bool		 extend_array;
	extend_array = array->size() < len;
	attrib = findAttribute(gdp, owner, name, tsize, store, interp);
	if (attrib.isValid())
	{
	    if (attrib.getAttribute() != gdp.getP())
	    {
		GA_TypeInfo	tinfo = getGATypeInfo(interp);
		if (tinfo == GA_TYPE_VECTOR)
		    tinfo = isReallyVector(name, tsize);
		attrib.getAttribute()->setTypeInfo(tinfo);
	    }
	    switch (attrib.getStorageClass())
	    {
		case GA_STORECLASS_REAL:
		    switch (array->getDataType().getPod())
		    {
			case Alembic::AbcGeom::kFloat16POD:
			    setNumericAttribute<T, fpreal32, fpreal16>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array);
			    break;
			case Alembic::AbcGeom::kFloat32POD:
			    setNumericAttribute<T, fpreal32, fpreal32>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array);
			    break;
			case Alembic::AbcGeom::kFloat64POD:
			    setNumericAttribute<T, fpreal32, fpreal64>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array);
			    break;
			default:
			    UT_ASSERT(0 && "Bad alembic type");
			    break;
		    }
		    break;
		case GA_STORECLASS_INT:
		    switch (array->getDataType().getPod())
		    {
			case Alembic::AbcGeom::kUint32POD:
			    setNumericAttribute<T, int32, uint32>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array);
			    break;
			case Alembic::AbcGeom::kInt32POD:
			    setNumericAttribute<T, int32, int32>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array);
			    break;
			case Alembic::AbcGeom::kUint64POD:
			    setNumericAttribute<T, int32, uint64>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array);
			    break;
			case Alembic::AbcGeom::kInt64POD:
			    setNumericAttribute<T, int32, int64>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array);
			    break;
			case Alembic::AbcGeom::kUint8POD:
			    setNumericAttribute<T, int32, uint8>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array);
			    break;
			case Alembic::AbcGeom::kInt8POD:
			    setNumericAttribute<T, int32, int8>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array);
			    break;
			case Alembic::AbcGeom::kUint16POD:
			    setNumericAttribute<T, int32, uint16>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array);
			    break;
			case Alembic::AbcGeom::kInt16POD:
			    setNumericAttribute<T, int32, int16>(
				gdp, attrib, array, tsize, start, start+len,
				extend_array);
			    break;
			default:
			    UT_ASSERT(0 && "Bad alembic type");
		    }
		    break;
		case GA_STORECLASS_STRING:
		    setStringAttribute<T>(gdp, attrib,
				array, tsize, start, start+len,
				extend_array);
		    break;
		default:
		    UT_ASSERT(0 && "Bad GA storage");
	    }
	}
    }

#if 0
    template <typename T>
    static void
    setArrayAttribute(GABC_GEOWalker &walk,
		    const char *name, GA_AttributeOwner owner,
		    const T &array, exint alen)
    {
    }
#endif

    /// Template argument @T is expected to be a
    ///  ITypedGeomParam<TRAITS>
    template <typename T>
    static void
    setGeomAttribute(GABC_GEOWalker &walk, const char *name,
			T &param, ISampleSelector &iss,
			exint npoint, exint nvertex, exint nprim)
    {
	if (!param.valid())
	    return;
	GU_Detail		&gdp = walk.detail();
	GA_AttributeOwner	 owner = getGAOwner(param.getScope());
	typename T::sample_type	psample;
	const_cast<T &>(param).getExpanded(psample, iss);
	switch (owner)
	{
	    case GA_ATTRIB_POINT:
		setAttribute(gdp, owner, name,
			psample.getVals(), walk.pointCount(), npoint);
		break;
	    case GA_ATTRIB_VERTEX:
		setAttribute(gdp, owner, name,
			psample.getVals(), walk.vertexCount(), nvertex);
		break;
	    case GA_ATTRIB_PRIMITIVE:
		setAttribute(gdp, owner, name,
			psample.getVals(), walk.primitiveCount(), nprim);
		break;

	    case GA_ATTRIB_DETAIL:
		// TODO: We map detail attributes to primitive attributes, so
		// we need to extend the array to fill all elements!
		setAttribute(gdp, owner, name,
			psample.getVals(), walk.primitiveCount(), nprim);
		break;
	    default:
		UT_ASSERT(0 && "Bad GA owner");
	}
    }

    static exint
    appendPoints(GABC_GEOWalker &walk, exint npoint)
    {
	GU_Detail	&gdp = walk.detail();
	exint		 startpoint = walk.pointCount();
	for (exint i = 0; i < npoint; ++i)
	{
	    UT_VERIFY(gdp.appendPointOffset() == startpoint+i);
	}
	return startpoint;
    }

    void
    appendParticles(GABC_GEOWalker &walk, exint npoint)
    {
	GU_Detail	&gdp = walk.detail();
	// Build the particle, appending points as we go
	GU_PrimParticle::build(&gdp, npoint, 1);
    }

    static void
    setKnotVector(GA_Basis &basis, FloatArraySamplePtr &knots)
    {
	GA_KnotVector	&kvec = basis.getKnotVector();
	UT_ASSERT(kvec.entries() == knots->size());
	for (int i = 0; i < knots->size(); ++i)
	    kvec.setValue(i, knots->get()[i]);
	// Adjust flags based on knot values
	basis.validate(GA_Basis::GA_BASIS_ADAPT_FLAGS);
    }

    void
    appendNURBS(GABC_GEOWalker &walk,
	    int uorder, FloatArraySamplePtr &uknots,
	    int vorder, FloatArraySamplePtr &vknots)
    {
	GU_Detail	&gdp = walk.detail();
	int		 cols = uknots->size() - uorder;
	int		 rows = vknots->size() - vorder;

	// Build the surface
	GU_PrimNURBSurf::build(&gdp, rows, cols,
				    uorder, vorder,
				    0,	// periodic in U
				    0,	// periodic in V
				    0,	// basis interpolates ends in U
				    0,	// basis interpolates ends in V
				    GEO_PATCH_QUADS);
    }

    void
    appendCurves(GABC_GEOWalker &walk, exint npoint, Int32ArraySamplePtr counts)
    {
	GU_Detail	&gdp = walk.detail();
	exint		 startpoint = appendPoints(walk, npoint);
	exint		 ncurves = counts->size();
	exint		 voff = startpoint;
	for (exint i = 0; i < ncurves; ++i)
	{
	    exint	nvtx = (*counts)[i];
	    GU_PrimPoly	*face = GU_PrimPoly::build(&gdp, nvtx,
					GU_POLY_OPEN, 0);
	    for (exint v = 0; v < nvtx; ++v)
	    {
		UT_ASSERT(voff + v < startpoint + npoint);
		face->setVertexPoint(v, GA_Offset(voff+v));
	    }
	    voff += nvtx;
	}
    }

    void
    appendFaces(GABC_GEOWalker &walk, exint npoint,
		    Int32ArraySamplePtr counts,
		    Int32ArraySamplePtr indices)
    {
	// First, append points
	GU_Detail	&gdp = walk.detail();
	exint		 startpoint = appendPoints(walk, npoint);
	exint		 nfaces = counts->size();
	exint		 voff = 0;
	for (exint i = 0; i < nfaces; ++i)
	{
	    exint	nvtx = (*counts)[i];
	    GU_PrimPoly	*face = GU_PrimPoly::build(&gdp, nvtx,
					GU_POLY_CLOSED, 0);
	    UT_ASSERT(face->getMapOffset() == i+walk.primitiveCount());
	    for (exint v = 0; v < nvtx; ++v)
	    {
		exint	pt = (*indices)[voff+v];
		UT_ASSERT(pt >= 0 && pt < npoint);
		face->setVertexPoint(v, GA_Offset(startpoint+pt));
	    }
	    voff += nvtx;
	}
    }

    template <typename T>
    static void
    arbitraryGeomAttributeT(GABC_GEOWalker &walk,
		ICompoundProperty parent,
		const PropertyHeader &head,
		const char *name,
		exint npoint, exint nvertex, exint nprim)
    {
	T		 param(parent, head.getName());
	ISampleSelector	 iss = walk.timeSample();
	setGeomAttribute(walk, name, param, iss, npoint, nvertex, nprim);
    }

    #define MATCH_ARB(TYPENAME)	\
	if (Alembic::AbcGeom::TYPENAME::matches(head)) { \
	    arbitraryGeomAttributeT<Alembic::AbcGeom::TYPENAME>( \
		    walk, parent, head, name, npoint, nvertex, nprim); \
	    return; \
	}
    static void
    arbitraryGeomAttribute(GABC_GEOWalker &walk,
	    ICompoundProperty parent,
	    const PropertyHeader &head,
	    const char *name,
	    exint npoint, exint nvertex, exint nprim)
    {
	// Yuck.
	MATCH_ARB(IBoolGeomParam);
	MATCH_ARB(IUcharGeomParam);
	MATCH_ARB(ICharGeomParam);
	MATCH_ARB(IUInt16GeomParam);
	MATCH_ARB(IInt16GeomParam);
	MATCH_ARB(IUInt32GeomParam);
	MATCH_ARB(IInt32GeomParam);
	MATCH_ARB(IUInt64GeomParam);
	MATCH_ARB(IInt64GeomParam);
	MATCH_ARB(IHalfGeomParam);
	MATCH_ARB(IFloatGeomParam);
	MATCH_ARB(IDoubleGeomParam);
	MATCH_ARB(IStringGeomParam);
	MATCH_ARB(IWstringGeomParam);
	MATCH_ARB(IV2sGeomParam);
	MATCH_ARB(IV2iGeomParam);
	MATCH_ARB(IV2fGeomParam);
	MATCH_ARB(IV2dGeomParam);
	MATCH_ARB(IV3sGeomParam);
	MATCH_ARB(IV3iGeomParam);
	MATCH_ARB(IV3fGeomParam);
	MATCH_ARB(IV3dGeomParam);
	MATCH_ARB(IP2sGeomParam);
	MATCH_ARB(IP2iGeomParam);
	MATCH_ARB(IP2fGeomParam);
	MATCH_ARB(IP2dGeomParam);
	MATCH_ARB(IP3sGeomParam);
	MATCH_ARB(IP3iGeomParam);
	MATCH_ARB(IP3fGeomParam);
	MATCH_ARB(IP3dGeomParam);
	MATCH_ARB(IBox2sGeomParam);
	MATCH_ARB(IBox2iGeomParam);
	MATCH_ARB(IBox2fGeomParam);
	MATCH_ARB(IBox2dGeomParam);
	MATCH_ARB(IBox3sGeomParam);
	MATCH_ARB(IBox3iGeomParam);
	MATCH_ARB(IBox3fGeomParam);
	MATCH_ARB(IBox3dGeomParam);
	MATCH_ARB(IM33fGeomParam);
	MATCH_ARB(IM33dGeomParam);
	MATCH_ARB(IM44fGeomParam);
	MATCH_ARB(IM44dGeomParam);
	MATCH_ARB(IQuatfGeomParam);
	MATCH_ARB(IQuatdGeomParam);
	MATCH_ARB(IC3hGeomParam);
	MATCH_ARB(IC3fGeomParam);
	MATCH_ARB(IC3cGeomParam);
	MATCH_ARB(IC4hGeomParam);
	MATCH_ARB(IC4fGeomParam);
	MATCH_ARB(IC4cGeomParam);
	MATCH_ARB(IN2fGeomParam);
	MATCH_ARB(IN2dGeomParam);
	MATCH_ARB(IN3fGeomParam);
	MATCH_ARB(IN3dGeomParam);
    }
    #undef MATCH_ARB

    GA_AttributeOwner
    arbitraryGAOwner(const PropertyHeader &ph)
    {
	return getGAOwner(Alembic::AbcGeom::GetGeometryScope(ph.getMetaData()));
    }
    
    void
    fillArb(GABC_GEOWalker &walk, ICompoundProperty arb,
		exint npoint, exint nvertex, exint nprim)
    {
	exint	narb = arb ? arb.getNumProperties() : 0;

	for (exint i = 0; i < narb; ++i)
	{
	    const PropertyHeader	&head = arb.getPropertyHeader(i);
	    UT_String			 name = head.getName().c_str();
	    GA_AttributeOwner		 owner = arbitraryGAOwner(head);
	    if (!walk.translateAttributeName(owner, name))
		continue;
	    GA_Storage	store = getGAStorage(head.getDataType());
	    if (store == GA_STORE_INVALID)
		continue;
	    arbitraryGeomAttribute(walk, arb, head,
		    name, npoint, nvertex, nprim);
	}
    }

    void
    makeAbcPrim(GABC_GEOWalker &walk, const IObject &obj,
	    const ObjectHeader &ohead)
    {
	switch (GABC_Util::getNodeType(obj))
	{
	    case GABC_POLYMESH:
	    case GABC_SUBD:
	    case GABC_CURVES:
	    case GABC_POINTS:
	    case GABC_NUPATCH:
		break;
	    case GABC_XFORM:
		// The only primitives built from xforms are maya locators
		UT_ASSERT(GABC_Util::isMayaLocator(obj));
		break;
	    default:
		return;	// Invalid primitive type
	}
	GABC_GUPrim *abc = GABC_GUPrim::build(&walk.detail(),
				walk.filename(),
				obj,
				walk.time(),
				walk.includeXform());
	abc->setAttributeNameMap(walk.nameMapPtr());
	abc->setUseTransform(walk.includeXform());
	if (!abc->isConstant())
	    walk.setNonConstant();
	walk.trackPtVtxPrim(obj, 0, 0, 1, false);

    }

    GABC_AnimationType
    getAnimationType(GABC_GEOWalker &walk, const IObject &obj)
    {
	GABC_AnimationType	atype;
	atype = GABC_Util::getAnimationType(walk.filename(), obj, false);
	if (atype == GABC_ANIMATION_TOPOLOGY)
	    walk.setNonConstantTopology();
	return atype;
    }

    void
    makeSubD(GABC_GEOWalker &walk, const IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	ISubD			subd(const_cast<IObject &>(obj),
					Alembic::Abc::kWrapExisting);
	ISubDSchema		&ps = subd.getSchema();
	P3fArraySamplePtr	P = ps.getPositionsProperty().getValue(iss);
	Int32ArraySamplePtr	counts=ps.getFaceCountsProperty().getValue(iss);
	Int32ArraySamplePtr	indices = ps.getFaceIndicesProperty().getValue(iss);
	exint			npoint = P->size();
	exint			nvertex = indices->size();
	exint			nprim = counts->size();

	//fprintf(stderr, "SubD: %d %d %d\n", int(npoint), int(nvertex), int(nprim));

	GABC_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GABC_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	else if (walk.reusePrimitives())
	{
	    if (!walk.includeXform() || walk.transformConstant())
	    {
		walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, false);
		return;
	    }
	}
	if (!walk.reusePrimitives())
	{
	    // Asser that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() ==walk.primitiveCount());
	    appendFaces(walk, npoint, counts, indices);
	}

	// Set properties
	setAttribute(walk.detail(), GA_ATTRIB_POINT, "P",
		ps.getPositionsProperty().getValue(iss),
		walk.pointCount(), npoint);
	if (ps.getVelocitiesProperty().valid())
	    setAttribute(walk.detail(), GA_ATTRIB_POINT, "v",
		    ps.getVelocitiesProperty().getValue(iss),
		    walk.pointCount(), npoint);
	setGeomAttribute(walk, "uv", ps.getUVsParam(), iss,
		npoint, nvertex, nprim);
	fillArb(walk, ps.getArbGeomParams(), npoint, nvertex, nprim);

	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    void
    makePolyMesh(GABC_GEOWalker &walk, const IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	IPolyMesh		polymesh(const_cast<IObject &>(obj),
					Alembic::Abc::kWrapExisting);
	IPolyMeshSchema		&ps = polymesh.getSchema();
	P3fArraySamplePtr	P = ps.getPositionsProperty().getValue(iss);
	Int32ArraySamplePtr	counts=ps.getFaceCountsProperty().getValue(iss);
	Int32ArraySamplePtr	indices = ps.getFaceIndicesProperty().getValue(iss);
	exint			npoint = P->size();
	exint			nvertex = indices->size();
	exint			nprim = counts->size();

	//fprintf(stderr, "PolyMesh %s: %d %d %d\n", obj.getFullName().c_str(), int(npoint), int(nvertex), int(nprim));

	GABC_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GABC_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	else if (walk.reusePrimitives())
	{
	    if (!walk.includeXform() || walk.transformConstant())
	    {
		walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, false);
		return;
	    }
	}
	if (!walk.reusePrimitives())
	{
	    // Asser that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() ==walk.primitiveCount());
	    appendFaces(walk, npoint, counts, indices);
	}

	// Set properties
	setAttribute(walk.detail(), GA_ATTRIB_POINT, "P",
		ps.getPositionsProperty().getValue(iss),
		walk.pointCount(), npoint);
	if (ps.getVelocitiesProperty().valid())
	    setAttribute(walk.detail(), GA_ATTRIB_POINT, "v",
		    ps.getVelocitiesProperty().getValue(iss),
		    walk.pointCount(), npoint);
	setGeomAttribute(walk, "uv", ps.getUVsParam(), iss,
		npoint, nvertex, nprim);
	setGeomAttribute(walk, "N", ps.getNormalsParam(), iss,
		npoint, nvertex, nprim);
	fillArb(walk, ps.getArbGeomParams(), npoint, nvertex, nprim);

	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    void
    makeCurves(GABC_GEOWalker &walk, const IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	ICurves			curves(const_cast<IObject &>(obj),
					Alembic::Abc::kWrapExisting);
	ICurvesSchema		&cs = curves.getSchema();
	P3fArraySamplePtr	P = cs.getPositionsProperty().getValue(iss);
	Int32ArraySamplePtr	nvtx =cs.getNumVerticesProperty().getValue(iss);
	exint			npoint = P->size();
	exint			nvertex = npoint;
	exint			nprim = nvtx->size();

#if 0
	//fprintf(stderr, "Curves: %d %d %d\n", int(npoint), int(nvertex), int(nprim));

	// Due to a bug in Alembic 1.0.5, the topology variance for curves
	// isn't correctly returned by the schema.  We have to do additional
	// checks.
	Alembic::AbcGeom::MeshTopologyVariance	top = cs.getTopologyVariance();
	if (top == Alembic::AbcGeom::kHomogenousTopology &&
		!cs.getNumVerticesProperty().isConstant())
	{
	    top = Alembic::AbcGeom::kHeterogenousTopology;
	}
#endif

	GABC_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GABC_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	else if (walk.reusePrimitives())
	{
	    if (!walk.includeXform() || walk.transformConstant())
	    {
		walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, false);
		return;
	    }
	}
	if (!walk.reusePrimitives())
	{
	    // Asser that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() ==walk.primitiveCount());
	    appendCurves(walk, npoint, nvtx);
	}

	// Set properties
	setAttribute(walk.detail(), GA_ATTRIB_POINT, "P",
		cs.getPositionsProperty().getValue(iss),
		walk.pointCount(), npoint);
	if (cs.getVelocitiesProperty().valid())
	    setAttribute(walk.detail(), GA_ATTRIB_POINT, "v",
		    cs.getVelocitiesProperty().getValue(iss),
		    walk.pointCount(), npoint);
	setGeomAttribute(walk, "uv", cs.getUVsParam(), iss,
		npoint, nvertex, nprim);
	setGeomAttribute(walk, "width", cs.getWidthsParam(), iss,
		npoint, nvertex, nprim);
	setGeomAttribute(walk, "N", cs.getNormalsParam(), iss,
		npoint, nvertex, nprim);
	fillArb(walk, cs.getArbGeomParams(), npoint, nvertex, nprim);

	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    static void
    locatorAttribute(GABC_GEOWalker &walk, const char *name,
		    fpreal x, fpreal y, fpreal z)
    {
	GA_RWAttributeRef	href = findAttribute(walk.detail(),
						GA_ATTRIB_PRIMITIVE,
						name, 3, GA_STORE_REAL64,
						"float");
	GA_RWHandleV3		h(href.getAttribute());
	if (h.isValid())
	{
	    UT_Vector3	v(x, y, z);
	    h.set(GA_Offset(walk.primitiveCount()), v);
	}
    }

    void
    makeLocator(GABC_GEOWalker &walk, IXform &xform)
    {
	Alembic::Abc::IScalarProperty	loc(xform.getProperties(), "locator");
	ISampleSelector			iss = walk.timeSample();
	const int			npoint = 1;
	const int			nvertex = 1;
	const int			nprim = 1;

#if 0
	// IXform doesn't have getTopologyVariance()
	//updateTopology(walk, loc.getTopologyVariance());
	updateTopology(walk,
		loc.isConstant() ? Alembic::AbcGeom::kConstantTopology
				: Alembic::AbcGeom::kHomogenousTopology);
#endif
	GABC_AnimationType	atype = getAnimationType(walk, xform);
	if (atype != GABC_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	else if (walk.reusePrimitives())
	{
	    if (!walk.includeXform() || walk.transformConstant())
	    {
		walk.trackPtVtxPrim(xform, npoint, nvertex, nprim, false);
		return;
	    }
	}
	if (!walk.reusePrimitives())
	{
	    // Asser that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() ==walk.primitiveCount());
	    appendParticles(walk, npoint);
	}

	double	ldata[6];	// Local translate/scale
	V3d	ls, lh, lr, lt;

	loc.get(ldata, iss);
	if (!Imath::extractSHRT(walk.getTransform(), ls, lh, lr, lt))
	{
	    ls = V3d(1,1,1);
	    lr = V3d(0,0,0);
	    lt = V3d(0,0,0);
	}

	walk.detail().setPos3(GA_Offset(walk.pointCount()),
			ldata[0], ldata[1], ldata[2]);
	locatorAttribute(walk, "localPosition", ldata[0], ldata[1], ldata[2]);
	locatorAttribute(walk, "localScale", ldata[3], ldata[4], ldata[5]);
	locatorAttribute(walk, "parentTrans", lt.x, lt.y, lt.z);
	locatorAttribute(walk, "parentRot", lr.x, lr.y, lr.z);
	locatorAttribute(walk, "parentScale", ls.x, ls.y, ls.z);

	walk.trackPtVtxPrim(xform, npoint, nvertex, nprim, true);
    }

    void
    makePoints(GABC_GEOWalker &walk, const IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	IPoints			points(const_cast<IObject &>(obj),
					Alembic::Abc::kWrapExisting);
	IPointsSchema		&ps = points.getSchema();
	P3fArraySamplePtr	P = ps.getPositionsProperty().getValue(iss);

	exint			npoint = P->size();
	exint			nvertex = npoint;
	exint			nprim = 1;

	//fprintf(stderr, "Points: %d %d %d\n", int(npoint), int(nvertex), int(nprim));

#if 0
	// IPoints doesn't have getTopologyVariance()
	//updateTopology(walk, ps.getTopologyVariance());
	updateTopology(walk,
		ps.isConstant() ? Alembic::AbcGeom::kConstantTopology
				: Alembic::AbcGeom::kHomogenousTopology);
#endif
	GABC_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GABC_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	else if (walk.reusePrimitives())
	{
	    if (!walk.includeXform() || walk.transformConstant())
	    {
		walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, false);
		return;
	    }
	}
	if (!walk.reusePrimitives())
	{
	    // Asser that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() ==walk.primitiveCount());
	    appendParticles(walk, npoint);
	}

	// Set properties
	setAttribute(walk.detail(), GA_ATTRIB_POINT, "P",
		ps.getPositionsProperty().getValue(iss),
		walk.pointCount(), npoint);
	if (ps.getVelocitiesProperty().valid())
	    setAttribute(walk.detail(), GA_ATTRIB_POINT, "v",
		    ps.getVelocitiesProperty().getValue(iss),
		    walk.pointCount(), npoint);
	if (ps.getIdsProperty().valid())
	    setAttribute(walk.detail(), GA_ATTRIB_POINT, "id",
		    ps.getIdsProperty().getValue(iss),
		    walk.pointCount(), npoint);
	Alembic::AbcGeom::IFloatGeomParam	widths = ps.getWidthsParam();
	setGeomAttribute(walk, "width", widths, iss, npoint, nvertex, nprim);
	fillArb(walk, ps.getArbGeomParams(), npoint, nvertex, nprim);

	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }

    void
    makeNuPatch(GABC_GEOWalker &walk, const IObject &obj)
    {
	ISampleSelector		iss = walk.timeSample();
	INuPatch		nupatch(const_cast<IObject &>(obj),
					Alembic::Abc::kWrapExisting);
	INuPatchSchema		&ps = nupatch.getSchema();
	INuPatchSample		patch = ps.getValue(iss);
	FloatArraySamplePtr	uknots = patch.getUKnot();
	FloatArraySamplePtr	vknots = patch.getVKnot();
	int			uorder = patch.getUOrder();
	int			vorder = patch.getVOrder();
	P3fArraySamplePtr	P = ps.getPositionsProperty().getValue(iss);
	exint			npoint = P->size();
	exint			nvertex = npoint;
	exint			nprim = 1;

	// Verify that we have the expected point count
	UT_ASSERT(P->size() == (uknots->size()-uorder)*(vknots->size()-vorder));

	//fprintf(stderr, "NuPatch: %d %d %d\n", int(npoint), int(nvertex), int(nprim));

	GABC_AnimationType	atype = getAnimationType(walk, obj);
	if (atype != GABC_ANIMATION_CONSTANT)
	{
	    walk.setNonConstant();
	}
	else if (walk.reusePrimitives())
	{
	    if (!walk.includeXform() || walk.transformConstant())
	    {
		walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, false);
		return;
	    }
	}
	if (!walk.reusePrimitives())
	{
	    // Asser that we need to create the polygons
	    UT_ASSERT(walk.detail().getNumPoints() == walk.pointCount());
	    UT_ASSERT(walk.detail().getNumPrimitives() ==walk.primitiveCount());
	    appendNURBS(walk, uorder, uknots, vorder, vknots);
	}
	GA_Offset	 primoff = GA_Offset(walk.primitiveCount());
	GU_PrimNURBSurf	*surf = UTverify_cast<GU_PrimNURBSurf *>(
			    walk.detail().getGEOPrimitive(primoff));
	setKnotVector(*surf->getUBasis(), uknots);
	setKnotVector(*surf->getVBasis(), vknots);

	// Set properties
	setAttribute(walk.detail(), GA_ATTRIB_POINT, "P",
		ps.getPositionsProperty().getValue(iss),
		walk.pointCount(), npoint);
	if (ps.getVelocitiesProperty().valid())
	    setAttribute(walk.detail(), GA_ATTRIB_POINT, "v",
		    ps.getVelocitiesProperty().getValue(iss),
		    walk.pointCount(), npoint);
	setGeomAttribute(walk, "uv", ps.getUVsParam(), iss,
		npoint, nvertex, nprim);
	setGeomAttribute(walk, "N", ps.getNormalsParam(), iss,
		npoint, nvertex, nprim);
	fillArb(walk, ps.getArbGeomParams(), npoint, nvertex, nprim);

	walk.trackPtVtxPrim(obj, npoint, nvertex, nprim, true);
    }
}

GABC_GEOWalker::GABC_GEOWalker(GU_Detail &gdp)
    : myDetail(gdp)
    , myObjectPattern("*")
    , myNameMapPtr()
    , myBoss(UTgetInterrupt())
    , myBossId(-1)
    , myMatrix(identity44d)
    , myPathAttribute()
    , myTime(0)
    , myPointCount(0)
    , myVertexCount(0)
    , myPrimitiveCount(0)
    , myGroupMode(ABC_GROUP_SHAPE_NODE)
    , myIncludeXform(true)
    , myReusePrimitives(false)
    , myBuildLocator(true)
    , myBuildAbcPrim(false)
    , myPathAttributeChanged(true)
    , myIsConstant(true)
    , myTopologyConstant(true)
    , myTransformConstant(true)
    , myAllTransformConstant(true)
    , myRebuiltNURBS(false)
{
    if (myBoss)
	myBossId = myBoss->opStart("Building geometry from Alembic tree");
    myAttributePatterns[GA_ATTRIB_VERTEX] = "*";
    myAttributePatterns[GA_ATTRIB_POINT] = "*";
    myAttributePatterns[GA_ATTRIB_PRIMITIVE] = "*";
    myAttributePatterns[GA_ATTRIB_DETAIL] = "*";
}

GABC_GEOWalker::~GABC_GEOWalker()
{
    // In theory, this should be true even if we were interrupted.
    if (myBoss)
	myBoss->opEnd(myBossId);
}

void
GABC_GEOWalker::setReusePrimitives(bool v)
{
    myReusePrimitives = v && myDetail.getNumPrimitives() > 0;
}

void
GABC_GEOWalker::setPathAttribute(const GA_RWAttributeRef &a)
{
    GA_RWHandleS	h(a.getAttribute());
    UT_ASSERT(h.isValid() && "Require a string attribute!");
    if (h.isValid())
	myPathAttribute = h;
}

void
GABC_GEOWalker::updateAbcPrims()
{
    for (GA_Iterator it(detail().getPrimitiveRange()); !it.atEnd(); ++it)
    {
	GEO_Primitive	*prim = detail().getGEOPrimitive(*it);
	GABC_GUPrim	*abc = UTverify_cast<GABC_GUPrim *>(prim);
	if (!abc->isConstant())
	{
	    setNonConstant();
	}
	// Topology is always constant since the point/primitive count doesn't
	// change.
	// A change in the attribute map will cause an entire rebuild.
	abc->setFrame(time());
    }
}

bool
GABC_GEOWalker::preProcess(const IObject &root)
{
    IObject	parent = const_cast<IObject &>(root).getParent();
    if (parent.valid())
    {
	UT_Matrix4D	m;
	bool		c, i;
	if (!GABC_Util::getWorldTransform(filename(), parent, time(), m, c, i))
	{
	    m.identity();
	}
	for (int r = 0; r < 4; ++r)
	    for (int c = 0; c < 4; ++c)
		myMatrix.x[r][c] = m(r,c);
    }
    else
    {
	myMatrix = identity44d;
    }
    return true;
}

bool
GABC_GEOWalker::process(const IObject &obj)
{
    const ObjectHeader	&ohead = obj.getHeader();

    //fprintf(stderr, "Process: %s\n", (const char *)obj.getFullName().c_str());
    if (IXform::matches(ohead))
    {
	IXform	xform(obj, Alembic::Abc::kWrapExisting);
	if (buildLocator() && GABC_Util::isMayaLocator(xform))
	{
	    if (matchObjectName(obj))
	    {
		if (buildAbcPrim())
		    makeAbcPrim(*this, obj, ohead);
		else
		    makeLocator(*this, xform);
	    }
	    return true;	// Process locator children
	}
	else if (includeXform())
	{
	    PushTransform	push(*this, xform);
	    walkChildren(obj);
	    return false;	// Since we walked manually, return false
	}
	// Let the walker just process children naturally
	return true;
    }

    if (matchObjectName(obj))
    {
	if (buildAbcPrim())
	    makeAbcPrim(*this, obj, ohead);
	else
	{
	    switch (GABC_Util::getNodeType(obj))
	    {
		case GABC_POLYMESH:
		    makePolyMesh(*this, obj);
		    break;
		case GABC_SUBD:
		    makeSubD(*this, obj);
		    break;
		case GABC_CURVES:
		    makeCurves(*this, obj);
		    break;
		case GABC_POINTS:
		    makePoints(*this, obj);
		    break;
		case GABC_NUPATCH:
		    makeNuPatch(*this, obj);
		    break;
		default:
		    {
			IObject	parent = const_cast<IObject &>(obj).getParent();
			if (parent.valid())
			{
			    fprintf(stderr, "Unknown alembic node type: %s\n",
				    obj.getFullName().c_str());
			}
		    }
	    }
	}
    }

    return true;
}

bool
GABC_GEOWalker::interrupted() const
{
    return myBoss && myBoss->opInterrupt();
}

bool
GABC_GEOWalker::matchObjectName(const IObject &obj) const
{
    UT_String	path(obj.getFullName());
    return path.multiMatch(objectPattern());
}

bool
GABC_GEOWalker::translateAttributeName(GA_AttributeOwner own, UT_String &name)
{
    if (nameMapPtr())
	name = nameMapPtr()->getName(name);
    name.forceValidVariableName();
    return name.multiMatch(myAttributePatterns[own]) != 0;
}

void
GABC_GEOWalker::pushTransform(const M44d &xform, bool const_xform,
	GABC_GEOWalker::TransformState &stash,
	bool inheritXforms)
{
    stash.push(myMatrix, myTransformConstant);
    if (!const_xform)
    {
	myTransformConstant = false;
	myAllTransformConstant = false;
    }
    if (inheritXforms && includeXform())
	myMatrix = xform * myMatrix;
    else
	myMatrix = xform;
}
void
GABC_GEOWalker::popTransform(const GABC_GEOWalker::TransformState &stash)
{
    stash.pop(myMatrix, myTransformConstant);
}

static IObject
getParentXform(IObject kid)
{
    IObject	parent;
    while (true)
    {
	parent = kid.getParent();
	if (!parent)
	{
	    UT_ASSERT(0 && "There should have been a transform");
	    return kid;
	}
	if (IXform::matches(parent.getHeader()))
	    return parent;
	kid = parent;
    }
}

bool
GABC_GEOWalker::getGroupName(UT_String &name, const IObject &obj) const
{
    switch (myGroupMode)
    {
	case ABC_GROUP_NONE:
	    return false;	// No group name
	case ABC_GROUP_SHAPE_NODE:
	    name.harden(obj.getFullName().c_str());
	    break;
	case ABC_GROUP_XFORM_NODE:
	    name.harden(getParentXform(obj).getFullName().c_str());
	    break;
    }
    name.forceValidVariableName();
    return true;
}

void
GABC_GEOWalker::trackPtVtxPrim(const IObject &obj,
				exint npoint, exint nvertex, exint nprim,
				bool do_transform)
{
    UT_ASSERT(myDetail.getNumPoints() >= myPointCount + npoint);
    UT_ASSERT(myDetail.getNumVertices() >= myVertexCount + nvertex);
    UT_ASSERT(myDetail.getNumPrimitives() >= myPrimitiveCount + nprim);
    if (myPathAttribute.isValid() && pathAttributeChanged())
    {
	std::string	 pathStr = obj.getFullName();
	const char	*path = pathStr.c_str();
	for (exint i = 0; i < nprim; ++i)
	{
	    myPathAttribute.set(GA_Offset(myPrimitiveCount+i), path);
	}
    }
    UT_String		 gname;
    GA_PrimitiveGroup	*g = NULL;
    if (getGroupName(gname, obj))
    {
	g = myDetail.newPrimitiveGroup(gname);
	for (exint i = 0; i < nprim; ++i)
	{
	    g->addOffset(GA_Offset(myPrimitiveCount+i));
	}
    }
    if (do_transform && !buildAbcPrim() &&
	    includeXform() && myMatrix != identity44d)
    {
	bool		 rmgroup = false;
	GA_PointGroup	*ptgroup;
	if (!g)
	{
	    g = myDetail.newInternalPrimitiveGroup();
	    rmgroup = true;
	}
	ptgroup = myDetail.newInternalPointGroup();
	for (exint i = 0; i < npoint; ++i)
	{
	    ptgroup->addOffset(GA_Offset(myPointCount+i));
	}
	UT_Matrix4	m4(myMatrix.x);
	detail().transform(m4, g, ptgroup);
	detail().destroyPointGroup(ptgroup);
	if (rmgroup)
	{
	    detail().destroyPrimitiveGroup(g);
	    g = NULL;
	}
    }
    myPointCount += npoint;
    myVertexCount += nvertex;
    myPrimitiveCount += nprim;
}

#include <UT/UT_StopWatch.h>
void
GABC_GEOWalker::test()
{
    UT_StopWatch	timer;
    timer.start();
    {
	GU_Detail		gdp;
	GABC_GEOWalker	walk(gdp);
	if (GABC_Util::walk("test.abc", walk))
	{
	    fprintf(stderr, "Build: %g\n", timer.lap());
	    gdp.save("test.bgeo", NULL);
	    fprintf(stderr, "Save: %g\n", timer.lap());
	}
	else
	    fprintf(stderr, "Unable to walk test.abc\n");
    }
    fprintf(stderr, "Done: %g\n", timer.lap());
}
