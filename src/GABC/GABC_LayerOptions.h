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

#ifndef __GABC_LayerOptions__
#define __GABC_LayerOptions__

#include "GABC_API.h"

#include <Alembic/Abc/Foundation.h>
#include <Alembic/AbcCoreLayer/Util.h>
#include <UT/UT_Array.h>
#include <UT/UT_String.h>
#include <UT/UT_StringHolder.h>

namespace GABC_NAMESPACE
{

class GABC_API GABC_LayerOptions
{
public:
    // The LayerType is being widely used for exporting layering archive
    // and it has different meaning for the nodes and the props.
    // In the case of the nodes:
    //     DEFER   : The node won't exist in the archive.
    //     PRUNE   : The empty sparse node with the prune metadata.
    //     SPARSE  : The sparse node with several props.
    //     FULL    : The common full node.
    //     REPLACE : The full node with the replace metadata.
    // And for the props:
    //     DEFER   : The property won't exist in the archive.
    //     PRUNE   : The empty property with the prune metadata.
    //     FULL    : The common property.
    // NOTE: Presently, the REPLACE and the SPARSE are illegal on the props.
    enum LayerType
    {
	DEFER,
	PRUNE,
	SPARSE,
	FULL,
	REPLACE
    };

    class Rules
    {
	struct Rule
	{
	    Rule(const UT_StringHolder &pat, LayerType type) :
		myNodePat(pat), myType(type) {}

	    UT_StringHolder	 myNodePat;
	    LayerType		 myType;
	};

    public:
				 Rules() {}
				~Rules() {}

	void			 append(const UT_String &pattern,
					LayerType type);
	LayerType		 getLayerType(const UT_String &str) const;

    private:
	UT_Array<Rule>		 myData;
    };

    class MultiRules
    {
	struct Rule
	{
	    Rule(const UT_StringHolder &pat, const UT_StringHolder &subPat,
		LayerType type) :
		myNodePat(pat), mySubPat(subPat), myType(type) {}

	    UT_StringHolder	 myNodePat;
	    UT_StringHolder	 mySubPat;
	    LayerType		 myType;
	};

    public:
				 MultiRules() {}
				~MultiRules() {}

	void			 append(const UT_String &pat,
					const UT_String &subPat,
					LayerType type);
	LayerType		 getLayerType(const UT_String &str,
					const UT_String &subStr) const;
	bool			 matchesNodePattern(const UT_String &str) const;

    private:
	UT_Array<Rule>		 myData;
    };

			 GABC_LayerOptions() {}
			~GABC_LayerOptions() {}

    static void		 getMetadata(Alembic::Abc::MetaData &md,
				LayerType type);
    static Alembic::Abc::SparseFlag
			 getSparseFlag(LayerType type);

    void		 appendNodeRule(const UT_String &nodePat,
				LayerType type);
    void		 appendVizRule(const UT_String &nodePat,
				LayerType type);
    void		 appendAttrRule(const UT_String &nodePat,
				const UT_String &attrPat,
				LayerType type);
    void		 appendUserPropRule(const UT_String &nodePat,
				const UT_String &userPropPat,
				LayerType type);

    // The method should never be called in saving process. The ROP_AbcNode
    // should always hold the actual node type.
    LayerType		 getNodeType(const UT_String &nodePath) const;

    // These methods accept an extra node type then map it as proper property
    // type before match the pattern.
    LayerType		 getVizType(const UT_String &nodePath,
				LayerType nodeType) const;
    LayerType		 getAttrType(const UT_String &nodePath,
				const UT_String &attrName,
				LayerType nodeType) const;
    LayerType		 getUserPropType(const UT_String &nodePath,
				const UT_String &userPropName,
				LayerType nodeType) const;

private:
    Rules		 myNodeData, myVizData;
    MultiRules		 myAttrData, myUserPropData;
};

} // GABC_NAMESPACE

#endif