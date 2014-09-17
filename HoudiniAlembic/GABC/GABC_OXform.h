/*
 * Copyright (c) 2014
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

#ifndef __GABC_OXform__
#define __GABC_OXform__

#include "GABC_Util.h"
#include <Alembic/Abc/Foundation.h>
#include <Alembic/AbcGeom/OXform.h>
#include <Alembic/AbcGeom/XformSample.h>
#include <UT/UT_Matrix4.h>

namespace GABC_NAMESPACE
{

// Extension of the base OXformSchema. Needed to be able to set the matrix
// of a XformSample for a frame without writing the data (can't call
// setMatrix() multiple times for an XformSample).
class GABC_OXformSchema : public Alembic::AbcGeom::OXformSchema
{
    typedef Alembic::Abc::Argument          Argument;

    typedef Alembic::Abc::M44d              M44d;

    typedef Alembic::AbcGeom::OXformSchema  OXformSchema;
    typedef Alembic::AbcGeom::XformSample   XformSample;

public:
    GABC_OXformSchema() {}

    template <class CPROP_PTR>
    GABC_OXformSchema( CPROP_PTR iParent,
            const std::string &iName,
            const Argument &iArg0 = Argument(),
            const Argument &iArg1 = Argument(),
            const Argument &iArg2 = Argument() )
        : OXformSchema(iParent, iName, iArg0, iArg1, iArg2)
    {
        myNextXform.identity();
    }

    template <class CPROP_PTR>
    explicit GABC_OXformSchema( CPROP_PTR iParent,
            const Argument &iArg0 = Argument(),
            const Argument &iArg1 = Argument(),
            const Argument &iArg2 = Argument() )
        : OXformSchema(iParent, iArg0, iArg1, iArg2)
    {
        myNextXform.identity();
    }

    // Set() now records the XformSample that should be used for the next frame
    // sample. SetMatrix() records the transformation matrix that the sample
    // should use. Call finalize() to write out the recorded sample and matrix.
    const UT_Matrix4D  &getNextXform()
                        {
                            return myNextXform;
                        }
    void                set(XformSample &ioSamp)
                        {
                            myNextSample = ioSamp;
                        }
    void                setMatrix(UT_Matrix4D &mat)
                        {
                            myNextXform = mat;
                        }
    void                finalize()
                        {
                            myNextSample.setMatrix(GABC_Util::getM(myNextXform));
                            OXformSchema::set(myNextSample);

                            myNextSample.reset();
                            myNextXform.identity();
                        }

private:
    UT_Matrix4D myNextXform;
    XformSample myNextSample;
};

typedef Alembic::Abc::OSchemaObject<GABC_OXformSchema> GABC_OXform;
typedef Alembic::Util::shared_ptr< GABC_OXform > GABC_OXformPtr;

}

#endif