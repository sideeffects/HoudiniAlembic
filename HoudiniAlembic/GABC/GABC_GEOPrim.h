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

#ifndef __GABC_GEOPrim__
#define __GABC_GEOPrim__

#include "GABC_API.h"
#include "GABC_Util.h"
#include "GABC_NameMap.h"
#include "GABC_IArchive.h"
#include "GABC_GTPrim.h"
#include <Alembic/AbcGeom/Foundation.h>		// For topology enum

#include <GT/GT_Handles.h>
#include <GT/GT_Primitive.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_Vertex.h>
#include <GA/GA_Defines.h>
#include <UT/UT_Array.h>

class GEO_Detail;

namespace GABC_NAMESPACE
{
class GABC_API GABC_GEOPrim : public GEO_Primitive
{
public:
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
    virtual bool	saveSharedLoadData(UT_JSONWriter &w,
				GA_SaveMap &save) const;
    virtual bool	loadSharedLoadData(int load_data_type,
				const GA_SharedLoadData *item);

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
    /// This method is implemented to assist with rendering.  When rendering
    /// velocity based motion blur, the renderer needs to know the bounds on
    /// the velocity to accurately compute a bounding box.
    void	getVelocityRange(UT_Vector3 &vmin, UT_Vector3 &vmax) const;

    /// @{
    /// Though not strictly required (i.e. not pure virtual), these methods
    /// should be implemented for proper behaviour.
    virtual GEO_Primitive	*copy(int preserve_shared_pts = 0) const;

    // Have we been deactivated and stashed?
    virtual void	stashed(int onoff, GA_Offset offset=GA_INVALID_OFFSET);

    // We need to invalidate the vertex offsets
    virtual void	clearForDeletion();

    virtual void	copyOffsetPrimitive(const GEO_Primitive *src, GA_Index base);
    /// @}

    /// @{
    /// Optional interface methods.  Though not required, implementing these
    /// will give better behaviour for the primitive.
    virtual void	transform(const UT_Matrix4 &xform);
    virtual UT_Vector3	baryCenter() const;
    virtual fpreal	calcVolume(const UT_Vector3 &refpt) const;
    virtual fpreal	calcArea() const;
    virtual fpreal	calcPerimeter() const;
    virtual void	getLocalTransform(UT_Matrix3D &matrix) const;
    virtual void	setLocalTransform(const UT_Matrix3D &matrix);
    /// @}

    /// Ensure the Alembic primitive has a vertex
    void	ensureVertexCreated();
    /// Set vertex to reference the given point.  This will ensure the vertex
    /// is allocated.
    void	setVertexPoint(GA_Offset point);
    /// Called when loading to set the vertex
    void	assignVertex(GA_Offset vtx, bool update_topology);

    /// @{
    /// Alembic interface
    const std::string	&filename() const	{ return myFilename; }
    const std::string	&objectPath() const	{ return myObjectPath; }
    const GABC_IObject	&object() const	{ return myObject; }
    GABC_NodeType	 abcNodeType() const
			    { return myObject.nodeType(); }
    GABC_AnimationType	 animation() const	{ return myGTPrimitive->animation(); }
    bool		 visible() const	{ return myGTPrimitive->visible(); }
    /// @{
    /// Whether the GT primitive should check visibility
    bool		 useVisibility() const
				{ return myUseVisibility; }
    void		 setUseVisibility(bool v);
    /// @}

    bool		 isConstant() const
			 {
			     return animation() == GABC_ANIMATION_CONSTANT;
			 }
    fpreal		 frame() const	{ return myFrame; }

    void		 init(const std::string &filename,
				const std::string &objectpath,
				fpreal frame,
				bool use_transform,
				bool use_visibility);
    void		 init(const std::string &filename,
				const GABC_IObject &objectpath,
				fpreal frame,
				bool use_transform,
				bool use_visibility);
    void		 setFilename(const std::string &filename);
    void		 setObjectPath(const std::string &path);
    void		 setFrame(fpreal f);
    /// Returns the Alembic world-space transform combined with the Houdini
    /// geometry transform
    bool		 getTransform(UT_Matrix4D &xform) const;
    /// Returns the Alembic world-space transform
    bool		 getABCWorldTransform(UT_Matrix4D &xform) const;
    /// Returns the Alembic local-space transform
    bool		 getABCLocalTransform(UT_Matrix4D &xform) const;
    /// Returns the Houdini geometry transform
    bool		 useTransform() const
			    { return myUseTransform; }
    void		 setUseTransform(bool v);
    /// Accessors for viewport LOD
    GABC_ViewportLOD	 viewportLOD() const	{ return myViewportLOD; }
    void		 setViewportLOD(GABC_ViewportLOD vlod);
    /// @}

    /// Return the GT representation of the primitive
    GT_PrimitiveHandle	 gtPrimitive() const;
    /// Return the GT bounding box for the primitive
    GT_PrimitiveHandle	 gtBox() const;
    /// Return the point cloud for the primitive.
    GT_PrimitiveHandle	 gtPointCloud() const;
    void		 clearGT();

    /// Return a GT primitive with extra attributes given by the @c attrib_list
    /// and as defined on the @c attrib_prim.
    GT_PrimitiveHandle	gtPrimitive(const GT_PrimitiveHandle &attrib_prim,
				const UT_StringMMPattern *vertex_pattern,
				const UT_StringMMPattern *point_pattern,
				const UT_StringMMPattern *uniform_pattern,
				const UT_StringMMPattern *detail_pattern,
				const GT_RefineParms *parms=NULL) const;

    /// @{
    /// Geo transform
    void			setGeoTransform(const GT_TransformHandle &x);
    const GT_TransformHandle	&geoTransform() const
				    { return myGeoTransform; }
    /// @}

    /// @{
    /// Attribute name mappings
    void			setAttributeNameMap(const GABC_NameMapPtr &m);
    const GABC_NameMapPtr	&attributeNameMap() const
				    { return myAttributeNameMap; }
    /// @}

    static bool		getAlembicBounds(UT_BoundingBox &box,
				const GABC_IObject &obj,
				fpreal sample_time,
				bool &isConstant)
			{
			    return obj.getBoundingBox(box, sample_time,
						isConstant);
			}

protected:
    GA_DECLARE_INTRINSICS();

private:
    void	resolveObject();
    void	updateAnimation();
    bool	needTransform() const;

    void	copyMemberDataFrom(const GABC_GEOPrim &src);

    std::string		 myFilename;
    std::string		 myObjectPath;
    GABC_IObject	 myObject;
    fpreal		 myFrame;
    GT_TransformHandle	 myGeoTransform;
    GT_TransformHandle	 myGTTransform;
    GABC_NameMapPtr	 myAttributeNameMap;
    GABC_GTPrimitive	*myGTPrimitive;
    GA_Offset		 myVertex;
    GABC_ViewportLOD	 myViewportLOD;
    bool		 myUseTransform;
    bool		 myUseVisibility;

};
}

#endif
