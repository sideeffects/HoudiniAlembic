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

#ifndef __GABC_OArrayProperty__
#define __GABC_OArrayProperty__

#include "GABC_API.h"
#include "GABC_Include.h"
#include "GABC_OProperty.h"
#include <Alembic/AbcGeom/All.h>
#include <GT/GT_Handles.h>
#include <GT/GT_Types.h>

namespace GABC_NAMESPACE
{

class GABC_OError;
class GABC_OOptions;

/// Class for exporting attribute and user property data from a
/// GT_DataArray to an OArrayProperty.
///
/// See GABC_OProperty for more details.
///
class GABC_API GABC_OArrayProperty : public GABC_OProperty
{
public:
    using OArrayProperty = Alembic::Abc::OArrayProperty;
    using OUInt32ArrayProperty = Alembic::Abc::OUInt32ArrayProperty;

    using GeometryScope = Alembic::AbcGeom::GeometryScope;

    using PlainOldDataType = Alembic::Util::PlainOldDataType;

    /// This constructor is used if we're creating a user property.
    GABC_OArrayProperty(GABC_LayerOptions::LayerType ltype)
	: GABC_OProperty(ltype)
        , myScope(Alembic::AbcGeom::kUnknownScope)
        , myIsGeomParam(false)
    {}
    /// This constructor is used if we're creating an attribute.
    explicit GABC_OArrayProperty(GABC_LayerOptions::LayerType ltype,
	GeometryScope scope)
	: GABC_OProperty(ltype)
        , myScope(scope)
        , myIsGeomParam(true)
    {}

    ~GABC_OArrayProperty() override {}

    /// See GABC_OProperty for information about methods.
    bool    start(OCompoundProperty &parent,
                    const char *name,
                    const GT_DataArrayHandle &array,
                    GABC_OError &err,
                    const GABC_OOptions &options,
                    const PlainOldDataType pod =
                        Alembic::Util::kUnknownPOD) override;
    bool    update(const GT_DataArrayHandle &array,
                    GABC_OError &err,
                    const GABC_OOptions &options,
                    const PlainOldDataType pod
                        = Alembic::Abc::kUnknownPOD) override;
    bool    updateFromPrevious() override;
    exint   getNumSamples() const override
            {
                return myProperty.getNumSamples();
            }

private:
    GeometryScope		 myScope;
    OArrayProperty		 myProperty;
    OUInt32ArrayProperty	 myIndexProperty;
    bool			 myIsGeomParam;
};

} // GABC_NAMESPACE

#endif
