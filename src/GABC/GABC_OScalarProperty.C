/*
 * Copyright (c) 2018
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

#include "GABC_OOptions.h"
#include "GABC_OScalarProperty.h"
#include <GT/GT_DANumeric.h>
#include <UT/UT_StringArray.h>
#include <UT/UT_StackBuffer.h>
#include <UT/UT_EnvControl.h>

using namespace GABC_NAMESPACE;

namespace
{
    typedef Alembic::AbcCoreAbstract::DataType      DataType;
    
    typedef Alembic::Abc::OCompoundProperty         OCompoundProperty;
    typedef Alembic::Abc::OScalarProperty           OScalarProperty;
    typedef Alembic::Abc::TimeSamplingPtr           TimeSamplingPtr;

    // Simple properties
    typedef Alembic::Abc::OBoolProperty             OBoolProperty;
    typedef Alembic::Abc::OCharProperty             OCharProperty;
    typedef Alembic::Abc::OUcharProperty            OUcharProperty;
    typedef Alembic::Abc::OUcharProperty            OInt16Property;
    typedef Alembic::Abc::OUInt16Property           OUInt16Property;
    typedef Alembic::Abc::OInt32Property            OInt32Property;
    typedef Alembic::Abc::OUInt32Property           OUInt32Property;
    typedef Alembic::Abc::OInt64Property            OInt64Property;
    typedef Alembic::Abc::OUInt64Property           OUInt64Property;
    typedef Alembic::Abc::OHalfProperty             OHalfProperty;
    typedef Alembic::Abc::OFloatProperty            OFloatProperty;
    typedef Alembic::Abc::ODoubleProperty           ODoubleProperty;
    typedef Alembic::Abc::OStringProperty           OStringProperty;

    // Complex properties
    typedef Alembic::Abc::OP2sProperty              OP2sProperty;
    typedef Alembic::Abc::OP2iProperty              OP2iProperty;
    typedef Alembic::Abc::OP2fProperty              OP2fProperty;
    typedef Alembic::Abc::OP2dProperty              OP2dProperty;

    typedef Alembic::Abc::OP3sProperty              OP3sProperty;
    typedef Alembic::Abc::OP3iProperty              OP3iProperty;
    typedef Alembic::Abc::OP3fProperty              OP3fProperty;
    typedef Alembic::Abc::OP3dProperty              OP3dProperty;

    typedef Alembic::Abc::OV2sProperty              OV2sProperty;
    typedef Alembic::Abc::OV2iProperty              OV2iProperty;
    typedef Alembic::Abc::OV2fProperty              OV2fProperty;
    typedef Alembic::Abc::OV2dProperty              OV2dProperty;

    typedef Alembic::Abc::OV3sProperty              OV3sProperty;
    typedef Alembic::Abc::OV3iProperty              OV3iProperty;
    typedef Alembic::Abc::OV3fProperty              OV3fProperty;
    typedef Alembic::Abc::OV3dProperty              OV3dProperty;

    typedef Alembic::Abc::ON2fProperty              ON2fProperty;
    typedef Alembic::Abc::ON2dProperty              ON2dProperty;

    typedef Alembic::Abc::ON3fProperty              ON3fProperty;
    typedef Alembic::Abc::ON3dProperty              ON3dProperty;

    typedef Alembic::Abc::OQuatfProperty            OQuatfProperty;
    typedef Alembic::Abc::OQuatdProperty            OQuatdProperty;

    typedef Alembic::Abc::OC3hProperty              OC3hProperty;
    typedef Alembic::Abc::OC3fProperty              OC3fProperty;
    typedef Alembic::Abc::OC3cProperty              OC3cProperty;

    typedef Alembic::Abc::OC4hProperty              OC4hProperty;
    typedef Alembic::Abc::OC4fProperty              OC4fProperty;
    typedef Alembic::Abc::OC4cProperty              OC4cProperty;

    typedef Alembic::Abc::OBox2sProperty            OBox2sProperty;
    typedef Alembic::Abc::OBox2iProperty            OBox2iProperty;
    typedef Alembic::Abc::OBox2fProperty            OBox2fProperty;
    typedef Alembic::Abc::OBox2dProperty            OBox2dProperty;

    typedef Alembic::Abc::OBox3sProperty            OBox3sProperty;
    typedef Alembic::Abc::OBox3iProperty            OBox3iProperty;
    typedef Alembic::Abc::OBox3fProperty            OBox3fProperty;
    typedef Alembic::Abc::OBox3dProperty            OBox3dProperty;

    typedef Alembic::Abc::OM33fProperty             OM33fProperty;
    typedef Alembic::Abc::OM33dProperty             OM33dProperty;

    typedef Alembic::Abc::OM44fProperty             OM44fProperty;
    typedef Alembic::Abc::OM44dProperty             OM44dProperty;

    typedef Alembic::Util::Dimensions               Dimensions;
    typedef Alembic::Util::PlainOldDataType         PlainOldDataType;

#define REINTERPRET_DATA(FUNC) \
    do \
    { \
	for (int i = 0; i < tuple_size; ++i) \
	    buffer[i] = src->FUNC(0, i); \
    } while(false)

    // Alembic 8-bit integers, 16-bit integers, and unsigned 16-bit integers
    // are stored as 32-bit integers in the GT_DataArray. Similarly,
    // unsigned 32-bit integers are stored as 64-bit integers.
    //
    // Alembic takes data as a void * to a block of memory containing the
    // data it expects. This causes issues in the above cases when a 32-bit
    // int is interpreted as 4 8-bit ints or 2 16-bit ints.
    //
    // To fix this issue, arrays of the aforementioned 4 types must be
    // reinterpreted in a buffer.
    template <typename POD_T>
    static void
    reinterpretArray(POD_T *buffer,
	    const GT_DataArrayHandle &src,
	    int tuple_size)
    {
	switch(src->getStorage())
	{
	    case GT_STORE_UINT8:
		REINTERPRET_DATA(getU8);
		break;

	    case GT_STORE_INT8:
		REINTERPRET_DATA(getI8);
		break;

	    case GT_STORE_INT16:
		REINTERPRET_DATA(getI16);
		break;

	    case GT_STORE_INT32:
		REINTERPRET_DATA(getI32);
		break;

	    case GT_STORE_INT64:
		REINTERPRET_DATA(getI64);
		break;

	    case GT_STORE_REAL16:
		REINTERPRET_DATA(getF16);
		break;

	    case GT_STORE_REAL32:
		REINTERPRET_DATA(getF32);
		break;

	    case GT_STORE_REAL64:
		REINTERPRET_DATA(getF64);
		break;

	    default:
		UT_ASSERT(0 && "Invalid Storage Type.");
		for (int i = 0; i < tuple_size; ++i)
		    buffer[i] = 0;
	}
    }

    // Cast the GT_DataArray to the correct numeric type and pass
    // the data to Alembic.
    template <typename POD_T>
    static void
    writeProperty(OScalarProperty &prop,
            const GT_DataArrayHandle &src)
    {
        GT_DANumeric<POD_T>  *numeric;
        numeric = dynamic_cast<GT_DANumeric<POD_T> *>(src.get());

        if (numeric)
        {
            prop.set(numeric->data());
        }
        else
        {
            POD_T  *data = new POD_T[src->getTupleSize()];
            reinterpretArray(data, src, src->getTupleSize());
            prop.set(data);
            delete[] data;
        }
    }

    // Output string data to Alembic.
    static void
    writeStringProperty(OScalarProperty &prop,
	    const GT_DataArrayHandle &src)
    {
	std::string sample;
	const char *s = src->getS(0, 0);
	if(s)
	    sample = s;
	prop.set((void *)(&sample));
    }
}

GABC_OScalarProperty::~GABC_OScalarProperty()
{
    // Clear the buffer
    if (myBuffer)
    {
        switch (myPOD)
        {
            case Alembic::Util::kUint16POD:
                delete[] (uint16 *)(myBuffer);
                break;

            case Alembic::Util::kUint32POD:
                delete[] (uint32 *)(myBuffer);
                break;

            default:
                break;
        }
    }
}

#define DECL_PARAM(TYPE) \
do \
{ \
    myProperty = Alembic::Abc::TYPE(parent, name, ts); \
    valid = true; \
} while(false)

// Cast up to at least 32-bit float.
#define DECL_REAL_HI(FTYPE, DTYPE) \
do \
{ \
    if (myStorage == GT_STORE_REAL16) \
        { myStorage = GT_STORE_REAL32; DECL_PARAM(FTYPE); } \
    else if (myStorage == GT_STORE_REAL32) { DECL_PARAM(FTYPE); } \
    else if (myStorage == GT_STORE_REAL64) { DECL_PARAM(DTYPE); } \
} while(false)

// Cast down to at most 32-bit float.
#define DECL_REAL_LO(HTYPE, FTYPE) \
do \
{ \
         if (myStorage == GT_STORE_REAL16) { DECL_PARAM(HTYPE); } \
    else if (myStorage == GT_STORE_REAL32) { DECL_PARAM(FTYPE); } \
    else if (myStorage == GT_STORE_REAL64) \
        { myStorage = GT_STORE_REAL32; DECL_PARAM(FTYPE); } \
} while(false)

// Check and clamp tuple size
#define MAX_TUPLE_SIZE(MAX, MAX_TYPE) \
    if (myTupleSize >= MAX) \
    { \
        DECL_PARAM(MAX_TYPE); \
        myTupleSize = MAX; \
    }
#define MAX_AND_MIN_TUPLE_SIZE(MAX, MAX_TYPE, MIN, MIN_TYPE) \
    MAX_TUPLE_SIZE(MAX, MAX_TYPE) \
    else if (myTupleSize == MIN) \
    { \
        DECL_PARAM(MIN_TYPE); \
    }

#define DETERMINE_FROM_STORAGE(STORAGE, TYPE_ONE, FUNC, TYPE_TWO, TYPE_THREE) \
do \
{ \
    if (myStorage == STORAGE) { DECL_PARAM(TYPE_ONE); } \
    else { FUNC(TYPE_TWO, TYPE_THREE); } \
} while(false)

#define DECL_IF_STORAGE_MATCHES(STORAGE, TYPE) \
do \
{ \
    if (myStorage == STORAGE) \
    { \
        DECL_PARAM(TYPE); \
    } \
} while(false)

bool
GABC_OScalarProperty::isValidScalarData(const GT_DataArrayHandle &array)
{
    if (!array)
	return false;

    auto storage = array->getStorage();
    auto tuplesize = array->getTupleSize();
    switch (array->getTypeInfo())
    {
	case GT_TYPE_POINT:
	case GT_TYPE_HPOINT:
	case GT_TYPE_VECTOR:
	    if(tuplesize >= 2
		&& (storage == GT_STORE_INT32
			|| storage == GT_STORE_REAL16
			|| storage == GT_STORE_REAL32
			|| storage == GT_STORE_REAL64))
		return true;
            break;

	case GT_TYPE_NORMAL:
	    if(tuplesize >= 2
		&& (storage == GT_STORE_REAL16
			|| storage == GT_STORE_REAL32
			|| storage == GT_STORE_REAL64))
		return true;
	    break;

	case GT_TYPE_COLOR:
	    if(tuplesize >= 3
		&& (storage == GT_STORE_UINT8
			|| storage == GT_STORE_REAL16
			|| storage == GT_STORE_REAL32
			|| storage == GT_STORE_REAL64))
		return true;
	    break;

	case GT_TYPE_QUATERNION:
	    if(tuplesize >= 4
		&& (storage == GT_STORE_REAL16
			|| storage == GT_STORE_REAL32
			|| storage == GT_STORE_REAL64))
		return true;
	    break;

	case GT_TYPE_MATRIX3:
	    if(tuplesize >= 9
		&& (storage == GT_STORE_REAL16
			|| storage == GT_STORE_REAL32
			|| storage == GT_STORE_REAL64))
		return true;
	    break;

	case GT_TYPE_MATRIX:
	    if(tuplesize >= 16
		&& (storage == GT_STORE_REAL16
			|| storage == GT_STORE_REAL32
			|| storage == GT_STORE_REAL64))
		return true;
	    break;

        case GT_TYPE_BOX2:
	    if(tuplesize >= 4
		&& (storage == GT_STORE_INT32
			|| storage == GT_STORE_REAL16
			|| storage == GT_STORE_REAL32
			|| storage == GT_STORE_REAL64))
		return true;
	    break;

        case GT_TYPE_BOX:
	    if(tuplesize >= 6
		&& (storage == GT_STORE_INT32
			|| storage == GT_STORE_REAL16
			|| storage == GT_STORE_REAL32
			|| storage == GT_STORE_REAL64))
		return true;
	    break;

	case GT_TYPE_ST:
	default:
	    break;
    }

    if(tuplesize == 1)
    {
	switch (storage)
	{
	    case GT_STORE_UINT8:
	    case GT_STORE_INT8:
	    case GT_STORE_INT16:
	    case GT_STORE_INT32:
	    case GT_STORE_INT64:
	    case GT_STORE_REAL16:
	    case GT_STORE_REAL32:
	    case GT_STORE_REAL64:
	    case GT_STORE_STRING:
		return true;

	    default:
		break;
	}
    }

    return false;
}

bool
GABC_OScalarProperty::start(OCompoundProperty &parent,
        const char *name,
        const GT_DataArrayHandle &array,
        GABC_OError &err,
        const GABC_OOptions &options,
        const PlainOldDataType pod)
{
    if (!array)
    {
	return false;
    }

    const TimeSamplingPtr  &ts = options.timeSampling();
    bool                    valid = false;

    myStorage = array->getStorage();
    myTupleSize = array->getTupleSize();

    myPOD = pod;
    switch (myPOD)
    {
        case Alembic::Util::kUint16POD:
            myBuffer = (void *)(new uint16[myTupleSize]);
            break;

        case Alembic::Util::kUint32POD:
            myBuffer = (void *)(new uint32[myTupleSize]);
            break;

        default:
            break;
    }

    myType = array->getTypeInfo();
    switch (myType)
    {
	case GT_TYPE_POINT:
	case GT_TYPE_HPOINT:
	    if (pod != Alembic::Util::kUnknownPOD)
	    {
	        if (pod == Alembic::Util::kInt16POD)
	        {
                    UT_ASSERT(myStorage == GT_STORE_INT16);
                    MAX_AND_MIN_TUPLE_SIZE(3, OP3sProperty, 2, OP2sProperty);
	        }
	        else if (pod == Alembic::Util::kInt32POD)
	        {
                    UT_ASSERT(myStorage == GT_STORE_INT32);
                    MAX_AND_MIN_TUPLE_SIZE(3, OP3iProperty, 2, OP2iProperty);
	        }
	        else if (pod == Alembic::Util::kFloat32POD)
	        {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_AND_MIN_TUPLE_SIZE(3, OP3fProperty, 2, OP2fProperty);
	        }
	        else if (pod == Alembic::Util::kFloat64POD)
	        {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_AND_MIN_TUPLE_SIZE(3, OP3dProperty, 2, OP2dProperty);
	        }
	    }
            else
            {
                if (myTupleSize >= 3)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_INT32,
                            OP3iProperty,
                            DECL_REAL_HI,
                            OP3fProperty,
                            OP3dProperty);
                    myTupleSize = 3;    // Clamp to 3
                }
                else if (myTupleSize == 2)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_INT32,
                            OP2iProperty,
                            DECL_REAL_HI,
                            OP2fProperty,
                            OP2dProperty);
                }
            }
            break;

	case GT_TYPE_VECTOR:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kInt16POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_INT16);
                    MAX_AND_MIN_TUPLE_SIZE(3, OV3sProperty, 2, OV2sProperty);
                }
                else if (pod == Alembic::Util::kInt32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_INT32);
                    MAX_AND_MIN_TUPLE_SIZE(3, OV3iProperty, 2, OV2iProperty);
                }
                else if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_AND_MIN_TUPLE_SIZE(3, OV3fProperty, 2, OV2fProperty);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_AND_MIN_TUPLE_SIZE(3, OV3dProperty, 2, OV2dProperty);
                }
            }
            else
            {
                if (myTupleSize >= 3)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_INT32,
                            OV3iProperty,
                            DECL_REAL_HI,
                            OV3fProperty,
                            OV3dProperty);
                    myTupleSize = 3;    // Clamp to 3
                }
                else if (myTupleSize == 2)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_INT32,
                            OV2iProperty,
                            DECL_REAL_HI,
                            OV2fProperty,
                            OV2dProperty);
                }
            }
	    break;

	case GT_TYPE_NORMAL:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_AND_MIN_TUPLE_SIZE(3, ON3fProperty, 2, ON2fProperty);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_AND_MIN_TUPLE_SIZE(3, ON3dProperty, 2, ON2dProperty);
                }
            }
            else
            {
                if (myTupleSize >= 3)
                {
                    DECL_REAL_HI(ON3fProperty, ON3dProperty);
                    myTupleSize = 3;    // Clamp to 3
                }
                else if (myTupleSize == 2)
                {
                    DECL_REAL_HI(ON2fProperty, ON2dProperty);
                }
            }
	    break;

	case GT_TYPE_COLOR:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kUint8POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_UINT8);
                    MAX_AND_MIN_TUPLE_SIZE(4, OC4cProperty, 3, OC3cProperty);
                }
                else if (pod == Alembic::Util::kFloat16POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL16);
                    MAX_AND_MIN_TUPLE_SIZE(4, OC4hProperty, 3, OC3hProperty);
                }
                else if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_AND_MIN_TUPLE_SIZE(4, OC4fProperty, 3, OC3fProperty);
                }
            }
            else
            {
                if (myTupleSize >= 4)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_UINT8,
                            OC4cProperty,
                            DECL_REAL_LO,
                            OC4hProperty,
                            OC4fProperty);
                    myTupleSize = 4;    // Clamp to 4
                }
                else if (myTupleSize == 3)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_UINT8,
                            OC3cProperty,
                            DECL_REAL_LO,
                            OC3hProperty,
                            OC3fProperty);
                }
            }
	    break;

	case GT_TYPE_QUATERNION:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_TUPLE_SIZE(4, OQuatfProperty);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_TUPLE_SIZE(4, OQuatdProperty);
                }
            }
            else
            {
                if (myTupleSize >= 4)
                {
                    DECL_REAL_HI(OQuatfProperty, OQuatdProperty);
                    myTupleSize = 4;    // Clamp to 4
                }
            }
	    break;

	case GT_TYPE_MATRIX3:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_TUPLE_SIZE(9, OM33fProperty);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_TUPLE_SIZE(9, OM33dProperty);
                }
            }
            else
            {
                if (myTupleSize >= 9)
                {
                    DECL_REAL_HI(OM33fProperty, OM33dProperty);
                    myTupleSize = 9;    // Clamp to 9
                }
            }
	    break;

	case GT_TYPE_MATRIX:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_TUPLE_SIZE(16, OM44fProperty);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_TUPLE_SIZE(16, OM44dProperty);
                }
            }
            else
            {
                if (myTupleSize >= 16)
                {
                    DECL_REAL_HI(OM44fProperty, OM44dProperty);
                    myTupleSize = 16;    // Clamp to 16
                }
            }
	    break;

        case GT_TYPE_BOX2:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kInt16POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_INT16);
                    MAX_TUPLE_SIZE(4, OBox2sProperty);
                }
                else if (pod == Alembic::Util::kInt32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_INT32);
                    MAX_TUPLE_SIZE(4, OBox2iProperty);
                }
                else if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_TUPLE_SIZE(4, OBox2fProperty);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_TUPLE_SIZE(4, OBox2dProperty);
                }
            }
            else
            {
                if (myTupleSize >= 4)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_INT32,
                            OBox2iProperty,
                            DECL_REAL_HI,
                            OBox2fProperty,
                            OBox2dProperty);
                    myTupleSize = 4;    // Clamp to 4
                }
            }
	    break;

        case GT_TYPE_BOX:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kInt16POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_INT16);
                    MAX_TUPLE_SIZE(6, OBox3sProperty);
                }
                else if (pod == Alembic::Util::kInt32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_INT32);
                    MAX_TUPLE_SIZE(6, OBox3iProperty);
                }
                else if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_TUPLE_SIZE(6, OBox3fProperty);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_TUPLE_SIZE(6, OBox3dProperty);
                }
            }
            else
            {
                if (myTupleSize >= 6)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_INT32,
                            OBox3iProperty,
                            DECL_REAL_HI,
                            OBox3fProperty,
                            OBox3dProperty);
                    myTupleSize = 6;    // Clamp to 6
                }
            }
	    break;

	case GT_TYPE_ST:
	default:
	    break;
    }
    if (!valid)
    {
	myTupleSize = 1;    // Clamp to 1

        switch (pod)
        {
            case Alembic::Util::kUnknownPOD:
                switch (myStorage)
                {
                    case GT_STORE_UINT8:
                        DECL_PARAM(OUcharProperty);
                        break;

                    case GT_STORE_INT8:
                        DECL_PARAM(OCharProperty);
                        break;

                    case GT_STORE_INT16:
                        DECL_PARAM(OInt16Property);
                        break;

                    case GT_STORE_INT32:
                        DECL_PARAM(OInt32Property);
                        break;

                    case GT_STORE_INT64:
                        DECL_PARAM(OInt64Property);
                        break;

                    case GT_STORE_REAL16:
                        DECL_PARAM(OHalfProperty);
                        break;

                    case GT_STORE_REAL32:
                        DECL_PARAM(OFloatProperty);
                        break;

                    case GT_STORE_REAL64:
                        DECL_PARAM(ODoubleProperty);
                        break;

                    case GT_STORE_STRING:
                        DECL_PARAM(OStringProperty);
                        break;

                    default:
                        break;
                }
                break;

            case Alembic::Util::kBooleanPOD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT8, OBoolProperty);
                break;

            case Alembic::Util::kInt8POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT8, OCharProperty);
                break;

            case Alembic::Util::kUint8POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_UINT8, OUcharProperty);
                break;

            case Alembic::Util::kInt16POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT16, OInt16Property);
                break;

            case Alembic::Util::kUint16POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT32, OUInt16Property);
                break;

            case Alembic::Util::kInt32POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT32, OInt32Property);
                break;

            case Alembic::Util::kUint32POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT64, OUInt32Property);
                break;

            case Alembic::Util::kInt64POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT64, OInt64Property);
                break;

            case Alembic::Util::kUint64POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT64, OUInt64Property);
                break;

            case Alembic::Util::kFloat16POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_REAL16, OHalfProperty);
                break;

            case Alembic::Util::kFloat32POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_REAL32, OFloatProperty);
                break;

            case Alembic::Util::kFloat64POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_REAL64, ODoubleProperty);
                break;

            case Alembic::Util::kStringPOD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_STRING, OStringProperty);
                break;

            case Alembic::Util::kWstringPOD:
            default:
                break;
        }
    }

    if (!valid)
    {
        err.warning("Cannot determine property type for %s.", name);
    }
    return valid;
}

bool
GABC_OScalarProperty::updateFromPrevious()
{
    if (getNumSamples())
    {
        myProperty.setFromPrevious();
        return true;
    }

    return false;
}

bool
GABC_OScalarProperty::update(const GT_DataArrayHandle &array,
        GABC_OError &err,
	const GABC_OOptions &ctx,
        const PlainOldDataType pod)
{
    if ((myPOD != pod)
            || (myStorage != array->getStorage())
            || (myTupleSize != array->getTupleSize())
            || (myType != array->getTypeInfo()))
    {
        err.warning("Error trying to update user property: data type mismatch");
        return false;
    }

    if (myCache && array->isEqual(*myCache))
    {
        return updateFromPrevious();
    }
    // Keep previous version cached
    myCache = array->harden();

    switch (myPOD)
    {
        case Alembic::Util::kUnknownPOD:
            switch (myStorage)
            {
                case GT_STORE_UINT8:
                    writeProperty<uint8>(myProperty, array);
                    break;

                case GT_STORE_INT8:
                    writeProperty<int8>(myProperty, array);
                    break;

                case GT_STORE_INT16:
                    writeProperty<int16>(myProperty, array);
                    break;

                case GT_STORE_INT32:
                    writeProperty<int32>(myProperty, array);
                    break;

                case GT_STORE_INT64:
                    writeProperty<int64>(myProperty, array);
                    break;

                case GT_STORE_REAL16:
                    writeProperty<fpreal16>(myProperty, array);
                    break;

                case GT_STORE_REAL32:
                    writeProperty<fpreal32>(myProperty, array);
                    break;

                case GT_STORE_REAL64:
                    writeProperty<fpreal64>(myProperty, array);
                    break;

                case GT_STORE_STRING:
                    writeStringProperty(myProperty, array);
                    break;

                default:
                    UT_ASSERT(0 && "Unrecognized type");
                    return false;
            }
            break;

        case Alembic::Util::kInt8POD:
            writeProperty<int8>(myProperty, array);
            break;

        case Alembic::Util::kInt16POD:
            writeProperty<int16>(myProperty, array);
            break;

        case Alembic::Util::kUint16POD:
            reinterpretArray<uint16>((uint16 *)myBuffer, array, myTupleSize);
            myProperty.set(myBuffer);
            break;

        case Alembic::Util::kUint32POD:
            reinterpretArray<uint32>((uint32 *)myBuffer, array, myTupleSize);
            myProperty.set(myBuffer);
            break;

        case Alembic::Util::kStringPOD:
            writeStringProperty(myProperty, array);
            break;

        case Alembic::Util::kBooleanPOD:
            writeProperty<uint8>(myProperty, array);
            break;

        case Alembic::Util::kUint8POD:
            writeProperty<uint8>(myProperty, array);
            break;

        case Alembic::Util::kInt32POD:
            writeProperty<int32>(myProperty, array);
            break;

        case Alembic::Util::kInt64POD:
            writeProperty<int64>(myProperty, array);
            break;

        case Alembic::Util::kUint64POD:
            writeProperty<int64>(myProperty, array);
            break;

        case Alembic::Util::kFloat16POD:
            writeProperty<fpreal16>(myProperty, array);
            break;

        case Alembic::Util::kFloat32POD:
            writeProperty<fpreal32>(myProperty, array);
            break;

        case Alembic::Util::kFloat64POD:
            writeProperty<fpreal64>(myProperty, array);
            break;

        case Alembic::Util::kWstringPOD:
        default:
            UT_ASSERT(0 && "Unrecognized type");
            return false;
    }

    return true;
}
