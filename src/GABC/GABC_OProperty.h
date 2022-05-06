/*
 * Copyright (c) 2022
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

#ifndef __GABC_OProperty__
#define __GABC_OProperty__

#include "GABC_API.h"
#include "GABC_OError.h"
#include "GABC_LayerOptions.h"
#include <Alembic/AbcGeom/All.h>
#include <GT/GT_Handles.h>
#include <GT/GT_Types.h>

namespace GABC_NAMESPACE
{

class GABC_OOptions;

/// Base class for exporting attribute and user property data from a
/// GT_DataArray to Alembic.
///
/// The Alembic equivalent of attributes are Alembic arbitrary geometry
/// parameters (arbGeomParams). In addition, Alembic geometry can have user
/// properties which have no direct equivalent in Houdini. We store user
/// properties as a JSON dictionary exposed to users through a special
/// attribute (see: GABC_Util::theUserPropsValsAttrib).
///
/// User properties are stored using OScalarProperty and OArrayProperty
/// objects, while arbGeomParams are stored using OGeomParam objects which
/// contain an underlying OArrayProperty.
///
/// The children derived from this base class write data representing
/// attributes and user properties from GT_DataArray objects to the appropriate
/// type of OProperty.
///
class GABC_API GABC_OProperty
{
public:
    typedef Alembic::Abc::OCompoundProperty OCompoundProperty;
    typedef Alembic::Util::PlainOldDataType PlainOldDataType;

		    GABC_OProperty(GABC_LayerOptions::LayerType ltype)
			: myLayerType(ltype) {}
    virtual        ~GABC_OProperty() {}

    /// Creates the appropriate child object, based on the array's storage
    /// type, tuple size, and interpretation, and the Alembic POD (if provided)
    virtual bool    start(OCompoundProperty &parent,
                            const char *name,
                            const GT_DataArrayHandle &array,
                            GABC_OError &err,
                            const GABC_OOptions &options,
                            const PlainOldDataType pod
                                = Alembic::Abc::kUnknownPOD) = 0;

    /// Write GT_DataArray conents as samples to the property.
    virtual bool    update(const GT_DataArrayHandle &array,
                            GABC_OError &err,
		            const GABC_OOptions &options,
                            const PlainOldDataType pod
                                = Alembic::Abc::kUnknownPOD) = 0;

    /// Reuse the previous sample .
    virtual bool    updateFromPrevious() = 0;

    /// Get number of samples written to property so far.
    virtual exint   getNumSamples() const = 0;

    /// Returns true if storage only differs by precision.
    bool compatibleStorage(GT_Storage storage)
    {
	if(myStorage == storage)
	    return true;
	if(GTisFloat(myStorage))
	    return GTisFloat(storage);
	if(GTisInteger(myStorage))
	    return GTisInteger(storage);
	return false;
    }

protected:
    GT_DataArrayHandle		 myCache;
    GT_Size			 myTupleSize;
    GT_Storage			 myStorage;
    GT_Type			 myType;
    PlainOldDataType		 myPOD;
    void			*myBuffer;
    GABC_LayerOptions::LayerType myLayerType;
};

} // GABC_NAMESPACE

#endif
