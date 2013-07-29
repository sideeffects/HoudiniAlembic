/*
 * Copyright (c) 2013
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
#include <GABC/GABC_Include.h>	// For Windows linking

#include "VRAY_ProcAlembic.h"
#include <GABC/GABC_GEOWalker.h>
#include <GU/GU_PrimPacked.h>
#include <GABC/GABC_PackedImpl.h>
#include <GABC/GABC_IObject.h>
#include <GT/GT_Primitive.h>
#include <UT/UT_WorkArgs.h>
#include <UT/UT_EnvControl.h>
#include <UT/UT_StringMMPattern.h>
#include <UT/UT_JSONValue.h>
#include <UT/UT_JSONParser.h>
#include "VRAY_ProcGT.h"
#include "VRAY_IO.h"

//#define SHOW_COUNTS
#include <UT/UT_ShowCounts.h>
UT_COUNTER(thePrimCount, "Alembic Primitives");

using namespace GABC_NAMESPACE;

namespace
{
    typedef UT_SymbolMap<std::string>			vray_PropertyMap;
    typedef UT_SharedPtr<vray_PropertyMap>		vray_PropertyMapPtr;
    typedef VRAY_ProcAlembic::vray_MergePatternPtr	vray_MergePatternPtr;

    inline static const GA_PrimitiveTypeId &
    alembicTypeId()
    {
	return GABC_PackedImpl::typeId();
    }

    static const GABC_PackedImpl *implementation(const GU_PrimPacked *prim)
    {
	return UTverify_cast<const GABC_PackedImpl *>(prim->implementation());
    }

    static void
    enlargeVelocityBox(UT_BoundingBox &box,
	    const UT_Vector3 &vmin, const UT_Vector3 &vmax,
	    fpreal preblur, fpreal postblur)
    {
	for (int i = 0; i < 3; ++i)
	{
	    // Each point is in the range:
	    //	(P - preblur*v, P + postblur*v)
	    // Thus, the minimum values of the bounding box are enlarged by
	    // the maximum amount specified by:
	    //	  vmin < 0 ?  vmin*postblur
	    //	  vmax > 0 ? -vmax*preblur
	    // The maximum values are enlarged by
	    //	  vmin < 0 ? -vmin*preblur
	    //	  vmax > 0 ?  vmax*postblur
	    fpreal	dmin = 0, dmax = 0;
	    dmin = SYSmin(dmin,  vmin(i)*postblur);
	    dmin = SYSmin(dmin, -vmax(i)*preblur);
	    dmax = SYSmax(dmax, -vmin(i)*preblur);
	    dmax = SYSmax(dmax,  vmax(i)*postblur);
	    box(i, 0) += dmin;
	    box(i, 1) += dmax;
	}
    }

    class vray_ProcAlembicPrim : public VRAY_Procedural
    {
    public:
	vray_ProcAlembicPrim(const UT_Array<const GU_PrimPacked *> &list,
		fpreal preblur, fpreal postblur,
		const GU_PrimPacked *aprim,
		const vray_MergePatternPtr &merge,
		const vray_PropertyMapPtr &propertymap)
	    : myList(list)
	    , myPreBlur(preblur)
	    , myPostBlur(postblur)
	    , myMergePrim(aprim)
	    , myMergeInfo(merge)
	    , myPropertyMap(propertymap)
	{
	}
	virtual ~vray_ProcAlembicPrim()
	{
	}
	virtual const char	*getClassName()
				    { return "vray_ProcAlembicPrim"; }
	virtual int	 initialize(const UT_BoundingBox *)
				    { return myList.entries() > 0; }
	virtual void	 getBoundingBox(UT_BoundingBox &box)
			 {
			     box.initBounds();
			     for (exint i = 0; i < myList.entries(); ++i)
			     {
				 UT_BoundingBox	tbox;
				 myList(i)->getRenderingBounds(tbox);
				 if (myPreBlur != 0 || myPostBlur != 0)
				 {
				     UT_Vector3	vmin, vmax;
				     myList(i)->getVelocityRange(vmin, vmax);
				     enlargeVelocityBox(tbox, vmin, vmax,
					     myPreBlur, myPostBlur);
				 }
				 box.enlargeBounds(tbox);
			     }
			 }
	void	setIntegerProperty(const char *name,
			const GT_DataArrayHandle &data)
	{
	    GT_DataArrayHandle	storage;
	    changeSetting(name, data->entries(), data->getI32Array(storage));
	}
	void	setRealProperty(const char *name,
			const GT_DataArrayHandle &data)
	{
	    GT_DataArrayHandle	storage;

#if SIZEOF_FPREAL_IS_4
	    changeSetting(name, data->entries(), data->getF32Array(storage));
#else
	    changeSetting(name, data->entries(), data->getF64Array(storage));
#endif
	}
	void	changeShader(const char *name, const char *shader)
	{
	    // Mantra expects arguments for a shader, not a single string
	    UT_String	str(shader);
	    UT_WorkArgs	args;
	    str.parse(args);
	    if (args.getArgc())
		changeSetting(name, args.getArgc(), args.getArgv());
	}
	static bool	isShader(const char *name)
	{
	    return !strcmp(name, "surface") ||
		    !strcmp(name, "displace") ||
		    !strcmp(name, "matteshader");

	}
	void	setStringProperty(const char *name,
			const GT_DataArrayHandle &data)
	{
	    exint		size = data->entries();
	    int			tsize = data->getTupleSize();
	    UT_StackBuffer<char *>	values(size*tsize);
	    for (exint i = 0; i < size; ++i)
		for (int j = 0; j < tsize; ++j)
		    values[i*tsize+j] = (char *)data->getS(i, j);
	    if (size == 1 && tsize == 1 && isShader(name))
		changeShader(name, values[0]);
	    else
		changeSetting(name, size*tsize, values);
	}
	void	setObjectName(const GU_PrimPacked *pack)
	{
	    const GABC_PackedImpl	*prim = implementation(pack);
	    GABC_IObject	iobj = prim->object();
	    if (!iobj.valid())
		return;
	    UT_WorkBuffer	fullpath;
	    fullpath.strcpy(queryRootName());
	    fullpath.strcat(iobj.getFullName().c_str());
	    changeSetting("name", fullpath.buffer());
	}
	void	applyProperties(const GU_PrimPacked *pack)
	{
	    const GABC_PackedImpl	*prim = implementation(pack);
	    GABC_IObject	iobj = prim->object();
	    if (!iobj.valid())
		return;

	    fpreal		frame = prim->frame();
	    GEO_AnimationType	atype;

	    for (vray_PropertyMap::iterator it = myPropertyMap->begin();
		    !it.atEnd(); ++it)
	    {
		GT_DataArrayHandle	data;
		data = iobj.getUserProperty(it.name(), frame, atype);
		if (!data)
		    continue;
		if (GTisInteger(data->getStorage()))
		    setIntegerProperty(it.thing().c_str(), data);
		else if (GTisFloat(data->getStorage()))
		    setRealProperty(it.thing().c_str(), data);
		else if (GTisString(data->getStorage()))
		    setStringProperty(it.thing().c_str(), data);
		else
		    VRAYwarning("Alembic Procedural: Bad GT storage %s",
			    GTstorage(data->getStorage()));
	    }
	}
	virtual void	 render()
	{
	    exint				nsegs = myList.entries();
	    UT_Array<GT_PrimitiveHandle>	gtlist(nsegs, nsegs);
	    for (exint i = 0; i < nsegs; ++i)
		gtlist(i) = implementation(myList(i))->fullGT();

	    if (myMergeInfo && myMergePrim)
	    {
		GT_PrimitiveHandle	aprim = implementation(myMergePrim)->fullGT();
		if (aprim)
		{
		    for (exint i = 0; i < nsegs; ++i)
		    {
			// Merge attributes
			gtlist(i) = gtlist(i)->attributeMerge(*aprim,
					    myMergeInfo->vertex(),
					    myMergeInfo->point(),
					    myMergeInfo->uniform(),
					    myMergeInfo->detail());
		    }
		}
	    }
	    openProceduralObject();
		setObjectName(myList(0));
		processPrimitiveMaterial(myList(0));
		if (myPropertyMap)
		{
		    applyProperties(myList(0));
		    if (myMergePrim)
			applyProperties(myMergePrim);
		}
		// We set the shutter close to be 1 since the deformation
		// geometry already has the shutter built-in (i.e. the frame
		// associated with the Alembic shape primitive has the sample
		// time baked in).
		addProcedural(new VRAY_ProcGT(gtlist, 1.0));
	    closeObject();
	    for (exint i = 0; i < nsegs; ++i)
	    {
		const_cast<GU_PrimPacked *>(myList(i))->implementation()->clearData();
	    }
	}
    private:
	UT_Array<const GU_PrimPacked *>	 myList;
	const GU_PrimPacked		*myMergePrim;
	vray_MergePatternPtr		 myMergeInfo;
	vray_PropertyMapPtr		 myPropertyMap;
	fpreal				 myPreBlur, myPostBlur;
    };

    vray_PropertyMapPtr
    parseUserProperties(UT_String &pattern)
    {
	vray_PropertyMapPtr	pmap;
	pattern.trimBoundingSpace();
	if (!pattern.isstring())
	    return pmap;
	if (pattern.startsWith("{"))
	{
	    // Assume this is a map specified using JSON syntax
	    UT_AutoJSONParser	j(pattern.buffer(), pattern.length());
	    UT_JSONValue	jval;
	    if (!jval.parseValue(j))
	    {
		VRAYerror("Alembic procedural: error parsing properties: %s",
			pattern.buffer());
		return pmap;
	    }
	    // If it starts with a '{', it better be a map
	    UT_ASSERT(jval.getType() == UT_JSONValue::JSON_MAP);
	    const UT_JSONValueMap	*map = jval.getMap();
	    if (map)
	    {
		pmap.reset(new vray_PropertyMap());
		UT_StringArray		keys;
		map->getKeyReferences(keys);
		for (exint i = 0; i < keys.entries(); ++i)
		{
		    const UT_JSONValue	*item = map->get(keys(i));
		    if (item->getType() == UT_JSONValue::JSON_STRING)
		    {
			(*pmap)[keys(i)] = item->getS();
		    }
		    else
		    {
			VRAYwarning("Alembic procedural: %s '%s'",
				"Expected string for JSON user property:",
				keys(i).buffer());
		    }
		}
		if (!pmap->entries())
		    pmap.clear();
	    }
	}
	else
	{
	    // Assume this is a list of properties which define the map.  This
	    // assumes the user property names match the mantra property names.
	    pmap.reset(new vray_PropertyMap());
	    UT_WorkArgs	args;
	    UT_WorkArgs	split;
	    pattern.tokenize(args, ", \t\n");
	    for (int i = 0; i < args.getArgc(); ++i)
	    {
		UT_String	val(args.getArg(i));
		if (val.findChar(':'))
		{
		    // Ok, here we have a map abcproperty:mantraproperty
		    val.tokenize(split, ':');
		    if (split.getArgc() == 2)
		    {
			(*pmap)[split.getArg(0)] = split.getArg(1);
		    }
		    else
		    {
			VRAYwarning("Alembic procedural: %s %s",
				"Unexpected abcproperty:mantraproperty string",
				args.getArg(i));
		    }
		}
		else if (val.isstring())
		{
		    (*pmap)[val] = val.toStdString();
		}
	    }
	}
	return pmap;
    }
};

VRAY_ProcAlembic::VRAY_ProcAlembic()
    : myLoadDetails()
    , myConstDetails()
    , myMergeInfo()
    , myPreBlur(0)
    , myPostBlur(0)
    , myNonAlembic(true)
{
}

VRAY_ProcAlembic::~VRAY_ProcAlembic()
{
    for (int i = 0; i < myLoadDetails.entries(); ++i)
	freeGeometry(myLoadDetails(i));
    for (int i = 0; i < myConstDetails.entries(); ++i)
	freeGeometry(myConstDetails(i));
    for (int i = 0; i < myAttribDetails.entries(); ++i)
	freeGeometry(myAttribDetails(i));
}

VRAY_ProceduralArg	 VRAY_ProcAlembic::theArgs[] = {
    VRAY_ProceduralArg("useobjectgeometry",	"int",	"1"),
    VRAY_ProceduralArg("filename",		"string",	""),
    VRAY_ProceduralArg("frame",			"float",	"1"),
    VRAY_ProceduralArg("fps",			"float",	"24"),
    VRAY_ProceduralArg("objectpath",		"string",	""),
    VRAY_ProceduralArg("objectpattern",		"string",	"*"),
    VRAY_ProceduralArg("userpropertymap",	"string", 	""),
    VRAY_ProceduralArg("nonalembic",		"int",		"1"),
    VRAY_ProceduralArg("attribfile",		"string",	""),
    VRAY_ProceduralArg("pointattribs",		"string",	"*,^P,^N,^v"),
    VRAY_ProceduralArg("vertexattribs",		"string",	"*,^P,^N,^v"),
    VRAY_ProceduralArg("uniformattribs",	"string",	""),
    VRAY_ProceduralArg("detailattribs",		"string",	""),
    VRAY_ProceduralArg()
};

VRAY_Procedural *
VRAY_ProcAlembic::create(const char *)
{
    return new VRAY_ProcAlembic();
}

const char *
VRAY_ProcAlembic::getClassName()
{
    return "VRAY_ProcAlembic";
}

static void
moveAlembicTime(GU_Detail &gdp, fpreal finc)
{
    const GA_PrimitiveTypeId	&abctype = alembicTypeId();

    for (GA_Iterator it(gdp.getPrimitiveRange()); !it.atEnd(); ++it)
    {
	GEO_Primitive	*prim = gdp.getGEOPrimitive(*it);
	if (prim->getTypeId() != abctype)
	    continue;
	GU_PrimPacked	*packed = UTverify_cast<GU_PrimPacked *>(prim);
	GABC_PackedImpl	*abcprim = UTverify_cast<GABC_PackedImpl *>(packed->implementation());
	if (abcprim)
	    abcprim->setFrame(abcprim->frame() + finc);
    }
}

static bool
loadDetail(UT_Array<GU_Detail *> &details,
	const char *filename,
	fpreal frame,
	fpreal fps,
	const fpreal shutter[2],
	UT_String &objectpath,
	const UT_String &objectpattern)
{
    GABC_GEOWalker	walk(*details(0));
    bool		success = false;
    fpreal		fstart = frame, finc = 1;
    if (details.entries() > 1)
    {
	finc = (shutter[1] - shutter[0]) / fps;
	fstart = frame - finc * shutter[0];
    }

    walk.setObjectPattern(objectpattern);
    walk.setFrame(fstart, fps);
    walk.setIncludeXform(true);
    walk.setBuildLocator(false);
    walk.setPointMode(GABC_GEOWalker::ABCPRIM_NO_POINT);
    walk.setLoadMode(GABC_GEOWalker::LOAD_ABC_PRIMITIVES);
    walk.setGroupMode(GABC_GEOWalker::ABC_GROUP_NONE);

    if (objectpath.isstring())
    {
	UT_WorkArgs	args;
	objectpath.parse(args);
	if (args.getArgc())
	{
	    UT_StringArray	olist;
	    for (int i = 0; i < args.getArgc(); ++i)
		olist.append(args(i));
	    success = GABC_Util::walk(filename, walk, olist);
	}
    }
    else
    {
	success = GABC_Util::walk(filename, walk);
    }
    if (success && details.entries() > 1)
    {
	for (int i = 1; i < details.entries(); ++i)
	{
	    details(i)->merge(*details(0));
	    moveAlembicTime(*details(i), i*finc);
	}
    }
    return success;
}

void
VRAY_ProcAlembic::vray_MergePatterns::clear()
{
    delete myVertex;
    delete myPoint;
    delete myUniform;
    delete myDetail;
    myVertex = NULL;
    myPoint = NULL;
    myUniform = NULL;
    myDetail = NULL;
}

void
VRAY_ProcAlembic::vray_MergePatterns::init(const char *vpattern,
	const char *ppattern,
	const char *upattern,
	const char *dpattern)
{
    clear();
    if (UTisstring(vpattern))
    {
	myVertex = new UT_StringMMPattern();
	myVertex->compile(vpattern);
    }
    if (UTisstring(ppattern))
    {
	myPoint = new UT_StringMMPattern();
	myPoint->compile(ppattern);
    }
    if (UTisstring(upattern))
    {
	myUniform = new UT_StringMMPattern();
	myUniform->compile(upattern);
    }
    if (UTisstring(dpattern))
    {
	myDetail = new UT_StringMMPattern();
	myDetail->compile(dpattern);
    }
}

int
VRAY_ProcAlembic::initialize(const UT_BoundingBox *box)
{
    if (!GABC_PackedImpl::isInstalled())
    {
	VRAYerror("Alembic primitive support is not installed");
	return 0;
    }
    int		ival;
    bool	useobject;
    UT_String	filename("");
    UT_String	attribfile("");
    UT_String	objectpath;
    UT_String	objectpattern;
    UT_String	userpropertymap;

    myNonAlembic = true;
    if (import("useobjectgeometry", &ival, 1))
    {
	useobject = (ival != 0);
	if (!useobject)
	{
	    import("filename", filename);
	}
    }
    else
    {
	if (!import("filename", filename))
	    useobject = true;
    }
    if (!filename.isstring() && !useobject)
    {
	VRAYwarning("No geometry specified for Alembic procedural");
	return 0;
    }

    if (import("userpropertymap", userpropertymap))
    {
	myUserProperties = parseUserProperties(userpropertymap);
    }

    if (!import("attribfile", attribfile))
	attribfile = "";
    else
    {
	UT_String	point("");
	UT_String	vertex("");
	UT_String	uniform("");
	UT_String	detail("");
	if (!import("pointattribs", point))
	    point= "";
	if (!import("vertexattribs", vertex))
	    vertex= "";
	if (!import("uniformattribs", uniform))
	    uniform= "";
	if (!import("detailattribs", detail))
	    detail= "";
	myMergeInfo.reset(new vray_MergePatterns());
	myMergeInfo->init(vertex, point, uniform, detail);
	if (!myMergeInfo->valid())
	{
	    myMergeInfo.clear();
	    attribfile = "";
	}
    }

    if (filename.isstring())
    {
	fpreal	frame;
	fpreal	fps;
	fpreal	shutter[2];
	int	nsegs;
	if (!import("camera:shutter", shutter, 2))
	{
	    shutter[0] = 0;
	    shutter[1] = 1;
	}
	if (!import("object:geosamples", &nsegs, 1))
	    nsegs = 1;
	for (int i = 0; i < nsegs; ++i)
	    myLoadDetails.append(allocateGeometry());
	import("frame", &frame, 1);
	import("fps", &fps, 1);
	import("objectpath", objectpath);
	import("objectpattern", objectpattern);
	if (!loadDetail(myLoadDetails, filename, frame, fps, shutter,
		    objectpath, objectpattern))
	{
	    for (int i = 0; i < myLoadDetails.entries(); ++i)
		freeGeometry(myLoadDetails(i));
	    myLoadDetails.resize(0);
	}
    }
    else
    {
	void	*handle = queryObject(NULL); // Get handle to this object
	int	 nsamples = queryGeometrySamples(handle);
	int	 velblur;
	GA_Size	 nprims;

	if (import("object:velocityblur", &velblur, 1))
	{
	    // If we have velocity blur, we force the number of samples to 1
	    if (velblur)
	    {
		fpreal	shutter[2];
		fpreal	fps;
		if (!import("global:fps", &fps, 1))
		{
		    fps = UT_EnvControl::getInt(ENV_HOUDINI_FPS);
		}
		if (!import("camera:shutter", shutter, 2))
		{
		    shutter[0] = 0;
		    shutter[1] = 1;
		}
		myPreBlur = -shutter[0]/fps;
		myPostBlur = shutter[1]/fps;
		nsamples = 1;
	    }
	}

	import("nonalembic", &ival, 1);
	myNonAlembic = (ival != 0);

	myConstDetails.append(const_cast<GU_Detail *>(queryGeometry(handle,0)));
	referenceGeometry(myConstDetails(0));
	nprims = myConstDetails(0)->getNumPrimitives();

	for (int i = 1; i < nsamples; ++i)
	{
	    const GU_Detail *g = queryGeometry(handle, i);
	    if (!g || g->getNumPrimitives() != nprims)
	    {
		VRAYerror("Mis-matched Alembic primitive counts "
			"(%" SYS_PRId64 " vs %" SYS_PRId64 ")",
			g->getNumPrimitives(), nprims);
	    }
	    else
	    {
		myConstDetails.append(const_cast<GU_Detail *>(g));
		referenceGeometry(myConstDetails(i));
	    }
	}
    }
    if (attribfile.isstring())
    {
	fpreal	shutter[2];
	shutter[0] = 0;
	shutter[1] = 1;
	myAttribDetails.append(allocateGeometry());
	// Re-use the object path/pattern from the file load
	if (!loadDetail(myAttribDetails, attribfile, 0, 24, shutter,
		    objectpath, objectpattern))
	{
	    for (int i = 0; i < myAttribDetails.entries(); ++i)
		freeGeometry(myAttribDetails(i));
	    myAttribDetails.resize(0);
	    myMergeInfo.clear();
	}
    }
    return myLoadDetails.entries() || myConstDetails.entries();
}

static fpreal
findMaxAttributeValue(const GA_ROAttributeRef &attrib, const GA_Range &range)
{
    GA_ROHandleF	h(attrib);
    fpreal		v = 0;
    if (h.isValid())
    {
	for (GA_Iterator it(range); !it.atEnd(); ++it)
	    v = SYSmax(v, h(*it));
    }
    return v;
}

static void
findAttributeRange(const GA_ROAttributeRef &attrib, const GA_Range &range,
	UT_Vector3 &min, UT_Vector3 &max)
{
    GA_ROHandleV3	h(attrib);
    min = 0;
    max = 0;
    if (h.isValid())
    {
	for (GA_Iterator it(range); !it.atEnd(); ++it)
	{
	    UT_Vector3	v = h(*it);
	    for (int i = 0; i < 3; ++i)
	    {
		min(i) = SYSmin(min(i), v(i));
		max(i) = SYSmin(max(i), v(i));
	    }
	}
    }
}

static void
getBoxForRendering(const GU_Detail &gdp, UT_BoundingBox &box, bool nonalembic,
	fpreal preblur, fpreal postblur)
{
    // When computing bounding boxes of Alembic primitives, we need to enlarge
    // the bounds of curve/point meshes by the width attribute for proper
    // rendering bounds.
    if (!gdp.getNumPrimitives())
    {
	// We may be rendering points
	GA_Range pts(gdp.getPointRange());
	box.initBounds();
	gdp.enlargeBoundingBox(box, pts);
	// If there's a width/pscale attribute, we need to find the maximum
	// value
	fpreal			width = 0;
	GA_ROAttributeRef	a;
	a = gdp.findFloatTuple(GA_ATTRIB_POINT, "width");
	width = SYSmax(width, findMaxAttributeValue(a, pts));
	a = gdp.findFloatTuple(GA_ATTRIB_POINT, "pscale");
	width = SYSmax(width, findMaxAttributeValue(a, pts));
	box.expandBounds(0, width);

	if (preblur != 0|| postblur != 0)
	{
	    UT_Vector3	vmin, vmax;
	    GA_ROAttributeRef	v = gdp.findFloatTuple(GA_ATTRIB_POINT, "v", 3);
	    findAttributeRange(v, pts, vmin, vmax);
	    enlargeVelocityBox(box, vmin, vmax, preblur, postblur);
	}
    }
    else
    {
	const GA_PrimitiveTypeId	&abctype = alembicTypeId();
	bool				 dogeo = false;

	box.initBounds();
	for (GA_Iterator it(gdp.getPrimitiveRange()); !it.atEnd(); ++it)
	{
	    const GEO_Primitive	*prim = gdp.getGEOPrimitive(*it);
	    if (prim->getTypeId() != abctype)
	    {
		dogeo = true;
		continue;
	    }
	    UT_BoundingBox	 primbox;
	    const GU_PrimPacked	*abcprim;
	    abcprim = UTverify_cast<const GU_PrimPacked *>(prim);
	    abcprim->getRenderingBounds(primbox);
	    if (preblur != 0|| postblur != 0)
	    {
		UT_Vector3	vmin, vmax;
		abcprim->getVelocityRange(vmin, vmax);
		enlargeVelocityBox(primbox, vmin, vmax, preblur, postblur);
	    }
	    box.enlargeBounds(primbox);
	}
	if (dogeo && nonalembic)
	{
	    // There were non-alembic primitives, so we need to compute their
	    // bounds.
	    UT_BoundingBox	gbox;
	    gdp.getBBox(&gbox);
	    if (preblur != 0|| postblur != 0)
	    {
		UT_Vector3		vmin, vmax;
		GA_ROAttributeRef	v = gdp.findFloatTuple(
						    GA_ATTRIB_POINT, "v", 3);
		findAttributeRange(v, gdp.getPointRange(), vmin, vmax);
		enlargeVelocityBox(gbox, vmin, vmax, preblur, postblur);
	    }
	    box.enlargeBounds(gbox);
	}
    }
}

void
VRAY_ProcAlembic::getBoundingBox(UT_BoundingBox &box)
{
    const UT_Array<GU_Detail *>	&details = getDetailList();
    box.initBounds();
    for (int i = 0; i < details.entries(); ++i)
    {
	UT_BoundingBox	tbox;
	getBoxForRendering(*details(i), tbox,
		myNonAlembic, myPreBlur, myPostBlur);
	box.enlargeBounds(tbox);
    }
}

void
VRAY_ProcAlembic::render()
{
    const UT_Array<GU_Detail *>	&details = getDetailList();
    const GU_Detail		*gdp, *agdp;
    const GA_PrimitiveTypeId	&abctype = alembicTypeId();
    bool			 warned = false;
    bool			 addgeo = false;
    GA_Range			 baserange;
    UT_String			 groupname;
    const GA_PrimitiveGroup	*rendergroup = NULL;

    int nsegments = details.entries();
    UT_ASSERT(nsegments);
    gdp = details(0);
    agdp = myAttribDetails.entries() ? myAttribDetails(0) : NULL;
    if (agdp && agdp->getNumPrimitives() != gdp->getNumPrimitives())
	agdp = NULL;

    if (import("object:geometrygroup", groupname))
    {
	if (UTisstring(groupname))
	    rendergroup = gdp->findPrimitiveGroup(groupname);
    }
    baserange = gdp->getPrimitiveRange(rendergroup);

    if (!gdp->getNumPrimitives())
    {
	addgeo = myNonAlembic;	// Add any points
    }
    else
    {
	UT_Array<const GU_PrimPacked *>	abclist(nsegments, nsegments);

	for (GA_Iterator it(baserange); !it.atEnd(); ++it)
	{
	    const GEO_Primitive	*prim = gdp->getGEOPrimitive(*it);
	    const GEO_Primitive	*aprim = agdp ? agdp->getGEOPrimitive(*it):NULL;
	    if (prim->getTypeId() == abctype)
	    {
		const GU_PrimPacked		*abc_attrib = NULL;
		if (aprim && aprim->getTypeId() == abctype)
		    abc_attrib = UTverify_cast<const GU_PrimPacked *>(aprim);
		abclist(0) = UTverify_cast<const GU_PrimPacked *>(prim);
		for (int i = 1; i < nsegments; ++i)
		{
		    const GEO_Primitive	*seg;
		    seg = details(i)->primitives()(prim->getNum());
		    abclist(i) = UTverify_cast<const GU_PrimPacked *>(seg);
		}
		openProceduralObject();
		    addProcedural(new vray_ProcAlembicPrim(abclist,
				myPreBlur, myPostBlur, abc_attrib,
				myMergeInfo, myUserProperties));
		closeObject();
		UT_INC_COUNTER(thePrimCount);
	    }
	    else
	    {
		if (myNonAlembic)
		    addgeo = true;
		else if (!warned)
		{
		    void		*handle = queryObject(NULL);
		    const char	*name = queryObjectName(handle);
		    VRAYwarning("Object %s has non-alembic primitives", name);
		    warned = true;
		}
	    }
	}
    }
    if (addgeo)
    {
	fpreal	tstep;
	int	ngeo = details.entries();
	if (ngeo > 0)
	{
	    tstep = ngeo > 1 ? 1.0/(ngeo-1) : 1;
	    openGeometryObject();
		for (int i = 0; i < ngeo; ++i)
		    addGeometry(const_cast<GU_Detail *>(details(i)), tstep * i);
	    closeObject();
	}
    }
}
