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
#include <Alembic/AbcGeom/Visibility.h>
#include <UT/UT_Array.h>
#include <UT/UT_String.h>
#include <UT/UT_StringHolder.h>

namespace GABC_NAMESPACE
{

class GABC_API GABC_LayerOptions
{
public:

    // IMPORTANT: The order of the following two enums are significative.
    // Make sure you understand how they work before you change them.

    // The LayerType is being widely used for exporting layering archive
    // and it has different meaning for the nodes and the props.
    // In the case of the nodes:
    //     NONE    : The node won't exist in the archive.
    //     PRUNE   : The empty sparse node with the prune metadata.
    //     SPARSE  : The sparse node with several props.
    //     FULL    : The common full node.
    //     REPLACE : The full node with the replace metadata.
    // And for the props:
    //     NONE    : The property won't exist in the archive.
    //     PRUNE   : The empty property with the prune metadata.
    //     FULL    : The common property.
    // NOTE: Presently, the REPLACE and the SPARSE are illegal on the props.
    enum class LayerType
    {
	NONE,
	PRUNE,
	SPARSE,
	FULL,
	REPLACE
    };

    // Flag the visibility of the Alembic node. It contains all of the
    // ObjectVisibilities and a DEFAULT. Which means setting the visibility
    // by populating it from the houdini session.
    enum class VizType
    {
	NONE,
	DEFAULT,
	DEFER,
	HIDDEN,
	VISIBLE
    };

			 GABC_LayerOptions() {}
			~GABC_LayerOptions() {}

    static void		 getMetadata(Alembic::Abc::MetaData &md,
				LayerType type);
    static Alembic::Abc::SparseFlag
			 getSparseFlag(LayerType type);
    static Alembic::AbcGeom::ObjectVisibility
			 getOVisibility(VizType type);

    void		 appendNodeRule(const UT_StringRef &nodePat,
				LayerType type);
    void		 appendVizRule(const UT_StringRef &nodePat,
				VizType type);
    void		 appendAttrRule(const UT_StringRef &nodePat,
				const UT_StringRef &attrPat,
				LayerType type);
    void		 appendUserPropRule(const UT_StringRef &nodePat,
				const UT_StringRef &userPropPat,
				LayerType type);

    // The method should never be called in the saving process. The ROP_AbcNode
    // should always hold the actual node type.
    LayerType		 getNodeType(const UT_StringRef &nodePath) const;

    // These methods accept an extra node type then map it as proper property
    // type before matching the pattern.
    VizType		 getVizType(const UT_StringRef &nodePath,
				LayerType nodeType) const;
    LayerType		 getAttrType(const UT_StringRef &nodePath,
				const UT_StringRef &attrName,
				LayerType nodeType) const;
    LayerType		 getUserPropType(const UT_StringRef &nodePath,
				const UT_StringRef &userPropName,
				LayerType nodeType) const;

private:
    template <typename RULETYPE>
    class Rules
    {
	struct Rule
	{
	    Rule(const UT_StringHolder &pat, RULETYPE type) :
		myNodePat(pat), myType(type) {}

	    UT_StringHolder	 myNodePat;
	    RULETYPE		 myType;
	};

    public:
			 Rules() {}
			~Rules() {}

	void		 append(const UT_StringRef &pattern, RULETYPE type)
			 { myData.append(Rule(pattern, type)); }

	RULETYPE	 getRule(const UT_StringRef &str) const
			 {
			     for(auto it = myData.begin();
				 it != myData.end(); ++it)
			     {
				 if(str.multiMatch(it->myNodePat))
				     return it->myType;
			     }
			     return (RULETYPE) 0;
			 }

	bool		 matchesNodePattern(const UT_StringRef &str) const
			 {
			     for(auto it = myData.begin();
				 it != myData.end(); ++it)
			     {
				 if(str.multiMatch(it->myNodePat))
				     return true;
			     }
			     return false;
			 }

    private:
	UT_Array<Rule>	 myData;
    };

    template <typename RULETYPE>
    class MultiRules
    {
	struct Rule
	{
	    Rule(const UT_StringHolder &pat, const UT_StringHolder &subPat,
		RULETYPE type) :
		myNodePat(pat), mySubPat(subPat), myType(type) {}

	    UT_StringHolder	 myNodePat;
	    UT_StringHolder	 mySubPat;
	    RULETYPE		 myType;
	};

    public:
			 MultiRules() {}
			~MultiRules() {}

	void		 append(const UT_StringRef &pat,
				const UT_StringRef &subPat,
				RULETYPE type)
			 { myData.append(Rule(pat, subPat, type)); }

	RULETYPE	 getRule(const UT_StringRef &str,
				const UT_StringRef &subStr) const
			 {
			     for(auto it = myData.begin();
				 it != myData.end(); ++it)
			     {
				 if(str.multiMatch(it->myNodePat) &&
				     subStr.multiMatch(it->mySubPat))
				     return it->myType;
			     }
			     return (RULETYPE) 0;
			 }

	bool		 matchesNodePattern(const UT_StringRef &str) const
			 {
			     for(auto it = myData.begin();
				 it != myData.end(); ++it)
			     {
				 if(str.multiMatch(it->myNodePat))
				     return true;
			     }
			     return false;
			 }

    private:
	UT_Array<Rule>	 myData;
    };

    Rules<LayerType>	     myNodeData;
    Rules<VizType>	     myVizData;
    MultiRules<LayerType>    myAttrData;
    MultiRules<LayerType>    myUserPropData;
};

} // GABC_NAMESPACE

#endif