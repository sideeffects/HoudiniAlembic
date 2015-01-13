/*
 * Copyright (c) 2015
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

#ifndef __ROP_AbcOpXform__
#define __ROP_AbcOpXform__

#include "ROP_AbcObject.h"
#include "ROP_AbcContext.h"
#include "GABC/GABC_Util.h"

class OBJ_Node;

/// An Alembic Xform node, as represented in Houdini by an Alembic Xform,
/// Subnet, or Geometry object.
class ROP_AbcOpXform : public ROP_AbcObject
{
public:
    typedef Alembic::Abc::OCompoundProperty             OCompoundProperty;
    typedef Alembic::AbcGeom::OXform			OXform;
    typedef Alembic::AbcGeom::OVisibilityProperty	OVisibilityProperty;
    typedef Alembic::Abc::OObject			OObject;

    typedef GABC_NAMESPACE::GABC_Util::PropertyMap      PropertyMap;

    ROP_AbcOpXform(OBJ_Node *node, const ROP_AbcContext &ctx);
    virtual ~ROP_AbcOpXform();

    /// @{
    /// Interface defined on ROP_AbcObject
    virtual const char	*className() const	{ return "OpXform"; }
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

private:
    /// User properties state.
    ///
    /// NO_USER_PROPERTIES:	    No user properties found. This will happen
    ///                             if the meta data or value isn't set.
    /// ERROR_READING_PROPERTIES:   An error occured reading user property data,
    ///		                    as a result, no properties will be written.
    /// WRITE_USER_PROPERTIES:	    Try writing user properties for subsequent
    ///				    frames, but if there's a problem, fall back
    ///				    to existing samples.
    enum UserPropertiesState
    {
	NO_USER_PROPERTIES,
	ERROR_READING_PROPERTIES,
	WRITE_USER_PROPERTIES,

	UNSET=-1
    };

    void                setVisibility(const ROP_AbcContext &ctx);

    /// Based on the boolean write flag, this function will either make the
    /// user property schema or write user properties to that parent schema.
    /// The schema should always be created first before any writing.
    bool		makeOrWriteUserProperties(const OBJ_Node *node,
				GABC_OError &err,
				const ROP_AbcContext &ctx,
				bool write);
    /// Clears the user property mapping.
    void	        clearUserProperties();
    UT_Matrix4D         myMatrix;
    UT_BoundingBox      myBox;
    OXform              myOXform;
    OVisibilityProperty myVisibility;
    PropertyMap         myUserProperties;
    UserPropertiesState myUserPropertiesState;
    int                 myNodeId;
    bool                myTimeDependent;
    bool                myIdentity;
    bool                myGeometryContainer;
};

#endif

