//-*****************************************************************************
//
// Copyright (c) 2009-2013,
//  Side Effects Software Inc.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic, nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

#include <GABC/GABC_Include.h>	// For Windows linking

#include "VRAY_ProcAlembic.h"
#include <GABC/GABC_GEOWalker.h>
#include <GABC/GABC_GUPrim.h>
#include <UT/UT_WorkArgs.h>
#include <UT/UT_EnvControl.h>
#include "VRAY_ProcGT.h"
#include "VRAY_IO.h"

//#define SHOW_COUNTS
#include <UT/UT_ShowCounts.h>
UT_COUNTER(thePrimCount, "Alembic Primitives");

namespace
{
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
	vray_ProcAlembicPrim(const UT_Array<const GABC_GEOPrim *> &list,
		fpreal preblur, fpreal postblur)
	    : myList(list)
	    , myPreBlur(preblur)
	    , myPostBlur(postblur)
	{
	}
	virtual ~vray_ProcAlembicPrim()
	{
	}
	virtual bool	 isThreadSafe() const	{ return true; }
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
	virtual void	 render()
	{
	    exint		nsegs = myList.entries();
	    UT_Array<GT_PrimitiveHandle>	gtlist(nsegs, nsegs);
	    for (exint i = 0; i < nsegs; ++i)
		gtlist(i) = myList(i)->gtPrimitive();
	    openProceduralObject();
		// We set the shutter close to be 1 since the deformation
		// geometry already has the shutter built-in (i.e. the frame
		// associated with the Alembic shape primitive has the sample
		// time baked in).
		addProcedural(new VRAY_ProcGT(gtlist, 1.0));
	    closeObject();
	    for (exint i = 0; i < nsegs; ++i)
	    {
		const_cast<GABC_GEOPrim *>(myList(i))->clearGT();
	    }
	}
    private:
	UT_Array<const GABC_GEOPrim *>	myList;
	fpreal				myPreBlur, myPostBlur;
    };
};

VRAY_ProcAlembic::VRAY_ProcAlembic()
    : myLoadDetails()
    , myConstDetails()
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
}

VRAY_ProceduralArg	 VRAY_ProcAlembic::theArgs[] = {
    VRAY_ProceduralArg("useobjectgeometry",	"int",	"1"),
    VRAY_ProceduralArg("filename",	"string",	""),
    VRAY_ProceduralArg("frame",		"float",	"1"),
    VRAY_ProceduralArg("fps",		"float",	"24"),
    VRAY_ProceduralArg("objectpath",	"string",	""),
    VRAY_ProceduralArg("objectpattern",	"string",	"*"),
    VRAY_ProceduralArg("nonalembic",	"int",		"1"),
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
    const GA_PrimitiveTypeId	abctype = GABC_GUPrim::theTypeId();

    for (GA_Iterator it(gdp.getPrimitiveRange()); !it.atEnd(); ++it)
    {
	GEO_Primitive	*prim = gdp.getGEOPrimitive(*it);
	if (prim->getTypeId() != abctype)
	    continue;
	GABC_GEOPrim	*abcprim = UTverify_cast<GABC_GEOPrim *>(prim);
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
    bool		success;
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

int
VRAY_ProcAlembic::initialize(const UT_BoundingBox *box)
{
    if (!GABC_GUPrim::isInstalled())
    {
	VRAYerror("Alembic primitive support is not installed");
	return 0;
    }
    int		ival;
    bool	useobject;
    UT_String	filename("");
    UT_String	objectpath;
    UT_String	objectpattern;

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
	box.enlargeBounds(0, width);

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
	const GA_PrimitiveTypeId	abctype = GABC_GUPrim::theTypeId();
	bool				dogeo = false;

	box.initBounds();
	for (GA_Iterator it(gdp.getPrimitiveRange()); !it.atEnd(); ++it)
	{
	    const GEO_Primitive	*prim = gdp.getGEOPrimitive(*it);
	    if (prim->getTypeId() != abctype)
	    {
		dogeo = true;
		continue;
	    }
	    const GABC_GEOPrim	*abcprim;
	    UT_BoundingBox	 primbox;
	    abcprim = UTverify_cast<const GABC_GEOPrim *>(prim);
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
    const GU_Detail		*gdp;
    const GA_PrimitiveTypeId	 abctype = GABC_GUPrim::theTypeId();
    bool			 warned = false;
    bool			 addgeo = false;

    int nsegments = details.entries();
    UT_ASSERT(nsegments);
    gdp = details(0);

    if (!gdp->getNumPrimitives())
    {
	addgeo = myNonAlembic;	// Add any points
    }
    else
    {
	UT_Array<const GABC_GEOPrim *>	abclist(nsegments, nsegments);

	for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
	{
	    const GEO_Primitive	*prim = gdp->getGEOPrimitive(*it);
	    if (prim->getTypeId() == abctype)
	    {
		abclist(0) = UTverify_cast<const GABC_GEOPrim *>(prim);
		for (int i = 1; i < nsegments; ++i)
		{
		    const GEO_Primitive	*seg;
		    seg = details(i)->primitives()(prim->getNum());
		    abclist(i) = UTverify_cast<const GABC_GEOPrim *>(seg);
		}
		openProceduralObject();
		    addProcedural(new vray_ProcAlembicPrim(abclist, myPreBlur, myPostBlur));
		closeObject();
		UT_INC_COUNTER(thePrimCount);
	    }
	    else
	    {
		if (myNonAlembic)
		    addgeo = true;
		if (!warned)
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
		    addGeometry(const_cast<GU_Detail *>(gdp), tstep * i);
	    closeObject();
	}
    }
}
