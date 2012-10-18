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

#ifndef __GABC_GTArray__
#define __GABC_GTArray__

#include "GABC_API.h"
#include <GT/GT_DataArray.h>
#include "GABC_GTUtil.h"

/// GABC_GTArrayExtract takes a typed Alembic array and creates a GT data array.
class GABC_GTArrayExtract
{
public:
    // Types missing in TypedArraySample.h
    typedef Alembic::Abc::TypedArraySample<Alembic::Abc::Box2sTPTraits> Box2sArraySample;
    typedef boost::shared_ptr<Box2sArraySample>	  Box2sArraySamplePtr;
    typedef Alembic::Abc::TypedArraySample<Alembic::Abc::Box2iTPTraits> Box2iArraySample;
    typedef boost::shared_ptr<Box2iArraySample>	  Box2iArraySamplePtr;
    typedef Alembic::Abc::TypedArraySample<Alembic::Abc::Box2fTPTraits> Box2fArraySample;
    typedef boost::shared_ptr<Box2fArraySample>	  Box2fArraySamplePtr;
    typedef Alembic::Abc::TypedArraySample<Alembic::Abc::Box2dTPTraits> Box2dArraySample;
    typedef boost::shared_ptr<Box2dArraySample>	  Box2dArraySamplePtr;

    static GT_DataArrayHandle get(const Alembic::Abc::BoolArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::UcharArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::CharArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::UInt16ArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::Int16ArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::UInt32ArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::Int32ArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::UInt64ArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::Int64ArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::HalfArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::FloatArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::DoubleArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::StringArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::WstringArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::V2sArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::V2iArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::V2fArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::V2dArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::V3sArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::V3iArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::V3fArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::V3dArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::P2sArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::P2iArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::P2fArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::P2dArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::P3sArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::P3iArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::P3fArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::P3dArraySamplePtr &);
    static GT_DataArrayHandle get(const               Box2sArraySamplePtr &);
    static GT_DataArrayHandle get(const               Box2iArraySamplePtr &);
    static GT_DataArrayHandle get(const               Box2fArraySamplePtr &);
    static GT_DataArrayHandle get(const               Box2dArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::Box3sArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::Box3iArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::Box3fArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::Box3dArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::M33fArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::M33dArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::M44fArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::M44dArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::QuatfArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::QuatdArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::C3hArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::C3fArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::C3cArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::C4hArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::C4fArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::C4cArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::N2fArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::N2dArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::N3fArraySamplePtr &);
    static GT_DataArrayHandle get(const Alembic::Abc::N3dArraySamplePtr &);
};


/// Takes a TypedArraySamplePtr (i.e. V3fArraySamplePtr)
/// There are existing specializations for
///	- BoolArraySamplePtr
///	- UcharArraySamplePtr
///	- CharArraySamplePtr
///	- UInt32ArraySamplePtr
///	- Int32ArraySamplePtr
///	- UInt64ArraySamplePtr
///	- Int64ArraySamplePtr
///	- HalfArraySamplePtr
///	- FloatArraySamplePtr
///	- DoubleArraySamplePtr
///	- V2iArraySamplePtr
///	- V2fArraySamplePtr
///	- V2dArraySamplePtr
///	- V3iArraySamplePtr
///	- V3fArraySamplePtr
///	- V3dArraySamplePtr
///	- P2iArraySamplePtr
///	- P2fArraySamplePtr
///	- P2dArraySamplePtr
///	- P3iArraySamplePtr
///	- P3fArraySamplePtr
///	- P3dArraySamplePtr
///	- Box3iArraySamplePtr
///	- Box3fArraySamplePtr
///	- Box3dArraySamplePtr
///	- M33fArraySamplePtr
///	- M33dArraySamplePtr
///	- M44fArraySamplePtr
///	- M44dArraySamplePtr
///	- QuatfArraySamplePtr
///	- QuatdArraySamplePtr
///	- C3hArraySamplePtr
///	- C3fArraySamplePtr
///	- C3cArraySamplePtr
///	- C4hArraySamplePtr
///	- C4fArraySamplePtr
///	- C4cArraySamplePtr
///	- N2fArraySamplePtr
///	- N2dArraySamplePtr
///	- N3fArraySamplePtr
///	- N3dArraySamplePtr
template <typename T, typename POD_T>
class GABC_API GABC_GTNumericArray : public GT_DataArray
{
public:
    typedef GABC_GTNumericArray<T,POD_T>		this_type;
    typedef typename T::element_type::traits_type	TRAITS_T;
    GABC_GTNumericArray(const T &array)
	: myArray(array)
	, myData(reinterpret_cast<const POD_T *>(array->get()))
    {
	myStorage = GABC_GTUtil::getGTStorage(array->getDataType());
	myTupleSize = GABC_GTUtil::getGTTupleSize(array->getDataType());
	myType = GABC_GTUtil::getGTTypeInfo(
			T::element_type::traits_type::interpretation(),
			myTupleSize);
	mySize = myArray->size();
    }
    virtual ~GABC_GTNumericArray()
    {
    }

    /// Raw access to the data array
    const POD_T		*data() const { return myData; }

    /// @{
    /// Methods defined on GT_DataArray
    virtual const char	*className() const	{ return "GABC_GTNumericArray"; }
    virtual GT_Storage	getStorage() const	{ return myStorage; }
    virtual GT_Size	getTupleSize() const	{ return myTupleSize; }
    virtual GT_Size	entries() const		{ return mySize; }
    virtual GT_Type	getTypeInfo() const	{ return myType; }
    virtual int64	getMemoryUsage() const
			    { return sizeof(POD_T)*mySize*myTupleSize; }

    virtual const uint8		*get(GT_Offset off, uint8 *buf, int sz) const
				{
				    off = off * myTupleSize;
				    for (int i = 0; i < sz; ++i)
					buf[i] = myData[off+i];
				    return buf;
				}
    virtual const int32		*get(GT_Offset off, int32 *buf, int sz) const
				{
				    off = off * myTupleSize;
				    for (int i = 0; i < sz; ++i)
					buf[i] = myData[off+i];
				    return buf;
				}
    virtual const int64		*get(GT_Offset off, int64 *buf, int sz) const
				{
				    off = off * myTupleSize;
				    for (int i = 0; i < sz; ++i)
					buf[i] = myData[off+i];
				    return buf;
				}
    virtual const fpreal16	*get(GT_Offset off, fpreal16 *buf, int sz) const
				{
				    off = off * myTupleSize;
				    for (int i = 0; i < sz; ++i)
					buf[i] = myData[off+i];
				    return buf;
				}
    virtual const fpreal64	*get(GT_Offset off, fpreal64 *buf, int sz) const
				{
				    off = off * myTupleSize;
				    for (int i = 0; i < sz; ++i)
					buf[i] = myData[off+i];
				    return buf;
				}
    virtual const fpreal32	*get(GT_Offset off, fpreal32 *buf, int sz) const
				{
				    off = off * myTupleSize;
				    for (int i = 0; i < sz; ++i)
					buf[i] = myData[off+i];
				    return buf;
				}

    virtual uint8	getU8(GT_Offset offset, int index=0) const
			{
			    offset = offset * myTupleSize + index;
			    UT_ASSERT_P(offset>=0 && offset<mySize*myTupleSize);
			    return myData[offset];
			}
    virtual int32	getI32(GT_Offset offset, int index=0) const
			{
			    offset = offset * myTupleSize + index;
			    UT_ASSERT_P(offset>=0 && offset<mySize*myTupleSize);
			    return myData[offset];
			}
    virtual int64	getI64(GT_Offset offset, int index=0) const
			{
			    offset = offset * myTupleSize + index;
			    UT_ASSERT_P(offset>=0 && offset<mySize*myTupleSize);
			    return myData[offset];
			}
    virtual fpreal16	getF16(GT_Offset offset, int index=0) const
			{
			    offset = offset * myTupleSize + index;
			    UT_ASSERT_P(offset>=0 && offset<mySize*myTupleSize);
			    return myData[offset];
			}
    virtual fpreal32	getF32(GT_Offset offset, int index=0) const
			{
			    offset = offset * myTupleSize + index;
			    UT_ASSERT_P(offset>=0 && offset<mySize*myTupleSize);
			    return myData[offset];
			}
    virtual fpreal64	getF64(GT_Offset offset, int index=0) const
			{
			    offset = offset * myTupleSize + index;
			    UT_ASSERT_P(offset>=0 && offset<mySize*myTupleSize);
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
		     t_ABCFill(data, myStorage == GT_STORE_UINT8,
				   start, length, tsize, 1, stride);
		 }
    virtual void doFillArray(int32 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const
		 {
		     t_ABCFill(data, myStorage == GT_STORE_INT32,
			     start, length, tsize, 1, stride);
		 }
    virtual void doFillArray(int64 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const
		 {
		     t_ABCFill(data, myStorage == GT_STORE_INT64,
			     start, length, tsize, 1, stride);
		 }
    virtual void doFillArray(fpreal16 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const
		 {
		     t_ABCFill(data, myStorage == GT_STORE_REAL16,
			     start, length, tsize, 1, stride);
		 }
    virtual void doFillArray(fpreal32 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const
		 {
		     t_ABCFill(data, myStorage == GT_STORE_REAL32,
			     start, length, tsize, 1, stride);
		 }
    virtual void doFillArray(fpreal64 *data, GT_Offset start, GT_Size length,
			int tsize, int stride) const
		 {
		     t_ABCFill(data, myStorage == GT_STORE_REAL64,
			     start, length, tsize, 1, stride);
		 }
    /// @}

private:
    template <typename DEST_POD_T> inline void
    importTuple(GT_Offset idx, DEST_POD_T *data, GT_Size tsize) const
		{
		    if (tsize < 1)
			tsize = myTupleSize;
		    else
			tsize = SYSmin(tsize, myTupleSize);
		    const POD_T	*src = myData + idx*myTupleSize;
		    for (int i = 0; i < tsize; ++i)
			data[i] = src[i];
		}

    template <typename DEST_POD_T> inline void
    t_ABCFill(DEST_POD_T *dest, bool typematch, GT_Offset start, GT_Size length,
		    int tsize, int nrepeats, int stride) const
		{
		    if (tsize < 1)
			tsize = myTupleSize;
		    stride = SYSmax(stride, tsize);
		    int n = SYSmin(tsize, myTupleSize);
		    if (typematch && n == myTupleSize
				  && stride == myTupleSize && nrepeats==1)
		    {
			memcpy(dest, myData+start*myTupleSize,
				    length*n*sizeof(POD_T));
		    }
		    else
		    {
			const POD_T *src = myData+start*myTupleSize;
			for (GT_Offset i = 0; i < length; ++i,
						src += myTupleSize)
			{
			    for (GT_Offset r=0; r<nrepeats; ++r, dest += stride)
			    {
				for (int j = 0; j < n; ++j)
				    dest[j] = src[j];
			    }
			}
		    }
		}
    T		 myArray;	// Shared pointer to TypedArraySample
    const POD_T *myData;	// Actual sample data
    GT_Size	 mySize;	// Number of tuples in the data array
    GT_Storage	 myStorage;
    GT_Type	 myType;
    int		 myTupleSize;
};

typedef GABC_GTNumericArray<Alembic::AbcGeom::UcharArraySamplePtr,uint8>	GABC_GTUcharArray;
typedef GABC_GTNumericArray<Alembic::AbcGeom::UInt32ArraySamplePtr,int32>	GABC_GTUInt32Array;
typedef GABC_GTNumericArray<Alembic::AbcGeom::Int32ArraySamplePtr,int32>	GABC_GTInt32Array;
typedef GABC_GTNumericArray<Alembic::AbcGeom::UInt64ArraySamplePtr,int64>	GABC_GTUInt64Array;
typedef GABC_GTNumericArray<Alembic::AbcGeom::Int64ArraySamplePtr,int64>	GABC_GTInt64Array;
typedef GABC_GTNumericArray<Alembic::AbcGeom::HalfArraySamplePtr,fpreal16>	GABC_GTHalfArray;
typedef GABC_GTNumericArray<Alembic::AbcGeom::FloatArraySamplePtr,fpreal32>	GABC_GTFloatArray;
typedef GABC_GTNumericArray<Alembic::AbcGeom::DoubleArraySamplePtr,fpreal64>	GABC_GTDoubleArray;

// Unsupported types
//typedef GABC_GTNumericArray<Alembic::AbcGeom::BoolArraySamplePtr>	GABC_GTBoolArray;
//typedef GABC_GTNumericArray<Alembic::AbcGeom::CharArraySamplePtr>	GABC_GTCharArray;
//typedef GABC_GTNumericArray<Alembic::AbcGeom::UInt16ArraySamplePtr>	GABC_GTUInt16Array;
//typedef GABC_GTNumericArray<Alembic::AbcGeom::Int16ArraySamplePtr>	GABC_GTInt16Array;
//typedef GABC_GTNumericArray<Alembic::AbcGeom::WstringArraySamplePtr>	GABC_GTWstringArray;
//typedef GABC_GTNumericArray<Alembic::AbcGeom::StringArraySamplePtr>	GABC_GTStringArray;

#endif
