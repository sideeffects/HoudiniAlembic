//-*****************************************************************************
//
// Copyright (c) 2009-2012,
//  Sony Pictures Imageworks Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
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

#ifndef __SOP_ALEMBICIN_H__
#define __SOP_ALEMBICIN_H__

#include <UT/UT_Version.h>
#include <UT/UT_Interrupt.h>
#include <SOP/SOP_Node.h>
#include <GABC/GABC_GEOWalker.h>
#include <GABC/GABC_Util.h>

/// SOP to read Alembic geometry
class SOP_AlembicIn2 : public SOP_Node
{
public:
    //--------------------------------------------------------------------------
    // Standard hdk declarations
    static OP_Node *myConstructor(OP_Network *net, const char *name,
		    OP_Operator *entry);
    static PRM_Template myTemplateList[];

    /// Reload callback
    static int reloadGeo(void *data, int index,
			float time, const PRM_Template *tplate);

    static void installSOP(OP_OperatorTable *table);

protected:
    //--------------------------------------------------------------------------
    // Standard hdk declarations
    SOP_AlembicIn2(OP_Network *net, const char *name, OP_Operator *op);
    virtual ~SOP_AlembicIn2();

    virtual unsigned	disableParms();
    virtual OP_ERROR	cookMySop(OP_Context &context);

private:
    class Parms
    {
    public:
	Parms();
	Parms(const Parms &src);
	/// Compare this set of parameters with the other set of parameters to
	/// see if new geometry is needed (i.e. the filename has changed, or the
	/// path attribute has changed, etc.)
	bool	needsNewGeometry(const Parms &parms);

	Parms	&operator=(const Parms &src);

	bool				myBuildAbcPrim;
	bool				myBuildAbcXform;
	std::string			myFilename;
	UT_String			myObjectPath;
	UT_String			myObjectPattern;
	UT_String			myAttributePatterns[GA_ATTRIB_OWNER_N];
	bool				myIncludeXform;
	bool				myBuildLocator;
	GABC_GEOWalker::GroupMode	myGroupMode;
	GABC_GEOWalker::AFilter		myAnimationFilter;
	UT_String			myPathAttribute;
	UT_String			myFilenameAttribute;
	GEO_ABCNameMapPtr		myNameMapPtr;
    };

    void	evaluateParms(Parms &parms, OP_Context &context);

    Parms	myLastParms;
    bool	myTopologyConstant;
    bool	myEntireSceneIsConstant;

#if 0
    UT_String	myFileObjectCache;
    UT_String	myLastPathAttribute;
    UT_String	myLastObjectPattern;
    UT_String	myLastAttributePatterns[GA_ATTRIB_OWNER_N];
    size_t	myConstantPointCount;		// Point count for constant topology
    size_t	myConstantPrimitiveCount;	// Primitive count for constant topology
#endif
    int		myConstantUniqueId;	// Detail unique id for constant topology
};

#endif
