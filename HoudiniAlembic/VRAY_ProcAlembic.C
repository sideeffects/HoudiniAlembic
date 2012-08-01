//-*****************************************************************************
//
// Copyright (c) 2009-2012,
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
#include "VRAY_ProcGT.h"
#include "VRAY_IO.h"

//#define SHOW_COUNTS
#include <UT/UT_ShowCounts.h>
UT_COUNTER(thePrimCount, "Alembic Primitives");

namespace
{
    class vray_ProcAlembicPrim : public VRAY_Procedural
    {
    public:
	vray_ProcAlembicPrim(const GABC_GEOPrim *prim)
	{
	    myPrim = const_cast<GABC_GEOPrim *>(prim);
	}
	virtual ~vray_ProcAlembicPrim()
	{
	}
	virtual bool	 isThreadSafe() const	{ return true; }
	virtual const char	*getClassName()
				    { return "vray_ProcAlembicPrim"; }
	virtual int	 initialize(const UT_BoundingBox *)
				    { return myPrim != NULL; }
	virtual void	 getBoundingBox(UT_BoundingBox &box)
				{ myPrim->getBBox(&box); }
	virtual void	 render()
	{
	    GT_PrimitiveHandle	gt = myPrim->gtPrimitive();
	    openProceduralObject();
		addProcedural(new VRAY_ProcGT(gt));
	    closeObject();
	    myPrim->clearGT();
	}
    private:
	GABC_GEOPrim	*myPrim;
    };
};

VRAY_ProcAlembic::VRAY_ProcAlembic()
    : myDetail(NULL)
    , myConstDetails()
{
}

VRAY_ProcAlembic::~VRAY_ProcAlembic()
{
    delete myDetail;
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

static bool
loadDetail(GU_Detail *gdp,
	const char *filename,
	fpreal frame,
	fpreal fps,
	UT_String &objectpath,
	const UT_String &objectpattern)
{
    GABC_GEOWalker	walk(*gdp);
    bool		success;

    walk.setObjectPattern(objectpattern);
    walk.setFrame(frame, fps);
    walk.setIncludeXform(true);
    walk.setBuildLocator(false);
    walk.setBuildAbcPrim(true);
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

    myNonAlembic = false;
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
	return 0;
    if (filename.isstring())
    {
	fpreal	frame;
	fpreal	fps;
	myDetail = allocateGeometry();
	import("frame", &frame, 1);
	import("fps", &fps, 1);
	import("objectpath", objectpath);
	import("objectpattern", objectpattern);
	if (!loadDetail(myDetail, filename, frame, fps,
		    objectpath, objectpattern))
	{
	    freeGeometry(myDetail);
	    myDetail = NULL;
	}
    }
    else
    {
	void	*handle = queryObject(NULL);	// Get handle to this object
	int	 nsamples = queryGeometrySamples(handle);

	import("nonalembic", &ival, 1);
	myNonAlembic = (ival != 0);

	for (int i = 0; i < nsamples; ++i)
	{
	    myConstDetails.append(const_cast<GU_Detail *>(queryGeometry(handle, i)));
	    referenceGeometry(myConstDetails(i));
	}
    }
    return myDetail || myConstDetails.entries();
}

void
VRAY_ProcAlembic::getBoundingBox(UT_BoundingBox &box)
{
    if (myDetail)
	myDetail->getBBox(&box);
    else
    {
	box.initBounds();
	for (int i = 0; i < myConstDetails.entries(); ++i)
	{
	    UT_BoundingBox	tbox;
	    myConstDetails(i)->getBBox(&tbox);
	    box.enlargeBounds(tbox);
	}
    }
}

void
VRAY_ProcAlembic::render()
{
    const GU_Detail		*gdp;
    const GA_PrimitiveTypeId	 abctype = GABC_GUPrim::theTypeId();
    bool			 warned = false;
    bool			 addgeo = false;

    if (myConstDetails.entries())
	gdp = myConstDetails(0);
    else
	gdp = myDetail;

    for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
    {
	const GEO_Primitive	*prim = gdp->getGEOPrimitive(*it);
	if (prim->getTypeId() == abctype)
	{
	    GT_PrimitiveHandle	 gt;
	    const GABC_GEOPrim	*abcprim;
	    abcprim = UTverify_cast<const GABC_GEOPrim *>(prim);
	    openProceduralObject();
		addProcedural(new vray_ProcAlembicPrim(abcprim));
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
    if (addgeo)
    {
	fpreal	tstep;
	int	ngeo = myConstDetails.entries();
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
