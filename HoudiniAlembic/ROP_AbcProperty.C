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
 * NAME:	ROP_AbcProperty.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#include "ROP_AbcProperty.h"
#include <GT/GT_DANumeric.h>
#include <UT/UT_StringArray.h>
#include <UT/UT_StackBuffer.h>

ROP_AbcProperty::ROP_AbcProperty(Alembic::AbcGeom::GeometryScope scope)
    : myScope(scope)
{
}

ROP_AbcProperty::~ROP_AbcProperty()
{
}

namespace
{
    template <typename POD_T, GT_Storage T_STORE>
    static GT_DANumeric<POD_T, T_STORE> *
    extractGTArray(const GT_DataArrayHandle &src, int tuple_size)
    {
	GT_DANumeric<POD_T, T_STORE>	*num;
	num = new GT_DANumeric<POD_T, T_STORE>(src->entries(),
				tuple_size, src->getTypeInfo());
	src->fillArray(num->data(), 0, src->entries(),
		tuple_size, src->getTupleSize());
	return num;
    }

    template <typename POD_T, GT_Storage T_STORE>
    static bool
    writeProperty(
	    Alembic::Abc::OArrayProperty &prop,
	    const GT_DataArrayHandle &src,
	    int tuple_size)
    {
	GT_DataArrayHandle		 data;
	GT_DANumeric<POD_T, T_STORE>	*num = NULL;

	// If there's no numeric data associated with it, we have to extract the
	// into a numeric array
	num = extractGTArray<POD_T, T_STORE>(src, tuple_size);
	data.reset(num);

	UT_ASSERT(num);
	if (!num)
	    return false;

	Alembic::AbcCoreAbstract::ArraySample	sample(num->data(),
		prop.getDataType(), Alembic::Util::Dimensions(num->entries()));

	prop.set(sample);
	return true;
    }

    static bool
    writeStringProperty(Alembic::Abc::OArrayProperty &prop,
	    const GT_DataArrayHandle &src,
	    int tuple_size)
    {
	exint				nstrings = src->entries();
	UT_StackBuffer<std::string>	strings(nstrings); // Property strings

	// Get strings from GT
	src->fillStrings(strings, 0);

	Alembic::AbcCoreAbstract::ArraySample	sample(strings,
		prop.getDataType(), Alembic::Util::Dimensions(nstrings));
	prop.set(sample);
	return true;
    }
}

#define DECL_PARAM(TYPE)	{ \
    Alembic::AbcGeom::TYPE gp(parent, name, false, myScope, array_size, time); \
    myProperty = gp.getValueProperty(); \
    myProperty.setTimeSampling(ts); \
    valid = true; \
}

#define DECL_REALFD(FTYPE, DTYPE)	{ \
	if (myStorage == GT_STORE_REAL16) \
	    { myStorage = GT_STORE_REAL32; DECL_PARAM(FTYPE); } \
	else if (myStorage == GT_STORE_REAL32) { DECL_PARAM(FTYPE); } \
	else if (myStorage == GT_STORE_REAL64) { DECL_PARAM(DTYPE); } \
    }

#define DELC_REALHF(HTYPE, FTYPE) { \
	     if (myStorage == GT_STORE_REAL16) { DECL_PARAM(HTYPE); } \
	else if (myStorage == GT_STORE_REAL32) { DECL_PARAM(FTYPE); } \
	else if (myStorage == GT_STORE_REAL64) \
	    { myStorage = GT_STORE_REAL32; DECL_PARAM(FTYPE); } \
    }

bool
ROP_AbcProperty::init(Alembic::Abc::OCompoundProperty &parent,
	const char *name,
	const GT_DataArrayHandle &array,
	Alembic::AbcGeom::TimeSamplingPtr ts)
{
    if (!array)
	return false;

    exint			array_size = 1;
    Alembic::Util::uint32_t	time = 0;
    bool			valid = false;

    myStorage = array->getStorage();
    myTupleSize = array->getTupleSize();
    switch (array->getTypeInfo())
    {
	case GT_TYPE_POINT:
	case GT_TYPE_HPOINT:
	    if (myTupleSize >= 3)
	    {
		DECL_REALFD(OP3fGeomParam, OP3dGeomParam);
		myTupleSize = 3;	// Clamp to 3
	    }
	    else if (myTupleSize == 2)
	    {
		DECL_REALFD(OP2fGeomParam, OP2dGeomParam);
	    }
	    break;
	case GT_TYPE_VECTOR:
	    if (myTupleSize >= 3)
	    {
		DECL_REALFD(OV3fGeomParam, OV3dGeomParam);
		myTupleSize = 3;	// Clamp to 3
	    }
	    else if (myTupleSize == 2)
	    {
		DECL_REALFD(OV2fGeomParam, OV2dGeomParam);
	    }
	    break;
	case GT_TYPE_NORMAL:
	    if (myTupleSize >= 3)
	    {
		DECL_REALFD(ON3fGeomParam, ON3dGeomParam);
		myTupleSize = 3;	// Clamp to 3
	    }
	    else if (myTupleSize == 2)
	    {
		DECL_REALFD(ON2fGeomParam, ON2dGeomParam);
	    }
	    break;
	case GT_TYPE_COLOR:
	    if (myTupleSize >= 4)
	    {
		DELC_REALHF(OC4hGeomParam, OC4fGeomParam);
		myTupleSize = 4;	// Clamp to 4
	    }
	    else if (myTupleSize == 3)
	    {
		DELC_REALHF(OC3hGeomParam, OC3fGeomParam);
	    }
	    break;
	case GT_TYPE_QUATERNION:
	    if (myTupleSize >= 4)
	    {
		DECL_REALFD(OQuatfGeomParam, OQuatdGeomParam);
		myTupleSize = 4;	// Clamp to 4
	    }
	    break;
	case GT_TYPE_MATRIX3:
	    if (myTupleSize >= 9)
	    {
		DECL_REALFD(OM33fGeomParam, OM33dGeomParam);
		myTupleSize = 9;	// Clamp to 9
	    }
	    break;
	case GT_TYPE_MATRIX:
	    if (myTupleSize >= 16)
	    {
		DECL_REALFD(OM44fGeomParam, OM44dGeomParam);
		myTupleSize = 16;	// Clamp to 9
	    }
	    break;
	case GT_TYPE_ST:
	    break;

	default:
	    break;
    }
    if (!valid)
    {
	switch (array->getStorage())
	{
	    case GT_STORE_UINT8:
		myTupleSize = 1;	// Clamp to a scalar
		DECL_PARAM(OUcharGeomParam)
		break;
	    case GT_STORE_INT32:
		switch (myTupleSize)
		{
		    case 1:
			DECL_PARAM(OInt32GeomParam);
			break;
		    case 2:
			DECL_PARAM(OV2iGeomParam);
			break;
		    default:
			UT_ASSERT(myTupleSize >= 3);
			myTupleSize = 3;	// Clamp @ 3
			DECL_PARAM(OV3iGeomParam);
			break;
		}
		break;
	    case GT_STORE_INT64:
		myTupleSize = 1;	// Clamp to scalar
		DECL_PARAM(OInt64GeomParam);
		break;
	    case GT_STORE_REAL16:
		switch (myTupleSize)
		{
		    case 1:
		    case 2:
			myTupleSize = 1;	// Clamp to 1
			DECL_PARAM(OHalfGeomParam);
			break;
		    case 3:
			// Fake as a color
			DECL_PARAM(OC3hGeomParam);
			break;
		    default:
			// Fake as a color
			myTupleSize = 4;	// Clamp to 4
			DECL_PARAM(OC4hGeomParam);
			break;
		}
		break;
	    case GT_STORE_REAL32:
		switch (myTupleSize)
		{
		    case 1:
			DECL_PARAM(OFloatGeomParam);
			break;
		    case 2:
			DECL_PARAM(OV2fGeomParam);
			break;
		    case 3:
			DECL_PARAM(OV3fGeomParam);
			break;
		    case 4:
			DECL_PARAM(OQuatfGeomParam);
			break;
		    case 9:
			DECL_PARAM(OM33fGeomParam);
			break;
		    case 16:
			DECL_PARAM(OM44fGeomParam);
			break;
		}
		break;
	    case GT_STORE_REAL64:
		switch (myTupleSize)
		{
		    case 1:
			DECL_PARAM(ODoubleGeomParam);
			break;
		    case 2:
			DECL_PARAM(OV2dGeomParam);
			break;
		    case 3:
			DECL_PARAM(OV3dGeomParam);
			break;
		    case 4:
			DECL_PARAM(OQuatdGeomParam);
			break;
		    case 9:
			DECL_PARAM(OM33dGeomParam);
			break;
		    case 16:
			DECL_PARAM(OM44dGeomParam);
			break;
		}
		break;
	    case GT_STORE_STRING:
		DECL_PARAM(OStringGeomParam);
		break;
	    default:
		break;
	}
    }
    return valid;
}

bool
ROP_AbcProperty::writeSample(const GT_DataArrayHandle &array)
{
    switch (myStorage)
    {
	case GT_STORE_UINT8:
	    return writeProperty<uint8, GT_STORE_UINT8>(myProperty,
			array, myTupleSize);
	case GT_STORE_INT32:
	    return writeProperty<int32, GT_STORE_INT32>(myProperty,
			array, myTupleSize);
	case GT_STORE_INT64:
	    return writeProperty<int64, GT_STORE_INT64>(myProperty,
			array, myTupleSize);
	case GT_STORE_REAL16:
	    return writeProperty<fpreal16, GT_STORE_REAL16>(myProperty,
			array, myTupleSize);
	case GT_STORE_REAL32:
	    return writeProperty<fpreal32, GT_STORE_REAL32>(myProperty,
			array, myTupleSize);
	case GT_STORE_REAL64:
	    return writeProperty<fpreal64, GT_STORE_REAL64>(myProperty,
			array, myTupleSize);
	case GT_STORE_STRING:
	    return writeStringProperty(myProperty, array, myTupleSize);
	default:
	    break;
    }
    UT_ASSERT(0);
    return false;
}
