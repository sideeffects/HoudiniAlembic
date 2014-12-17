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

#ifndef __ROP_AbcGTCompoundShape__
#define __ROP_AbcGTCompoundShape__

#include "ROP_AbcGTShape.h"
#include <GABC/GABC_OError.h>
#include <GABC/GABC_OXform.h>
#include <GABC/GABC_PackedImpl.h>
#include <GABC/GABC_Types.h>

/// This class contains all of the geometry in a SOP node that has been grouped
/// into one partition (it may contain all of the geometry in the SOP). Houdini
/// geometry can have a heterogeneous mix of primitive types, so this class
/// splits the geometry into multiple simple shapes which can be represented in
/// Alembic.
///
/// Ex:     A ROP_AbcGTComnpoundShape object for a SOP containing geometry made
///         of curves and polygons will have 2 (or more) children: one
///         containing a curvemesh and one containing a polymesh.
///
class ROP_AbcGTCompoundShape
{
public:
    typedef Alembic::Abc::OObject	                OObject;

    typedef Alembic::AbcGeom::ObjectVisibility	        ObjectVisibility;

    typedef GABC_NAMESPACE::GABC_NodeType               GABC_NodeType;
    typedef GABC_NAMESPACE::GABC_OError                 GABC_OError;
    typedef GABC_NAMESPACE::GABC_PackedImpl             GABC_PackedImpl;

    typedef GABC_NAMESPACE::GABC_OXform                 OXform;

    typedef ROP_AbcGTShape::InverseMap                  InverseMap;
    typedef ROP_AbcGTShape::InverseMapInsert            InverseMapInsert;
    typedef ROP_AbcGTShape::GeoSet                      GeoSet;
    typedef ROP_AbcGTShape::XformMap                    XformMap;
    typedef ROP_AbcGTShape::XformMapInsert              XformMapInsert;

    typedef UT_Map<std::string, ROP_AbcGTShape *>       PackedMap;
    typedef std::pair<std::string, ROP_AbcGTShape *>    PackedMapInsert;

    // Helper class that stores a list of related ROP_AbcGTShapes. Can walk
    // through the list, remembering where we left off. If we walk past the end
    // of the list, we know to create more shapes. If we don't reach the end, we
    // know that we did not encounter all existing shapes this frame and need
    // to update them as hidden.
    class GTShapeList
    {
    public:
        GTShapeList()
            : myShapes()
            , myPos(0)
        {}
        ~GTShapeList()
        {
            exint   n = myShapes.entries();

            for (exint i = 0; i < n; ++i)
            {
                delete myShapes(i);
            }
        }

        ROP_AbcGTShape *    get(exint pos)
                            {
                                if (pos < myShapes.entries())
                                {
                                    return myShapes(pos);
                                }

                                return NULL;
                            }
        ROP_AbcGTShape *    getNext()
                            {
                                if (myPos < myShapes.entries())
                                {
                                    return myShapes(myPos++);
                                }

                                return NULL;
                            }
        void                reset() { myPos = 0; }
        void                insert(ROP_AbcGTShape *s)
                            {
                                myShapes.append(s);
                                ++myPos;
                            }
        void                updateUnvisited(GABC_OError &err,
                                    ObjectVisibility vis,
                                    exint frames)
                            {
                                exint   n = myShapes.entries();

                                for (exint i = myPos; i < n; ++i)
                                {
                                    myShapes(i)->nextFrameFromPrevious(err,
                                            vis,
                                            frames);
                                }
                            }

    private:
        UT_Array<ROP_AbcGTShape *>  myShapes;
        int                         myPos;
    };

    // Helper class used to store the relationship between the ROP_AbcGTShapes
    // in a GTShapeList helper. For deforming geometry, the key is the primitive
    // type: the shapes in a GTShapeList are all of the same type, thus
    // interchangable. For Packed Alembics, the key is a the unique ID of their
    // implementation.
    template <typename T>
    class GTShapeMap
    {
    public:
        typedef UT_Map<T, GTShapeList>    GTShapeListMap;
        typedef std::pair<T, GTShapeList> GTShapeListMapInsert;

        GTShapeMap() {}

        ROP_AbcGTShape *    getFirst()
                            {
                                return myMap.begin()->second.get(0);
                            }
        ROP_AbcGTShape *    getNext(T key)
                            {
                                auto    it = myMap.find(key);

                                if (it == myMap.end())
                                {
                                    return NULL;
                                }

                                return it->second.getNext();
                            }
        void                clear() { myMap.clear(); }
        void                insert(T key, ROP_AbcGTShape *value)
                            {
                                auto    it = myMap.find(key);

                                if (it == myMap.end())
                                {
                                    it = myMap.insert(GTShapeListMapInsert(key,
                                            GTShapeList())).first;
                                }

                                it->second.insert(value);
                            }
        void                reset()
                            {
                                for (auto it = myMap.begin();
                                        it != myMap.end();
                                        ++it)
                                {
                                    it->second.reset();
                                }
                            }
        void                updateHidden(GABC_OError &err)
                            {
                                updateFromPrevious(err,
                                        Alembic::AbcGeom::kVisibilityHidden,
                                        1);
                            }
        void                updateFromPrevious(GABC_OError &err,
                                    ObjectVisibility vis,
                                    exint frames)
                            {
                                for (auto it = myMap.begin();
                                        it != myMap.end();
                                        ++it)
                                {
                                    it->second.updateUnvisited(err,
                                            vis,
                                            frames);
                                }
                            }

    private:
        GTShapeListMap    myMap;
    };

    ROP_AbcGTCompoundShape(const std::string &identifier,
            InverseMap * const inv_map,
            GeoSet * const shape_set,
            XformMap * const xform_map,
	    bool is_partition,
	    bool polygons_as_subd,
	    bool show_unused_points,
	    bool geo_lock,
	    const ROP_AbcContext &ctx);
    ~ROP_AbcGTCompoundShape();

    // Output the first frame to Alembic. Does most of the setup.
    bool	first(const GT_PrimitiveHandle &prim,
			const OObject &parent,
			GABC_OError &err,
			const ROP_AbcContext &ctx,
			bool create_container,
                        ObjectVisibility vis = Alembic::AbcGeom::kVisibilityDeferred);
    // Output an additional frame to Alembic.
    bool	update(const GT_PrimitiveHandle &prim,
			GABC_OError &err,
			const ROP_AbcContext &ctx);
    // Output additional frames, reusing the data from the previous sample.
    bool	updateFromPrevious(GABC_OError &err,
                        ObjectVisibility vis = Alembic::AbcGeom::kVisibilityHidden,
                        exint frames = 1);

    exint       getElapsedFrames() const { return myElapsedFrames; }
    OObject	getShape();
private:
    void	clear();

    InverseMap         * const myInverseMap;
    GeoSet             * const myGeoSet;
    const OObject      *myShapeParent;
    OObject             myRoot;
    OXform             *myContainer;
    GTShapeMap<int>     myDeforming;
    GTShapeMap<int64>   myPacked;
    UT_DeepString       myPath;
    XformMap           * const myXformMap;
    exint               myElapsedFrames;
    exint               myNumShapes;
    std::string         myName;
    const bool          myGeoLock;
    const bool          myPolysAsSubd;
    const bool          myShowUnusedPoints;
};

#endif
