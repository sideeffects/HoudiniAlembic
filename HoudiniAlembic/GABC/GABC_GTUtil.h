/*
 * Copyright (c) 2015
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

#ifndef __GABC_GTUtil__
#define __GABC_GTUtil__

#include "GABC_API.h"
#include "GABC_Util.h"
#include <GT/GT_Types.h>
#include <Alembic/AbcGeom/All.h>

namespace GABC_NAMESPACE
{

namespace GABC_GTUtil
{
    /// Given an Alembic DataType, determine the corresponding GT_Storage.
    static inline GT_Storage
    getGTStorage(const Alembic::AbcGeom::DataType &dtype)
    {
	switch (dtype.getPod())
	{
	    case Alembic::AbcGeom::kBooleanPOD:
	    case Alembic::AbcGeom::kUint8POD:
	    case Alembic::AbcGeom::kInt8POD:
		return GT_STORE_UINT8;
	    case Alembic::AbcGeom::kUint16POD:
	    case Alembic::AbcGeom::kInt16POD:
	    case Alembic::AbcGeom::kUint32POD:
	    case Alembic::AbcGeom::kInt32POD:
		return GT_STORE_INT32;
	    case Alembic::AbcGeom::kUint64POD:
	    case Alembic::AbcGeom::kInt64POD:
		return GT_STORE_INT64;
	    case Alembic::AbcGeom::kFloat16POD:
		return GT_STORE_REAL16;
	    case Alembic::AbcGeom::kFloat32POD:
		return GT_STORE_REAL32;
	    case Alembic::AbcGeom::kFloat64POD:
		return GT_STORE_REAL64;
	    case Alembic::AbcGeom::kStringPOD:
		return GT_STORE_STRING;

	    case Alembic::AbcGeom::kWstringPOD:
	    case Alembic::AbcGeom::kNumPlainOldDataTypes:
	    case Alembic::AbcGeom::kUnknownPOD:
		break;
	}
	return GT_STORE_INVALID;
    }

    /// Given an Alembic data type interpretation, determine the corresponding
    /// GT_Type.
    static inline GT_Type
    getGTTypeInfo(const char *interp, int tsize)
    {
	if (!strcmp(interp, "point"))
	    return GT_TYPE_POINT;
	if (!strcmp(interp, "vector"))
	    return GT_TYPE_VECTOR;
	if (!strcmp(interp, "matrix"))
	    return tsize == 9 ? GT_TYPE_MATRIX3 : GT_TYPE_MATRIX;
	if (!strcmp(interp, "normal"))
	    return GT_TYPE_NORMAL;
	if (!strcmp(interp, "quat"))
	    return GT_TYPE_QUATERNION;
	if (!strcmp(interp, "rgb") || !strcmp(interp, "rgba"))
	    return GT_TYPE_COLOR;
	if (!strcmp(interp, "box"))
	    return tsize == 4 ? GT_TYPE_BOX2 : GT_TYPE_BOX;
	return GT_TYPE_NONE;
    }

}   // end GABC_GTUtil

}   // end GABC_NAMESPACE

#endif
