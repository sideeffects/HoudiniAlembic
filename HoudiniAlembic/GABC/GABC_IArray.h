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

#ifndef __GABC_IArray__
#define __GABC_IArray__

#include "GABC_API.h"
#include "GABC_Include.h"
#include "GABC_IItem.h"
#include "GABC_IArchive.h"
#include "GABC_GTUtil.h"
#include <UT/UT_Assert.h>
#include <GT/GT_Types.h>
#include <Alembic/Abc/IArchive.h>

namespace GABC_NAMESPACE
{
/// This class wraps an Alembic data array and provides convenience methods
/// that allow thread-safe access to its data.
///
/// Do not grab and hold onto the contained data array as this may cause
/// referencing issues on archives.
class GABC_API GABC_IArray : public GABC_IItem
{
public:
    typedef Alembic::Abc::ArraySamplePtr	ArraySamplePtr;
    typedef Alembic::Abc::IArrayProperty	IArrayProperty;
    typedef Alembic::Abc::ISampleSelector	ISampleSelector;
    typedef Alembic::Abc::index_t		index_t;
    typedef Alembic::Abc::chrono_t		chrono_t;
    typedef Alembic::Abc::PlainOldDataType	PlainOldDataType;

    /// Create an array wrapper for the given array sample.
    /// The @c interp is the "intepretation", which is one of
    ///	 - "point"
    ///	 - "vector"
    ///	 - "matrix"
    ///	 - "normal"
    ///	 - "quat"
    /// (see Abc/TypedPropertyTraits.h)
    static GABC_IArray getSample(GABC_IArchive &arch,
		const ArraySamplePtr &sample, const char *interp,
		int array_extent);
    /// Create an array wrapper for the given array sample with the specified
    /// GT_Type as the interpretation.
    static GABC_IArray getSample(GABC_IArchive &arch,
		const ArraySamplePtr &sample, GT_Type type, int array_extent);

    static GABC_IArray getSample(GABC_IArchive &arch,
		const IArrayProperty &prop, index_t index,
		GT_Type override_type=GT_TYPE_NONE)
    {
	return getSample(arch, prop, ISampleSelector(index), override_type);
    }
    static GABC_IArray getSample(GABC_IArchive &arch,
		const IArrayProperty &prop, chrono_t time,
		GT_Type override_type=GT_TYPE_NONE)
    {
	return getSample(arch, prop, ISampleSelector(time), override_type);
    }
    // If a type is specified, the implicit array type will be overridden
    static GABC_IArray getSample(GABC_IArchive &arch,
		const IArrayProperty &prop, const ISampleSelector &iss,
		GT_Type override_type);

    GABC_IArray()
	: GABC_IItem()
	, myContainer()
	, mySize(0)
	, myTupleSize(0)
	, myType(GT_TYPE_NONE)
    {
    }
    GABC_IArray(const GABC_IArray &src)
	: GABC_IItem(src)
	, myContainer(src.myContainer)
	, mySize(src.mySize)
	, myTupleSize(src.myTupleSize)
	, myType(src.myType)
    {
    }
    GABC_IArray(GABC_IArchive &archive,
		const ArraySamplePtr &array,
		GT_Size size,
		int tuple_size,
		GT_Storage storage,
		GT_Type tinfo=GT_TYPE_NONE)
	: GABC_IItem(&archive)
	, myContainer(array)
	, mySize(size)
	, myTupleSize(tuple_size)
	, myType(tinfo)
    {
    }
    virtual ~GABC_IArray();

    GABC_IArray	&operator=(const GABC_IArray &src)
		{
		    GABC_IItem::operator=(src);
		    myContainer = src.myContainer;
		    mySize = src.mySize;
		    myTupleSize = src.myTupleSize;
		    myType = src.myType;
		    return *this;
		}

    bool	valid() const	{ return myContainer.valid(); }

    class Container
    {
    public:
	Container()
	    : myArray()
	{
	}
	Container(const ArraySamplePtr &array)
	    : myArray(array)
	{
	}
	bool		valid() const
			{
			    return myArray && myArray->valid();
			}
	Container	&operator=(const Container &src)
			{
			    myArray = src.myArray;
			    return *this;
			}
	const void	*data() const
			{
			    UT_ASSERT(myArray && myArray->getData());
			    return myArray->getData();
			}
	PlainOldDataType	abcType() const
			{
			    UT_ASSERT(myArray && myArray->getData());
			    return myArray->getDataType().getPod();
			}
    private:
	ArraySamplePtr	 myArray;
    };

    const void		*data() const		{ return myContainer.data(); }
    PlainOldDataType	 abcType() const	{ return myContainer.abcType(); }
    int			 tupleSize() const
			    { return myTupleSize; }
    GT_Size		 entries() const	{ return mySize; }
    GT_Type		 gtType() const		{ return myType; }

    /// {
    virtual void	purge();
    /// @}
private:
    Container		myContainer;
    GT_Size		mySize;
    int			myTupleSize;
    GT_Type		myType;
    PlainOldDataType	myAbcType;
};
}

#endif
