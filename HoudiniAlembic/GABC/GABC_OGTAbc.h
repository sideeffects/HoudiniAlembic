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

#ifndef __GABC_OGTAbc__
#define __GABC_OGTAbc__

#include "GABC_API.h"
#include "GABC_Include.h"
#include "GABC_OOptions.h"
#include "GABC_OXform.h"
#include "GABC_Types.h"
#include <Alembic/AbcGeom/All.h>
#include <GT/GT_Handles.h>

namespace GABC_NAMESPACE
{

class GABC_API GABC_OGTAbc
{
public:
    // Properties
    typedef Alembic::Abc::OScalarProperty           OScalarProperty;
    typedef Alembic::Abc::OArrayProperty            OArrayProperty;
    typedef Alembic::Abc::OCompoundProperty         OCompoundProperty;

    // Output Objects
    typedef Alembic::AbcGeom::OObject               OObject;
    typedef Alembic::AbcGeom::OPolyMesh             OPolyMesh;
    typedef Alembic::AbcGeom::OSubD                 OSubD;
    typedef Alembic::AbcGeom::OPoints		    OPoints;
    typedef Alembic::AbcGeom::OCurves		    OCurves;
    typedef Alembic::AbcGeom::ONuPatch		    ONuPatch;
    typedef GABC_OXform                             OXform;

    // Visibility
    typedef Alembic::AbcGeom::ObjectVisibility      ObjectVisibility;
    typedef Alembic::AbcGeom::OVisibilityProperty   OVisibilityProperty;

    // PropertyMap class is used to store pointers to Alembic Property
    // objects. Ideally we would use a regular map, but these 3 classes
    // are templated and so is their common ancestor.
    class PropertyMap
    {
    public:
        typedef UT_Map<std::string, OScalarProperty *>      ScalarMap;
        typedef UT_Map<std::string, OArrayProperty *>       ArrayMap;
        typedef UT_Map<std::string, OCompoundProperty *>    CompoundMap;

        typedef std::pair<std::string, OScalarProperty *>   ScalarMapInsert;
        typedef std::pair<std::string, OArrayProperty *>    ArrayMapInsert;
        typedef std::pair<std::string, OCompoundProperty *> CompoundMapInsert;

        PropertyMap() {}
        ~PropertyMap()
        {
            clear();
        }

        void clear()
        {
            for (ScalarMap::iterator it = myScalarMap.begin();
                    it != myScalarMap.end();
                    ++it)
            {
                delete it->second;
            }
            myScalarMap.clear();

            for (ArrayMap::iterator it = myArrayMap.begin();
                    it != myArrayMap.end();
                    ++it)
            {
                delete it->second;
            }
            myArrayMap.clear();

            for (CompoundMap::iterator it = myCompoundMap.begin();
                    it != myCompoundMap.end();
                    ++it)
            {
                delete it->second;
            }
            myCompoundMap.clear();
        }

        void insert(const std::string &name, OScalarProperty *prop)
        {
            myScalarMap.insert(ScalarMapInsert(name, prop));
        }
        void insert(const std::string &name, OArrayProperty *prop)
        {
            myArrayMap.insert(ArrayMapInsert(name, prop));
        }
        void insert(const std::string &name, OCompoundProperty *prop)
        {
            myCompoundMap.insert(CompoundMapInsert(name, prop));
        }

        void setFromPrevious()
        {
            for (ScalarMap::iterator it = myScalarMap.begin(); it != myScalarMap.end(); ++it)
            {
                it->second->setFromPrevious();
            }
            for (ArrayMap::iterator it = myArrayMap.begin(); it != myArrayMap.end(); ++it)
            {
                it->second->setFromPrevious();
            }
        }

        OScalarProperty *findScalar(const std::string &name)
        {
            ScalarMap::iterator    it = myScalarMap.find(name);

            if (it == myScalarMap.end())
            {
                return NULL;
            }

            return it->second;
        }
        OArrayProperty *findArray(const std::string &name)
        {
            ArrayMap::iterator    it = myArrayMap.find(name);

            if (it == myArrayMap.end())
            {
                return NULL;
            }

            return it->second;
        }
        OCompoundProperty *findCompound(const std::string &name)
        {
            CompoundMap::iterator    it = myCompoundMap.find(name);

            if (it == myCompoundMap.end())
            {
                return NULL;
            }

            return it->second;
        }

    private:
        ScalarMap       myScalarMap;
        ArrayMap        myArrayMap;
        CompoundMap     myCompoundMap;
    };

    GABC_OGTAbc(const std::string &name);
    ~GABC_OGTAbc();

    bool    start(const GT_PrimitiveHandle &prim,
                    const OObject &parent,
                    fpreal cook_time,
                    const GABC_OOptions &ctx,
                    GABC_OError &err,
                    ObjectVisibility vis);
    bool    startXform(const GT_PrimitiveHandle &prim,
                    OXform *xform,
                    fpreal cook_time,
                    const GABC_OOptions &ctx,
                    GABC_OError &err,
                    ObjectVisibility vis);
    bool    update(const GT_PrimitiveHandle &prim,
                    fpreal cook_time,
                    const GABC_OOptions &ctx,
                    GABC_OError &err,
                    ObjectVisibility vis = Alembic::AbcGeom::kVisibilityDeferred);
    bool    updateFromPrevious(GABC_OError &err,
                    ObjectVisibility vis = Alembic::AbcGeom::kVisibilityHidden,
                    exint frames = 1);

private:
    void            clear();

    union {
    	OPolyMesh	   *myPolyMesh;
    	OSubD		   *mySubD;
    	OPoints		   *myPoints;
    	OCurves		   *myCurves;
    	ONuPatch	   *myNuPatch;
    	OXform             *myXform;
    	void		   *myVoidPtr;
    } myShape;

    GABC_NodeType           myType;
    OVisibilityProperty     myVisibility;
    PropertyMap             myArbProps;
    PropertyMap             myUserProps;
    std::string             myName;
};

} // GABC_NAMESPACE

#endif