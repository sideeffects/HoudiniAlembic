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
 * NAME:	GABC_GEOPrim.h ( GEO Library, C++)
 *
 * COMMENTS:	This is the base class for all triangle mesh types.
 */

#ifndef __GABC_GEOPrim__
#define __GABC_GEOPrim__

#include "GABC_API.h"
#include "GABC_Util.h"
#include <Alembic/AbcGeom/Foundation.h>		// For topology enum

#include <GT/GT_Handles.h>
#include <GT/GT_Primitive.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Vertex.h>
#include <GA/GA_Defines.h>
#include <UT/UT_Array.h>


class GEO_Detail;
class GEO_ABCNameMap;

typedef UT_IntrusivePtr<GEO_ABCNameMap>   GEO_ABCNameMapPtr;

/// Map to translate from Alembic attribute names to Houdini names
class GABC_API GEO_ABCNameMap
{
public:
    typedef UT_SymbolMap<UT_String>	MapType;

     GEO_ABCNameMap();
    ~GEO_ABCNameMap();

    /// Number of entries in the map
    exint	entries() const	{ return myMap.entries(); }

    /// Compare equality
    bool	isEqual(const GEO_ABCNameMap &src) const;

    /// @{
    /// Equality operator
    bool	operator==(const GEO_ABCNameMap &src) const
		    { return isEqual(src); }
    bool	operator!=(const GEO_ABCNameMap &src) const
		    { return !isEqual(src); }
    /// @}

    /// Get the name mapping.  If the name isn't mapped, the original name
    /// will be returned.
    /// If the attribute should be skipped, a NULL pointer will be returned.
    const char	*getName(const char *name) const;
    const char	*getName(const std::string &name) const
			{ return getName(name.c_str()); }

    /// Add a translation from the abcName to the houdini attribute name
    void		 addMap(const char *abcName, const char *houdiniName);

    /// Avoid adding an attribute of the given name.  This is done by 
    void		 skip(const char *abcName)
			    { addMap(abcName, NULL); }

    /// @{
    /// JSON I/O
    bool	save(UT_JSONWriter &w) const;
    static bool	load(GEO_ABCNameMapPtr &map, UT_JSONParser &p);
    /// @}

    /// @{
    /// Reference counting
    void	incref()	{ myRefCount.add(1); }
    void	decref()
		{
		    if (!myRefCount.add(-1))
			delete this;
		}
    /// @}

private:
    MapType		myMap;
    SYS_AtomicInt32	myRefCount;
};

static inline void intrusive_ptr_add_ref(GEO_ABCNameMap *m) { m->incref(); }
static inline void intrusive_ptr_release(GEO_ABCNameMap *m) { m->decref(); }

class GABC_API GABC_GEOPrim : public GEO_Primitive
{
public:
    typedef Alembic::Abc::IObject	IObject;

    GABC_GEOPrim(GEO_Detail *d, GA_Offset offset = GA_INVALID_OFFSET);
    virtual ~GABC_GEOPrim();

    /// @{
    /// Required interface methods
    virtual bool  	isDegenerate() const;
    virtual int		getBBox(UT_BoundingBox *bbox) const;
    virtual void	enlargePointBounds(UT_BoundingBox &box) const;
    virtual void	reverse();
    virtual UT_Vector3  computeNormal() const;
    virtual void	copyPrimitive(const GEO_Primitive *src, 
					    GEO_Point **ptredirect);
    virtual void	copyUnwiredForMerge(const GA_Primitive *src,
					    const GA_MergeMap &map);

    // Query the number of vertices in the array. This number may be smaller
    // than the actual size of the array.
    virtual GA_Size	getVertexCount() const;
    virtual GA_Offset	getVertexOffset(GA_Size) const;

    // Take the whole set of points into consideration when applying the
    // point removal operation to this primitive. The method returns 0 if
    // successful, -1 if it failed because it would have become degenerate,
    // and -2 if it failed because it would have had to remove the primitive
    // altogether.
    virtual int		 detachPoints(GA_PointGroup &grp);
    /// Before a point is deleted, all primitives using the point will be
    /// notified.  The method should return "false" if it's impossible to
    /// delete the point.  Otherwise, the vertices should be removed.
    virtual GA_DereferenceStatus        dereferencePoint(GA_Offset point,
						bool dry_run=false);
    virtual GA_DereferenceStatus        dereferencePoints(
						const GA_RangeMemberQuery &pt_q,
						bool dry_run=false);
    virtual const GA_PrimitiveJSON	*getJSON() const;

    /// Defragmentation
    virtual void	swapVertexOffsets(const GA_Defragment &defrag);

    /// Evalaute a point given a u,v coordinate (with derivatives)
    virtual bool	evaluatePointRefMap(GA_Offset result_vtx,
				GA_AttributeRefMap &hlist,
				fpreal u, fpreal v, uint du, uint dv) const;
    /// Evalaute position given a u,v coordinate (with derivatives)
    virtual int		evaluatePointV4( UT_Vector4 &pos, float u, float v = 0,
					unsigned du=0, unsigned dv=0) const
			 {
			    return GEO_Primitive::evaluatePoint(pos, u, v,
					du, dv);
			 }
    /// @}

    /// This method is implemented to assist with rendering.  To get accurate
    /// bounding boxes for rendering, the "width" of points and "curve" objects
    /// needs to be taken into account.
    ///
    /// Returns false if the bounds are invalid.
    bool	getRenderingBounds(UT_BoundingBox &box) const;

    /// @{
    /// Though not strictly required (i.e. not pure virtual), these methods
    /// should be implemented for proper behaviour.
    virtual GEO_Primitive	*copy(int preserve_shared_pts = 0) const;

    // Have we been deactivated and stashed?
    virtual void	stashed(int onoff, GA_Offset offset=GA_INVALID_OFFSET);

    // We need to invalidate the vertex offsets
    virtual void	clearForDeletion();

    virtual void	copyOffsetPrimitive(const GEO_Primitive *src, int base);
    /// @}

    /// @{
    /// Optional interface methods.  Though not required, implementing these
    /// will give better behaviour for the primitive.
    virtual void	transform(const UT_Matrix4 &xform);
    virtual UT_Vector3	baryCenter() const;
    virtual fpreal	calcVolume(const UT_Vector3 &refpt) const;
    virtual fpreal	calcArea() const;
    virtual fpreal	calcPerimeter() const;
    virtual void	getLocalTransform(UT_Matrix4D &matrix) const;
    virtual void	setLocalTransform(const UT_Matrix4D &matrix);
    /// @}

    /// @{
    /// Alembic interface
    const std::string	&getFilename() const	{ return myFilename; }
    const std::string	&getObjectPath() const	{ return myObjectPath; }
    const IObject	&getObject() const	{ return myObject; }
    GABC_AnimationType	 animation() const	{ return myAnimation; }
    bool		 isConstant() const
			 {
			     return myAnimation == GABC_ANIMATION_CONSTANT;
			 }
    fpreal		 getFrame() const	{ return myFrame; }

    void		 init(const std::string &filename,
				const std::string &objectpath,
				fpreal frame,
				bool use_transform);
    void		 init(const std::string &filename,
				const IObject &objectpath,
				fpreal frame,
				bool use_transform);
    void		 setFilename(const std::string &filename);
    void		 setObjectPath(const std::string &path);
    void		 setFrame(fpreal f);
    bool		 getTransform(UT_Matrix4D &xform) const;
    bool		 useTransform() const
			    { return myUseTransform; }
    void		 setUseTransform(bool v);
    /// @}

    /// Return the GT representation of the primitive
    GT_PrimitiveHandle	 gtPrimitive() const;
    /// Return the point cloud for the primitive.
    GT_PrimitiveHandle	 gtPointCloud() const;
    void		 clearGT();

    /// @{
    /// Geo transform
    void			setGeoTransform(const GT_TransformHandle &x);
    const GT_TransformHandle	&geoTransform() const
				    { return myGeoTransform; }
    /// @}

    /// @{
    /// Attribute name mappings
    void			setAttributeNameMap(const GEO_ABCNameMapPtr &m);
    const GEO_ABCNameMapPtr	&attributeNameMap() const
				    { return myAttributeNameMap; }
    /// @}

    static bool		getAlembicBounds(UT_BoundingBox &box,
				const IObject &obj,
				fpreal sample_time,
				bool &isConstant);

protected:
    GA_DECLARE_INTRINSICS();

private:
    bool	getABCTransform(UT_Matrix4D &xform) const;
    void	resolveObject();
    void	updateAnimation();

    void	copyMemberDataFrom(const GABC_GEOPrim &src);

    std::string		myFilename;
    std::string		myObjectPath;
    IObject		myObject;
    fpreal		myFrame;
    GT_TransformHandle	myGeoTransform;
    GEO_ABCNameMapPtr	myAttributeNameMap;

    mutable UT_BoundingBox	myBox;
    mutable GT_TransformHandle	myGTTransform;
    mutable GT_PrimitiveHandle	myGTPrimitive;
    mutable GABC_AnimationType	myAnimation;
    bool			myUseTransform;
};

#endif
