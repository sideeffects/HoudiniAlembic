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
 * NAME:	GABC_GTArray.C ( GT Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_GTArray.h"
#include <UT/UT_Assert.h>
#include <GT/GT_DAIndexedString.h>

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
}


// Instantiate the type and create an "extractor" function
#define NUMERIC_ARRAY(ABC_TYPE, GT_TYPE) \
    template GABC_API GABC_GTNumericArray<ABC_TYPE, GT_TYPE>::GABC_GTNumericArray(const ABC_TYPE &array); \
    GT_DataArrayHandle \
    GABC_GTArrayExtract::get(const ABC_TYPE &arr) \
    { \
	GABC_GTNumericArray<ABC_TYPE, GT_TYPE>	*a=NULL; \
	if (arr) \
	    a = new GABC_GTNumericArray<ABC_TYPE, GT_TYPE>(arr); \
	return GT_DataArrayHandle(a); \
    }

#define UNSUPPORTED_ARRAY(ABC_TYPE) \
    GT_DataArrayHandle \
    GABC_GTArrayExtract::get(const ABC_TYPE &) \
    { \
	return GT_DataArrayHandle(); \
    }

GT_DataArrayHandle
GABC_GTArrayExtract::get(const Alembic::Abc::StringArraySamplePtr &arr)
{
    if (!arr)
	return GT_DataArrayHandle();
    exint		 asize = arr->size();
    GT_DAIndexedString	*data = new GT_DAIndexedString(asize, 1);
    const std::string	*src = (const std::string *)arr->getData();
    for (exint i = 0; i < asize; ++i)
	data->setString(i, 0, src->c_str());
    return GT_DataArrayHandle(data);
}

NUMERIC_ARRAY(Alembic::Abc::BoolArraySamplePtr, uint8)
NUMERIC_ARRAY(Alembic::Abc::UcharArraySamplePtr, uint8)
NUMERIC_ARRAY(Alembic::Abc::CharArraySamplePtr, uint8)
UNSUPPORTED_ARRAY(Alembic::Abc::UInt16ArraySamplePtr)
UNSUPPORTED_ARRAY(Alembic::Abc::Int16ArraySamplePtr)
NUMERIC_ARRAY(Alembic::Abc::UInt32ArraySamplePtr, int32)
NUMERIC_ARRAY(Alembic::Abc::Int32ArraySamplePtr, int32)
NUMERIC_ARRAY(Alembic::Abc::UInt64ArraySamplePtr, int64)
NUMERIC_ARRAY(Alembic::Abc::Int64ArraySamplePtr, int64)
NUMERIC_ARRAY(Alembic::Abc::HalfArraySamplePtr, fpreal16)
NUMERIC_ARRAY(Alembic::Abc::FloatArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::DoubleArraySamplePtr, fpreal64)
//UNSUPPORTED_ARRAY(Alembic::Abc::StringArraySamplePtr)
UNSUPPORTED_ARRAY(Alembic::Abc::WstringArraySamplePtr)
UNSUPPORTED_ARRAY(Alembic::Abc::V2sArraySamplePtr)
NUMERIC_ARRAY(Alembic::Abc::V2iArraySamplePtr, int32)
NUMERIC_ARRAY(Alembic::Abc::V2fArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::V2dArraySamplePtr, fpreal64)
UNSUPPORTED_ARRAY(Alembic::Abc::V3sArraySamplePtr)
NUMERIC_ARRAY(Alembic::Abc::V3iArraySamplePtr, int32)
NUMERIC_ARRAY(Alembic::Abc::V3fArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::V3dArraySamplePtr, fpreal64)
UNSUPPORTED_ARRAY(Alembic::Abc::P2sArraySamplePtr)
NUMERIC_ARRAY(Alembic::Abc::P2iArraySamplePtr, int32)
NUMERIC_ARRAY(Alembic::Abc::P2fArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::P2dArraySamplePtr, fpreal64)
UNSUPPORTED_ARRAY(Alembic::Abc::P3sArraySamplePtr)
NUMERIC_ARRAY(Alembic::Abc::P3iArraySamplePtr, int32)
NUMERIC_ARRAY(Alembic::Abc::P3fArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::P3dArraySamplePtr, fpreal64)
UNSUPPORTED_ARRAY(GABC_GTArrayExtract::Box2sArraySamplePtr)
NUMERIC_ARRAY(GABC_GTArrayExtract::Box2iArraySamplePtr, int32)
NUMERIC_ARRAY(GABC_GTArrayExtract::Box2fArraySamplePtr, fpreal32)
NUMERIC_ARRAY(GABC_GTArrayExtract::Box2dArraySamplePtr, fpreal64)
UNSUPPORTED_ARRAY(Alembic::Abc::Box3sArraySamplePtr)
NUMERIC_ARRAY(Alembic::Abc::Box3iArraySamplePtr, int32)
NUMERIC_ARRAY(Alembic::Abc::Box3fArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::Box3dArraySamplePtr, fpreal64)
NUMERIC_ARRAY(Alembic::Abc::M33fArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::M33dArraySamplePtr, fpreal64)
NUMERIC_ARRAY(Alembic::Abc::M44fArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::M44dArraySamplePtr, fpreal64)
NUMERIC_ARRAY(Alembic::Abc::QuatfArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::QuatdArraySamplePtr, fpreal64)
NUMERIC_ARRAY(Alembic::Abc::C3hArraySamplePtr, fpreal16)
NUMERIC_ARRAY(Alembic::Abc::C3fArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::C3cArraySamplePtr, uint8)
NUMERIC_ARRAY(Alembic::Abc::C4hArraySamplePtr, fpreal16)
NUMERIC_ARRAY(Alembic::Abc::C4fArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::C4cArraySamplePtr, uint8)
NUMERIC_ARRAY(Alembic::Abc::N2fArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::N2dArraySamplePtr, fpreal64)
NUMERIC_ARRAY(Alembic::Abc::N3fArraySamplePtr, fpreal32)
NUMERIC_ARRAY(Alembic::Abc::N3dArraySamplePtr, fpreal64)
