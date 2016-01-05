/*
 * Copyright (c) 2016
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

#ifndef __ROP_AbcGTShape__
#define __ROP_AbcGTShape__

#include "ROP_AbcContext.h"
#include "ROP_AbcObject.h"
#include <GT/GT_Handles.h>
#include <GABC/GABC_OError.h>
#include <GABC/GABC_OGTAbc.h>
#include <GABC/GABC_OGTGeometry.h>
#include <GABC/GABC_OXform.h>
#include <UT/UT_WorkArgs.h>

class ROP_AbcGTInstance;

/// This class represents a single GT_Primitive. If "Build Hierarchy From
/// Attribute" is enabled, this class is where the hierarchy will be constructed.
class ROP_AbcGTShape : public ROP_AbcObject
{
public:
    enum ShapeType {
        GEOMETRY,
        INSTANCE,
        ALEMBIC
    };

    static const char	*shapeType(ShapeType t)
    {
	switch (t)
	{
	    case GEOMETRY:	return "Geometry";
	    case INSTANCE:	return "Instance";
	    case ALEMBIC:	return "Alembic";
	}
	return "<invalid>";
    }

    typedef Alembic::Abc::OObject		    OObject;

    typedef Alembic::AbcGeom::ObjectVisibility	    ObjectVisibility;

    typedef GABC_NAMESPACE::GABC_OError		    GABC_OError;
    typedef GABC_NAMESPACE::GABC_OGTAbc             GABC_OGTAbc;
    typedef GABC_NAMESPACE::GABC_OGTGeometry	    GABC_OGTGeometry;

    typedef GABC_NAMESPACE::GABC_OXform             OXform;

    typedef UT_Map<std::string, UT_Matrix4D>        InverseMap;
    typedef std::pair<std::string, UT_Matrix4D>     InverseMapInsert;
    typedef UT_Set<std::string>                     GeoSet;
    typedef UT_Map<std::string, OXform *>           XformMap;
    typedef std::pair<std::string, OXform *>        XformMapInsert;
    typedef UT_Map<std::string, GABC_OGTAbc *>      XformUserPropsMap;
    typedef std::pair<std::string, GABC_OGTAbc *>   XformUserPropsMapInsert;

    ROP_AbcGTShape(const std::string &name,
            const char * const path,
            InverseMap * const inv_map,
            GeoSet * const shape_set,
            XformMap * const xform_map,
	    XformUserPropsMap *const user_prop_map,
            const ShapeType type,
            bool geo_lock);
    virtual ~ROP_AbcGTShape();

    static bool	isPrimitiveSupported(const GT_PrimitiveHandle &prim);

    /// Write first frame to the archive
    bool	firstFrame(const GT_PrimitiveHandle &prim,
			    const OObject &parent,
                            const ObjectVisibility vis,
			    const ROP_AbcContext &ctx,
			    GABC_OError &err,
			    bool calc_inverse,
			    const bool subd_mode,
			    const bool add_unused_pts);
    /// Write the next frame to the archive
    bool	nextFrame(const GT_PrimitiveHandle &prim,
			    const ROP_AbcContext &ctx,
			    GABC_OError &err,
			    bool calc_inverse);
    /// Write the next frame to the archive with the same sample
    /// values as the previous frame.
    bool	nextFrameFromPrevious(GABC_OError &err,
                        ObjectVisibility vis = Alembic::AbcGeom::kVisibilityHidden,
                        exint frames = 1);

    /// Return the primitive type for this shape.
    int         getPrimitiveType() const;

    /// Return the OObject for the shape
    OObject	getOObject() const;

    virtual void	dump(int indent=0) const;

private:
    /// @{
    /// Interface defined on ROP_AbcObject.
    ///
    /// For GTShapes, these methods are not implemented.  Time dependence is
    /// known only by the parent.
    virtual const char	*className() const	{ return "OGTShape"; }
    virtual bool	start(const OObject &parent,
				GABC_OError &err,
				const ROP_AbcContext &ctx,
				UT_BoundingBox &box);
    virtual bool	update(GABC_OError &err,
				const ROP_AbcContext &ctx,
				UT_BoundingBox &box);
    virtual bool	selfTimeDependent() const;
    virtual bool	getLastBounds(UT_BoundingBox &box) const;
    /// @}

    void	clear();
private:
    union {
        GABC_OGTGeometry   *myShape;
        ROP_AbcGTInstance  *myInstance;
        GABC_OGTAbc        *myAlembic;
        void               *myVoidPtr;
    } myObj;

    InverseMap		*const myInverseMap;
    GeoSet		*const myGeoSet;
    XformMap		*const myXformMap;
    XformUserPropsMap   *const myXformUserPropsMap;

    const ShapeType	 myType;
    UT_String		 myPath;
    UT_WorkArgs		 myTokens;
    // Use std::string since the name is shared by the ABCGTGeometry and
    // std::string has COW semantics.
    const std::string	 myName;
    exint		 myElapsedFrames;
    int			 myPrimType;
    const bool		 myGeoLock;
};

#endif

