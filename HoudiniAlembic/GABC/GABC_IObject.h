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

#ifndef __GABC_IObject__
#define __GABC_IObject__

#include "GABC_API.h"
#include "GABC_Include.h"
#include "GABC_Types.h"
#include "GABC_IItem.h"
#include <GEO/GEO_PackedNameMap.h>
#include <UT/UT_Matrix4.h>
#include <UT/UT_BoundingBox.h>
#include <GT/GT_DataArray.h>
#include <GT/GT_Handles.h>
#include <Alembic/Abc/IObject.h>
#include <Alembic/AbcGeom/GeometryScope.h>

class GEO_Primitive;
class UT_StringArray;

namespace GABC_NAMESPACE
{

enum GABC_VisibilityType
{
    GABC_VISIBLE_DEFER		= -1,
    GABC_VISIBLE_HIDDEN		= 0,
    GABC_VISIBLE_VISIBLE	= 1
};

/// This class wraps an Alembic IObject and provides convenience methods that
/// allow thread-safe access to its data.
///
/// Do not grab and hold onto the contained IObject as this may cause
/// referencing issues on archives.
class GABC_API GABC_IObject : public GABC_IItem
{
public:
    typedef Alembic::Abc::M44d			    M44d;
    typedef Alembic::Abc::IObject		    IObject;
    typedef Alembic::Abc::ObjectHeader		    ObjectHeader;
    typedef Alembic::Abc::ICompoundProperty	    ICompoundProperty;
    typedef Alembic::Abc::PropertyHeader	    PropertyHeader;
    typedef Alembic::Abc::TimeSamplingPtr	    TimeSamplingPtr;
    typedef Alembic::AbcGeom::GeometryScope	    GeometryScope;
    typedef Alembic::Abc::CompoundPropertyReaderPtr CompoundPropertyReaderPtr;

    GABC_IObject();
    GABC_IObject(const GABC_IObject &obj);
    GABC_IObject(const GABC_IArchivePtr &arch, const IObject &obj);
    GABC_IObject(const GABC_IArchivePtr &arch, const std::string &objectpath);
    virtual ~GABC_IObject();

    /// Initialize (called by GABC_IArchive)
    static void		init();

    /// Return the number of children
    exint		getNumChildren() const;

    /// @{
    /// Get the given child
    GABC_IObject	getChild(exint index) const;
    GABC_IObject	getChild(const std::string &name) const;
    /// @}

    /// Get the parent
    GABC_IObject	getParent() const
			{
			    if (!valid())
				return GABC_IObject();
			    return GABC_IObject(archive(),object().getParent());
			}

    /// Get my header
    ObjectHeader	getHeader() const
			    { return object().getHeader(); }
    /// Get the child header
    ObjectHeader	getChildHeader(exint i) const
			    { return object().getChildHeader(i); }

    /// Get the name of the object
    std::string		getName() const	{ return myObject.getName(); }

    /// Get the full name of the object
    const std::string	&getFullName() const { return myObjectPath; }

    /// @{
    /// Interface from GABC_IItem
    virtual void	purge();
    /// @}

    /// Test validity
    bool		valid() const	{ return myObject.valid(); }

    /// Assignment operator
    GABC_IObject	&operator=(const GABC_IObject &src);

    /// Query the type of this node
    GABC_NodeType	nodeType() const;

    /// Query whether the node is a Maya locator
    bool		isMayaLocator() const;

    /// Query visibility
    GABC_VisibilityType	visibility(bool &animated, fpreal t,
				bool check_parent=false) const;

    /// Get animation type for this node.
    /// @note This only checks animation types of intrinsic properties
    GEO_AnimationType	getAnimationType(bool include_transform) const;

    /// Get the bounding box.  Returns false if there are no bounds defined
    bool		getBoundingBox(UT_BoundingBox &box, fpreal t,
				bool &isConstant) const;

    /// Get the bounding box for rendering (includes the "width" attribute for
    /// curves and points).
    bool		getRenderingBoundingBox(UT_BoundingBox &box,
				fpreal t) const;

    enum
    {
	GABC_LOAD_LEAN_AND_MEAN	= 0x00,	// Only load intrinsic attributes
	GABC_LOAD_ARBS		= 0x01,	// Load arbitrary attributes
	GABC_LOAD_FACESETS	= 0x02,	// Load face sets
	GABC_LOAD_HOUDINI	= 0x04,	// Load houdini attributes too
	GABC_LOAD_FULL		= 0xff,	// Load full geometry

	// Forcibly load geometry untransformed
	GABC_LOAD_FORCE_UNTRANSFORMED	= 0x1000,
    };

    UT_StringHolder getAttributes(const GEO_PackedNameMapPtr &namemap,
                                int load_style,
                                GT_Owner owner) const;

    UT_StringHolder getFaceSets(const UT_StringHolder &attrib, fpreal t, int load_style) const;

    /// Get a representation of the shape.  If the @c GEO_Primitive pointer is
    /// non-NULL, the primitive attributes will be added as GT arrays.
    GT_PrimitiveHandle	getPrimitive(const GEO_Primitive *prim,
				fpreal t,
				GEO_AnimationType &atype,
				const GEO_PackedNameMapPtr &namemap,
                                const UT_StringHolder &facesetAttrib,
				int load_style=GABC_LOAD_FULL) const;

    /// Update primitive time.  Given a primitive created by @c getPrimitive(),
    /// update the transform/attribute arrays with values from the new time.
    GT_PrimitiveHandle	updatePrimitive(const GT_PrimitiveHandle &src,
				const GEO_Primitive *prim,
				fpreal new_time,
				const GEO_PackedNameMapPtr &namemap,
                                const UT_StringHolder &facesetAttrib,
				int load_style=GABC_LOAD_FULL) const;

    /// Get a representation of the point cloud.  This doesn't include any
    /// attributes.
    GT_PrimitiveHandle	getPointCloud(fpreal t, GEO_AnimationType &a) const;

    /// Get a representation of the bounding box.  This doesn't include any
    /// attributes.
    GT_PrimitiveHandle	getBoxGeometry(fpreal t, GEO_AnimationType &a) const;

    /// Get a point for the centroid of the bounding box.  This doesn't include
    /// any attributes.
    GT_PrimitiveHandle	getCentroidGeometry(fpreal t, GEO_AnimationType &a) const;

    /// Get local transform for the object.  For shape objects, this will
    /// return an identity transform.  The animation type only considers the
    /// local transform (not all the parent transforms).
    ///
    /// The transform will be blended between samples if the time doesn't align
    /// perfectly with a sample point.
    bool		getLocalTransform(UT_Matrix4D &xform, fpreal t,
				GEO_AnimationType &atype,
				bool &inherits) const;

    /// Get world transform for the object.  The animation type includes all
    /// parent transforms.
    ///
    /// The transform will be blended between samples if the time doesn't align
    /// perfectly with a sample point.
    bool		getWorldTransform(UT_Matrix4D &xform, fpreal t,
				GEO_AnimationType &atype) const;

    /// Check whether the transform (or parent transforms) is animated
    bool		isTransformAnimated() const;

    /// Query the number of geometry properties
    exint		 getNumGeometryProperties() const;
    /// Lookup the data array for the Nth geometry property at the given time.
    ///  - The @c name parameter is filled out with the property name
    ///  - The @c scope parameter is filled out with the property scope
    ///  - The @c atype parameter is filled out with the animation type
    GT_DataArrayHandle	 getGeometryProperty(exint index, fpreal t,
				std::string &name,
				GeometryScope &scope,
				GEO_AnimationType &atype) const;

    /// Convert an arbitrary property to a GT_DataArray.  This handles indexed
    /// array types as well as straight data types.
    ///
    /// This is a convenience method to extract an array given the compound
    /// property and the property header.
    ///
    /// If supplied, the animation type is filled out.
    GT_DataArrayHandle	convertIProperty(ICompoundProperty &arb,
					const PropertyHeader &head,
					fpreal time,
					GEO_AnimationType *atype=NULL,
					exint expected_size=-1) const;

    /// Get position property from shape node
    GT_DataArrayHandle	getPosition(fpreal t, GEO_AnimationType &atype) const;
    /// Get velocity property from shape node
    GT_DataArrayHandle	getVelocity(fpreal t, GEO_AnimationType &atype) const;
    /// Get the width property from the shape node (curves/points)
    GT_DataArrayHandle	getWidth(fpreal t, GEO_AnimationType &atype) const;

    /// Lookup the data array for the named geometry property at the given time.
    ///  - The @c scope parameter is filled out with the property scope
    ///  - The @c atype parameter is filled out with the animation type
    GT_DataArrayHandle	 getGeometryProperty(const std::string &name, fpreal t,
				GeometryScope &scope,
				GEO_AnimationType &atype) const;

    /// Get number of user properties for this node
    exint		getNumUserProperties() const;
    /// Lookup the data array for the Nth geometry property at the given time.
    ///  - The @c name parameter is filled out with the property name
    ///  - The @c atype parameter is filled out with the animation type
    GT_DataArrayHandle	getUserProperty(exint index, fpreal t,
				std::string &name,
				GEO_AnimationType &atype) const;
    /// Lookup the data array for the Nth geometry property at the given time.
    ///  - The @c name parameter is filled out with the property name
    ///  - The @c atype parameter is filled out with the animation type
    GT_DataArrayHandle	getUserProperty(const std::string &name, fpreal t,
				GEO_AnimationType &atype) const;

    /// Access the time sampling pointer
    TimeSamplingPtr	timeSampling() const;

    exint		numSamples() const;

    /// Clamp the time to the animated time range
    fpreal		clampTime(fpreal input_time) const;

    /// @{
    /// Get the world transform for the node.  This includes all it's parent
    /// transforms.
    bool		worldTransform(fpreal t,
				    UT_Matrix4D &xform,
				    bool &isConstant,
				    bool &inheritsXform) const;
    bool		worldTransform(fpreal t,
				    M44d &xform,
				    bool &isConstant,
				    bool &inheritsXform) const;
    /// @}

    /// @{
    /// Get the local transform for the node.  This is an identity matrix 
    bool		localTransform(fpreal t,
				    UT_Matrix4D &xform,
				    bool &isConstant,
				    bool &inheritsXform) const;
    bool		localTransform(fpreal t,
				    M44d &xform,
				    bool &isConstant,
				    bool &inheritsXform) const;
    /// @}

    /// Alembic's 128b prop hash wang'hashed to 64b. Returns false if no hash
    /// exists (getPropertiesHash() returns false, such as for HDF5).
    bool		getPropertiesHash(int64 &hash) const;

    /// @{
    /// Member data access
    const std::string	&objectPath() const		{ return myObjectPath; }
    const IObject	&object() const			{ return myObject; }
    /// @}

    /// Get the user properties for this node
    ICompoundProperty   getUserProperties() const;

private:
    ICompoundProperty   getArbGeomParams() const;
    void                setObject(const IObject &o)	{ myObject = o; }

    std::string         myObjectPath;
    IObject             myObject;

    friend class        GABC_IArchive;
};
}

#endif
