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

#include "GABC_IGTArray.h"
#include <GT/GT_DANumeric.h>
#include <UT/UT_Assert.h>

namespace
{
    // Compile time asserts need to be declared in a function
    class dummy
    {
	dummy()
	{
	    // If this is not true, we can't instantiate a BoolArraySamplePtr
	    UT_ASSERT_COMPILETIME(sizeof(bool) == sizeof(uint8));
	}
    };

    // Copy out of ABC array into a GT array
    template <typename ABC_TYPE, typename GT_TYPE, GT_Storage GT_STORAGE>
    GT_DataArray *
    translate(const GABC_IArray &iarray)
    {
	GT_DANumeric<GT_TYPE, GT_STORAGE>	*array;
	const ABC_TYPE	*src = (const ABC_TYPE *)(iarray.data());
	GT_TYPE		*dest;
	exint		 asize = iarray.entries();
	int		 tsize = iarray.tupleSize();
	exint		 npod = asize * tsize;

	array = new GT_DANumeric<GT_TYPE, GT_STORAGE>(asize, tsize,
				iarray.gtType());
	dest = array->data();
	for (exint i = 0; i < npod; ++i)
	    dest[i] = src[i];
	return array;
    }
}

GABC_IGTStringArray::GABC_IGTStringArray(const GABC_IArray &array)
    : GT_DAIndexedString(array.entries(), array.tupleSize())
{
    // The array is stored as an array of std::string objects
    const std::string	*data = reinterpret_cast<const std::string *>(array.data());
    for (exint i = 0; i < array.entries(); ++i)
    {
	for (int j = 0; j < array.tupleSize(); ++j, ++data)
	    setString(i, j, data->c_str());
    }
}

GT_DataArrayHandle
GABCarray(const GABC_IArray &iarray)
{
    if (!iarray.valid())
	return GT_DataArrayHandle();

    GT_DataArray	*data;
    switch (iarray.abcType())
    {
	// Compatible storage types between Alembic & GT
	case Alembic::Abc::kUint8POD:
	    data = new GABC_IGTArray<uint8, GT_STORE_UINT8>(iarray);
	    break;
	case Alembic::Abc::kInt32POD:
	    data = new GABC_IGTArray<int32, GT_STORE_INT32>(iarray);
	    break;
	case Alembic::Abc::kInt64POD:
	case Alembic::Abc::kUint64POD:	// Store uint64 in int64 too
	    data = new GABC_IGTArray<int64, GT_STORE_INT64>(iarray);
	    break;
	case Alembic::Abc::kFloat16POD:
	    data = new GABC_IGTArray<fpreal16, GT_STORE_REAL16>(iarray);
	    break;
	case Alembic::Abc::kFloat32POD:
	    data = new GABC_IGTArray<fpreal32, GT_STORE_REAL32>(iarray);
	    break;
	case Alembic::Abc::kFloat64POD:
	    data = new GABC_IGTArray<fpreal64, GT_STORE_REAL64>(iarray);
	    break;
	case Alembic::Abc::kStringPOD:
	    data = new GABC_IGTStringArray(iarray);
	    break;
	case Alembic::Abc::kBooleanPOD:
	    data = translate<bool, uint8, GT_STORE_UINT8>(iarray);
	    break;
	case Alembic::Abc::kInt8POD:
	    data = translate<int8, int32, GT_STORE_INT32>(iarray);
	    break;
	case Alembic::Abc::kUint16POD:
	    data = translate<uint16, int32, GT_STORE_INT32>(iarray);
	    break;
	case Alembic::Abc::kInt16POD:
	    data = translate<int16, int32, GT_STORE_INT32>(iarray);
	    break;
	case Alembic::Abc::kUint32POD:
	    data = translate<uint32, int64, GT_STORE_INT64>(iarray);
	    break;

	case Alembic::Abc::kWstringPOD:
	case Alembic::Abc::kNumPlainOldDataTypes:
	case Alembic::Abc::kUnknownPOD:
	    break;
    }
    return GT_DataArrayHandle(data);
}
