/*
 * Copyright (c) 2020
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

#ifndef __RAY_ProcAlembic__
#define __RAY_ProcAlembic__

#include <GABC/GABC_API.h>

#include "RAY_Procedural.h"
#include <UT/UT_Array.h>
#include <UT/UT_StringArray.h>
#include <UT/UT_SymbolTable.h>
#include <UT/UT_SharedPtr.h>
#include <UT/UT_UniquePtr.h>

class GU_Detail;
class GSTY_SubjectPrimGroup;

class RAY_ProcAlembic : public RAY_Procedural
{
public:
    // Map between user properties and mantra rendering properties
    typedef UT_SymbolMap<std::string>		ray_PropertyMap;
    typedef UT_SharedPtr<ray_PropertyMap>	ray_PropertyMapPtr;

    class ray_MergePatterns
    {
    public:
	ray_MergePatterns()
	    : myVertex(nullptr)
	    , myPoint(nullptr)
	    , myUniform(nullptr)
	    , myDetail(nullptr)
	{
	}
	~ray_MergePatterns()
	{
	    clear();
	}

	bool	valid() const
		    { return myVertex || myPoint || myUniform || myDetail; }
	void	clear();
	void	init(const char *vpattern,
		    const char *ppattern,
		    const char *upattern,
		    const char *dpattern);

	UT_StringMMPattern	*vertex() 	{ return myVertex; }
	UT_StringMMPattern	*point() 	{ return myPoint; }
	UT_StringMMPattern	*uniform() 	{ return myUniform; }
	UT_StringMMPattern	*detail() 	{ return myDetail; }

    private:
	UT_StringMMPattern	*myVertex;
	UT_StringMMPattern	*myPoint;
	UT_StringMMPattern	*myUniform;
	UT_StringMMPattern	*myDetail;
    };
    typedef UT_SharedPtr<ray_MergePatterns>	ray_MergePatternPtr;

    RAY_ProcAlembic();
    ~RAY_ProcAlembic() override;

    static RAY_Procedural	*create(const char *);
    static RAY_ProceduralArg	 theArgs[];

    const char          *className() const override;

    int                 initialize(const UT_BoundingBox *box) override;
    void                getBoundingBox(UT_BoundingBox &box) override;
    void                render() override;

private:
    RAY_ROProceduralGeo	getDetail()
    {
	UT_ASSERT(myLoadDetail.isValid() || myConstDetail.isValid());
	if (myLoadDetail.isValid())
	    return myLoadDetail;
	return myConstDetail;
    }

    RAY_ROProceduralGeo	myConstDetail;
    RAY_ProceduralGeo		myLoadDetail;
    RAY_ProceduralGeo		myAttribDetail;
    ray_MergePatternPtr	myMergeInfo;
    ray_PropertyMapPtr		myUserProperties;
    UT_UniquePtr<GSTY_SubjectPrimGroup> myGroupSharingHolder;
    fpreal			myPreBlur, myPostBlur;
    bool			myNonAlembic;
    bool			myVelocityBlur;
};

#endif
