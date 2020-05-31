/*
 * Copyright (c) 2020
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

#ifndef __GABC_PackedGT__
#define __GABC_PackedGT__

#include "GABC_API.h"
#include "GABC_Types.h"
#include "GABC_PackedImpl.h"
#include <GT/GT_GEOPrimCollectPacked.h>
#include <GT/GT_PackedAlembic.h>
#include <GT/GT_PrimInstance.h>

class GU_PrimPacked;

namespace GABC_NAMESPACE
{

class GABC_API GABC_AlembicCache
{
public:
		GABC_AlembicCache()
		{
		    myVisibilityAnimated = false;
		    myTransformAnimated = false;
		}

    void	setVisibilityAnimated(bool anim) { myVisibilityAnimated = anim;}
    void	setTransformAnimated(bool anim)  { myTransformAnimated = anim; }

    void	getVisibility(const GABC_PackedImpl *impl, bool &visible);
    void	getTransform(const GABC_PackedImpl *impl,
		    UT_Matrix4D &transform);

private:
    UT_Map<fpreal, bool>	myVisibility;
    UT_Map<fpreal, UT_Matrix4D>	myTransform;

    bool			myVisibilityAnimated;
    bool			myTransformAnimated;
};

/// Collector for packed primitives.
class GABC_CollectPacked : public GT_GEOPrimCollect
{
public:
	     GABC_CollectPacked() {}
            ~GABC_CollectPacked() override;

    /// @{
    /// Interface defined for GT_GEOPrimCollect
    GT_GEOPrimCollectData *
		beginCollecting(const GT_GEODetailListHandle &geometry,
				const GT_RefineParms *parms) const override;
    GT_PrimitiveHandle
			collect(const GT_GEODetailListHandle &geo,
				const GEO_Primitive *const* prim_list,
				int nsegments,
				GT_GEOPrimCollectData *data) const override;
    GT_PrimitiveHandle
		  endCollecting(const GT_GEODetailListHandle &geometry,
				GT_GEOPrimCollectData *data) const override;
    /// @}
private:
};

/// Collection class for a single archive's worth of Alembic primitives.
/// This is generally only useful for the viewport.
class GABC_API GABC_PackedArchive : public GT_PackedAlembicArchive
{
public:
    GABC_PackedArchive(const UT_StringHolder &archive_name,
		       const GT_GEODetailListHandle &source_list,
		       const GABC_NAMESPACE::GABC_IArchivePtr &archive,
		       int index);

    const char          *className() const override { return "GABC_PackedArchive"; }

    bool                bucketPrims(const GT_PackedAlembicArchive *prev_archive,
			    const GT_RefineParms *ref_parms,
			    bool force_update) override;

    GT_PrimitiveHandle  doSoftCopy() const override { return nullptr; }
    
    int			getIndex() const { return myIndex; }
    
private:
    bool archiveMatch(const GT_PackedAlembicArchive *archive) const override;

    GABC_NAMESPACE::GABC_IArchivePtr myArchive;
    int myIndex;
};

/// Single Alembic shape (non-instanced)
class GABC_API GABC_PackedAlembic : public GT_PackedAlembic
{
public:
	     GABC_PackedAlembic(const GU_ConstDetailHandle &prim_gdh,
				const GU_PrimPacked *prim,
				const GT_DataArrayHandle &vp_mat,
				const GT_DataArrayHandle &vp_remap,
				bool build_packed_attribs = false);
    
	     GABC_PackedAlembic(const GABC_PackedAlembic &src);
            ~GABC_PackedAlembic() override;

    void                 initVisAnim() override;
    
    const char          *className() const override
                            { return "GABC_PackedAlembic"; }

    GT_PrimitiveHandle	doSoftCopy() const override
			    { return new GABC_PackedAlembic(*this); }

    GT_PrimitiveHandle  getPointCloud(const GT_RefineParms *p,
				bool &xform) const override;
    GT_PrimitiveHandle  getFullGeometry(const GT_RefineParms *p,
				bool &xform) const override;
    GT_PrimitiveHandle  getBoxGeometry(const GT_RefineParms *p) const override;
    GT_PrimitiveHandle  getCentroidGeometry(
                                const GT_RefineParms *p) const override;

    bool                canInstance() const override    { return true; }
    bool                getInstanceKey(UT_Options &options) const override;
    GT_PrimitiveHandle  getInstanceGeometry(const GT_RefineParms *p,
				bool ignore_visibility=false) const override;
    GT_TransformHandle  getInstanceTransform() const override;

    bool                isVisible() override;

    bool                refine(GT_Refine &refiner,
			       const GT_RefineParms *parms=NULL) const override;

    bool                getCachedGeometry(
                                GT_PrimitiveHandle &ph) const override;
    
    void                getCachedTransform(
                                GT_TransformHandle &ph) const override;
    void                getCachedVisibility(bool &visible) const override;

    bool                updateGeoPrim(const GU_ConstDetailHandle &dtl,
			        const GT_RefineParms &refine) override;
private:
    int64			myColorID;
    UT_Matrix4D			myTransform;
    mutable GABC_AlembicCache	myCache;
};

/// Packed instance with alembic extensions
class GABC_API GABC_PackedInstance : public GT_AlembicInstance
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
    ~GABC_PackedInstance() override;
    
    bool                updateGeoPrim(const GU_ConstDetailHandle &dtl,
				    const GT_RefineParms &refine) override;

private:
    mutable UT_Array<GABC_AlembicCache>	myCache;
};
    
}; // GABC_NAMESPACE

#endif
