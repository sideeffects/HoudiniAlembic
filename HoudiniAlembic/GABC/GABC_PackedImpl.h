/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Side Effects Software Inc
 *	123 Front Street West, Suite 1401
 *	Toronto, Ontario
 *	Canada   M5J 2M2
 *	416-504-9876
 *
 * NAME:	GABC_PackedImpl.h (GABC Library, C++)
 *
 * COMMENTS:
 */

#ifndef __GABC_PackedImpl__
#define __GABC_PackedImpl__

#include "GABC_API.h"
#include "GABC_Util.h"
#include <GU/GU_PackedImpl.h>

namespace GABC_NAMESPACE
{

class GABC_API GABC_PackedImpl : public GU_PackedImpl
{
public:
    static void				install(GA_PrimitiveFactory *fact);
    static bool				isInstalled();
    static const GA_PrimitiveTypeId	&typeId();

    GABC_PackedImpl();
    GABC_PackedImpl(const GABC_PackedImpl &src);
    virtual ~GABC_PackedImpl();

    static GU_PrimPacked	*build(GU_Detail &gdp,
					const std::string &filename,
					const GABC_IObject &obj,
					fpreal frame,
					bool useTransform,
					bool useVisibility);


    /// Get the factory associated with this procedural
    virtual GU_PackedFactory	*getFactory() const;

    /// Create a copy of this resolver
    virtual GU_PackedImpl	*copy() const;

    /// Test whether the deferred load primitive data is valid
    virtual bool	isValid() const;

    /// Method to clear any data associated with the implementation.  It's
    /// possible that the implementation may need to re-construct the data, but
    /// this should clear what it can.
    virtual void	clearData();

    /// Give a UT_Options of load data, create resolver data for the primitive
    virtual bool	load(const UT_Options &options,
				const GA_LoadMap &map);

    /// Depending on the update, the procedural should call one of:
    ///	- transformDirty()
    ///	- attributeDirty()
    ///	- topologyDirty()
    virtual void	update(const UT_Options &options);

    /// Copy the resolver data into the UT_Options for saving
    virtual bool	save(UT_Options &options,
				const GA_SaveMap &map) const;

    /// Handle unknown token/value pairs when loading the primitive.  By
    /// default, this adds a warning and skips the next object.  Return false
    /// if there was a critical error.
    virtual bool	loadUnknownToken(const char *token, UT_JSONParser &p,
				const GA_LoadMap &map);

    /// Get the bounding box for the geometry (not including transforms)
    virtual bool	getBounds(UT_BoundingBox &box) const;

    /// Get the rendering bounding box for the geometry (not including
    /// transforms).  For curve and point geometry, this needs to include any
    /// "width" attributes.
    virtual bool	getRenderingBounds(UT_BoundingBox &box) const;

    /// When rendering with velocity blur, the renderer needs to know the
    /// bounds on velocity to accurately compute the bounding box.
    virtual void	getVelocityRange(UT_Vector3 &min,
				UT_Vector3 &max) const;

    /// Some procedurals have an "intrinsic" transform.  These are combined
    /// with the local transform on the geometry primitive.
    ///
    /// The default method returns false and leaves the transform unchanged.
    virtual bool	getLocalTransform(UT_Matrix4D &m) const;

    /// Unpack the procedural into a GU_Detail.  By default, this calls
    /// getGTFull() and converts the GT geometry to a GU_Detail.
    virtual bool	unpack(GU_Detail &destgdp) const;

    /// Return full geometry
    GT_PrimitiveHandle	fullGT() const;

    const GABC_IObject	 object() const;
    const std::string	&filename() const	{ return myFilename; }
    const std::string	&objectPath() const	{ return myObjectPath; }
    fpreal		 frame() const		{ return myFrame; }
    bool		 useTransform() const	{ return myUseTransform; }
    bool		 useVisibility() const	{ return myUseVisibility; }
    GABC_NodeType	 nodeType() const	{ return object().nodeType(); }

    void	setObject(const GABC_IObject &v);
    void	setFilename(const std::string &v)	{ myFilename = v; }
    void	setObjectPath(const std::string &v)	{ myObjectPath = v; }
    void	setFrame(fpreal f)			{ myFrame = f; }
    void	setUseTransform(bool v)			{ myUseTransform = v; }
    void	setUseVisibility(bool v)		{ myUseVisibility = v; }

    bool	isConstant() const
		    { return animationType() == GEO_ANIMATION_CONSTANT; }
    GEO_AnimationType	animationType() const;
protected:
#if 0
    /// Optional method to compute centroid (default uses bounding box)
    virtual UT_Vector3	getBaryCenter() const;
    /// Optional method to calculate volume (default uses bounding box)
    virtual fpreal	computeVolume(const UT_Vector3 &refpt) const;
    /// Optional method to calculate surface area (default uses bounding box)
    virtual fpreal	computeArea() const;
    /// Optional method to calculate perimeter (default uses bounding box)
    virtual fpreal	computePerimeter() const;
#endif

    mutable GABC_IObject	 myObject;
    std::string			 myFilename;
    std::string			 myObjectPath;
    fpreal			 myFrame;
    bool			 myUseTransform;
    bool			 myUseVisibility;
};

}

#endif
