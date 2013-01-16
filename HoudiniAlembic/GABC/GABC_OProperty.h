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
 * NAME:	GABC_OProperty.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#ifndef __GABC_OProperty__
#define __GABC_OProperty__

#include "GABC_API.h"
#include "GABC_Include.h"
#include <GT/GT_Handles.h>
#include <GT/GT_Types.h>
#include <Alembic/AbcGeom/All.h>

class GABC_OOptions;

/// Class to store a generic GT data array in an Alembic compound property
class GABC_API GABC_OProperty
{
public:
    typedef Alembic::Abc::OCompoundProperty	OCompoundProperty;
    typedef Alembic::Abc::OArrayProperty	OArrayProperty;

     GABC_OProperty(Alembic::AbcGeom::GeometryScope scope);
    ~GABC_OProperty();

    /// Add the data array to the compound property
    bool	start(OCompoundProperty &parent,
		    const char *name,
		    const GT_DataArrayHandle &array,
		    const GABC_OOptions &options);

    /// Write the next sample to the properties.
    bool	update(const GT_DataArrayHandle &array,
		    const GABC_OOptions &options);

    /// @{
    /// Accessors
    Alembic::AbcGeom::GeometryScope	scope() const { return myScope; }
    GT_Storage				storage() const { return myStorage; }
    GT_Size				tupleSize() const { return myTupleSize; }
    /// @}

private:
    OArrayProperty			myProperty;
    Alembic::AbcGeom::GeometryScope	myScope;
    GT_DataArrayHandle			myCache;
    GT_Storage				myStorage;
    GT_Size				myTupleSize;
};

#endif
