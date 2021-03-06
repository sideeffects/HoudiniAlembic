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

#ifndef __GABC_PackedImpl__
#define __GABC_PackedImpl__

#include "GABC_API.h"
#include "GABC_Util.h"
#include <GU/GU_PackedImpl.h>
#include <GT/GT_Primitive.h>
#include <UT/UT_Lock.h>

class GT_AlembicCache;

namespace GABC_NAMESPACE
{

class GABC_API GABC_PackedImpl : public GU_PackedImpl
{
public:
    static void				install(GA_PrimitiveFactory *fact);
    static bool				isInstalled();
    static GA_PrimitiveTypeId typeId()
    {
        return theTypeId;
    }

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
					const UT_StringHolder &filename,
					const GABC_IObject &obj,
					fpreal frame,
					bool useTransform,
					bool useVisibility);


    /// Get the factory associated with this procedural
    virtual GU_PackedFactory	*getFactory() const;

    /// Create a copy of this resolver
    virtual GU_PackedImpl	*copy() const;

    /// Report memory usage (includes all shared memory)
    virtual int64 getMemoryUsage(bool inclusive) const;

    /// Count memory usage using a UT_MemoryCounter in order to count
    /// shared memory correctly.
    virtual void countMemory(UT_MemoryCounter &counter, bool inclusive) const;

    /// Test whether the deferred load primitive data is valid
    virtual bool	isValid() const;

    /// Method to clear any data associated with the implementation.  It's
    /// possible that the implementation may need to re-construct the data, but
    /// this should clear what it can.
    virtual void	clearData();

    /// Give a UT_Options of load data, create resolver data for the primitive
    virtual bool	load(GU_PrimPacked *prim, const UT_Options &options,
				const GA_LoadMap &map)
			    { return loadFrom(prim, options, map); }
    virtual bool	supportsJSONLoad() const	{ return true; }
    virtual bool	loadFromJSON(GU_PrimPacked *prim, const UT_JSONValueMap &options,
				const GA_LoadMap &map)
			    { return loadFrom(prim, options, map); }

    /// Depending on the update, the procedural should call one of:
    /// - prim->transformDirty()
    /// - prim->attributeDirty()
    /// - prim->topologyDirty()
    virtual void	update(GU_PrimPacked *prim, const UT_Options &options);

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
    ///	  <tt>"%s.%d" % (getFactory()->name(), prim->getMapIndex()) </tt>
    virtual void	getPrimitiveName(const GU_PrimPacked *prim, UT_WorkBuffer &wbuf) const;

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
    bool		visibleGT(bool *is_animated = NULL) const;
    GT_PrimitiveHandle	fullGT(int load_style=GABC_IObject::GABC_LOAD_FULL) const;
    GT_PrimitiveHandle	pointGT() const;
    GT_PrimitiveHandle	boxGT() const;
    GT_PrimitiveHandle	centroidGT() const;
    /// @}

    /// Get the geometry for "instancing".  This geometry doesn't have the
    /// transform to world space, nor does it have the Houdini attributes from
    /// the primitive.
    GT_PrimitiveHandle	instanceGT(bool ignore_visibility = false) const;

    /// The xformGT will return the transform for the primitive, regardless of
    /// whether the load_style for full geometry was set to force untransformed
    /// geometry.
    GT_TransformHandle	xformGT() const;

    const GABC_IObject	&object() const;
    const UT_StringHolder &filename() const { return myFilename; }
    UT_StringHolder	  intrinsicFilename(const GU_PrimPacked *prim) const { return myFilename; }
    const UT_StringHolder &objectPath() const	{ return myObjectPath; }
    UT_StringHolder	 intrinsicObjectPath(const GU_PrimPacked *prim) const { return myObjectPath; }
    UT_StringHolder	 intrinsicSourcePath(const GU_PrimPacked *prim) const
			    { return object().getSourcePath(); }
    int64		 intrinsicPointCount(const GU_PrimPacked *prim) const
			    { return object().getPointCount(myFrame); }
    fpreal		 frame() const		{ return myFrame; }
    fpreal               intrinsicFrame(const GU_PrimPacked *prim) const { return myFrame; }
    bool		 useTransform() const	{ return myUseTransform; }
    bool                 intrinsicUseTransform(const GU_PrimPacked *prim) const { return myUseTransform; }
    bool		 useVisibility() const	{ return myUseVisibility; }
    bool                 intrinsicUseVisibility(const GU_PrimPacked *prim) const { return myUseVisibility; }
    GABC_NodeType	 nodeType() const	{ return object().nodeType(); }
    GEO_AnimationType	 animationType() const;
    int			 currentLoadStyle() const { return myCache.loadStyle();}
    bool		 isConstant() const
			 {
			     return animationType() == GEO_ANIMATION_CONSTANT;
			 }
    const char		*intrinsicAnimation(const GU_PrimPacked *prim) const
			    { return GEOanimationType(animationType()); }
    const char		*intrinsicNodeType(const GU_PrimPacked *prim) const
			    { return GABCnodeType(nodeType()); }
    int64		 intrinsicVisibility(const GU_PrimPacked *prim) const
			    { return computeVisibility(false); }
    int64		 intrinsicFullVisibility(const GU_PrimPacked *prim) const
			    { return computeVisibility(true); }
    
    UT_StringHolder      intrinsicPoint(const GU_PrimPacked *prim) const 
                            { return getAttributeNames(GT_OWNER_POINT); }
    UT_StringHolder      intrinsicVertex(const GU_PrimPacked *prim) const 
                            { return getAttributeNames(GT_OWNER_VERTEX); }
    UT_StringHolder      intrinsicPrimitive(const GU_PrimPacked *prim) const 
                            { return getAttributeNames(GT_OWNER_PRIMITIVE); }
    UT_StringHolder      intrinsicDetail(const GU_PrimPacked *prim) const 
                            { return getAttributeNames(GT_OWNER_DETAIL); }
    UT_StringHolder      intrinsicFaceSet(const GU_PrimPacked *prim) const
                            { return getFaceSetNames(); }

    /// Returns a sys_wang64 hash of the sum of 64B values making up the Alembic
    /// property hash.
    int64		 getPropertiesHash() const;

    void	setObject(const GABC_IObject &v);
    void	setFilename(GU_PrimPacked *prim, const UT_StringHolder &v);
    void	setObjectPath(GU_PrimPacked *prim, const UT_StringHolder &v);
    void	setFrame(GU_PrimPacked *prim, fpreal f);
    void	setUseTransform(GU_PrimPacked *prim, bool v);
    void	setUseVisibility(GU_PrimPacked *prim, bool v);

    void	setViewportCache(GT_AlembicCache *cache) const;
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

    /// Method to load from either UT_Options or UT_JSONValueMap
    template <typename T>
    bool	loadFrom(GU_PrimPacked *prim, const T &options, const GA_LoadMap &map);

    class GTCache
    {
    public:
	GTCache()
	    : myAnimationType(GEO_ANIMATION_INVALID)
	{
	    clear();
	}
	GTCache(const GTCache &)
	    : myAnimationType(GEO_ANIMATION_INVALID)
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

        int64   getMemoryUsage(bool inclusive) const;

	void	clear();		// Clear all values
	void	updateFrame(fpreal frame);

	bool				 visible(const GABC_PackedImpl *abc,
						 bool *is_animated = NULL);
	const GT_PrimitiveHandle	&full(const GABC_PackedImpl *abc,
					      int load_style);
	const GT_PrimitiveHandle	&points(const GABC_PackedImpl *abc);
	const GT_PrimitiveHandle	&box(const GABC_PackedImpl *abc);
	const GT_PrimitiveHandle	&centroid(const GABC_PackedImpl *abc);
	GEO_AnimationType	 animationType(const GABC_PackedImpl *abc);
	GEO_AnimationType	 animationType() const
				    { return myAnimationType; }

	/// Return the current transform handle
	const GT_TransformHandle	&xform(const GABC_PackedImpl *abc)
	{
	    refreshTransform(abc);
	    return myTransform;
	}

	int	loadStyle() const { return myLoadStyle; }

    private:
	void	refreshTransform(const GABC_PackedImpl *abc);
	void	updateTransform(const GABC_PackedImpl *abc);

	GT_PrimitiveHandle	 myPrim;
	GT_TransformHandle	 myTransform;
	GEO_AnimationType	 myAnimationType;
	GEO_ViewportLOD		 myRep;
	fpreal			 myFrame;
	int			 myLoadStyle;
    };

private:
    void markDirty(GU_PrimPacked *prim);

    GABC_VisibilityType computeVisibility(bool include_parent) const;

    UT_StringHolder getAttributeNames(GT_Owner owner) const;
    UT_StringHolder getFaceSetNames() const;

    void	clearGT();
    bool	unpackGeometry(GU_Detail &destgdp, bool allow_psoup) const;

    mutable UT_Lock		myLock;
    mutable GABC_IObject	myObject;
    mutable GTCache		myCache;
    mutable bool		myCachedUniqueID;
    mutable int64		myUniqueID;
    UT_StringHolder		myFilename;
    UT_StringHolder		myObjectPath;
    fpreal			myFrame;
    bool			myUseTransform;
    bool			myUseVisibility;

    mutable GABC_VisibilityType myConstVisibility;
    mutable bool		myHasConstBounds;
    mutable UT_BoundingBox	myConstBounds;
    mutable GT_AlembicCache  *myViewportCache;

    static GA_PrimitiveTypeId theTypeId;
};

}

#endif
