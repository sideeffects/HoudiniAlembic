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

#ifndef __GABC_OProperty__
#define __GABC_OProperty__

#include "GABC_API.h"
#include "GABC_Include.h"
#include <GT/GT_Handles.h>
#include <GT/GT_Types.h>
#include <Alembic/AbcGeom/All.h>

namespace GABC_NAMESPACE
{

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
}

#endif
