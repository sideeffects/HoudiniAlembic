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
 * NAME:	GABC_IArray.h ( GT Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_IArray.h"
#include <UT/UT_StackBuffer.h>

namespace
{
    typedef Alembic::Abc::DataType	DataType;
}

GABC_IArray::~GABC_IArray()
{
    GABC_AutoLock	lock(archive());
    purge();
    setArchive(NULL);
}

void
GABC_IArray::purge()
{
    // Allow the references to be cleared by the archive
    myContainer = Container();
    mySize = 0;
    myTupleSize = 0;
    myType = GT_TYPE_NONE;
}

GABC_IArray
GABC_IArray::getSample(GABC_IArchive &arch, const IArrayProperty &prop,
	const ISampleSelector &iss, GT_Type override_type)
{
    ArraySamplePtr	sample;

    {
	// Lock to get the sample from the property
	GABC_AutoLock	lock(arch);
	prop.get(sample, iss);
	if (!sample->valid())
	    return GABC_IArray();
    }

    if (override_type != GT_TYPE_NONE)
    {
	return getSample(arch, sample, override_type);
    }
    return getSample(arch, sample,
	    prop.getMetaData().get("interpretation").c_str());
}

GABC_IArray
GABC_IArray::getSample(GABC_IArchive &arch, const ArraySamplePtr &sample,
	const char *interp)
{
    if (!sample->valid())
	return GABC_IArray();

    const DataType	&dtype = sample->getDataType();
    return getSample(arch, sample,
		GABC_GTUtil::getGTTypeInfo(interp, dtype.getExtent()));
}

GABC_IArray
GABC_IArray::getSample(GABC_IArchive &arch, const ArraySamplePtr &sample,
	GT_Type tinfo)
{
    if (!sample->valid())
	return GABC_IArray();

    UT_ASSERT(sample && sample->getData());

    const DataType	&dtype = sample->getDataType();
    GT_Size		 size = sample->size();
    GT_Size		 tsize = GABC_GTUtil::getGTTupleSize(dtype);
    GT_Storage		 store = GABC_GTUtil::getGTStorage(dtype);
    return GABC_IArray(arch, sample, size, tsize, store, tinfo);
}
