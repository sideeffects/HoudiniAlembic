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
 * NAME:	ROP_AbcGeometry.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#include "ROP_AbcGeometry.h"
#include <GT/GT_PrimPolygonMesh.h>
#include <GT/GT_PrimSubdivisionMesh.h>
#include <GT/GT_PrimPolygon.h>
#include <GT/GT_PrimCurveMesh.h>
#include <GT/GT_PrimCurve.h>
#include <GT/GT_PrimPointMesh.h>
#include <GT/GT_PrimNuPatch.h>
#include <GT/GT_TrimNuCurves.h>
#include <GT/GT_DANumeric.h>
#include <GT/GT_DAConstantValue.h>
#include <GT/GT_GEOPrimTPSurf.h>
#include <UT/UT_Vector2.h>
#include <UT/UT_VectorTypes.h>
#include <SOP/SOP_Node.h>
#include <stdarg.h>
#include "ROP_AbcProperty.h"
#include "ROP_AbcError.h"
#include "ROP_AbcContext.h"

namespace
{
    typedef Alembic::Abc::Int32ArraySample	Int32ArraySample;
    typedef Alembic::Abc::FloatArraySample	FloatArraySample;
    typedef Alembic::Abc::V2fArraySample	V2fArraySample;
    typedef Alembic::Abc::V3fArraySample	V3fArraySample;
    typedef Alembic::Abc::P3fArraySample	P3fArraySample;

    Int32ArraySample
    constInt32Array(GT_Size entries, int32 value, GT_DataArrayHandle &storage)
    {
	GT_Int32Array	*result = new GT_Int32Array(entries, 1);
	for (GT_Size i = 0; i < entries; ++i)
	    result->data()[i] = value;
	storage.reset(result);
	return Int32ArraySample(result->data(), result->entries());
    }

    static Int32ArraySample
    subdInt32Array(const GT_PrimSubdivisionMesh::Tag &tag,
	    GT_DataArrayHandle &storage, int index=0)
    {
	const GT_DataArrayHandle	&data = tag.intArray(index);
	return Int32ArraySample(data->getI32Array(storage), data->entries());
    }

    static FloatArraySample
    subdReal32Array(const GT_PrimSubdivisionMesh::Tag &tag,
	    GT_DataArrayHandle &storage, int index=0)
    {
	const GT_DataArrayHandle	&data = tag.realArray(index);
	return FloatArraySample(data->getF32Array(storage), data->entries());
    }

    class SkipList
    {
    public:
	SkipList()
	    : myStrings()
	{
	}
	SkipList(const char *arg0, ...)
	    : myStrings()
	{
	    const char	*token;
	    va_list	 args;
	    bool	 done = false;

	    if (arg0)
	    {
		myStrings.append(arg0);
		va_start(args, arg0);
		while (!done)
		{
		    token = va_arg(args, const char *);
		    if (!token)
			break;
		    myStrings.append(token);
		}
		va_end(args);
	    }
	}
	~SkipList() {}

	bool	contains(const char *token) const
		{
		    for (int i = 0; i < myStrings.entries(); ++i)
		    {
			if (!strcmp(token, myStrings(i)))
			    return true;
		    }
		    return false;
		}

    private:
	UT_PtrArray<const char *>	myStrings;
    };

    static bool
    makeCompoundProperties(ROP_AbcError &err,
			UT_SymbolTable &table,
			const GT_AttributeListHandle &attribs,
			Alembic::Abc::OCompoundProperty &cp,
			Alembic::AbcGeom::GeometryScope scope,
			Alembic::AbcGeom::TimeSamplingPtr ts,
			const SkipList &skips = SkipList())
    {
	int		nwritten = 0;
	UT_Thing	thing;
	if (!attribs)
	    return true;
	for (exint i = 0; i < attribs->entries(); ++i)
	{
	    const char			*name = attribs->getName(i);
	    const GT_DataArrayHandle	&data = attribs->get(i);
	    if (!data || skips.contains(name))
		continue;
	    if (data->getTypeInfo() == GT_TYPE_HIDDEN)
		continue;
	    if (table.findSymbol(name, &thing))
		continue;

	    ROP_AbcProperty	*prop;
	    prop = new ROP_AbcProperty(scope);
	    if (!prop->init(cp, name, data, ts))
	    {
		delete prop;
	    }
	    else
	    {
		table.addSymbol(name, prop);
		err.info("   Arbitrary geometry attribute: %s", name);
		nwritten++;
	    }
	}
	return nwritten > 0;
    }

    static void
    writeCompoundProperties(const UT_SymbolTable &table,
		const GT_AttributeListHandle &attribs)
    {
	UT_Thing	thing;
	if (!attribs || !table.entries())
	    return;

	for (int i = 0; i < attribs->entries(); ++i)
	{
	    const char	*name = attribs->getName(i);
	    if (table.findSymbol(name, &thing))
	    {
		ROP_AbcProperty	 *prop = thing.asPointer<ROP_AbcProperty>();
		prop->writeSample(attribs->get(i));
	    }
	}
    }

    static bool
    isToggleEnabled(OP_Node *node, const char *parameter, fpreal now)
    {
	int	value;
	if (node->evalParameterOrProperty(parameter, 0, now, value))
	    return value != 0;
	return false;
    }

    static bool
    isSubD(SOP_Node *sop, fpreal now)
    {
	OP_Network	*obj = sop->getCreator();
	UT_ASSERT(obj);

	// Check to see whether mantra/RIB sub-division surface rendering
	if (isToggleEnabled(obj, "vm_rendersubd", now) ||
	    isToggleEnabled(obj, "ri_rendersubd", now))
	{
	    return true;
	}
	return false;
    }
};

ROP_AbcGeometry::ROP_AbcGeometry(ROP_AbcError &err,
		SOP_Node *sop,
		const char *name,
		const GT_PrimitiveHandle &prim,
		Alembic::AbcGeom::OXform *parent,
		Alembic::AbcGeom::TimeSamplingPtr ts,
		const ROP_AbcContext &context)
    : myName(UT_String::ALWAYS_DEEP, name)
    , myCurves(NULL)
    , myPolyMesh(NULL)
    , mySubD(NULL)
    , myPoints(NULL)
    , myNuPatch(NULL)
    , myLocator(NULL)
    , myLocatorTrans(NULL)
    , myPointAttribs(0)
    , myVertexAttribs(0)
    , myUniformAttribs(0)
    , myDetailAttribs(0)
    , myPointCount(0)
    , myVertexCount(0)
    , myUniformCount(0)
    , myDetailCount(0)
    , myPrimitiveType(GT_PRIM_UNDEFINED)
    , myTopologyWarned(false)
{
    myPrimitiveType = prim
		? static_cast<GT_PrimitiveType>(prim->getPrimitiveType())
		: GT_PRIM_UNDEFINED;
    switch (myPrimitiveType)
    {
	case GT_PRIM_SUBDIVISION_MESH:
	    mySubD = new Alembic::AbcGeom::OSubD(*parent, name, ts);
	    break;
	case GT_PRIM_POLYGON:
	case GT_PRIM_POLYGON_MESH:
	    if (isSubD(sop, ts->getSampleTime(0)))
		mySubD = new Alembic::AbcGeom::OSubD(*parent, name, ts);
	    else
		myPolyMesh = new Alembic::AbcGeom::OPolyMesh(*parent, name, ts);
	    break;

	case GT_PRIM_CURVE:
	case GT_PRIM_CURVE_MESH:
	    myCurves = new Alembic::AbcGeom::OCurves(*parent, name, ts);
	    break;

	case GT_PRIM_POINT_MESH:
        // could be particles or locator
	    {
		const GT_PrimPointMesh *mesh = UTverify_cast<const GT_PrimPointMesh *>(prim.get());
		const GT_AttributeListHandle &detail = mesh->getDetailAttributes();
		if (detail && detail->hasName("localPosition"))
		{
		    std::string locatorName(name);
		    std::string transformName(name);
		    size_t found = transformName.find("Shape");
		    if (found != std::string::npos)
			transformName = transformName.substr(0, found);
		    myLocatorTrans = new Alembic::AbcGeom::OXform(*parent, transformName, ts);
		    myLocator = new Alembic::AbcGeom::OXform(*myLocatorTrans, locatorName, ts);
		}
		else
		    myPoints = new Alembic::AbcGeom::OPoints(*parent, name, ts);
	    }
		break;

	case GT_GEO_PRIMTPSURF:
	case GT_PRIM_NUPATCH:
	    myNuPatch = new Alembic::AbcGeom::ONuPatch(*parent, name, ts);
	    break;
	default:
	    err.warning("Unsupported primitive: %d", myPrimitiveType);
	    myPrimitiveType = GT_PRIM_UNDEFINED;
	    break;
    }
    if (myPrimitiveType != GT_PRIM_UNDEFINED)
    {
	setElementCounts(prim);
	if (context.saveAttributes())
	    makeProperties(err, prim, ts);
    }
}

ROP_AbcGeometry::~ROP_AbcGeometry()
{
    clearProperties();
    delete myCurves;
    delete myPolyMesh;
    delete mySubD;
    delete myPoints;
    delete myNuPatch;
    delete myLocator;
    delete myLocatorTrans;
}

void
ROP_AbcGeometry::clearProperties()
{
    for (int i = 0; i < MAX_PROPERTIES; ++i)
    {
	for (UT_SymbolTable::traverser it = myProperties[i].begin();
		!it.atEnd(); ++it)
	{
	    delete it.thing().asPointer<ROP_AbcProperty>();
	}
	myProperties[i].clear();
    }
}


void
ROP_AbcGeometry::writeProperties(const GT_PrimitiveHandle &prim)
{
    writeCompoundProperties(myProperties[POINT_PROPERTIES],
	    prim->getPointAttributes());
    writeCompoundProperties(myProperties[VERTEX_PROPERTIES],
	    prim->getVertexAttributes());
    writeCompoundProperties(myProperties[UNIFORM_PROPERTIES],
	    prim->getUniformAttributes());
    writeCompoundProperties(myProperties[DETAIL_PROPERTIES],
	    prim->getDetailAttributes());
}

void
ROP_AbcGeometry::makeProperties(ROP_AbcError &err,
	const GT_PrimitiveHandle &prim,
	Alembic::AbcGeom::TimeSamplingPtr ts)
{
    clearProperties();
    Alembic::Abc::OCompoundProperty	cp;
    if (myPolyMesh)
    {
	SkipList	skips("P", "v", "N", "uv", NULL);
	cp = myPolyMesh->getSchema().getArbGeomParams();
	makeCompoundProperties(err, myProperties[VERTEX_PROPERTIES],
			prim->getVertexAttributes(),
			cp, Alembic::AbcGeom::kFacevaryingScope, ts, skips);
	makeCompoundProperties(err, myProperties[POINT_PROPERTIES],
			prim->getPointAttributes(),
			cp, Alembic::AbcGeom::kVaryingScope, ts, skips);
	makeCompoundProperties(err, myProperties[UNIFORM_PROPERTIES],
			prim->getUniformAttributes(),
			cp, Alembic::AbcGeom::kUniformScope,
			ts);
	makeCompoundProperties(err, myProperties[DETAIL_PROPERTIES],
			prim->getDetailAttributes(),
			cp, Alembic::AbcGeom::kConstantScope, ts);
    }
    else if (myCurves)
    {
	SkipList	skips("P", "v", "N", "uv", "width", NULL);
	cp = myCurves->getSchema().getArbGeomParams();
	makeCompoundProperties(err, myProperties[VERTEX_PROPERTIES],
			prim->getVertexAttributes(),
			cp, Alembic::AbcGeom::kVertexScope, ts, skips);
	makeCompoundProperties(err, myProperties[UNIFORM_PROPERTIES],
			prim->getUniformAttributes(),
			cp, Alembic::AbcGeom::kUniformScope, ts, skips);
	makeCompoundProperties(err, myProperties[DETAIL_PROPERTIES],
			prim->getDetailAttributes(),
			cp, Alembic::AbcGeom::kConstantScope, ts, skips);
    }
    else if (myPoints)
    {
	SkipList	skips("P", "id", "width", "v", NULL);
	cp = myPoints->getSchema().getArbGeomParams();
	makeCompoundProperties(err, myProperties[POINT_PROPERTIES],
			prim->getPointAttributes(),
			cp, Alembic::AbcGeom::kVaryingScope, ts, skips);
	makeCompoundProperties(err, myProperties[DETAIL_PROPERTIES],
			prim->getDetailAttributes(),
			cp, Alembic::AbcGeom::kConstantScope, ts, skips);
    }
    else if (myLocator)
    {
        SkipList skips("P", "id", "width", "v", NULL);
        cp = myLocator->getSchema().getArbGeomParams();
        makeCompoundProperties(err, myProperties[VERTEX_PROPERTIES],
                prim->getVertexAttributes(),
                cp, Alembic::AbcGeom::kVertexScope, ts, skips);
		makeCompoundProperties(err, myProperties[DETAIL_PROPERTIES],
				prim->getDetailAttributes(),
				cp, Alembic::AbcGeom::kConstantScope, ts, skips);
        makeCompoundProperties(err, myProperties[UNIFORM_PROPERTIES],
                prim->getUniformAttributes(),
                cp, Alembic::AbcGeom::kUniformScope, ts, skips);
    }
    else if (mySubD)
    {
	SkipList	skips("P", "v", "uv", "creaseweight", NULL);
	cp = mySubD->getSchema().getArbGeomParams();
	makeCompoundProperties(err, myProperties[VERTEX_PROPERTIES],
			prim->getVertexAttributes(),
			cp, Alembic::AbcGeom::kFacevaryingScope, ts, skips);
	makeCompoundProperties(err, myProperties[POINT_PROPERTIES],
			prim->getPointAttributes(),
			cp, Alembic::AbcGeom::kVaryingScope, ts, skips);
	makeCompoundProperties(err, myProperties[UNIFORM_PROPERTIES],
			prim->getUniformAttributes(),
			cp, Alembic::AbcGeom::kUniformScope, ts);
	makeCompoundProperties(err, myProperties[DETAIL_PROPERTIES],
			prim->getDetailAttributes(),
			cp, Alembic::AbcGeom::kConstantScope, ts);
    }
    else if (myNuPatch)
    {
	SkipList	skips("P", "Pw", "v", "uv", "N", NULL);
	cp = myNuPatch->getSchema().getArbGeomParams();
	makeCompoundProperties(err, myProperties[VERTEX_PROPERTIES],
			prim->getVertexAttributes(),
			cp, Alembic::AbcGeom::kFacevaryingScope, ts, skips);
	makeCompoundProperties(err, myProperties[DETAIL_PROPERTIES],
			prim->getDetailAttributes(),
			cp, Alembic::AbcGeom::kConstantScope, ts);
    }
}

static void
getAttributeCount(const GT_AttributeListHandle &alist,
	exint &acount, exint &ecount)
{
    acount = ecount = 0;
    if (alist)
    {
	acount = alist->entries();
	ecount = alist->get(0)->entries();
    }
}

void
ROP_AbcGeometry::setElementCounts(const GT_PrimitiveHandle &prim)
{
    ::getAttributeCount(prim->getPointAttributes(),
		myPointAttribs, myPointCount);
    ::getAttributeCount(prim->getVertexAttributes(),
		myVertexAttribs, myVertexCount);
    ::getAttributeCount(prim->getUniformAttributes(),
		myUniformAttribs, myUniformCount);
    ::getAttributeCount(prim->getDetailAttributes(),
		myDetailAttribs, myDetailCount);
}

bool
ROP_AbcGeometry::isValid() const
{
    return myCurves ||
		myPolyMesh ||
		mySubD ||
		myPoints ||
		myNuPatch ||
		(myLocatorTrans && myLocator);
}

template <typename T, typename POD_T>
static bool
fillArray(UT_ValArray<T> &array, const GT_DataArrayHandle &gtarray, int tsize)
{
    if (!gtarray || gtarray->getTupleSize() < tsize)
	return false;
    GT_Size	length = gtarray->entries();
    array.resize(length);
    array.entries(length);
    gtarray->fillArray((POD_T *)array.array(), 0, length, tsize);
    return true;
}

/// Split an array of vector3 into 3 scalar arrays
static void
splitVector3Array(const UT_Vector3Array &xyz,
	    UT_FloatArray &x,
	    UT_FloatArray &y,
	    UT_FloatArray &z)
{
    x.resizeIfNeeded(xyz.entries());
    y.resizeIfNeeded(xyz.entries());
    z.resizeIfNeeded(xyz.entries());
    for (exint i = 0; i < xyz.entries(); ++i)
    {
	x.append(xyz(i).x());
	y.append(xyz(i).y());
	z.append(xyz(i).z());
    }
}

template <typename T, typename POD_T>
static bool
fillCounts(UT_ValArray<T> &array, const GT_CountArray &counts)
{
    fillArray<T, POD_T>(array, counts.extractCounts(), 1);
    return true;
}

template <typename T, typename POD_T,
	 typename ABC_SAMPLE, typename ABC_ARRAY, typename ABC_TYPE>
static bool
fillAttribute(ABC_SAMPLE &sample, UT_ValArray<T> &array,
	const char *name, int tsize,
	const GT_AttributeListHandle &point,
	const GT_AttributeListHandle &vertex = GT_AttributeListHandle(),
	const GT_AttributeListHandle &uniform = GT_AttributeListHandle(),
	const GT_AttributeListHandle &detail = GT_AttributeListHandle())
{
    GT_DataArrayHandle	data;
    if (point && (data = point->get(name)))
    {
	if (fillArray<T, POD_T>(array, data, tsize))
	{
	    sample.setScope(Alembic::AbcGeom::kVertexScope);
	    sample.setVals(
		ABC_ARRAY((ABC_TYPE *)array.array(), array.entries()));
	    return true;
	}
    }
    if (vertex && (data = vertex->get(name)))
    {
	if (fillArray<T, POD_T>(array, data, tsize))
	{
	    sample.setScope(Alembic::AbcGeom::kFacevaryingScope);
	    sample.setVals(
		ABC_ARRAY((const ABC_TYPE *)array.array(), array.entries()));
	    return true;
	}
    }
    if (uniform && (data = uniform->get(name)))
    {
	if (fillArray<T, POD_T>(array, data, tsize))
	{
	    sample.setScope(Alembic::AbcGeom::kUniformScope);
	    sample.setVals(
		ABC_ARRAY((ABC_TYPE *)array.array(), array.entries()));
	    return true;
	}
    }
    if (detail && (data = detail->get(name)))
    {
	if (fillArray<T, POD_T>(array, data, tsize))
	{
	    sample.setScope(Alembic::AbcGeom::kConstantScope);
	    sample.setVals(
		ABC_ARRAY((ABC_TYPE *)array.array(), array.entries()));
	    return true;
	}
    }
    return false;
}

bool
ROP_AbcGeometry::writeSubdMesh(ROP_AbcError &err,
	    const GT_PrimitiveHandle &prim)
{
    const GT_PrimPolygonMesh		*mesh = UTverify_cast<const GT_PrimPolygonMesh *>(prim.get());
    const GT_AttributeListHandle	&pt = mesh->getPointAttributes();
    const GT_AttributeListHandle	&vtx = mesh->getVertexAttributes();
    if (!pt || !pt->get("P"))
    {
	return err.error("%s missing subd mesh 'P' attribute",
		(const char *)myName);
    }

    UT_Int32Array				vertices;
    UT_Int32Array				counts;
    UT_Vector3Array				P;
    UT_Vector3Array				vel;
    UT_Vector2Array				uv;
    Alembic::AbcGeom::OP3fGeomParam::Sample	abcP;
    Alembic::AbcGeom::OV2fGeomParam::Sample	abcUV;
    Alembic::AbcGeom::OV3fGeomParam::Sample	abcVel;

    fillArray<int32, int32>(vertices, mesh->getVertexList(), 1);
    fillCounts<int32, int32>(counts, mesh->getFaceCountArray());

    fillAttribute<UT_Vector3, fpreal32,
		Alembic::AbcGeom::OP3fGeomParam::Sample,
		P3fArraySample,
		Alembic::AbcGeom::V3f>(abcP, P, "P", 3,
				pt, GT_AttributeListHandle());
    fillAttribute<UT_Vector2, fpreal32,
		Alembic::AbcGeom::OV2fGeomParam::Sample,
		V2fArraySample,
		Alembic::AbcGeom::V2f>(abcUV, uv, "uv", 2, pt, vtx);
    fillAttribute<UT_Vector3, fpreal32,
		Alembic::AbcGeom::OV3fGeomParam::Sample,
		V3fArraySample,
		Alembic::AbcGeom::V3f>(abcVel, vel, "v", 3,
				pt, mesh->getVertexAttributes());

    // Subdivision tags ------------------------------------
    // GT_DataArrays store the data for the array samples.  Must be declared
    // outside the conditions so their storage remains in scope.
    GT_DataArrayHandle	creaseIndices;
    GT_DataArrayHandle	creaseLengths;
    GT_DataArrayHandle	creaseSharpness;
    GT_DataArrayHandle	cornerIndices;
    GT_DataArrayHandle	cornerSharpness;
    GT_DataArrayHandle	holeIndices;
    // Alembic typed array samples are used to pass the data to the subd sample
    Int32ArraySample	abcCreaseIndices;
    Int32ArraySample	abcCreaseLengths;
    FloatArraySample	abcCreaseSharpness;
    Int32ArraySample	abcCornerIndices;
    FloatArraySample	abcCornerSharpness;
    Int32ArraySample	abcHoleIndices;

    if (prim->getPrimitiveType() == GT_PRIM_SUBDIVISION_MESH)
    {
	// Here, we have a subdivision mesh, so we can extract the tag
	// information.
	const GT_PrimSubdivisionMesh		*subd;
	const GT_PrimSubdivisionMesh::Tag	*tag;

	subd = UTverify_cast<const GT_PrimSubdivisionMesh *>(prim.get());
	tag = subd->findTag("crease");
	if (tag && tag->intCount() == 1 && tag->realCount() == 1)
	{
	    abcCreaseIndices = subdInt32Array(*tag, creaseIndices);
	    abcCreaseSharpness = subdReal32Array(*tag, creaseSharpness);
	    abcCreaseLengths = constInt32Array(tag->realArray()->entries(), 2,
					creaseLengths);
	}
	tag = subd->findTag("corner");
	if (tag && tag->intCount() == 1 && tag->realCount() == 1)
	{
	    abcCornerIndices = subdInt32Array(*tag, cornerIndices);
	    abcCornerSharpness = subdReal32Array(*tag, cornerSharpness);
	}
	tag = subd->findTag("hole");
	if (tag && tag->intCount() == 1)
	    abcHoleIndices = subdInt32Array(*tag, holeIndices);
    }
    Alembic::AbcGeom::OSubDSchema::Sample	sample(
	    abcP.getVals(),
	    Int32ArraySample(vertices.array(), vertices.entries()),
	    Int32ArraySample(counts.array(), counts.entries()),
	    abcCreaseIndices,	// Crease indices
	    abcCreaseLengths,	// Crease lengths
	    abcCreaseSharpness,	// Crease sharpnesses
	    abcCornerIndices,	// Corner indices
	    abcCornerSharpness,	// Corner sharpnesses
	    abcHoleIndices);	// Holes

    if (vel.entries())
	sample.setVelocities(abcVel.getVals());
    if (uv.entries())
	sample.setUVs(abcUV);

    mySubD->getSchema().set(sample);
    writeProperties(prim);

    err.info("SubDMesh: %" SYS_PRId64 " vertices, %" SYS_PRId64 " faces",
		vertices.entries(), counts.entries());
    return true;
}

bool
ROP_AbcGeometry::writePolygonMesh(ROP_AbcError &err,
	    const GT_PrimitiveHandle &prim)
{
    if (!myPolyMesh)
    {
	UT_ASSERT(mySubD);
	return writeSubdMesh(err, prim);
    }
    const GT_PrimPolygonMesh		*mesh = UTverify_cast<const GT_PrimPolygonMesh *>(prim.get());
    const GT_AttributeListHandle	&pt = mesh->getPointAttributes();
    const GT_AttributeListHandle	&vtx = mesh->getVertexAttributes();
    if (!pt || !pt->get("P"))
    {
	return err.error("%s missing polygon mesh 'P' attribute",
		(const char *)myName);
    }

    UT_Int32Array				vertices;
    UT_Int32Array				counts;
    UT_Vector3Array				P;
    UT_Vector3Array				N;
    UT_Vector3Array				vel;
    UT_Vector2Array				uv;
    Alembic::AbcGeom::OP3fGeomParam::Sample	abcP;
    Alembic::AbcGeom::ON3fGeomParam::Sample	abcN;
    Alembic::AbcGeom::OV2fGeomParam::Sample	abcUV;
    Alembic::AbcGeom::OV3fGeomParam::Sample	abcVel;

    fillArray<int32, int32>(vertices, mesh->getVertexList(), 1);
    fillCounts<int32, int32>(counts, mesh->getFaceCountArray());

    fillAttribute<UT_Vector3, fpreal32,
		Alembic::AbcGeom::OP3fGeomParam::Sample,
		Alembic::AbcGeom::P3fArraySample,
		Alembic::AbcGeom::V3f>(abcP, P, "P", 3, pt, GT_AttributeListHandle());
    fillAttribute<UT_Vector3, fpreal32,
		Alembic::AbcGeom::ON3fGeomParam::Sample,
		Alembic::AbcGeom::N3fArraySample,
		Alembic::AbcGeom::N3f>(abcN, N, "N", 3, pt, vtx);
    fillAttribute<UT_Vector2, fpreal32,
		Alembic::AbcGeom::OV2fGeomParam::Sample,
		Alembic::AbcGeom::V2fArraySample,
		Alembic::AbcGeom::V2f>(abcUV, uv, "uv", 2, pt, vtx);
    fillAttribute<UT_Vector3, fpreal32,
		Alembic::AbcGeom::OV3fGeomParam::Sample,
		Alembic::AbcGeom::V3fArraySample,
		Alembic::AbcGeom::V3f>(abcVel, vel, "v", 3,
				pt, mesh->getVertexAttributes());

    Alembic::AbcGeom::OPolyMeshSchema::Sample	mesh_sample(
	    abcP.getVals(),
	    Alembic::AbcGeom::Int32ArraySample(vertices.array(), vertices.entries()),
	    Alembic::AbcGeom::Int32ArraySample(counts.array(), counts.entries()),
	    abcUV, abcN);

    if (vel.entries())
	mesh_sample.setVelocities(abcVel.getVals());

    myPolyMesh->getSchema().set(mesh_sample);
    writeProperties(prim);

    err.info("PolygonMesh: %" SYS_PRId64 " vertices, %" SYS_PRId64 " faces",
		vertices.entries(), counts.entries());
    return true;
}

bool
ROP_AbcGeometry::writeLocator(ROP_AbcError &err,
		const GT_PrimitiveHandle &prim)
{
    const GT_PrimPointMesh *mesh = UTverify_cast<const GT_PrimPointMesh *>(prim.get());
    const GT_AttributeListHandle &detail = mesh->getDetailAttributes();

    double val[6] = {0};
    Imath::V3d t, r, s;
    if (detail)
    {
        GT_DataArrayHandle data = detail->get("localPosition");
        if (data && data->getTupleSize() == 3 && data->entries() == 1)
        {
            val[0] = data->getF32(0, 0);
            val[1] = data->getF32(0, 1);
            val[2] = data->getF32(0, 2);
        }

        data = detail->get("localScale");
        if (data && data->getTupleSize() == 3 && data->entries() == 1)
        {
            val[3] = data->getF32(0, 0);
            val[4] = data->getF32(0, 1);
            val[5] = data->getF32(0, 2);
        }

        data = detail->get("parentTrans");
        if (data && data->getTupleSize() == 3 && data->entries() == 1)
        {
            t[0] = data->getF32(0, 0);
            t[1] = data->getF32(0, 1);
            t[2] = data->getF32(0, 2);
        }

        data = detail->get("parentRot");
        if (data && data->getTupleSize() == 3 && data->entries() == 1)
        {
            r[0] = data->getF32(0, 0);
            r[1] = data->getF32(0, 1);
            r[2] = data->getF32(0, 2);
        }

        data = detail->get("parentScale");
        if (data && data->getTupleSize() == 3 && data->entries() == 1)
        {
            s[0] = data->getF32(0, 0);
            s[1] = data->getF32(0, 1);
            s[2] = data->getF32(0, 2);
        }
    }

    Alembic::Abc::OCompoundProperty locator_sample = myLocator->getProperties();
    // of type double[6]
    Alembic::AbcCoreAbstract::DataType dType(Alembic::Util::kFloat64POD, 6);
    Alembic::Abc::OScalarProperty sp =
            Alembic::Abc::OScalarProperty(locator_sample, "locator", dType);
    sp.set(val);

    Alembic::AbcGeom::OXformSchema xformSchema = myLocatorTrans->getSchema();
    Alembic::AbcGeom::XformSample xformSample;

    // push transform, rotate, scale
    // transform
    Alembic::AbcGeom::XformOp op(Alembic::AbcGeom::kTranslateOperation,
                                Alembic::AbcGeom::kTranslateHint);
    if (t[0] != 0.0 || t[1] != 0.0 || t[2] != 0.0)
    {
        op.setChannelValue(0, t[0]);
        op.setChannelValue(1, t[1]);
        op.setChannelValue(2, t[2]);
        xformSample.addOp(op);
    }
    // rotate (x, y, z) - add in reverse order since it is a stack
    op = Alembic::AbcGeom::XformOp(Alembic::AbcGeom::kRotateZOperation,
                                Alembic::AbcGeom::kRotateHint);
    op.setChannelValue(0, Alembic::AbcGeom::RadiansToDegrees(r[2]));
    xformSample.addOp(op);
    op = Alembic::AbcGeom::XformOp(Alembic::AbcGeom::kRotateYOperation,
                                Alembic::AbcGeom::kRotateHint);
    op.setChannelValue(0, Alembic::AbcGeom::RadiansToDegrees(r[1]));
    xformSample.addOp(op);
    op = Alembic::AbcGeom::XformOp(Alembic::AbcGeom::kRotateXOperation,
                                Alembic::AbcGeom::kRotateHint);
    op.setChannelValue(0, Alembic::AbcGeom::RadiansToDegrees(r[0]));
    xformSample.addOp(op);    
    // scale
    op = Alembic::AbcGeom::XformOp(Alembic::AbcGeom::kScaleOperation,
                                Alembic::AbcGeom::kScaleHint);
    if (s[0] != 0.0 || s[1] != 0.0 || s[2] != 0.0)
    {
        op.setChannelValue(0, s[0]);
        op.setChannelValue(1, s[1]);
        op.setChannelValue(2, s[2]);
        xformSample.addOp(op);
    }

    if (xformSample.getNumOps() != 0)
        xformSchema.set(xformSample);

    writeProperties(prim);

    return true;
}

bool
ROP_AbcGeometry::writePointMesh(ROP_AbcError &err,
		const GT_PrimitiveHandle &prim)
{
    const GT_PrimPointMesh		*mesh = UTverify_cast<const GT_PrimPointMesh *>(prim.get());
    const GT_AttributeListHandle	&pts = mesh->getPointAttributes();
    const GT_AttributeListHandle	&detail = mesh->getDetailAttributes();

    if (!pts || !pts->get("P"))
    {
	return err.error("%s missing curve mesh 'P' attribute",
		(const char *)myName);
    }

    UT_Vector3Array				P;
    UT_Vector3Array				v;
    UT_Int64Array				id;
    UT_FloatArray				widths;
    Alembic::AbcGeom::OP3fGeomParam::Sample	abcP;
    Alembic::AbcGeom::OV3fGeomParam::Sample	abcV;
    Alembic::AbcGeom::OUInt64GeomParam::Sample	abcID;
    Alembic::AbcGeom::OFloatGeomParam::Sample	abcWidths;

    fillAttribute<UT_Vector3, fpreal32,
		Alembic::AbcGeom::OP3fGeomParam::Sample,
		Alembic::AbcGeom::P3fArraySample,
		Alembic::AbcGeom::V3f>(abcP, P, "P", 3, pts);
    fillAttribute<UT_Vector3, fpreal32,
		Alembic::AbcGeom::OV3fGeomParam::Sample,
		Alembic::AbcGeom::V3fArraySample,
		Alembic::AbcGeom::V3f>(abcV, v, "v", 3, pts);
    fillAttribute<fpreal32, fpreal32,
		Alembic::AbcGeom::OFloatGeomParam::Sample,
		FloatArraySample,
		fpreal32>(abcWidths, widths, "width", 1,
				pts, GT_AttributeListHandle(),
				GT_AttributeListHandle(), detail);
    if (pts->get("id"))
    {
	fillAttribute<int64, int64,
		    Alembic::AbcGeom::OUInt64GeomParam::Sample,
		    Alembic::AbcGeom::UInt64ArraySample,
		    Alembic::AbcGeom::uint64_t>(abcID, id, "id", 1, pts);
    }
    else if (pts->get("__vertex_id"))
    {
	fillAttribute<int64, int64,
		    Alembic::AbcGeom::OUInt64GeomParam::Sample,
		    Alembic::AbcGeom::UInt64ArraySample,
		    Alembic::AbcGeom::uint64_t>(abcID, id, "__vertex_id", 1, pts);
    }
    else if (pts->get("__point_id"))
    {
	fillAttribute<int64, int64,
		    Alembic::AbcGeom::OUInt64GeomParam::Sample,
		    Alembic::AbcGeom::UInt64ArraySample,
		    Alembic::AbcGeom::uint64_t>(abcID, id, "__point_id", 1, pts);
    }
    else
    {
	for (exint i = 0; i < P.entries(); ++i)
	    id.append(i);
	abcID.setScope(Alembic::AbcGeom::kVertexScope);
	Alembic::AbcGeom::UInt64ArraySample	idarray((Alembic::AbcGeom::uint64_t *)id.array(), id.entries());
	abcID.setVals(idarray);
    }

    Alembic::AbcGeom::OPointsSchema::Sample	point_sample(
		abcP.getVals(),
		abcID.getVals(),
		abcV.getVals(),
		abcWidths);

    myPoints->getSchema().set(point_sample);
    writeProperties(prim);

    err.info("PointMesh %" SYS_PRId64 " points", P.entries());
    return true;
}

bool
ROP_AbcGeometry::writeCurveMesh(ROP_AbcError &err,
		const GT_PrimitiveHandle &prim)
{
    const GT_PrimCurveMesh		*mesh = UTverify_cast<const GT_PrimCurveMesh *>(prim.get());
    const GT_AttributeListHandle	&vtx = mesh->getVertexAttributes();
    const GT_AttributeListHandle	&uniform = mesh->getUniformAttributes();
    const GT_AttributeListHandle	&detail = mesh->getDetailAttributes();
    if (!vtx || !vtx->get("P"))
    {
	return err.error("%s missing curve mesh 'P' attribute",
		(const char *)myName);
    }

    UT_Int32Array				counts;
    UT_Vector3Array				P;
    UT_Vector3Array				N;
    UT_Vector3Array				vel;
    UT_Vector2Array				uv;
    UT_FloatArray				widths;
    Alembic::AbcGeom::OP3fGeomParam::Sample	abcP;
    Alembic::AbcGeom::ON3fGeomParam::Sample	abcN;
    Alembic::AbcGeom::OV2fGeomParam::Sample	abcUV;
    Alembic::AbcGeom::OFloatGeomParam::Sample	abcWidths;
    Alembic::AbcGeom::CurveType			degree;
    Alembic::AbcGeom::CurvePeriodicity		periodicity;
    Alembic::AbcGeom::BasisType			basis;
    Alembic::AbcGeom::OV3fGeomParam::Sample	abcVel;

    switch (mesh->getBasis())
    {
	case GT_BASIS_LINEAR:
	    degree = Alembic::AbcGeom::kLinear;
	    basis = Alembic::AbcGeom::kNoBasis;
	    break;
	case GT_BASIS_BEZIER:
	    degree = Alembic::AbcGeom::kCubic;
	    basis = Alembic::AbcGeom::kBezierBasis;
	    break;
    }

    periodicity = (mesh->getWrap())
			? Alembic::AbcGeom::kPeriodic
			: Alembic::AbcGeom::kNonPeriodic;

    fillCounts<int32, int32>(counts, mesh->getCurveCountArray());

    fillAttribute<UT_Vector3, fpreal32,
		Alembic::AbcGeom::OP3fGeomParam::Sample,
		Alembic::AbcGeom::P3fArraySample,
		Alembic::AbcGeom::V3f>(abcP, P, "P", 3, vtx);
    fillAttribute<UT_Vector3, fpreal32,
		Alembic::AbcGeom::ON3fGeomParam::Sample,
		Alembic::AbcGeom::N3fArraySample,
		Alembic::AbcGeom::N3f>(abcN, N, "N", 3, vtx);
    fillAttribute<UT_Vector2, fpreal32,
		Alembic::AbcGeom::OV2fGeomParam::Sample,
		Alembic::AbcGeom::V2fArraySample,
		Alembic::AbcGeom::V2f>(abcUV, uv, "uv", 2, vtx);
    fillAttribute<fpreal32, fpreal32,
		Alembic::AbcGeom::OFloatGeomParam::Sample,
		Alembic::AbcGeom::FloatArraySample,
		fpreal32>(abcWidths, widths, "width", 1,
			vtx, GT_AttributeListHandle(), uniform, detail);
    fillAttribute<UT_Vector3, fpreal32,
		Alembic::AbcGeom::OV3fGeomParam::Sample,
		Alembic::AbcGeom::V3fArraySample,
		Alembic::AbcGeom::V3f>(abcVel, vel, "v", 3, vtx);

    Alembic::AbcGeom::OCurvesSchema::Sample	curve_sample(
	    abcP.getVals(),
	    Alembic::AbcGeom::Int32ArraySample(counts.array(), counts.entries()),
	    degree,
	    periodicity,
	    abcWidths,
	    abcUV,
	    abcN,
	    basis);

    if (vel.entries())
	curve_sample.setVelocities(abcVel.getVals());

    myCurves->getSchema().set(curve_sample);
    writeProperties(prim);

    err.info("CurveMesh %" SYS_PRId64 " curves", counts.entries());
    return true;
}

bool
ROP_AbcGeometry::writeNuPatch(ROP_AbcError &err,
	    const GT_PrimitiveHandle &prim)
{
    GT_PrimitiveHandle		 tmp_nupatch;
    if (prim->getPrimitiveType() == GT_GEO_PRIMTPSURF)
    {
	// Need to convert to a GT_PrimNuPatch primitive
	const GT_GEOPrimTPSurf	*geopatch;
	geopatch = UTverify_cast<const GT_GEOPrimTPSurf *>(prim.get());
	tmp_nupatch = geopatch->buildNuPatch();
    }
    else
    {
	tmp_nupatch = prim;
    }
    UT_ASSERT(tmp_nupatch->getPrimitiveType() == GT_PRIM_NUPATCH);
    if (tmp_nupatch->getPrimitiveType() != GT_PRIM_NUPATCH)
	return err.error("%s needs a NuPatch", (const char *)myName);

    const GT_PrimNuPatch		*nupatch;
    nupatch = UTverify_cast<const GT_PrimNuPatch *>(tmp_nupatch.get());

    const GT_AttributeListHandle	&pt = nupatch->getVertexAttributes();
    if (!pt || !pt->get("P"))
    {
	return err.error("%s missing NuPatch 'P' attribute",
		(const char *)myName);
    }

    UT_FloatArray				uknots;
    UT_FloatArray				vknots;
    UT_Vector3Array				P;
    UT_FloatArray				Pw;
    GT_DataArrayHandle				gtPw;
    UT_Vector3Array				vel;
    UT_Vector2Array				uv;
    Alembic::AbcGeom::OP3fGeomParam::Sample	abcP;
    Alembic::AbcGeom::OV2fGeomParam::Sample	abcUV;
    Alembic::AbcGeom::OV3fGeomParam::Sample	abcVel;

    gtPw = pt->get("Pw");
    if (gtPw)
    {
	fillArray<fpreal32, fpreal32>(Pw, gtPw, 1);
    }
    fillArray<fpreal32, fpreal32>(uknots, nupatch->getUKnots(), 1);
    fillArray<fpreal32, fpreal32>(vknots, nupatch->getVKnots(), 1);

    fillAttribute<UT_Vector3, fpreal32,
		Alembic::AbcGeom::OP3fGeomParam::Sample,
		Alembic::AbcGeom::P3fArraySample,
		Alembic::AbcGeom::V3f>(abcP, P, "P", 3, pt, GT_AttributeListHandle());
    fillAttribute<UT_Vector2, fpreal32,
		Alembic::AbcGeom::OV2fGeomParam::Sample,
		Alembic::AbcGeom::V2fArraySample,
		Alembic::AbcGeom::V2f>(abcUV, uv, "uv", 2, pt, pt);
    fillAttribute<UT_Vector3, fpreal32,
		Alembic::AbcGeom::OV3fGeomParam::Sample,
		Alembic::AbcGeom::V3fArraySample,
		Alembic::AbcGeom::V3f>(abcVel, vel, "v", 3,
				pt, nupatch->getVertexAttributes());

    Alembic::AbcGeom::ONuPatchSchema::Sample	sample(
	    abcP.getVals(),
	    nupatch->getNu(),
	    nupatch->getNv(),
	    nupatch->getUOrder(),
	    nupatch->getVOrder(),
	    Alembic::Abc::FloatArraySample(uknots.array(), uknots.entries()),
	    Alembic::Abc::FloatArraySample(vknots.array(), vknots.entries()),
	    Alembic::AbcGeom::ON3fGeomParam::Sample(),
	    abcUV,
	    Alembic::Abc::FloatArraySample(Pw.array(), Pw.entries())
    );

    if (vel.entries())
	sample.setVelocities(abcVel.getVals());

    // The data for the trim curves must be persistent until the sample is
    // written, so it must be declared outside the check for trimmed curves.
    UT_IntArray	trim_nCurves;
    UT_IntArray	trim_n;
    UT_IntArray	trim_order;
    UT_FloatArray	trim_knot;
    UT_FloatArray	trim_min;
    UT_FloatArray	trim_max;
    UT_Vector3Array	trim_uvw;
    UT_FloatArray	trim_u;
    UT_FloatArray	trim_v;
    UT_FloatArray	trim_w;

    if (nupatch->isTrimmed())
    {
	const GT_TrimNuCurves	*trims = nupatch->getTrimCurves();

	fillCounts<int32, int32>(trim_nCurves, trims->getLoopCountArray());
	fillCounts<int32, int32>(trim_n, trims->getCurveCountArray());
	fillArray<int32, int32>(trim_order, trims->getOrders(), 1);
	fillArray<fpreal32, fpreal32>(trim_knot, trims->getKnots(), 1);
	fillArray<fpreal32, fpreal32>(trim_min, trims->getMin(), 1);
	fillArray<fpreal32, fpreal32>(trim_max, trims->getMax(), 1);
	fillArray<UT_Vector3, fpreal32>(trim_uvw, trims->getUV(), 3);
	splitVector3Array(trim_uvw, trim_u, trim_v, trim_w);

	sample.setTrimCurve(
	    trim_nCurves.entries(),
	    Alembic::AbcGeom::Int32ArraySample(trim_nCurves.array(), trim_nCurves.entries()),
	    Alembic::AbcGeom::Int32ArraySample(trim_n.array(), trim_n.entries()),
	    Alembic::AbcGeom::Int32ArraySample(trim_order.array(), trim_order.entries()),
	    Alembic::AbcGeom::FloatArraySample(trim_knot.array(), trim_knot.entries()),
	    Alembic::AbcGeom::FloatArraySample(trim_min.array(), trim_min.entries()),
	    Alembic::AbcGeom::FloatArraySample(trim_max.array(), trim_max.entries()),
	    Alembic::AbcGeom::FloatArraySample(trim_u.array(), trim_u.entries()),
	    Alembic::AbcGeom::FloatArraySample(trim_v.array(), trim_v.entries()),
	    Alembic::AbcGeom::FloatArraySample(trim_w.array(), trim_w.entries())
	);
    }

    myNuPatch->getSchema().set(sample);
    writeProperties(prim);

    err.info("NuPatch: %d x %d", nupatch->getNu(), nupatch->getNv());
    return true;
}

static GT_PrimitiveHandle
curveMeshFromCurve(const GT_PrimitiveHandle &src)
{
    UT_ASSERT(src->getPrimitiveType() == GT_PRIM_CURVE);
    const GT_PrimCurve	*curve;
    curve = UTverify_cast<const GT_PrimCurve *>(src.get());
    return GT_PrimitiveHandle( new GT_PrimCurveMesh(*curve) );
}

static GT_PrimitiveHandle
polygonMeshFromPolygon(const GT_PrimitiveHandle &src)
{
    UT_ASSERT(src->getPrimitiveType() == GT_PRIM_POLYGON);
    const GT_PrimPolygon	*poly;
    poly = UTverify_cast<const GT_PrimPolygon *>(src.get());
    return GT_PrimitiveHandle( new GT_PrimPolygonMesh(*poly) );
}

bool
ROP_AbcGeometry::writeSample(ROP_AbcError &err, const GT_PrimitiveHandle &prim)
{
    if (prim->getPrimitiveType() != myPrimitiveType)
    {
	if (!myTopologyWarned)
	{
	    myTopologyWarned = true;
	    err.warning("%s geometry primitive type mismatch (%d != %d)",
		    (const char *)myName,
		    prim->getPrimitiveType(),
		    myPrimitiveType);
	}
	return false;
    }

    switch (myPrimitiveType)
    {
	case GT_PRIM_SUBDIVISION_MESH:
	    return writeSubdMesh(err, prim);
	case GT_PRIM_POLYGON_MESH:
	    return writePolygonMesh(err, prim);
	case GT_PRIM_CURVE_MESH:
	    return writeCurveMesh(err, prim);

	case GT_PRIM_POLYGON:
	    return writePolygonMesh(err, polygonMeshFromPolygon(prim));
	case GT_PRIM_CURVE:
	    return writeCurveMesh(err, curveMeshFromCurve(prim));

	case GT_PRIM_POINT_MESH:
	    if (myPoints)
		return writePointMesh(err, prim);
	    else if (myLocator)
		return writeLocator(err, prim);
	    UT_ASSERT(0);
	    break;

	case GT_GEO_PRIMTPSURF:
	case GT_PRIM_NUPATCH:
	    return writeNuPatch(err, prim);

	default:
	    break;
    }
    err.error("Unhandled primitive case: %d", myPrimitiveType);
    return false;
}
