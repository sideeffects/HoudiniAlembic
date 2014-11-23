/*
 * Copyright (c) 2014
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
#include "GABC_OArrayProperty.h"
#include <GT/GT_DANumeric.h>
#include <UT/UT_StringArray.h>
#include <UT/UT_StackBuffer.h>
#include <UT/UT_EnvControl.h>

using namespace GABC_NAMESPACE;

namespace
{
    typedef Alembic::AbcCoreAbstract::DataType          DataType;
    typedef Alembic::AbcCoreAbstract::ArraySample       ArraySample;

    typedef Alembic::Abc::OCompoundProperty             OCompoundProperty;
    typedef Alembic::Abc::OArrayProperty                OArrayProperty;
    typedef Alembic::Abc::OUInt32ArrayProperty          OUInt32ArrayProperty;
    typedef Alembic::Abc::TimeSamplingPtr               TimeSamplingPtr;

    // Simple properties
    typedef Alembic::AbcGeom::OBoolGeomParam            OBoolGeomParam;
    typedef Alembic::AbcGeom::OCharGeomParam            OCharGeomParam;
    typedef Alembic::AbcGeom::OUcharGeomParam           OUcharGeomParam;
    typedef Alembic::AbcGeom::OUcharGeomParam           OInt16GeomParam;
    typedef Alembic::AbcGeom::OUInt16GeomParam          OUInt16GeomParam;
    typedef Alembic::AbcGeom::OInt32GeomParam           OInt32GeomParam;
    typedef Alembic::AbcGeom::OUInt32GeomParam          OUInt32GeomParam;
    typedef Alembic::AbcGeom::OInt64GeomParam           OInt64GeomParam;
    typedef Alembic::AbcGeom::OUInt64GeomParam          OUInt64GeomParam;
    typedef Alembic::AbcGeom::OHalfGeomParam            OHalfGeomParam;
    typedef Alembic::AbcGeom::OFloatGeomParam           OFloatGeomParam;
    typedef Alembic::AbcGeom::ODoubleGeomParam          ODoubleGeomParam;
    typedef Alembic::AbcGeom::OStringGeomParam          OStringGeomParam;

    // Complex properties
    typedef Alembic::AbcGeom::OP2sGeomParam             OP2sGeomParam;
    typedef Alembic::AbcGeom::OP2iGeomParam             OP2iGeomParam;
    typedef Alembic::AbcGeom::OP2fGeomParam             OP2fGeomParam;
    typedef Alembic::AbcGeom::OP2dGeomParam             OP2dGeomParam;

    typedef Alembic::AbcGeom::OP3sGeomParam             OP3sGeomParam;
    typedef Alembic::AbcGeom::OP3iGeomParam             OP3iGeomParam;
    typedef Alembic::AbcGeom::OP3fGeomParam             OP3fGeomParam;
    typedef Alembic::AbcGeom::OP3dGeomParam             OP3dGeomParam;

    typedef Alembic::AbcGeom::OV2sGeomParam             OV2sGeomParam;
    typedef Alembic::AbcGeom::OV2iGeomParam             OV2iGeomParam;
    typedef Alembic::AbcGeom::OV2fGeomParam             OV2fGeomParam;
    typedef Alembic::AbcGeom::OV2dGeomParam             OV2dGeomParam;

    typedef Alembic::AbcGeom::OV3sGeomParam             OV3sGeomParam;
    typedef Alembic::AbcGeom::OV3iGeomParam             OV3iGeomParam;
    typedef Alembic::AbcGeom::OV3fGeomParam             OV3fGeomParam;
    typedef Alembic::AbcGeom::OV3dGeomParam             OV3dGeomParam;

    typedef Alembic::AbcGeom::ON2fGeomParam             ON2fGeomParam;
    typedef Alembic::AbcGeom::ON2dGeomParam             ON2dGeomParam;

    typedef Alembic::AbcGeom::ON3fGeomParam             ON3fGeomParam;
    typedef Alembic::AbcGeom::ON3dGeomParam             ON3dGeomParam;

    typedef Alembic::AbcGeom::OQuatfGeomParam           OQuatfGeomParam;
    typedef Alembic::AbcGeom::OQuatdGeomParam           OQuatdGeomParam;

    typedef Alembic::AbcGeom::OC3hGeomParam             OC3hGeomParam;
    typedef Alembic::AbcGeom::OC3fGeomParam             OC3fGeomParam;
    typedef Alembic::AbcGeom::OC3cGeomParam             OC3cGeomParam;

    typedef Alembic::AbcGeom::OC4hGeomParam             OC4hGeomParam;
    typedef Alembic::AbcGeom::OC4fGeomParam             OC4fGeomParam;
    typedef Alembic::AbcGeom::OC4cGeomParam             OC4cGeomParam;

    typedef Alembic::AbcGeom::OBox2sGeomParam           OBox2sGeomParam;
    typedef Alembic::AbcGeom::OBox2iGeomParam           OBox2iGeomParam;
    typedef Alembic::AbcGeom::OBox2fGeomParam           OBox2fGeomParam;
    typedef Alembic::AbcGeom::OBox2dGeomParam           OBox2dGeomParam;

    typedef Alembic::AbcGeom::OBox3sGeomParam           OBox3sGeomParam;
    typedef Alembic::AbcGeom::OBox3iGeomParam           OBox3iGeomParam;
    typedef Alembic::AbcGeom::OBox3fGeomParam           OBox3fGeomParam;
    typedef Alembic::AbcGeom::OBox3dGeomParam           OBox3dGeomParam;

    typedef Alembic::AbcGeom::OM33fGeomParam            OM33fGeomParam;
    typedef Alembic::AbcGeom::OM33dGeomParam            OM33dGeomParam;

    typedef Alembic::AbcGeom::OM44fGeomParam            OM44fGeomParam;
    typedef Alembic::AbcGeom::OM44dGeomParam            OM44dGeomParam;

    typedef Alembic::Util::Dimensions                   Dimensions;
    typedef Alembic::Util::PlainOldDataType             PlainOldDataType;

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
    reinterpretArray(OArrayProperty &prop,
        const GT_DataArrayHandle &src,
        int tuple_size)
    {
        POD_T      *data;
        POD_T      *orig;
        DataType    dt = prop.getDataType();
        int         entries = src->entries();
        int         items = entries * tuple_size;
        int	    array_size;

        UT_ASSERT((tuple_size % dt.getExtent()) == 0);
        array_size = tuple_size / dt.getExtent();

        data = new POD_T[items];
        orig = data;

        for (int i = 0; i < entries; ++i)
        {
            for (int j = 0; j < tuple_size; ++j)
            {
                *data = src->getI32(i, j);
                ++data;
            }
        }

        ArraySample sample(orig, dt, Dimensions(entries * array_size));
        prop.set(sample);
        delete[] orig;
    }

    // Reinterpret the GT_DataArray to the correct numeric type and pass
    // the data to Alembic.
    template <typename POD_T, GT_Storage GT_STORE>
    static void
    writeProperty(OArrayProperty &prop,
	    const GT_DataArrayHandle &src,
	    int tuple_size)
    {
        DataType                        dt = prop.getDataType();
        GT_DANumeric<POD_T, GT_STORE>  *numeric;
	int	                        array_size;

	UT_ASSERT((tuple_size % dt.getExtent()) == 0);

	array_size = tuple_size / dt.getExtent();
        numeric = UTverify_cast<GT_DANumeric<POD_T, GT_STORE> *>(src.get());

        ArraySample sample(numeric->data(),
                dt,
                Dimensions(numeric->entries() * array_size));
        prop.set(sample);
    }

    static void
    writeStringProperty(OArrayProperty &prop,
	    const GT_DataArrayHandle &src,
	    int tuple_size)
    {
	exint				nstrings = src->entries() * tuple_size;
	UT_StackBuffer<std::string>	strings(nstrings); // Property strings

	// Get strings from GT
	for (int i = 0; i < tuple_size; ++i)
	{
	    src->fillStrings(strings + src->entries()*i, i);
        }

	ArraySample sample(strings, prop.getDataType(), Dimensions(nstrings));
	prop.set(sample);
    }

    static void
    writeStringProperty(OArrayProperty &prop,
	    OUInt32ArrayProperty &iprop,
	    const GT_DataArrayHandle &src,
	    int tuple_size)
    {
        //
	UT_StringArray  gtstrings;
	UT_IntArray     gtindices;
	exint           nstrings = 0;
	exint           nullstring = -1;

	src->getIndexedStrings(gtstrings, gtindices);
	UT_StackBuffer<std::string>	strings(gtstrings.entries()+1);
	for (exint i = 0; i < gtindices.entries(); ++i)
	{
	    if (gtindices(i) >= 0)
	    {
		if (nullstring < 0 && !gtstrings(i).isstring())
		    nullstring = nstrings;
		strings[nstrings] = gtstrings(i).toStdString();
		gtindices(i) = nstrings;	// Destination target
		nstrings++;
	    }
	}
	// Now, set the indices
	GT_DataArrayHandle	 storage;
	exint			 asize = src->entries();
	exint			 numindex = asize * tuple_size;
	const int32		*indices = src->getI32Array(storage);
	bool			 nullused = false;
	UT_StackBuffer<int32>	 indexstorage(numindex);

	// GT has its string tuples interleaved.  That is:
	//  [ offset0,index0
	//    offset0,index1
	//    offset0,index2
	//	  offset1,index0
	//	  offset1,index1
	//	  ...]
	// Alembic needs each array index in its own band of data, so we
	// need to de-interleave
	if (tuple_size == 1)
	    memcpy(indexstorage, indices, sizeof(int32)*numindex);
	else
	{
	    for (int i = 0; i < tuple_size; ++i)
	    {
		int32		*dest = indexstorage + i*asize;
		const int32	*src = indices + i;
		for (exint j = 0; j < asize; ++j)
		{
		    // We access positions 0,t,2t,3t,.. each time, but
		    // we move the starting point (what src[0] points to)
		    // up by 1 for each i
		    dest[j] = src[j * tuple_size];
                }
	    }
	}
	int32	*data = indexstorage;
	for (exint i = 0; i < numindex; ++i)
	{
	    if (data[i] < 0 || data[i] >= gtstrings.entries())
	    {
		data[i] = nullstring < 0 ? nstrings : nullstring;
		nullused = true;
	    }
	    else
	    {
		data[i] = gtindices(data[i]);
	    }
	}
	indices = indexstorage;
	if (nullstring < 0 && nullused)
	{
	    nullstring = nstrings;
	    strings[nstrings] = "";
	    nstrings++;
	}
	ArraySample	stringdata(strings, prop.getDataType(),
				    Dimensions(nstrings));
	prop.set(stringdata);

	ArraySample	isample((const uint32 *)indices,
				DataType(Alembic::Util::kUint32POD, 1),
				Dimensions(numindex));

	iprop.set(isample);
    }
}

// When creating an attribute, we need to make an OGeomParam and pull the
// OArrayProperty out of it. For a user property, we can directly create
// the OArrayProperty.
#define DECL_PARAM(TYPE) \
do \
{ \
    if (myIsGeomParam) \
    { \
        Alembic::AbcGeom::TYPE gp(parent, name, false, myScope, array_size, time); \
        myProperty = gp.getValueProperty(); \
        myProperty.setTimeSampling(ts); \
    } \
    else \
    { \
        myProperty = Alembic::AbcGeom::TYPE::prop_type(parent, name, ts); \
    } \
    valid = true; \
} while(false)

// Indexed parameters need an additional OArrayProperty for the indices.
#define DECL_INDEX_PARAM(TYPE) \
do \
{ \
    Alembic::AbcGeom::TYPE gp(parent, name, true, myScope, 1, time); \
    myProperty = gp.getValueProperty(); \
    myIndexProperty = gp.getIndexProperty(); \
    myProperty.setTimeSampling(ts); \
    myIndexProperty.setTimeSampling(ts); \
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
GABC_OArrayProperty::start(OCompoundProperty &parent,
        const char *name,
        const GT_DataArrayHandle &array,
        GABC_OError &err,
        const GABC_OOptions &options,
        const PlainOldDataType pod)
{
    if (!array)
	return false;

    const TimeSamplingPtr  &ts = options.timeSampling();
    exint                   array_size = 1;
    exint                   time = 0;
    bool                    valid = false;

    myStorage = array->getStorage();
    myTupleSize = array->getTupleSize();
    myType = array->getTypeInfo();
    myPOD = pod;
    switch (myType)
    {
	case GT_TYPE_POINT:
	case GT_TYPE_HPOINT:
	    if (pod != Alembic::Util::kUnknownPOD)
	    {
	        if (pod == Alembic::Util::kInt16POD)
	        {
                    UT_ASSERT(myStorage == GT_STORE_INT32);
                    MAX_AND_MIN_TUPLE_SIZE(3, OP3sGeomParam, 2, OP2sGeomParam);
	        }
	        else if (pod == Alembic::Util::kInt32POD)
	        {
                    UT_ASSERT(myStorage == GT_STORE_INT32);
                    MAX_AND_MIN_TUPLE_SIZE(3, OP3iGeomParam, 2, OP2iGeomParam);
	        }
	        else if (pod == Alembic::Util::kFloat32POD)
	        {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_AND_MIN_TUPLE_SIZE(3, OP3fGeomParam, 2, OP2fGeomParam);
	        }
	        else if (pod == Alembic::Util::kFloat64POD)
	        {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_AND_MIN_TUPLE_SIZE(3, OP3dGeomParam, 2, OP2dGeomParam);
	        }
	    }
            else
            {
                if (myTupleSize >= 3)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_INT32,
                            OP3iGeomParam,
                            DECL_REAL_HI,
                            OP3fGeomParam,
                            OP3dGeomParam);
                    myTupleSize = 3;    // Clamp to 3
                }
                else if (myTupleSize == 2)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_INT32,
                            OP2iGeomParam,
                            DECL_REAL_HI,
                            OP2fGeomParam,
                            OP2dGeomParam);
                }
            }
            break;

	case GT_TYPE_VECTOR:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kInt16POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_INT32);
                    MAX_AND_MIN_TUPLE_SIZE(3, OV3sGeomParam, 2, OV2sGeomParam);
                }
                else if (pod == Alembic::Util::kInt32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_INT32);
                    MAX_AND_MIN_TUPLE_SIZE(3, OV3iGeomParam, 2, OV2iGeomParam);
                }
                else if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_AND_MIN_TUPLE_SIZE(3, OV3fGeomParam, 2, OV2fGeomParam);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_AND_MIN_TUPLE_SIZE(3, OV3dGeomParam, 2, OV2dGeomParam);
                }
            }
            else
            {
                if (myTupleSize >= 3)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_INT32,
                            OV3iGeomParam,
                            DECL_REAL_HI,
                            OV3fGeomParam,
                            OV3dGeomParam);
                    myTupleSize = 3;    // Clamp to 3
                }
                else if (myTupleSize == 2)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_INT32,
                            OV2iGeomParam,
                            DECL_REAL_HI,
                            OV2fGeomParam,
                            OV2dGeomParam);
                }
            }
	    break;

	case GT_TYPE_NORMAL:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_AND_MIN_TUPLE_SIZE(3, ON3fGeomParam, 2, ON2fGeomParam);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_AND_MIN_TUPLE_SIZE(3, ON3dGeomParam, 2, ON2dGeomParam);
                }
            }
            else
            {
                if (myTupleSize >= 3)
                {
                    DECL_REAL_HI(OV3fGeomParam, OV3dGeomParam);
                    myTupleSize = 3;    // Clamp to 3
                }
                else if (myTupleSize == 2)
                {
                    DECL_REAL_HI(OV2fGeomParam, OV2dGeomParam);
                }
            }
	    break;

	case GT_TYPE_COLOR:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kUint8POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_UINT8);
                    MAX_AND_MIN_TUPLE_SIZE(4, OC4cGeomParam, 3, OC3cGeomParam);
                }
                else if (pod == Alembic::Util::kFloat16POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL16);
                    MAX_AND_MIN_TUPLE_SIZE(4, OC4hGeomParam, 3, OC3hGeomParam);
                }
                else if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_AND_MIN_TUPLE_SIZE(4, OC4fGeomParam, 3, OC3fGeomParam);
                }
            }
            else
            {
                if (myTupleSize >= 4)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_UINT8,
                            OC4cGeomParam,
                            DECL_REAL_LO,
                            OC4hGeomParam,
                            OC4fGeomParam);
                    myTupleSize = 4;    // Clamp to 4
                }
                else if (myTupleSize == 3)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_UINT8,
                            OC3cGeomParam,
                            DECL_REAL_LO,
                            OC3hGeomParam,
                            OC3fGeomParam);
                }
            }
	    break;

	case GT_TYPE_QUATERNION:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_TUPLE_SIZE(4, OQuatfGeomParam);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_TUPLE_SIZE(4, OQuatdGeomParam);
                }
            }
            else
            {
                if (myTupleSize >= 4)
                {
                    DECL_REAL_HI(OQuatfGeomParam, OQuatdGeomParam);
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
                    MAX_TUPLE_SIZE(9, OM33fGeomParam);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_TUPLE_SIZE(9, OM33dGeomParam);
                }
            }
            else
            {
                if (myTupleSize >= 9)
                {
                    DECL_REAL_HI(OM33fGeomParam, OM33dGeomParam);
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
                    MAX_TUPLE_SIZE(16, OM44fGeomParam);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_TUPLE_SIZE(16, OM44dGeomParam);
                }
            }
            else
            {
                if (myTupleSize >= 16)
                {
                    DECL_REAL_HI(OM44fGeomParam, OM33dGeomParam);
                    myTupleSize = 16;    // Clamp to 16
                }
            }
	    break;

        case GT_TYPE_BOX2:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kInt16POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_INT32);
                    MAX_TUPLE_SIZE(4, OBox2sGeomParam);
                }
                else if (pod == Alembic::Util::kInt32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_INT32);
                    MAX_TUPLE_SIZE(4, OBox2iGeomParam);
                }
                else if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_TUPLE_SIZE(4, OBox2fGeomParam);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_TUPLE_SIZE(4, OBox2dGeomParam);
                }
            }
            else
            {
                if (myTupleSize >= 4)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_INT32,
                            OBox2iGeomParam,
                            DECL_REAL_HI,
                            OBox2fGeomParam,
                            OBox2dGeomParam);
                    myTupleSize = 4;    // Clamp to 4
                }
            }
	    break;

        case GT_TYPE_BOX:
	    if (pod != Alembic::Util::kUnknownPOD)
            {
                if (pod == Alembic::Util::kInt16POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_INT32);
                    MAX_TUPLE_SIZE(6, OBox3sGeomParam);
                }
                else if (pod == Alembic::Util::kInt32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_INT32);
                    MAX_TUPLE_SIZE(6, OBox3iGeomParam);
                }
                else if (pod == Alembic::Util::kFloat32POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL32);
                    MAX_TUPLE_SIZE(6, OBox3fGeomParam);
                }
                else if (pod == Alembic::Util::kFloat64POD)
                {
                    UT_ASSERT(myStorage == GT_STORE_REAL64);
                    MAX_TUPLE_SIZE(6, OBox3dGeomParam);
                }
            }
            else
            {
                if (myTupleSize >= 6)
                {
                    DETERMINE_FROM_STORAGE(GT_STORE_INT32,
                            OBox3iGeomParam,
                            DECL_REAL_HI,
                            OBox3fGeomParam,
                            OBox3dGeomParam);
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
	array_size = myTupleSize;

        switch (pod)
        {
            case Alembic::Util::kUnknownPOD:
                switch (myStorage)
                {
                    case GT_STORE_UINT8:
                        DECL_PARAM(OUcharGeomParam);
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
                        // If the tuple size isn't 1
                        //   or if there are no indexed strings
                        //   or if more than half of strings have unique indices
                        // Then generate a flat array of strings (not indirect)
                        if (!myIsGeomParam
                                || myTupleSize != 1
                                || array->getStringIndexCount() < 0
                                || array->getStringIndexCount() > array->entries()/2
                                || UT_EnvControl::getInt(ENV_HOUDINI_DISABLE_ALEMBIC_INDEXED_ARRAYS))
                        {
                            DECL_PARAM(OStringGeomParam);
                        }
                        else
                        {
                            DECL_INDEX_PARAM(OStringGeomParam);
                        }
                        break;

                    default:
                        break;
                }
                break;

            case Alembic::Util::kBooleanPOD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_UINT8, OBoolGeomParam);
                break;

            case Alembic::Util::kInt8POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT32, OCharGeomParam);
                break;

            case Alembic::Util::kUint8POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_UINT8, OUcharGeomParam);
                break;

            case Alembic::Util::kInt16POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT32, OInt16GeomParam);
                break;

            case Alembic::Util::kUint16POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT32, OUInt16GeomParam);
                break;

            case Alembic::Util::kInt32POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT32, OInt32GeomParam);
                break;

            case Alembic::Util::kUint32POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT64, OUInt32GeomParam);
                break;

            case Alembic::Util::kInt64POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT64, OInt64GeomParam);
                break;

            case Alembic::Util::kUint64POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT64, OUInt64GeomParam);
                break;

            case Alembic::Util::kFloat16POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_REAL16, OHalfGeomParam);
                break;

            case Alembic::Util::kFloat32POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_REAL32, OFloatGeomParam);
                break;

            case Alembic::Util::kFloat64POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_REAL64, ODoubleGeomParam);
                break;

            case Alembic::Util::kStringPOD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_STRING, OStringGeomParam);
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
GABC_OArrayProperty::update(const GT_DataArrayHandle &array,
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

//  TODO: Alembic does a check like this for geometry regardless of whether we
//          do or not. However, our check is 10% faster. It's likely the same
//          case for attribute caching. I haven't done a check to be sure.
//    if (ctx.optimizeSpace() >= GABC_OOptions::OPTIMIZE_ATTRIBUTES)
    if (true)
    {
	if (myCache && array->isEqual(*myCache))
	{
	    return updateFromPrevious();
	}
	// Keep previous version cached
	myCache = array->harden();
    }

    switch (myPOD)
    {
        case Alembic::Util::kUnknownPOD:
            switch (myStorage)
            {
                case GT_STORE_UINT8:
                    writeProperty<uint8, GT_STORE_UINT8>(myProperty,
                            array,
                            myTupleSize);
                    break;

                case GT_STORE_INT32:
                    writeProperty<int32, GT_STORE_INT32>(myProperty,
                            array,
                            myTupleSize);
                    break;

                case GT_STORE_INT64:
                    writeProperty<int64, GT_STORE_INT64>(myProperty,
                            array,
                            myTupleSize);
                    break;

                case GT_STORE_REAL16:
                    writeProperty<fpreal16, GT_STORE_REAL16>(myProperty,
                            array,
                            myTupleSize);
                    break;

                case GT_STORE_REAL32:
                    writeProperty<fpreal32, GT_STORE_REAL32>(myProperty,
                            array,
                            myTupleSize);
                    break;

                case GT_STORE_REAL64:
                    writeProperty<fpreal64, GT_STORE_REAL64>(myProperty,
                            array,
                            myTupleSize);
                    break;

                case GT_STORE_STRING:
                    if (!myIndexProperty)
                    {
                        // Non indexed strings
                        writeStringProperty(myProperty, array, myTupleSize);
                    }
                    else
                    {
                        // Indexed strings
                        writeStringProperty(myProperty, myIndexProperty,
                                array, myTupleSize);
                    }
                    break;

                default:
                    UT_ASSERT(0 && "Unrecognized type");
                    return false;
            }
            break;

        case Alembic::Util::kInt8POD:
            reinterpretArray<int8>(myProperty, array, myTupleSize);
            break;

        case Alembic::Util::kInt16POD:
            reinterpretArray<int16>(myProperty, array, myTupleSize);
            break;

        case Alembic::Util::kUint16POD:
            reinterpretArray<uint16>(myProperty, array, myTupleSize);
            break;

        case Alembic::Util::kUint32POD:
            reinterpretArray<uint32>(myProperty, array, myTupleSize);
            break;

        case Alembic::Util::kStringPOD:
            if (!myIndexProperty)
            {
                // Non indexed strings
                writeStringProperty(myProperty, array, myTupleSize);
            }
            else
            {
                // Indexed strings
                writeStringProperty(myProperty, myIndexProperty,
                        array, myTupleSize);
            }
            break;

        case Alembic::Util::kBooleanPOD:
            writeProperty<uint8, GT_STORE_UINT8>(myProperty,
                    array,
                    myTupleSize);
            break;

        case Alembic::Util::kUint8POD:
            writeProperty<uint8, GT_STORE_UINT8>(myProperty,
                    array,
                    myTupleSize);
            break;

        case Alembic::Util::kInt32POD:
            writeProperty<int32, GT_STORE_INT32>(myProperty,
                    array,
                    myTupleSize);
            break;

        case Alembic::Util::kInt64POD:
            writeProperty<int64, GT_STORE_INT64>(myProperty,
                    array,
                    myTupleSize);
            break;

        case Alembic::Util::kUint64POD:
            writeProperty<int64, GT_STORE_INT64>(myProperty,
                    array,
                    myTupleSize);
            break;

        case Alembic::Util::kFloat16POD:
            writeProperty<fpreal16, GT_STORE_REAL16>(myProperty,
                    array,
                    myTupleSize);
            break;

        case Alembic::Util::kFloat32POD:
            writeProperty<fpreal32, GT_STORE_REAL32>(myProperty,
                    array,
                    myTupleSize);
            break;

        case Alembic::Util::kFloat64POD:
            writeProperty<fpreal64, GT_STORE_REAL64>(myProperty,
                    array,
                    myTupleSize);
            break;

        case Alembic::Util::kWstringPOD:
        default:
            UT_ASSERT(0 && "Unrecognized type");
            return false;
    }

    return true;
}

bool
GABC_OArrayProperty::updateFromPrevious()
{
    if (getNumSamples())
    {
        myProperty.setFromPrevious();
        return true;
    }

    return false;
}