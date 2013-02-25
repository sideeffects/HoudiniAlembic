/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Side Effects Software Inc
 *	477 Richmond Street West
 *	Toronto, Ontario
 *	Canada   M5V 3E7
 *	416-504-9876
 *
 * NAME:	GABC_GUPrim.h ( GU Library, C++)
 *
 * COMMENTS: Tetra utilitiy functions.
 */

#ifndef __GABC_GUPrim__
#define __GABC_GUPrim__

#include "GABC_API.h"
#include <GA/GA_PrimitiveDefinition.h>
#include "GABC_GEOPrim.h"
#include <GU/GU_Detail.h>
#include <GU/GU_Prim.h>

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

#endif
