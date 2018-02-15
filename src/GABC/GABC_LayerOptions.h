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

#include <UT/UT_Array.h>
#include <UT/UT_String.h>
#include <UT/UT_StringHolder.h>

namespace GABC_NAMESPACE
{

enum GABC_AbcLayerFlag
{
    GABC_ALEMBIC_LAYERING_NULL,
    GABC_ALEMBIC_LAYERING_FULL,
    GABC_ALEMBIC_LAYERING_REPLACE,
    GABC_ALEMBIC_LAYERING_PRUNE
};

class GABC_API GABC_LayerOptions
{

    class FlagStorage
    {
	struct Item
	{
	    Item(UT_StringHolder pat, GABC_AbcLayerFlag flag) :
		myPat(pat), myFlag(flag) {}
	    UT_StringHolder	 myPat;
	    GABC_AbcLayerFlag	 myFlag;
	};

    public:
				 FlagStorage() {};
				~FlagStorage() {};

	void			 append(const UT_String &pattern,
				    GABC_AbcLayerFlag flag);
	GABC_AbcLayerFlag	 match(const UT_String &str);

    private:
	UT_Array<Item>		 myData;
    };

    class MultiFlagStorage
    {
	struct Item
	{
	    Item(UT_StringHolder pat, UT_StringHolder subPat,
		GABC_AbcLayerFlag flag) :
		myPat(pat), mySubPat(subPat), myFlag(flag) {}
	    UT_StringHolder	 myPat;
	    UT_StringHolder	 mySubPat;
	    GABC_AbcLayerFlag	 myFlag;
	};

    public:
				 MultiFlagStorage() {};
				~MultiFlagStorage() {};

	void			 append(const UT_String &pat,
					const UT_String &subPat,
					GABC_AbcLayerFlag flag);
	GABC_AbcLayerFlag	 match(const UT_String &str,
					const UT_String &subStr);

    private:
	UT_Array<Item>		 myData;
    };

public:
			 GABC_LayerOptions(bool enable=false, bool full=false) :
				myIsActived(enable),
				myIsFullAncestor(full) {};
			~GABC_LayerOptions() {};

    void		 updateNodePat(const UT_String &nodePat,
				GABC_AbcLayerFlag flag);
    void		 updateVizPat(const UT_String &nodePat,
				GABC_AbcLayerFlag flag);
    void		 updateAttrPat(const UT_String &nodePat,
				const UT_String &attrPat,
				GABC_AbcLayerFlag flag);
    void		 updateUserPropPat(const UT_String &nodePat,
				const UT_String &userPropPat,
				GABC_AbcLayerFlag flag);

    GABC_AbcLayerFlag	 matchNode(const UT_String &nodePath);
    GABC_AbcLayerFlag	 matchViz(const UT_String &nodePath);
    GABC_AbcLayerFlag	 matchAttr(const UT_String &nodePath,
				const UT_String &attrName);
    GABC_AbcLayerFlag	 matchUserProp(const UT_String &nodePath,
				const UT_String &userPropName);

private:
    bool		 myIsActived, myIsFullAncestor;
    FlagStorage		 myNodeData, myVizData;
    MultiFlagStorage	 myAttrData, myUserPropData;
};

} // GABC_NAMESPACE

#endif