/*
 * Copyright (c) 2016
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

/// This class is an extension of the base Alembic OXformSchema. This class
/// allows an XformSample to be stored for a frame without writing the data
/// from the sample. This means that the sample information can be modified
/// if necessary before being output to the archive.
///
/// This is important for the "Build Hierarchy From Attribute" functionality.
/// To make sure that roundtripping an Alembic archive does not add a
/// transform each time, the transform of the first encountered packed Alembic
/// is merged with the transform of it's parent. Thus when processing an Xform
/// object, we want to record it's sample, but not write it immediately; we may
/// need to modify it later.
class GABC_OXformSchema : public Alembic::AbcGeom::OXformSchema
{
public:
    typedef Alembic::Abc::Argument          Argument;

    typedef Alembic::Abc::M44d              M44d;

    typedef Alembic::AbcGeom::OXformSchema  OXformSchema;
    typedef Alembic::AbcGeom::XformSample   XformSample;

    GABC_OXformSchema() {}

    /// Copies of the base constructors.
    template <class CPROP_PTR>
    GABC_OXformSchema( CPROP_PTR iParent,
            const std::string &iName,
            const Argument &iArg0 = Argument(),
            const Argument &iArg1 = Argument(),
            const Argument &iArg2 = Argument() )
        : OXformSchema(iParent, iName, iArg0, iArg1, iArg2)
        , myXformSet(false)
    {
        myNextXform.identity();
    }
    template <class CPROP_PTR>
    explicit GABC_OXformSchema( CPROP_PTR iParent,
            const Argument &iArg0 = Argument(),
            const Argument &iArg1 = Argument(),
            const Argument &iArg2 = Argument() )
        : OXformSchema(iParent, iArg0, iArg1, iArg2)
        , myXformSet(false)
    {
        myNextXform.identity();
    }


    /// Record an XformSample to output.
    void                set(XformSample &ioSamp)
                        {
                            myNextSample = ioSamp;
                        }
    /// Records a matrix to output with a sample.
    void                setMatrix(const UT_Matrix4D &mat)
                        {
                            myNextXform = mat;
                            myXformSet = true;
                        }
    /// Return the currently set matrix, if there is one.
    const UT_Matrix4D  &getNextXform()
                        {
			    return myNextXform;
                        }
    /// Write out the sample and matrix data.
    void                finalize()
                        {
                            if (myNextSample.getNumOps() == 0)
                            {
                                myNextSample.setMatrix(GABC_Util::getM(myNextXform));
                            }
                            OXformSchema::set(myNextSample);

                            myNextSample.reset();
                            myXformSet = false;
                        }

    // No data will be written to Alembic until finalize() is called. For this
    // reason, finalize() must be called exactly once each frame. The logic in
    // ROP_AbcGTShape is careful to make sure that no Xform has finalize called
    // more than once per frame. The logic in ROP_AbCSOP calls finalize on any
    // Xforms that have not been finalized yet at the end of frame output.
    //
    // There are 4 use cases for this class:
    //
    // CASE 1: You have a sample that you're certain is complete.
    //
    //      GABC_OXform     myXform;
    //      XformSample     mySample;
    //
    //      mySample.setInheritsXforms(...);
    //      mySample.setTranslation(...);
    //      mySample.setRotation(...);
    //      mySample.setScale(...);
    //      mySample.setMatrix(...);
    //      ...
    //
    //      myXform.set(mySample);
    //      myXform.finalize();
    //
    // CASE 2: You have a sample, but the transformation may change later.
    //
    //      GABC_OXform     myXform;
    //      UT_Matrix4D     myMatrix;
    //      XformSample     mySample;
    //
    //      mySample.setInheritsXforms(...);
    //      myMatrix = ...;
    //      myXform.set(mySample);
    //      myXform.setMatrix(myMatrix);
    //      ...
    //
    //      myMatrix = ...;
    //      myXform.setMatrix(myMatrix);
    //      myXform.finalize();
    //
    // CASE 3: You have a transformation you want to use with the default
    //          XformSample. It may or may not change later.
    //
    //      GABC_OXform     myXform;
    //      UT_Matrix4D     myMatrix;
    //
    //      myMatrix = ...;
    //      myXform.setMatrix(myMatrix);
    //      ...
    //
    //      myMatrix = ...;
    //      myXform.setMatrix(myMatrix);
    //      myXform.finalize();
    //
    // CASE 4: You want to output a default XformSample with an identity
    //          transform.
    //
    //      GABC_OXform     myXform;
    //
    //      myOxform.finalize();
    //

private:
    UT_Matrix4D myNextXform;
    XformSample myNextSample;
    bool        myXformSet;
};

typedef Alembic::Abc::OSchemaObject<GABC_OXformSchema> GABC_OXform;
typedef Alembic::Util::shared_ptr< GABC_OXform > GABC_OXformPtr;

}

#endif
