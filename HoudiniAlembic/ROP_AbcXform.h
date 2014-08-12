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

#ifndef __ROP_AbcXform__
#define __ROP_AbcXform__

#include <Alembic/Abc/Foundation.h>
#include <Alembic/AbcGeom/OXform.h>
#include <Alembic/AbcGeom/XformSample.h>

// Extension of standard Alembic OXformSchema. Stores matrix
// from most recent XformSample. This is used to calculate child bounds.
class ROP_AbcXformSchema : public Alembic::AbcGeom::OXformSchema
{
    typedef Alembic::Abc::M44d              M44d;

    typedef Alembic::Abc::Argument          Argument;

    typedef Alembic::AbcGeom::OXformSchema  OXformSchema;
    typedef Alembic::AbcGeom::XformSample   XformSample;

public:
    ROP_AbcXformSchema() {}

    template <class CPROP_PTR>
    ROP_AbcXformSchema( CPROP_PTR iParent,
            const std::string &iName,
            const Argument &iArg0 = Argument(),
            const Argument &iArg1 = Argument(),
            const Argument &iArg2 = Argument() )
        : OXformSchema(iParent, iName, iArg0, iArg1, iArg2)
    {}

    template <class CPROP_PTR>
    explicit ROP_AbcXformSchema( CPROP_PTR iParent,
            const Argument &iArg0 = Argument(),
            const Argument &iArg1 = Argument(),
            const Argument &iArg2 = Argument() )
        : OXformSchema(iParent, iArg0, iArg1, iArg2)
    {}

    M44d &  getRecentXform()
            {
                return myRecentXform;
            }
    void    set(XformSample &ioSamp)
            {
                myRecentXform = ioSamp.getMatrix();
                OXformSchema::set(ioSamp);
            }

private:
    M44d    myRecentXform;
};

typedef Alembic::Abc::OSchemaObject<ROP_AbcXformSchema> ROP_AbcXform;
typedef Alembic::Util::shared_ptr< ROP_AbcXform > ROP_AbcXformPtr;

#endif