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

#include "GABC_OProperty.h"
#include <GT/GT_DANumeric.h>
#include <UT/UT_StringArray.h>
#include <UT/UT_StackBuffer.h>
#include "GABC_OOptions.h"

namespace
{
    typedef Alembic::Abc::OCompoundProperty		OCompoundProperty;
    typedef Alembic::Abc::OArrayProperty		OArrayProperty;
    typedef Alembic::Abc::TimeSamplingPtr		TimeSamplingPtr;
    typedef Alembic::AbcCoreAbstract::ArraySample	ArraySample;
    typedef Alembic::Util::Dimensions			Dimensions;

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
	    OArrayProperty &prop,
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

	// Get extent of the Alembic type
	int	type_extent = prop.getDataType().getExtent();
	// Compute the number of Alembic elements for the array
	int	array_size = tuple_size / type_extent;
	UT_ASSERT(tuple_size%type_extent == 0);

	ArraySample	sample(num->data(), prop.getDataType(),
				Dimensions(num->entries()*array_size));

	prop.set(sample);
	return true;
    }

    static bool
    writeStringProperty(OArrayProperty &prop,
	    const GT_DataArrayHandle &src,
	    int tuple_size)
    {
	exint				nstrings = src->entries() * tuple_size;
	UT_StackBuffer<std::string>	strings(nstrings); // Property strings

	// Get strings from GT
	for (int i = 0; i < tuple_size; ++i)
	    src->fillStrings(strings + src->entries()*i, i);

	ArraySample	sample(strings, prop.getDataType(),
				Dimensions(nstrings));
	prop.set(sample);
	return true;
    }
}

GABC_OProperty::GABC_OProperty(Alembic::AbcGeom::GeometryScope scope)
    : myScope(scope)
{
}

GABC_OProperty::~GABC_OProperty()
{
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
GABC_OProperty::start(OCompoundProperty &parent,
	const char *name,
	const GT_DataArrayHandle &array,
	const GABC_OOptions &options)
{
    if (!array)
	return false;

    exint			array_size = 1;
    Alembic::Util::uint32_t	time = 0;
    bool			valid = false;
    const TimeSamplingPtr	&ts = options.timeSampling();

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
		DECL_PARAM(OUcharGeomParam)
		break;
	    case GT_STORE_INT32:
		DECL_PARAM(OInt32GeomParam);
		break;
	    case GT_STORE_INT64:
		DECL_PARAM(OInt64GeomParam);
		break;
	    case GT_STORE_REAL16:
		DECL_PARAM(OHalfGeomParam);
		break;
	    case GT_STORE_REAL32:
		DECL_PARAM(OFloatGeomParam);
		break;
	    case GT_STORE_REAL64:
		DECL_PARAM(ODoubleGeomParam);
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
GABC_OProperty::update(const GT_DataArrayHandle &array,
	const GABC_OOptions &ctx)
{
    if (ctx.optimizeSpace() >= GABC_OOptions::OPTIMIZE_ATTRIBUTES)
    {
	if (myCache && array->isEqual(*myCache))
	{
	    myProperty.setFromPrevious();
	    return true;
	}
	// Keep previous version cached
	myCache = array->harden();
    }
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
