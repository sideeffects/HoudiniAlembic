/*
 * Copyright (c) 2015
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

#include "GABC_IArray.h"
#include <UT/UT_StackBuffer.h>

using namespace GABC_NAMESPACE;

namespace
{
    typedef Alembic::Abc::DataType		DataType;
    typedef Alembic::Abc::IArrayProperty	IArrayProperty;

    static int
    arrayExtent(const IArrayProperty &prop)
    {
	std::string	 extent_s = prop.getMetaData().get("arrayExtent");
	return (extent_s == "") ? 1 : atoi(extent_s.c_str());
    }
}

GABC_IArray::~GABC_IArray()
{
    GABC_AlembicLock	lock(archive());
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
GABC_IArray::getSample(GABC_IArchive &arch,
        const IArrayProperty &prop,
	const ISampleSelector &iss,
	GT_Type override_type)
{
    ArraySamplePtr	sample;

    {
	// Lock to get the sample from the property
	GABC_AlembicLock	lock(arch);
	prop.get(sample, iss);
	if (!sample->valid())
	    return GABC_IArray();
    }

    int	array_extent = arrayExtent(prop);
    if (override_type != GT_TYPE_NONE)
    {
	return getSample(arch, sample, override_type, array_extent,
		prop.isConstant());
    }
    return getSample(arch,
            sample,
	    prop.getMetaData().get("interpretation").c_str(),
	    array_extent,
	    prop.isConstant());
}

GABC_IArray
GABC_IArray::getSample(GABC_IArchive &arch,
        const ArraySamplePtr &sample,
	const char *interp,
	int array_extent,
	bool is_constant)
{
    if (!sample->valid())
	return GABC_IArray();

    const DataType	&dtype = sample->getDataType();
    return getSample(arch,
                sample,
		GABC_GTUtil::getGTTypeInfo(interp, dtype.getExtent()),
		array_extent,
		is_constant);
}

GABC_IArray
GABC_IArray::getSample(GABC_IArchive &arch,
        const ArraySamplePtr &sample,
	GT_Type tinfo,
	int array_extent,
	bool is_constant)
{
    if (!sample->valid())
	return GABC_IArray();

    UT_ASSERT(sample && sample->getData());
    UT_ASSERT(array_extent == 1 || sample->size() % array_extent == 0);

    const DataType	&dtype = sample->getDataType();
    GT_Size		 size = sample->size()/array_extent;
    GT_Size		 tsize = dtype.getExtent() * array_extent;
    GT_Storage		 store = GABC_GTUtil::getGTStorage(dtype);
    return GABC_IArray(arch, sample, size, tsize, store, tinfo, is_constant);
}
