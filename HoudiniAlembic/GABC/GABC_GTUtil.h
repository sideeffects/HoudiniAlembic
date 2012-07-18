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
 * NAME:	GABC_GTUtil.h ( GT Library, C++)
 *
 * COMMENTS:
 */

#ifndef __GABC_GTUtil__
#define __GABC_GTUtil__

#include "GABC_API.h"
#include "GABC_Util.h"
#include <GT/GT_Types.h>
#include <Alembic/AbcGeom/All.h>

namespace GABC_GTUtil
{
    /// Given an Alembic DataType, determine the corresponding GT_Storage
    static inline GT_Storage
    getGTStorage(const Alembic::AbcGeom::DataType &dtype)
    {
	switch (dtype.getPod())
	{
	    case Alembic::AbcGeom::kUint8POD:
	    case Alembic::AbcGeom::kInt8POD:
		return GT_STORE_UINT8;
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

	    case Alembic::AbcGeom::kBooleanPOD:
	    case Alembic::AbcGeom::kUint16POD:
	    case Alembic::AbcGeom::kInt16POD:
	    case Alembic::AbcGeom::kWstringPOD:
	    case Alembic::AbcGeom::kNumPlainOldDataTypes:
	    case Alembic::AbcGeom::kUnknownPOD:
		break;
	}
	return GT_STORE_INVALID;
    }

    /// Given a DataType, determine the correspondign tuple size
    static inline int
    getGTTupleSize(const Alembic::AbcGeom::DataType &dtype)
    {
	return dtype.getExtent();
    }

    /// Given a data type interpretation, determine the corresponding GT_Type
    /// information
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
	return GT_TYPE_NONE;
    }
};

#endif
