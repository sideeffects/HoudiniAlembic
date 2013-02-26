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
 * NAME:	GABC_Visibility.h ( GABC Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_Visibility.h"
#include "GABC_Include.h"
#include "GABC_IObject.h"
#include "GABC_IArchive.h"
#include <GT/GT_DABool.h>
#include <GT/GT_DAConstantValue.h>
#include <Alembic/AbcGeom/Visibility.h>

namespace
{
    typedef Alembic::Abc::index_t			index_t;
    typedef Alembic::Abc::IObject			IObject;
    typedef Alembic::Abc::ISampleSelector		ISampleSelector;
    typedef Alembic::AbcGeom::IVisibilityProperty	IVisibilityProperty;

#if 0
    GT_DataArrayHandle	theTrue(new GT_IntConstant(1, 1));
    GT_DataArrayHandle	theFalse(new GT_IntConstant(1, 0));
#endif
}

void
GABC_VisibilityCache::clear()
{
    myVisible = GABC_VISIBLE_DEFER;
    delete myCache;
    myCache = NULL;
}

void
GABC_VisibilityCache::set(GABC_VisibilityType vtype,
	const GABC_ChannelCache *vcache)
{
    clear();
    myVisible = vtype;
    myCache = vcache ? new GABC_ChannelCache(*vcache) : NULL;
}

bool
GABC_VisibilityCache::set(const GABC_IObject &obj, bool check_parent)
{
    GABC_AutoLock	lock(obj.archive());
    IVisibilityProperty	vprop = Alembic::AbcGeom::GetVisibilityProperty(const_cast<IObject &>(obj.object()));
    if (!vprop.valid())
    {
	if (!check_parent)
	{
	    set(GABC_VISIBLE_DEFER);
	    return false;
	}
	GABC_IObject	parent(obj.getParent());
	if (parent.valid())
	    set(parent, true);
	else
	    set(GABC_VISIBLE_VISIBLE);
	return true;
    }
    GT_DataArrayHandle	data;

    if (vprop.isConstant())
    {
	if (vprop.getValue(ISampleSelector((index_t)0)))
	    set(GABC_VISIBLE_VISIBLE);
	else
	    set(GABC_VISIBLE_HIDDEN);
	return true;
    }
    exint	 nsamples = vprop.getNumSamples();
    exint	 ntrue = 0;
    GT_DABool	*bits = new GT_DABool(nsamples);
    bits->setAllBits(false);
    for (exint i = 0; i < nsamples; ++i)
    {
	if (vprop.getValue(ISampleSelector((index_t)i)))
	{
	    bits->setBit(i, true);
	    ntrue++;
	}
    }
    if (ntrue == nsamples || ntrue == 0)
    {
	set(ntrue ? GABC_VISIBLE_VISIBLE : GABC_VISIBLE_HIDDEN);
	return true;
    }
    clear();
    myCache = new GABC_ChannelCache(GT_DataArrayHandle(bits),
			vprop.getTimeSampling());
    myVisible = bits->getI32(0) == 0 ? GABC_VISIBLE_HIDDEN : GABC_VISIBLE_VISIBLE;
    return true;
}
