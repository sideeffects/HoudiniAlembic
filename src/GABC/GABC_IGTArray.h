/*
 * Copyright (c) 2020
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

#ifndef __GABC_IGTArray__
#define __GABC_IGTArray__

#include "GABC_API.h"
#include <GT/GT_DataArray.h>
#include <GT/GT_DAIndexedString.h>
#include "GABC_IArray.h"

namespace GABC_NAMESPACE
{
template <typename POD_T>
class GABC_API GABC_IGTArray : public GT_DataArray
{
public:
    GABC_IGTArray(const GABC_IArray &array)
	: GT_DataArray()
	, myArray(array)
	, myData(static_cast<const POD_T *>(array.data()))
    {
	if (array.isConstant())
	    setDataId(1);
    }
    ~GABC_IGTArray() override
    {
    }

    /// Raw access to the data array
    const POD_T		*data() const { return myData; }

    /// @{
    /// Methods defined on GT_DataArray
    const char          *className() const override
                            { return "GABC_IGTArray"; }
    GT_Storage          getStorage() const override
                            { return GTstorage<POD_T>(); }
    GT_Type             getTypeInfo() const override
                            { return myArray.gtType(); }
    GT_Size             getTupleSize() const override
                            { return myArray.tupleSize(); }
    GT_Size             entries() const override
                            { return myArray.entries(); }
    int64               getMemoryUsage() const override
			{
			    return sizeof(*this)
				+ sizeof(POD_T)*entries()*getTupleSize();
			}

    const uint8         *get(GT_Offset off, uint8 *buf, int sz) const override
			{
			    off = off * getTupleSize();
			    for (int i = 0; i < sz; ++i)
				buf[i] = myData[off+i];
			    return buf;
			}
    const int8          *get(GT_Offset off, int8 *buf, int sz) const override
			{
			    off = off * getTupleSize();
			    for (int i = 0; i < sz; ++i)
				buf[i] = myData[off+i];
			    return buf;
			}
    const int16         *get(GT_Offset off, int16 *buf, int sz) const override
			{
			    off = off * getTupleSize();
			    for (int i = 0; i < sz; ++i)
				buf[i] = myData[off+i];
			    return buf;
			}
    const int32         *get(GT_Offset off, int32 *buf, int sz) const override
			{
			    off = off * getTupleSize();
			    for (int i = 0; i < sz; ++i)
				buf[i] = myData[off+i];
			    return buf;
			}
    const int64         *get(GT_Offset off, int64 *buf, int sz) const override
			{
			    off = off * getTupleSize();
			    for (int i = 0; i < sz; ++i)
				buf[i] = myData[off+i];
			    return buf;
			}
    const fpreal16      *get(GT_Offset off, fpreal16 *buf, int sz) const override
			{
			    off = off * getTupleSize();
			    for (int i = 0; i < sz; ++i)
				buf[i] = myData[off+i];
			    return buf;
			}
    const fpreal64      *get(GT_Offset off, fpreal64 *buf, int sz) const override
			{
			    off = off * getTupleSize();
			    for (int i = 0; i < sz; ++i)
				buf[i] = myData[off+i];
			    return buf;
			}
    const fpreal32      *get(GT_Offset off,
                             fpreal32 *buf, int sz) const override
			{
			    off = off * getTupleSize();
			    for (int i = 0; i < sz; ++i)
				buf[i] = myData[off+i];
			    return buf;
			}

    uint8               getU8(GT_Offset offset, int index=0) const override
			{
			    offset = offset * getTupleSize() + index;
			    UT_ASSERT_P(offset>=0
				    && offset<entries()*getTupleSize());
			    return myData[offset];
			}
    int32               getI32(GT_Offset offset, int index=0) const override
			{
			    offset = offset * getTupleSize() + index;
			    UT_ASSERT_P(offset>=0
				    && offset<entries()*getTupleSize());
			    return myData[offset];
			}
    int64               getI64(GT_Offset offset, int index=0) const override
			{
			    offset = offset * getTupleSize() + index;
			    UT_ASSERT_P(offset>=0
				    && offset<entries()*getTupleSize());
			    return myData[offset];
			}
    fpreal16            getF16(GT_Offset offset, int index=0) const override
			{
			    offset = offset * getTupleSize() + index;
			    UT_ASSERT_P(offset>=0
				    && offset<entries()*getTupleSize());
			    return myData[offset];
			}
    fpreal32            getF32(GT_Offset offset, int index=0) const override
			{
			    offset = offset * getTupleSize() + index;
			    UT_ASSERT_P(offset>=0
				    && offset<entries()*getTupleSize());
			    return myData[offset];
			}
    fpreal64            getF64(GT_Offset offset, int index=0) const override
			{
			    offset = offset * getTupleSize() + index;
			    UT_ASSERT_P(offset>=0
				    && offset<entries()*getTupleSize());
			    return myData[offset];
			}
    GT_String           getS(GT_Offset, int) const override
			    { return UT_StringHolder::theEmptyString; }
    GT_Size             getStringIndexCount() const override
                            { return -1; }
    GT_Offset           getStringIndex(GT_Offset, int) const override
                            { return -1; }
    void                getIndexedStrings(UT_StringArray &,
				    UT_IntArray &) const override {}

    const uint8         *getU8Array(GT_DataArrayHandle &buffer) const override
		{
		    if (SYSisSame<POD_T, uint8>())
			return reinterpret_cast<const uint8 *>(data());
		    return GT_DataArray::getU8Array(buffer);
		}
    const int32         *getI32Array(GT_DataArrayHandle &buffer) const override
		{
		    if (SYSisSame<POD_T, int32>())
			return reinterpret_cast<const int32 *>(data());
		    return GT_DataArray::getI32Array(buffer);
		}
    const int64         *getI64Array(GT_DataArrayHandle &buffer) const override
		{
		    if (SYSisSame<POD_T, int64>())
			return reinterpret_cast<const int64 *>(data());
		    return GT_DataArray::getI64Array(buffer);
		}
    const fpreal16      *getF16Array(GT_DataArrayHandle &buffer) const override
		{
		    if (SYSisSame<POD_T, fpreal16>())
			return reinterpret_cast<const fpreal16 *>(data());
		    return GT_DataArray::getF16Array(buffer);
		}
    const fpreal32      *getF32Array(GT_DataArrayHandle &buffer) const override
		{
		    if (SYSisSame<POD_T, fpreal32>())
			return reinterpret_cast<const fpreal32 *>(data());
		    return GT_DataArray::getF32Array(buffer);
		}
    const fpreal64      *getF64Array(GT_DataArrayHandle &buffer) const override
		{
		    if (SYSisSame<POD_T, fpreal64>())
			return reinterpret_cast<const fpreal64 *>(data());
		    return GT_DataArray::getF64Array(buffer);
		}

    void    doImport(GT_Offset idx, uint8 *data, GT_Size size) const override
			{ importTuple<uint8>(idx, data, size); }
    void    doImport(GT_Offset idx, int8 *data, GT_Size size) const override
			{ importTuple<int8>(idx, data, size); }
    void    doImport(GT_Offset idx, int16 *data, GT_Size size) const override
			{ importTuple<int16>(idx, data, size); }
    void    doImport(GT_Offset idx, int32 *data, GT_Size size) const override
			{ importTuple<int32>(idx, data, size); }
    void    doImport(GT_Offset idx, int64 *data, GT_Size size) const override
			{ importTuple<int64>(idx, data, size); }
    void    doImport(GT_Offset idx, fpreal16 *data, GT_Size size) const override
			{ importTuple<fpreal16>(idx, data, size); }
    void    doImport(GT_Offset idx, fpreal32 *data, GT_Size size) const override
			{ importTuple<fpreal32>(idx, data, size); }
    void    doImport(GT_Offset idx, fpreal64 *data, GT_Size size) const override
			{ importTuple<fpreal64>(idx, data, size); }

    void    doFillArray(uint8 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const override
		 { t_ABCFill(data, start, length, tsize, stride); }
    void    doFillArray(int8 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const override
		 { t_ABCFill(data, start, length, tsize, stride); }
    void    doFillArray(int16 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const override
		 { t_ABCFill(data, start, length, tsize, stride); }
    void    doFillArray(int32 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const override
		 { t_ABCFill(data, start, length, tsize, stride); }
    void    doFillArray(int64 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const override
		 { t_ABCFill(data, start, length, tsize, stride); }
    void    doFillArray(fpreal16 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const override
		 { t_ABCFill(data, start, length, tsize, stride); }
    void    doFillArray(fpreal32 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const override
		 { t_ABCFill(data, start, length, tsize, stride); }
    void    doFillArray(fpreal64 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const override
		 { t_ABCFill(data, start, length, tsize, stride); }
    /// @}

private:
    template <typename DEST_POD_T> inline void
    importTuple(GT_Offset idx, DEST_POD_T *data, GT_Size tsize) const
		{
		    if (tsize < 1)
			tsize = getTupleSize();
		    else
			tsize = SYSmin(tsize, getTupleSize());
		    const POD_T	*src = myData + idx*getTupleSize();
		    for (int i = 0; i < tsize; ++i)
			data[i] = src[i];
		}

    inline void
    t_ABCFill(POD_T *dest, GT_Offset start, GT_Size length,
		    int tsize, int stride) const
		{
		    if (tsize < 1)
			tsize = getTupleSize();
		    stride = SYSmax(stride, tsize);
		    int n = SYSmin(tsize, getTupleSize());
		    if (n == getTupleSize() && stride == getTupleSize())
		    {
			memcpy(dest, myData+start*getTupleSize(),
				    length*n*sizeof(POD_T));
		    }
		    else
		    {
			const POD_T *src = myData+start*getTupleSize();
			for (GT_Offset i = 0; i < length; ++i,
					src += getTupleSize(), dest += stride)
			{
			    for (int j = 0; j < n; ++j)
				dest[j] = src[j];
			}
		    }
		}

    template <typename DEST_POD_T> inline void
    t_ABCFill(DEST_POD_T *dest, GT_Offset start, GT_Size length,
		    int tsize, int stride) const
		{
		    if (tsize < 1)
			tsize = getTupleSize();
		    stride = SYSmax(stride, tsize);
		    int n = SYSmin(tsize, getTupleSize());
		    const POD_T *src = myData+start*getTupleSize();
		    for (GT_Offset i = 0; i < length; ++i,
					src += getTupleSize(), dest += stride)
		    {
			for (int j = 0; j < n; ++j)
			    dest[j] = src[j];
		    }
		}

    GABC_IArray	 myArray;	// Shared pointer to TypedArraySample
    const POD_T *myData;	// Actual sample data
};

class GABC_IGTStringArray : public GT_DAIndexedString
{
public:
    GABC_IGTStringArray(const GABC_IArray &array);
    ~GABC_IGTStringArray() override
    {
    }
private:
};

using GABC_GTUnsigned8Array = GABC_IGTArray<uint8>;
using GABC_GTInt8Array = GABC_IGTArray<int8>;
using GABC_GTInt16Array = GABC_IGTArray<int16>;
using GABC_GTInt32Array = GABC_IGTArray<int32>;
using GABC_GTInt64Array = GABC_IGTArray<int64>;
using GABC_GTReal16Array = GABC_IGTArray<fpreal16>;
using GABC_GTReal32Array = GABC_IGTArray<fpreal32>;
using GABC_GTReal64Array = GABC_IGTArray<fpreal64>;

GABC_API extern GT_DataArrayHandle GABCarray(const GABC_IArray &iarray);
}

#endif
