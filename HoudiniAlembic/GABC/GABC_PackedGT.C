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

#include "GABC_PackedGT.h"
#include "GABC_PackedImpl.h"
#include <GT/GT_GEOPrimPacked.h>

using namespace GABC_NAMESPACE;

namespace
{

class GABC_API GABC_PackedGT : public GT_GEOPrimPacked
{
public:
    GABC_PackedGT(const GU_PrimPacked *prim)
	: GT_GEOPrimPacked(prim)
    {
    }
    GABC_PackedGT(const GABC_PackedGT &src)
	: GT_GEOPrimPacked(src)
    {
    }
    virtual ~GABC_PackedGT()
    {
    }

    virtual const char	*className() const	{ return "GABC_PackedGT"; }
    virtual GT_PrimitiveHandle	doSoftCopy() const
				    { return new GABC_PackedGT(*this); }

    virtual GT_PrimitiveHandle	getPointCloud(const GT_RefineParms *p) const;
    virtual GT_PrimitiveHandle	getFullGeometry(const GT_RefineParms *p) const;
private:
};

GT_PrimitiveHandle
GABC_PackedGT::getPointCloud(const GT_RefineParms *) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return impl->pointGT();
}

GT_PrimitiveHandle
GABC_PackedGT::getFullGeometry(const GT_RefineParms *) const
{
    const GABC_PackedImpl	*impl;
    impl = UTverify_cast<const GABC_PackedImpl *>(getImplementation());
    return impl->fullGT();
}

}

GABC_CollectPacked::~GABC_CollectPacked()
{
}

GT_PrimitiveHandle
GABC_CollectPacked::collect(const GT_GEODetailListHandle &geo,
	const GEO_Primitive *const* prim,
	int nsegments, GT_GEOPrimCollectData *data) const
{
    const GU_PrimPacked	*pack = UTverify_cast<const GU_PrimPacked *>(prim[0]);
    return GT_PrimitiveHandle(new GABC_PackedGT(pack));
}
