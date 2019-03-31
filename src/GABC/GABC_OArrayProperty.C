/*
 * Copyright (c) 2019
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
    using DataType = Alembic::AbcCoreAbstract::DataType;
    using ArraySample = Alembic::AbcCoreAbstract::ArraySample;
    using MetaData = Alembic::AbcCoreAbstract::MetaData;

    using OCompoundProperty = Alembic::Abc::OCompoundProperty;
    using OArrayProperty = Alembic::Abc::OArrayProperty;
    using OUInt32ArrayProperty = Alembic::Abc::OUInt32ArrayProperty;
    using TimeSamplingPtr = Alembic::Abc::TimeSamplingPtr;

    // Simple properties
    using OBoolGeomParam = Alembic::AbcGeom::OBoolGeomParam;
    using OCharGeomParam = Alembic::AbcGeom::OCharGeomParam;
    using OUcharGeomParam = Alembic::AbcGeom::OUcharGeomParam;
    using OUcharGeomParam = Alembic::AbcGeom::OUcharGeomParam;
    using OUInt16GeomParam = Alembic::AbcGeom::OUInt16GeomParam;
    using OInt32GeomParam = Alembic::AbcGeom::OInt32GeomParam;
    using OUInt32GeomParam = Alembic::AbcGeom::OUInt32GeomParam;
    using OInt64GeomParam = Alembic::AbcGeom::OInt64GeomParam;
    using OUInt64GeomParam = Alembic::AbcGeom::OUInt64GeomParam;
    using OHalfGeomParam = Alembic::AbcGeom::OHalfGeomParam;
    using OFloatGeomParam = Alembic::AbcGeom::OFloatGeomParam;
    using ODoubleGeomParam = Alembic::AbcGeom::ODoubleGeomParam;
    using OStringGeomParam = Alembic::AbcGeom::OStringGeomParam;

    // Complex properties
    using OP2sGeomParam = Alembic::AbcGeom::OP2sGeomParam;
    using OP2iGeomParam = Alembic::AbcGeom::OP2iGeomParam;
    using OP2fGeomParam = Alembic::AbcGeom::OP2fGeomParam;
    using OP2dGeomParam = Alembic::AbcGeom::OP2dGeomParam;

    using OP3sGeomParam = Alembic::AbcGeom::OP3sGeomParam;
    using OP3iGeomParam = Alembic::AbcGeom::OP3iGeomParam;
    using OP3fGeomParam = Alembic::AbcGeom::OP3fGeomParam;
    using OP3dGeomParam = Alembic::AbcGeom::OP3dGeomParam;

    using OV2sGeomParam = Alembic::AbcGeom::OV2sGeomParam;
    using OV2iGeomParam = Alembic::AbcGeom::OV2iGeomParam;
    using OV2fGeomParam = Alembic::AbcGeom::OV2fGeomParam;
    using OV2dGeomParam = Alembic::AbcGeom::OV2dGeomParam;

    using OV3sGeomParam = Alembic::AbcGeom::OV3sGeomParam;
    using OV3iGeomParam = Alembic::AbcGeom::OV3iGeomParam;
    using OV3fGeomParam = Alembic::AbcGeom::OV3fGeomParam;
    using OV3dGeomParam = Alembic::AbcGeom::OV3dGeomParam;

    using ON2fGeomParam = Alembic::AbcGeom::ON2fGeomParam;
    using ON2dGeomParam = Alembic::AbcGeom::ON2dGeomParam;

    using ON3fGeomParam = Alembic::AbcGeom::ON3fGeomParam;
    using ON3dGeomParam = Alembic::AbcGeom::ON3dGeomParam;

    using OQuatfGeomParam = Alembic::AbcGeom::OQuatfGeomParam;
    using OQuatdGeomParam = Alembic::AbcGeom::OQuatdGeomParam;

    using OC3hGeomParam = Alembic::AbcGeom::OC3hGeomParam;
    using OC3fGeomParam = Alembic::AbcGeom::OC3fGeomParam;
    using OC3cGeomParam = Alembic::AbcGeom::OC3cGeomParam;

    using OC4hGeomParam = Alembic::AbcGeom::OC4hGeomParam;
    using OC4fGeomParam = Alembic::AbcGeom::OC4fGeomParam;
    using OC4cGeomParam = Alembic::AbcGeom::OC4cGeomParam;

    using OBox2sGeomParam = Alembic::AbcGeom::OBox2sGeomParam;
    using OBox2iGeomParam = Alembic::AbcGeom::OBox2iGeomParam;
    using OBox2fGeomParam = Alembic::AbcGeom::OBox2fGeomParam;
    using OBox2dGeomParam = Alembic::AbcGeom::OBox2dGeomParam;

    using OBox3sGeomParam = Alembic::AbcGeom::OBox3sGeomParam;
    using OBox3iGeomParam = Alembic::AbcGeom::OBox3iGeomParam;
    using OBox3fGeomParam = Alembic::AbcGeom::OBox3fGeomParam;
    using OBox3dGeomParam = Alembic::AbcGeom::OBox3dGeomParam;

    using OM33fGeomParam = Alembic::AbcGeom::OM33fGeomParam;
    using OM33dGeomParam = Alembic::AbcGeom::OM33dGeomParam;

    using OM44fGeomParam = Alembic::AbcGeom::OM44fGeomParam;
    using OM44dGeomParam = Alembic::AbcGeom::OM44dGeomParam;

    using Dimensions = Alembic::Util::Dimensions;
    using PlainOldDataType = Alembic::Util::PlainOldDataType;

#define REINTERPRET_DATA(FUNC) \
    do \
    { \
        for (int i = 0; i < entries; ++i) \
        { \
            for (int j = 0; j < tuple_size; ++j) \
            { \
                *data = src->FUNC(i, j); \
                ++data; \
            } \
        } \
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
    //
    // In addition, attribute data is sometimes poorly defined and needs to
    // be reinterpreted before output to Alembic.
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

        switch (src->getStorage())
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
                *data = 0;
        }

        ArraySample sample(orig, dt, Dimensions(entries * array_size));
        prop.set(sample);
        delete[] orig;
    }

    // Cast the GT_DataArray to the correct numeric type and pass
    // the data to Alembic.
    template <typename POD_T>
    static void
    writeProperty(OArrayProperty &prop,
	    const GT_DataArrayHandle &src,
	    int tuple_size)
    {
	DataType		 dt = prop.getDataType();
	GT_DANumeric<POD_T>	*numeric;
	int			 array_size;

        numeric = dynamic_cast<GT_DANumeric<POD_T> *>(src.get());

        // Sometimes the data GT_DataArray will be a little wonky (usually
        // happens with attribute data), so it will need to be converted
        // in a buffer array.
        if (!numeric)
        {
            return reinterpretArray<POD_T>(prop, src, tuple_size);
        }

	UT_ASSERT((tuple_size % dt.getExtent()) == 0);
	array_size = tuple_size / dt.getExtent();
        ArraySample sample(numeric->data(),
                dt,
                Dimensions(numeric->entries() * array_size));
        prop.set(sample);
    }

    // Output string data to Alembic.
    static void
    writeStringProperty(OArrayProperty &prop,
	    const GT_DataArrayHandle &src,
	    int tuple_size)
    {
        exint                           entries = src->entries();
	exint                           nstrings = entries * tuple_size;
	UT_StackBuffer<std::string>     strings(nstrings); // Property strings
	std::string                    *data = strings.array();

	// Get strings from GT
	exint maxidx = src->getStringIndexCount();
	// avoid reading the full string table if we know we are only going to
	// extract a subset
	if (maxidx && maxidx < nstrings)
	{
            UT_StringArray  strs;
            int             idx;

            src->getStrings(strs);
            for (int i = 0; i < entries; ++i)
            {
                for (int j = 0; j < tuple_size; ++j)
                {
                    idx = src->getStringIndex(i, j);

                    if (idx >= 0 && strs(idx).isstring())
                    {
                        data[j] = strs(idx);
                    }
                    else
                    {
                        data[j] = "";
                    }
                }

                data += tuple_size;
            }
	}
	else
	{
            for (int i = 0; i < entries; ++i)
            {
                for (int j = 0; j < tuple_size; ++j)
                    data[j] = src->getS(i, j).toStdString();

                data += tuple_size;
            }
        }

	ArraySample sample(strings, prop.getDataType(), Dimensions(nstrings));
	prop.set(sample);
    }

    // Output indexed string data to Alembic.
    static void
    writeStringProperty(OArrayProperty &prop,
	    OUInt32ArrayProperty &iprop,
	    const GT_DataArrayHandle &src,
	    int tuple_size)
    {
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
        Alembic::AbcGeom::TYPE gp(parent, name, false, myScope, array_size, time, md); \
        myProperty = gp.getValueProperty(); \
        myProperty.setTimeSampling(ts); \
    } \
    else \
    { \
        myProperty = Alembic::AbcGeom::TYPE::prop_type(parent, name, ts, md); \
    } \
    valid = true; \
} while(false)

// Indexed parameters need an additional OArrayProperty for the indices.
#define DECL_INDEX_PARAM(TYPE) \
do \
{ \
    Alembic::AbcGeom::TYPE gp(parent, name, true, myScope, 1, time, md); \
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
    MetaData                md;
    exint                   array_size = 1;
    exint                   time = 0;
    bool                    valid = false;

    UT_ASSERT(myLayerType == GABC_LayerOptions::LayerType::PRUNE
	|| myLayerType == GABC_LayerOptions::LayerType::FULL);
    GABC_LayerOptions::getMetadata(md, myLayerType);

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
                    UT_ASSERT(myStorage == GT_STORE_INT16);
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
                    UT_ASSERT(myStorage == GT_STORE_INT16);
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
                    DECL_REAL_HI(ON3fGeomParam, ON3dGeomParam);
                    myTupleSize = 3;    // Clamp to 3
                }
                else if (myTupleSize == 2)
                {
                    DECL_REAL_HI(ON2fGeomParam, ON2dGeomParam);
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
                // Maya only supports a very specific data type as their colors.
                // Additionally, in order for Maya to recognize the colors, the
                // property needs to have the "mayaColorSet" metadata defined.
                // "1" means the "active" color. Since Houdini doesn't have the
                // concept of "active" color, we'll just leave it as "0".
                if(myScope == Alembic::AbcGeom::kFacevaryingScope)
                    md.set("mayaColorSet", "0");

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
                    DECL_REAL_HI(OM44fGeomParam, OM44dGeomParam);
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
                    UT_ASSERT(myStorage == GT_STORE_INT16);
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
        UT_String attrib_name(name);

        switch (pod)
        {
            case Alembic::Util::kUnknownPOD:
                switch (myStorage)
                {
                    case GT_STORE_UINT8:
                        DECL_PARAM(OUcharGeomParam);
                        break;

                    case GT_STORE_INT8:
                        DECL_PARAM(OCharGeomParam);
                        break;

                    case GT_STORE_INT16:
                        DECL_PARAM(OInt16GeomParam);
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
                        if (myTupleSize > 1
                                && attrib_name.multiMatch(options.uvAttribPattern()))
                        {
                            // In Houdini, additional UV attributes can be created with the data type of 2f,
                            // where arrayExtent = 2 and podExtent = 1. They are NOT interpreted as vectors.
                            // However, while exporting Alembic files to a third party tool, such as Maya, 
                            // UV attributes must be interpreted as vectors with the data type of 2fv,
                            // where arrayExtent = 1 and podExtent = 2.
                            // See Bug #69971 for more information 
                            array_size = 1;
                            DECL_PARAM(OV2fGeomParam);
                            myTupleSize = 2; // Clamp to 2
                        }
                        else
                        {
                            DECL_PARAM(OFloatGeomParam);
                        }
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
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT8, OBoolGeomParam);
                break;

            case Alembic::Util::kInt8POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT8, OCharGeomParam);
                break;

            case Alembic::Util::kUint8POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_UINT8, OUcharGeomParam);
                break;

            case Alembic::Util::kInt16POD:
                DECL_IF_STORAGE_MATCHES(GT_STORE_INT16, OInt16GeomParam);
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
    if(myLayerType == GABC_LayerOptions::LayerType::PRUNE)
	return true;
    UT_ASSERT(myLayerType == GABC_LayerOptions::LayerType::FULL);

    if ((myPOD != pod)
            || !compatibleStorage(array->getStorage())
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
                    writeProperty<uint8>(myProperty, array, myTupleSize);
                    break;

                case GT_STORE_INT8:
                    writeProperty<int8>(myProperty, array, myTupleSize);
                    break;

                case GT_STORE_INT16:
                    writeProperty<int16>(myProperty, array, myTupleSize);
                    break;

                case GT_STORE_INT32:
                    writeProperty<int32>(myProperty, array, myTupleSize);
                    break;

                case GT_STORE_INT64:
                    writeProperty<int64>(myProperty, array, myTupleSize);
                    break;

                case GT_STORE_REAL16:
                    writeProperty<fpreal16>(myProperty, array, myTupleSize);
                    break;

                case GT_STORE_REAL32:
                    writeProperty<fpreal32>(myProperty, array, myTupleSize);
                    break;

                case GT_STORE_REAL64:
                    writeProperty<fpreal64>(myProperty, array, myTupleSize);
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
            writeProperty<int8>(myProperty, array, myTupleSize);
            break;

        case Alembic::Util::kInt16POD:
            writeProperty<int16>(myProperty, array, myTupleSize);
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
            writeProperty<uint8>(myProperty, array, myTupleSize);
            break;

        case Alembic::Util::kUint8POD:
            writeProperty<uint8>(myProperty, array, myTupleSize);
            break;

        case Alembic::Util::kInt32POD:
            writeProperty<int32>(myProperty, array, myTupleSize);
            break;

        case Alembic::Util::kInt64POD:
            writeProperty<int64>(myProperty, array, myTupleSize);
            break;

        case Alembic::Util::kUint64POD:
            writeProperty<int64>(myProperty, array, myTupleSize);
            break;

        case Alembic::Util::kFloat16POD:
            writeProperty<fpreal16>(myProperty, array, myTupleSize);
            break;

        case Alembic::Util::kFloat32POD:
            writeProperty<fpreal32>(myProperty, array, myTupleSize);
            break;

        case Alembic::Util::kFloat64POD:
            writeProperty<fpreal64>(myProperty, array, myTupleSize);
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
	if (myIndexProperty)
	    myIndexProperty.setFromPrevious();
        return true;
    }

    return false;
}
