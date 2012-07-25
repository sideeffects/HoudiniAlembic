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
 * NAME:	ROP_AbcProperty.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcProperty__
#define __ROP_AbcProperty__

#include <GT/GT_Handles.h>
#include <GT/GT_Types.h>
#include "ROP_AlembicInclude.h"

class ROP_AbcProperty
{
public:
     ROP_AbcProperty(Alembic::AbcGeom::GeometryScope scope);
    ~ROP_AbcProperty();

    bool	init(Alembic::Abc::OCompoundProperty &parent,
		    const char *name,
		    const GT_DataArrayHandle &array,
		    Alembic::AbcGeom::TimeSamplingPtr ts);

    /// Write a sample
    bool	writeSample(const GT_DataArrayHandle &array);

    /// Return the scope
    Alembic::AbcGeom::GeometryScope	getScope() const { return myScope; }

private:
    Alembic::Abc::OArrayProperty	myProperty;
    Alembic::AbcGeom::GeometryScope	myScope;
    GT_Storage				myStorage;
    GT_Size				myTupleSize;
};

#endif

