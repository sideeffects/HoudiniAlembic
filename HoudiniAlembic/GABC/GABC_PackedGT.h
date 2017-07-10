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
 * NAME:	GABC_PackedGT.h (GABC Library, C++)
 *
 * COMMENTS:
 */

#ifndef __GABC_PackedGT__
#define __GABC_PackedGT__

#include "GABC_API.h"
#include "GABC_Types.h"
#include <GT/GT_GEOPrimCollectPacked.h>
#include <GT/GT_PrimInstance.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GU/GU_DetailHandle.h>

class GU_PrimPacked;

namespace GABC_NAMESPACE
{

/// Collector for packed primitives.
class GABC_CollectPacked : public GT_GEOPrimCollect
{
public:
	     GABC_CollectPacked() {}
    virtual ~GABC_CollectPacked();

    /// @{
    /// Interface defined for GT_GEOPrimCollect
    virtual GT_GEOPrimCollectData *
		beginCollecting(const GT_GEODetailListHandle &geometry,
				const GT_RefineParms *parms) const;
    virtual GT_PrimitiveHandle
			collect(const GT_GEODetailListHandle &geo,
				const GEO_Primitive *const* prim_list,
				int nsegments,
				GT_GEOPrimCollectData *data) const;
    virtual GT_PrimitiveHandle
		  endCollecting(const GT_GEODetailListHandle &geometry,
				GT_GEOPrimCollectData *data) const;
    /// @}
private:
};

}; // GABC_NAMESPACE

class GABC_API GABC_AlembicCache
{
public:
    GABC_AlembicCache()
    {
	myVisibilityAnimated = false;
	myTransformAnimated = false;
    }
    
    bool	getVisibility(fpreal t, bool &visible) const;
    bool	getTransform(fpreal t, UT_Matrix4F &transform) const;

    
    void	setVisibilityAnimated(bool anim) { myVisibilityAnimated = anim;}
    void	setTransformAnimated(bool anim)  { myTransformAnimated = anim; }

    void	cacheVisibility(fpreal t, bool visible)
		    { myVisibility[ myVisibilityAnimated ? t : 0.0] = visible; }
    void	cacheTransform(fpreal t, const UT_Matrix4F &transform)
		    { myTransform[ myTransformAnimated ? t : 0.0] = transform; }
    
private:
    UT_Map<fpreal, bool>	myVisibility;
    UT_Map<fpreal, UT_Matrix4F>	myTransform;
    
    unsigned int	myVisibilityAnimated : 1,
			myTransformAnimated : 1;

};

/// Collection class for a single archive's worth of Alembic primitives.
/// This is generally only useful for the viewport.
class GABC_API GABC_PackedArchive : public GT_Primitive
{
public:
    GABC_PackedArchive(const UT_StringHolder &archive_name,
		       const GT_GEODetailListHandle &source_list,
		       const GABC_NAMESPACE::GABC_IArchivePtr &archive);

    const UT_StringHolder &archiveName() const { return myName; }
    
    virtual int		 getPrimitiveType() const
				    { return GT_PRIM_ALEMBIC_ARCHIVE; }
    virtual const char	*className() const { return "GABC_PackedArchive"; }

    void	appendAlembic(GA_Offset alembic_prim_offset)
			    { myAlembicOffsets.append(alembic_prim_offset); }

    bool	bucketPrims(const GABC_PackedArchive *prev_archive,
			    const GT_RefineParms *ref_parms,
			    bool force_update);

    int		getNumChildPrims() const
		    {  return (myConstShapes.entries() +
			       myTransformShapes.entries() +
			       myDeformShapes.entries() +
			       myCombinedShapes.entries()); }

    const UT_Array<GT_PrimitiveHandle> &constantShapes() const
		    { return myConstShapes; }
    const UT_Array<GT_PrimitiveHandle> &transformShapes() const
		    { return myTransformShapes; }
    const UT_Array<GT_PrimitiveHandle> &deformShapes() const
		    { return myDeformShapes; }
    const UT_Array<GT_PrimitiveHandle> &combinedShapes() const
		    { return myCombinedShapes; }
    
    virtual void	enlargeBounds(UT_BoundingBox boxes[],
				      int nsegments) const;
    virtual int		getMotionSegments() const  { return 1; }
    virtual int64	getMemoryUsage() const;
    virtual GT_PrimitiveHandle	doSoftCopy() const;

    void		setRefinedSubset(bool reduced_consts,
					 UT_IntArray &const_prims,
					 bool reduced_transforms,
					 UT_IntArray &trans_prims);
    bool		hasConstantSubset() const { return myHasConstSubset; }
    const UT_IntArray  &getConstantSubset() const { return myConstSubset; }
    bool		hasTransformSubset() const { return myHasTransSubset; }
    const UT_IntArray  &getTransformSubset() const { return myTransSubset; }

    int64		getAlembicVersion() const { return myAlembicVersion; }
    
private:
    bool		archiveMatch(const GABC_PackedArchive *archive) const;
    
    UT_StringHolder		 myName;
    GT_GEODetailListHandle	 myDetailList;
    GA_OffsetArray		 myAlembicOffsets;
    GABC_NAMESPACE::GABC_IArchivePtr myArchive;
    
    UT_Array<GT_PrimitiveHandle> myConstShapes;
    UT_Array<GT_PrimitiveHandle> myTransformShapes;
    UT_Array<GT_PrimitiveHandle> myDeformShapes;
    UT_Array<GT_PrimitiveHandle> myCombinedShapes;
    UT_IntArray			 myConstSubset;
    UT_IntArray			 myTransSubset;
    bool			 myHasConstSubset;
    bool			 myHasTransSubset;
    int64			 myAlembicVersion;
};


/// Single Alembic shape (non-instanced)
class GABC_API GABC_PackedAlembic : public GT_GEOPrimPacked
{
public:
	     GABC_PackedAlembic(const GU_ConstDetailHandle &prim_gdh,
				const GU_PrimPacked *prim,
				bool tmp_new_scheme = false);
    
	     GABC_PackedAlembic(const GABC_PackedAlembic &src);
    virtual ~GABC_PackedAlembic();

    virtual int		 getPrimitiveType() const
				    { return GT_PRIM_ALEMBIC_SHAPE; }
    virtual const char	*className() const	{ return "GABC_PackedAlembic"; }
    virtual GT_PrimitiveHandle	doSoftCopy() const
				    { return new GABC_PackedAlembic(*this); }

    virtual GT_PrimitiveHandle	getPointCloud(const GT_RefineParms *p,
					bool &xform) const;
    virtual GT_PrimitiveHandle	getFullGeometry(const GT_RefineParms *p,
					bool &xform) const;
    virtual GT_PrimitiveHandle	getBoxGeometry(const GT_RefineParms *p) const;
    virtual GT_PrimitiveHandle	getCentroidGeometry(const GT_RefineParms *p) const;

    virtual bool		canInstance() const	{ return true; }
    virtual bool		getInstanceKey(UT_Options &options) const;
    virtual GT_PrimitiveHandle	getInstanceGeometry(const GT_RefineParms *p,
					bool ignore_visibility=false) const;
    virtual GT_TransformHandle	getInstanceTransform() const;

    GT_TransformHandle		fullCachedTransform();

    virtual bool		refine(GT_Refine &refiner,
				       const GT_RefineParms *parms=NULL) const;
    virtual bool		getUniqueID(int64 &id) const
				{ id = myID; return true; }

    void			setAnimationType(GEO_AnimationType t)
				{ myAnimType = t; }
    GEO_AnimationType		animationType() const { return myAnimType; }

    void			setVisibilityAnimated(bool anim)
				    { myAnimVis = anim; }
    bool			visibilityAnimated() const { return myAnimVis; }

    bool	 		getCachedGeometry(GT_PrimitiveHandle &ph) const;
    
    void			cacheTransform(const GT_TransformHandle &ph);
    bool	 		getCachedTransform(GT_TransformHandle &ph) const;
    void			cacheVisibility(bool visible);
    bool	 		getCachedVisibility(bool &visible) const;

    GT_TransformHandle		applyPrimTransform(const GT_TransformHandle &th)
					const;
    GT_TransformHandle		getLocalTransform() const;

    int64	      alembicVersion() const { return myAlembicVersion; }
    void	      setAlembicVersion(int64 v) { myAlembicVersion = v; }
    
    /// Temporary until switchover
    bool			tmpIsNewScheme() const {return myTmpNewScheme;}
private:
    int64	      myID;
    GEO_AnimationType myAnimType;
    GABC_AlembicCache myCache;
    bool	      myAnimVis;
    bool	      myVisibleConst; // only valid when myAnimVis is false.
    bool	      myTmpNewScheme;
    int64	      myAlembicVersion;
};

/// Alembic mesh which contains multiple alembic primitives merged together.
class GABC_API GABC_PackedAlembicMesh : public GT_Primitive
{
public:
    GABC_PackedAlembicMesh(const GT_PrimitiveHandle &packed_geo, int64 id);
    GABC_PackedAlembicMesh(const GT_PrimitiveHandle &packed_geo, int64 id,
			   UT_Array<GT_PrimitiveHandle> &individual_meshes);
    GABC_PackedAlembicMesh(const GABC_PackedAlembicMesh &mesh);
    
    virtual int		 getPrimitiveType() const
				    { return GT_PRIM_ALEMBIC_SHAPE_MESH; }
    virtual const char	*className() const
				    { return "GABC_PackedAlembicMesh"; }
    virtual GT_PrimitiveHandle	doSoftCopy() const
				  { return new GABC_PackedAlembicMesh(*this); }
    virtual bool	refine(GT_Refine &refiner,
			       const GT_RefineParms *parms=NULL) const;
    virtual void	enlargeBounds(UT_BoundingBox boxes[],
				      int nsegments) const;
    virtual int		getMotionSegments() const  { return 1; }
    virtual int64	getMemoryUsage() const;
    virtual bool	getUniqueID(int64 &id) const
			{ id = myID; return true; }

    void		update(bool initial_update);
    bool		hasAnimatedTransforms() const
			    { return myTransformArray.get() != NULL; }
    bool		hasAnimatedVisibility() const
			    { return myTransformArray.get() != NULL; }
    
    int64	      alembicVersion() const { return myAlembicVersion; }
    void	      setAlembicVersion(int64 v) { myAlembicVersion = v; }
    
private:
    GT_PrimitiveHandle myMeshGeo;
    GT_DataArrayHandle myTransformArray;
    GT_DataArrayHandle myVisibilityArray;
    UT_Array<GT_PrimitiveHandle> myPrims;
    int64	       myID;
    int64	       myTransID;
    int64	       myVisID;
    int64	       myAlembicVersion;
};

/// Packed instance with alembic extensions
class GABC_API GABC_PackedInstance : public GT_PrimInstance
{
public:
    GABC_PackedInstance();
    GABC_PackedInstance(const GABC_PackedInstance &src);
    GABC_PackedInstance(const GT_PrimitiveHandle &geometry,
	    const GT_TransformArrayHandle &transforms,
 	    GEO_AnimationType animation,
	    const GT_GEOOffsetList &packed_prim_offsets=GT_GEOOffsetList(),
	    const GT_AttributeListHandle &uniform=GT_AttributeListHandle(),
	    const GT_AttributeListHandle &detail=GT_AttributeListHandle(),
	    const GT_GEODetailListHandle &source=GT_GEODetailListHandle());
    virtual ~GABC_PackedInstance();

    virtual const char	*className() const { return "GABC_PackedInstance"; }
    
    GEO_AnimationType animationType() const { return myAnimType; }
    
    int64	      alembicVersion() const { return myAlembicVersion; }
    void	      setAlembicVersion(int64 v) { myAlembicVersion = v; }
    
private:
    GEO_AnimationType myAnimType;
    UT_Array<GABC_AlembicCache> myCache;
    int64	      myAlembicVersion;
};
    

#endif
