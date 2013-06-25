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
#include <GT/GT_GEOPrimCollectPacked.h>

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

}


#endif
