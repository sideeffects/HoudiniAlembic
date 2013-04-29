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

#ifndef __GABC_GUPrim__
#define __GABC_GUPrim__

#include "GABC_API.h"
#if !defined(GABC_PACKED)
#include <GA/GA_PrimitiveDefinition.h>
#include "GABC_GEOPrim.h"
#include <GU/GU_Detail.h>
#include <GU/GU_Prim.h>

namespace GABC_NAMESPACE
{

class GABC_API GABC_GUPrim : public GABC_GEOPrim, public GU_Primitive
{
public:
    // This constructor creates a new GABC_GUPrim but does
    // not append it to the detail
    GABC_GUPrim(GU_Detail *gdp, GA_Offset offset=GA_INVALID_OFFSET)
	: GABC_GEOPrim(gdp, offset)
	, GU_Primitive() 
    { } 
    virtual ~GABC_GUPrim();

    /// Report approximate memory usage.
    virtual int64	getMemoryUsage() const;

    static bool		isInstalled()
			    { return theDef != NULL; }

    /// Allows you to find out what this primitive type was named.
    static GA_PrimitiveTypeId	 theTypeId() { return theDef->getId(); }

    /// Must be invoked during the factory callback to add us to the
    /// list of primitives
    static void		registerMyself(GA_PrimitiveFactory *factory);

    virtual const GA_PrimitiveDefinition	&getTypeDef() const
						    { return *theDef; }

    // Conversion Methods

    virtual GEO_Primitive	*convert(GU_ConvertParms &parms,
					 GA_PointGroup *usedpts = 0);
    virtual GEO_Primitive	*convertNew(GU_ConvertParms &parms);

    virtual void		*castTo (void) const;
    virtual const GEO_Primitive	*castToGeo(void) const;

    // NOTE:  For static member functions please call in the following
    //        manner.  <ptrvalue> = GABC_GUPrim::<functname>
    //        i.e.        partptr = GABC_GUPrim::build(params...);

    // Optional Build Method 

    static GABC_GUPrim	*build(GU_Detail *gdp,
				const std::string &filename,
				const std::string &objectpath,
				fpreal frame=0,
				bool use_transform=true,
				bool check_visibility=true);
    static GABC_GUPrim	*build(GU_Detail *gdp,
				const std::string &filename,
				const GABC_IObject &object,
				fpreal frame=0,
				bool use_transform=true,
				bool check_visibility=true);

    virtual void	normal(NormalComp &output) const;

    virtual int		intersectRay(const UT_Vector3 &o, const UT_Vector3 &d,
				float tmax = 1E17F, float tol = 1E-12F,
				float *distance = 0, UT_Vector3 *pos = 0,
				UT_Vector3 *nml = 0, int accurate = 0,
				float *u = 0, float *v = 0,
				int ignoretrim = 1) const;

    // Persistent is true if the returned cache is not to be deleted by
    // the caller.
    virtual GU_RayIntersect	*createRayCache(int &persistent);

protected:
    bool		 doConvert(const GU_ConvertParms &parms) const;
private:
    static GA_PrimitiveDefinition	 *theDef;
};
}

#else

#include "GABC_PackedImpl.h"
#include <GU/GU_PrimPacked.h>
class GA_PrimitiveFactory;
namespace GABC_NAMESPACE
{
    namespace GABC_GUPrim
    {
	GABC_API void	registerMyself(GA_PrimitiveFactory *factory);
    }
}
#endif


#endif
