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
 * NAME:	GABC_GTArray.h ( GT Library, C++)
 *
 * COMMENTS:
 */

#ifndef __GABC_IGTArray__
#define __GABC_IGTArray__

#include "GABC_API.h"
#include <GT/GT_DataArray.h>
#include <GT/GT_DAIndexedString.h>
#include "GABC_IArray.h"

template <typename POD_T, GT_Storage STORE_T>
class GABC_API GABC_IGTArray : public GT_DataArray
{
public:
    GABC_IGTArray(const GABC_IArray &array)
	: myArray(array)
	, myData(static_cast<const POD_T *>(array.data()))
    {
    }
    virtual ~GABC_IGTArray()
    {
    }

    /// Raw access to the data array
    const POD_T		*data() const { return myData; }

    /// @{
    /// Methods defined on GT_DataArray
    virtual const char	*className() const	{ return "GABC_IGTArray"; }
    virtual GT_Storage	getStorage() const	{ return STORE_T; }
    virtual GT_Type	getTypeInfo() const	{ return myArray.gtType(); }
    virtual GT_Size	getTupleSize() const	{ return myArray.tupleSize(); }
    virtual GT_Size	entries() const		{ return myArray.entries(); }
    virtual int64	getMemoryUsage() const
			{
			    return sizeof(*this)
				+ sizeof(POD_T)*entries()*getTupleSize();
			}

    virtual const uint8		*get(GT_Offset off, uint8 *buf, int sz) const
				{
				    off = off * getTupleSize();
				    for (int i = 0; i < sz; ++i)
					buf[i] = myData[off+i];
				    return buf;
				}
    virtual const int32		*get(GT_Offset off, int32 *buf, int sz) const
				{
				    off = off * getTupleSize();
				    for (int i = 0; i < sz; ++i)
					buf[i] = myData[off+i];
				    return buf;
				}
    virtual const int64		*get(GT_Offset off, int64 *buf, int sz) const
				{
				    off = off * getTupleSize();
				    for (int i = 0; i < sz; ++i)
					buf[i] = myData[off+i];
				    return buf;
				}
    virtual const fpreal16	*get(GT_Offset off, fpreal16 *buf, int sz) const
				{
				    off = off * getTupleSize();
				    for (int i = 0; i < sz; ++i)
					buf[i] = myData[off+i];
				    return buf;
				}
    virtual const fpreal64	*get(GT_Offset off, fpreal64 *buf, int sz) const
				{
				    off = off * getTupleSize();
				    for (int i = 0; i < sz; ++i)
					buf[i] = myData[off+i];
				    return buf;
				}
    virtual const fpreal32	*get(GT_Offset off, fpreal32 *buf, int sz) const
				{
				    off = off * getTupleSize();
				    for (int i = 0; i < sz; ++i)
					buf[i] = myData[off+i];
				    return buf;
				}

    virtual uint8	getU8(GT_Offset offset, int index=0) const
			{
			    offset = offset * getTupleSize() + index;
			    UT_ASSERT_P(offset>=0
				    && offset<entries()*getTupleSize());
			    return myData[offset];
			}
    virtual int32	getI32(GT_Offset offset, int index=0) const
			{
			    offset = offset * getTupleSize() + index;
			    UT_ASSERT_P(offset>=0
				    && offset<entries()*getTupleSize());
			    return myData[offset];
			}
    virtual int64	getI64(GT_Offset offset, int index=0) const
			{
			    offset = offset * getTupleSize() + index;
			    UT_ASSERT_P(offset>=0
				    && offset<entries()*getTupleSize());
			    return myData[offset];
			}
    virtual fpreal16	getF16(GT_Offset offset, int index=0) const
			{
			    offset = offset * getTupleSize() + index;
			    UT_ASSERT_P(offset>=0
				    && offset<entries()*getTupleSize());
			    return myData[offset];
			}
    virtual fpreal32	getF32(GT_Offset offset, int index=0) const
			{
			    offset = offset * getTupleSize() + index;
			    UT_ASSERT_P(offset>=0
				    && offset<entries()*getTupleSize());
			    return myData[offset];
			}
    virtual fpreal64	getF64(GT_Offset offset, int index=0) const
			{
			    offset = offset * getTupleSize() + index;
			    UT_ASSERT_P(offset>=0
				    && offset<entries()*getTupleSize());
			    return myData[offset];
			}
    virtual GT_String	getS(GT_Offset, int) const		{ return NULL; }
    virtual GT_Size	getStringIndexCount() const		{ return -1; }
    virtual GT_Offset	getStringIndex(GT_Offset, int) const	{ return -1; }
    virtual void	getIndexedStrings(UT_StringArray &,
				    UT_IntArray &) const {}

    virtual const uint8		*getU8Array(GT_DataArrayHandle &buffer) const
		{
		    if (SYSisSame<POD_T, uint8>())
			return reinterpret_cast<const uint8 *>(data());
		    return GT_DataArray::getU8Array(buffer);
		}
    virtual const int32		*getI32Array(GT_DataArrayHandle &buffer) const
		{
		    if (SYSisSame<POD_T, int32>())
			return reinterpret_cast<const int32 *>(data());
		    return GT_DataArray::getI32Array(buffer);
		}
    virtual const int64		*getI64Array(GT_DataArrayHandle &buffer) const
		{
		    if (SYSisSame<POD_T, int64>())
			return reinterpret_cast<const int64 *>(data());
		    return GT_DataArray::getI64Array(buffer);
		}
    virtual const fpreal16	*getF16Array(GT_DataArrayHandle &buffer) const
		{
		    if (SYSisSame<POD_T, fpreal16>())
			return reinterpret_cast<const fpreal16 *>(data());
		    return GT_DataArray::getF16Array(buffer);
		}
    virtual const fpreal32	*getF32Array(GT_DataArrayHandle &buffer) const
		{
		    if (SYSisSame<POD_T, fpreal32>())
			return reinterpret_cast<const fpreal32 *>(data());
		    return GT_DataArray::getF32Array(buffer);
		}
    virtual const fpreal64	*getF64Array(GT_DataArrayHandle &buffer) const
		{
		    if (SYSisSame<POD_T, fpreal64>())
			return reinterpret_cast<const fpreal64 *>(data());
		    return GT_DataArray::getF64Array(buffer);
		}

    virtual void doImport(GT_Offset idx, uint8 *data, GT_Size size) const
			{ importTuple<uint8>(idx, data, size); }
    virtual void doImport(GT_Offset idx, int32 *data, GT_Size size) const
			{ importTuple<int32>(idx, data, size); }
    virtual void doImport(GT_Offset idx, int64 *data, GT_Size size) const
			{ importTuple<int64>(idx, data, size); }
    virtual void doImport(GT_Offset idx, fpreal16 *data, GT_Size size) const
			{ importTuple<fpreal16>(idx, data, size); }
    virtual void doImport(GT_Offset idx, fpreal32 *data, GT_Size size) const
			{ importTuple<fpreal32>(idx, data, size); }
    virtual void doImport(GT_Offset idx, fpreal64 *data, GT_Size size) const
			{ importTuple<fpreal64>(idx, data, size); }

    virtual void doFillArray(uint8 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const
		 {
		     t_ABCFill(data, getStorage() == GT_STORE_UINT8,
				   start, length, tsize, 1, stride);
		 }
    virtual void doFillArray(int32 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const
		 {
		     t_ABCFill(data, getStorage() == GT_STORE_INT32,
			     start, length, tsize, 1, stride);
		 }
    virtual void doFillArray(int64 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const
		 {
		     t_ABCFill(data, getStorage() == GT_STORE_INT64,
			     start, length, tsize, 1, stride);
		 }
    virtual void doFillArray(fpreal16 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const
		 {
		     t_ABCFill(data, getStorage() == GT_STORE_REAL16,
			     start, length, tsize, 1, stride);
		 }
    virtual void doFillArray(fpreal32 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const
		 {
		     t_ABCFill(data, getStorage() == GT_STORE_REAL32,
			     start, length, tsize, 1, stride);
		 }
    virtual void doFillArray(fpreal64 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const
		 {
		     t_ABCFill(data, getStorage() == GT_STORE_REAL64,
			     start, length, tsize, 1, stride);
		 }
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

    template <typename DEST_POD_T> inline void
    t_ABCFill(DEST_POD_T *dest, bool typematch, GT_Offset start, GT_Size length,
		    int tsize, int nrepeats, int stride) const
		{
		    if (tsize < 1)
			tsize = getTupleSize();
		    stride = SYSmax(stride, tsize);
		    int n = SYSmin(tsize, getTupleSize());
		    if (typematch && n == getTupleSize()
				  && stride == getTupleSize() && nrepeats==1)
		    {
			memcpy(dest, myData+start*getTupleSize(),
				    length*n*sizeof(POD_T));
		    }
		    else
		    {
			const POD_T *src = myData+start*getTupleSize();
			for (GT_Offset i = 0; i < length; ++i,
						src += getTupleSize())
			{
			    for (GT_Offset r=0; r<nrepeats; ++r, dest += stride)
			    {
				for (int j = 0; j < n; ++j)
				    dest[j] = src[j];
			    }
			}
		    }
		}
    GABC_IArray	 myArray;	// Shared pointer to TypedArraySample
    const POD_T *myData;	// Actual sample data
};

class GABC_IGTStringArray : public GT_DAIndexedString
{
public:
    GABC_IGTStringArray(const GABC_IArray &array);
    virtual ~GABC_IGTStringArray()
    {
    }
private:
};

typedef GABC_IGTArray<uint8, GT_STORE_UINT8>		GABC_GTUnsigned8Array;
typedef GABC_IGTArray<int32, GT_STORE_INT32>		GABC_GTInt32Array;
typedef GABC_IGTArray<int64, GT_STORE_INT64>		GABC_GTInt64Array;
typedef GABC_IGTArray<fpreal16, GT_STORE_REAL16>	GABC_GTReal16Array;
typedef GABC_IGTArray<fpreal32, GT_STORE_REAL32>	GABC_GTReal32Array;
typedef GABC_IGTArray<fpreal64, GT_STORE_REAL64>	GABC_GTReal64Array;

GABC_API extern GT_DataArrayHandle GABCarray(const GABC_IArray &iarray);
#if 0
GABC_API extern GT_DataArrayhandle GABCarray(
				const Alembic::Abc::IScalarProperty &prop,
				const Alembic::Abc::ISampleSelector &iss);
#endif

#endif
