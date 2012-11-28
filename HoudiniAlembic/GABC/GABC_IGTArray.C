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
 * NAME:	GABC_IGTArray.C ( GT Library, C++)
 *
 * COMMENTS:
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
    , myArray(array)
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
