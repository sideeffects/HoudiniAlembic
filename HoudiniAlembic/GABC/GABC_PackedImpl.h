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
#include <GT/GT_Primitive.h>

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

    /// Note: The caller is responsible for setting the vertex/point for the
    /// primitive.
    /// In non-Alembic packed primitive code, you probably just want to call
    ///   GU_PrimPacked::build(gdp, "AlembicRef")
    /// which handles the point creation automatically (see the packedsphere
    /// HDK sample code).
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
    virtual void	getWidthRange(fpreal &min, fpreal &max) const;

    /// Return the primitive's "description".  This should be a unique
    /// identifier for the primitive and defaults to:
    ///	  <tt>"%s.%d" % (getFactory()->name(), getPrim()->getNum()) </tt>
    virtual void	getPrimitiveName(UT_WorkBuffer &wbuf) const;

    /// Some procedurals have an "intrinsic" transform.  These are combined
    /// with the local transform on the geometry primitive.
    ///
    /// The default method returns false and leaves the transform unchanged.
    virtual bool	getLocalTransform(UT_Matrix4D &m) const;

    /// Unpack the procedural into a GU_Detail.  By default, this calls
    /// getGTFull() and converts the GT geometry to a GU_Detail.
    virtual bool	unpack(GU_Detail &destgdp) const;

    /// Unpack without using polygon soups
    virtual bool	unpackUsingPolygons(GU_Detail &destgdp) const;

    /// @{
    /// Return GT representations of geometry
    bool		visibleGT() const;
    GT_PrimitiveHandle	fullGT(int load_style=GABC_IObject::GABC_LOAD_FULL) const;
    GT_PrimitiveHandle	pointGT() const;
    GT_PrimitiveHandle	boxGT() const;
    GT_PrimitiveHandle	centroidGT() const;
    /// @}

    const GABC_IObject	&object() const;
    const std::string	&filename() const { return myFilename; }
    std::string		 intrinsicFilename() const { return myFilename; }
    const std::string	&objectPath() const	{ return myObjectPath; }
    std::string		 intrinsicObjectPath() const { return myObjectPath; }
    fpreal		 frame() const		{ return myFrame; }
    bool		 useTransform() const	{ return myUseTransform; }
    bool		 useVisibility() const	{ return myUseVisibility; }
    GABC_NodeType	 nodeType() const	{ return object().nodeType(); }
    GEO_AnimationType	 animationType() const;
    bool		 isConstant() const
			 {
			     return animationType() == GEO_ANIMATION_CONSTANT;
			 }
    const char		*intrinsicAnimation() const
			    { return GEOanimationType(animationType()); }
    const char		*intrinsicNodeType() const
			    { return GABCnodeType(nodeType()); }
    int64		 intrinsicVisibility() const
			    { return computeVisibility(false); }
    int64		 intrinsicFullVisibility() const
			    { return computeVisibility(true); }

    void	setObject(const GABC_IObject &v);
    void	setFilename(const std::string &v);
    void	setObjectPath(const std::string &v);
    void	setFrame(fpreal f);
    void	setUseTransform(bool v);
    void	setUseVisibility(bool v);
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

    class GTCache
    {
    public:
	GTCache()
	    : myVisibility(NULL)
	{
	    clear();
	}
	GTCache(const GTCache &)
	    : myVisibility(NULL)
	{
	    clear();	// Just clear
	}
	~GTCache()
	{
	    clear();
	}
	GTCache	&operator=(const GTCache &src)
	{
	    clear();	// Don't copy, just clear
	    return *this;
	}

	void	clear();		// Clear all values
	void	updateFrame(fpreal frame);

	bool				 visible(const GABC_PackedImpl *abc);
	const GT_PrimitiveHandle	&full(const GABC_PackedImpl *abc,
						int load_style);
	const GT_PrimitiveHandle	&points(const GABC_PackedImpl *abc);
	const GT_PrimitiveHandle	&box(const GABC_PackedImpl *abc);
	const GT_PrimitiveHandle	&centroid(const GABC_PackedImpl *abc);
	GEO_AnimationType	 animationType(const GABC_PackedImpl *abc);

    private:
	void	updateTransform(const GABC_PackedImpl *abc);

	GT_PrimitiveHandle	 myPrim;
	GT_TransformHandle	 myTransform;
	GEO_AnimationType	 myAnimationType;
	GEO_ViewportLOD		 myRep;
	GABC_VisibilityCache	*myVisibility;
	fpreal			 myFrame;
	int			 myLoadStyle;
    };

private:
    void	markDirty();

    GABC_VisibilityType computeVisibility(bool include_parent) const;
    void	clearGT();
    bool	unpackGeometry(GU_Detail &destgdp, bool allow_psoup) const;

    mutable GABC_IObject	 myObject;
    mutable GTCache		 myCache;
    std::string			 myFilename;
    std::string			 myObjectPath;
    fpreal			 myFrame;
    bool			 myUseTransform;
    bool			 myUseVisibility;
};

}

#endif
