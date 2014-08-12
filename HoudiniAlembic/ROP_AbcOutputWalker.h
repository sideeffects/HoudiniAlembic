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

#ifndef __ROP_AbcOutputWalker__
#define __ROP_AbcOutputWalker__

#include "ROP_AbcContext.h"
#include "ROP_AbcXform.h"
#include <GABC/GABC_API.h>
#include <GABC/GABC_Include.h>
#include <GABC/GABC_IObject.h>
#include <GABC/GABC_OError.h>
#include <GABC/GABC_Util.h>
#include <GT/GT_Transform.h>

// ROP_AbcOutputWalker walks through an Alembic archive, reading
// samples and writing the out to a new archive (with some
// modifications if necessary).
class ROP_AbcOutputWalker
{
public:
    class Record;
    class PropertyMap;

    enum OObjectType {
        XFORM,
        POLYMESH,
        SUBD,
        POINTS,
        CURVES,
        NUPATCH,
        CAMERA
    };

    typedef Alembic::Abc::DataType                  DataType;
    typedef Alembic::Abc::ISampleSelector           ISampleSelector;
    typedef Alembic::Abc::MetaData                  MetaData;
    typedef Alembic::Abc::PropertyHeader            PropertyHeader;
    typedef Alembic::Abc::WrapExistingFlag          WrapExistingFlag;

    // Reader/Writers
    typedef Alembic::Abc::ObjectReader              ObjectReader;
    typedef Alembic::Abc::ObjectWriter              ObjectWriter;

    // Reader/Writer Ptrs
    typedef Alembic::Abc::CompoundPropertyReaderPtr CompoundPropertyReaderPtr;
    typedef Alembic::Abc::ObjectReaderPtr           ObjectReaderPtr;
    typedef Alembic::Abc::ObjectWriterPtr           ObjectWriterPtr;

    // Geometry
    typedef Alembic::Abc::Box3d                     Box3d;

    // Array Samples
    typedef Alembic::Abc::UcharArraySample          UcharArraySample;
    typedef Alembic::Abc::UInt32ArraySample         UInt32ArraySample;
    typedef Alembic::Abc::Int32ArraySample          Int32ArraySample;
    typedef Alembic::Abc::FloatArraySample          FloatArraySample;
    typedef Alembic::Abc::V3fArraySample            V3fArraySample;
    typedef Alembic::Abc::ArraySamplePtr            ArraySamplePtr;
    typedef Alembic::Abc::UcharArraySamplePtr       UcharArraySamplePtr;
    typedef Alembic::Abc::Int32ArraySamplePtr       Int32ArraySamplePtr;
    typedef Alembic::Abc::UInt32ArraySamplePtr      UInt32ArraySamplePtr;
    typedef Alembic::Abc::UInt64ArraySamplePtr      UInt64ArraySamplePtr;
    typedef Alembic::Abc::FloatArraySamplePtr       FloatArraySamplePtr;
    typedef Alembic::Abc::N3fArraySamplePtr         N3fArraySamplePtr;
    typedef Alembic::Abc::P3fArraySamplePtr         P3fArraySamplePtr;
    typedef Alembic::Abc::V2fArraySamplePtr         V2fArraySamplePtr;
    typedef Alembic::Abc::V3fArraySamplePtr         V3fArraySamplePtr;

    // Properties
    typedef Alembic::Abc::IScalarProperty           IScalarProperty;
    typedef Alembic::Abc::OScalarProperty           OScalarProperty;
    typedef Alembic::Abc::IArrayProperty            IArrayProperty;
    typedef Alembic::Abc::OArrayProperty            OArrayProperty;
    typedef Alembic::Abc::ICompoundProperty         ICompoundProperty;
    typedef Alembic::Abc::OCompoundProperty         OCompoundProperty;

    // General
    typedef Alembic::AbcGeom::IObject               IObject;
    typedef Alembic::AbcGeom::OObject               OObject;
    // Xform
    typedef Alembic::AbcGeom::IXform                IXform;
    typedef ROP_AbcXform                            OXform;
    typedef Alembic::AbcGeom::IXformSchema          IXformSchema;
    typedef Alembic::AbcGeom::OXformSchema          OXformSchema;
    typedef Alembic::AbcGeom::XformSample           XformSample;
    // Camera
    typedef Alembic::AbcGeom::ICamera               ICamera;
    typedef Alembic::AbcGeom::OCamera               OCamera;
    typedef Alembic::AbcGeom::ICameraSchema         ICameraSchema;
    typedef Alembic::AbcGeom::OCameraSchema         OCameraSchema;
    typedef Alembic::AbcGeom::CameraSample          CameraSample;
    // PolyMesh
    typedef Alembic::AbcGeom::IPolyMesh             IPolyMesh;
    typedef Alembic::AbcGeom::IPolyMeshSchema       IPolyMeshSchema;
    typedef Alembic::AbcGeom::OPolyMesh             OPolyMesh;
    typedef Alembic::AbcGeom::OPolyMeshSchema       OPolyMeshSchema;
    // Subdivision
    typedef Alembic::AbcGeom::ISubD                 ISubD;
    typedef Alembic::AbcGeom::ISubDSchema           ISubDSchema;
    typedef Alembic::AbcGeom::OSubD                 OSubD;
    typedef Alembic::AbcGeom::OSubDSchema           OSubDSchema;
    // Points
    typedef Alembic::AbcGeom::IPoints		    IPoints;
    typedef Alembic::AbcGeom::IPointsSchema	    IPointsSchema;
    typedef Alembic::AbcGeom::OPoints		    OPoints;
    typedef Alembic::AbcGeom::OPointsSchema	    OPointsSchema;
    // Curves
    typedef Alembic::AbcGeom::ICurves		    ICurves;
    typedef Alembic::AbcGeom::ICurvesSchema	    ICurvesSchema;
    typedef Alembic::AbcGeom::OCurves		    OCurves;
    typedef Alembic::AbcGeom::OCurvesSchema	    OCurvesSchema;
    // NuPatch
    typedef Alembic::AbcGeom::INuPatch		    INuPatch;
    typedef Alembic::AbcGeom::INuPatchSchema	    INuPatchSchema;
    typedef Alembic::AbcGeom::ONuPatch		    ONuPatch;
    typedef Alembic::AbcGeom::ONuPatchSchema	    ONuPatchSchema;
    // FaceSet
    typedef Alembic::AbcGeom::IFaceSet		    IFaceSet;
    typedef Alembic::AbcGeom::IFaceSetSchema	    IFaceSetSchema;
    typedef Alembic::AbcGeom::OFaceSet		    OFaceSet;
    typedef Alembic::AbcGeom::OFaceSetSchema	    OFaceSetSchema;

    // Curve/NURBS
    typedef Alembic::AbcGeom::BasisType             BasisType;
    typedef Alembic::AbcGeom::CurvePeriodicity      CurvePeriodicity;
    typedef Alembic::AbcGeom::CurveType             CurveType;

    // Parameters
    typedef Alembic::AbcGeom::IFloatGeomParam       IFloatGeomParam;
    typedef Alembic::AbcGeom::OFloatGeomParam       OFloatGeomParam;
    typedef Alembic::AbcGeom::IN3fGeomParam         IN3fGeomParam;
    typedef Alembic::AbcGeom::ON3fGeomParam         ON3fGeomParam;
    typedef Alembic::AbcGeom::IV2fGeomParam         IV2fGeomParam;
    typedef Alembic::AbcGeom::OV2fGeomParam         OV2fGeomParam;

    // Visibility
    typedef Alembic::AbcGeom::IVisibilityProperty   IVisibilityProperty;
    typedef Alembic::AbcGeom::OVisibilityProperty   OVisibilityProperty;

    // GABC
    typedef GABC_NAMESPACE::GABC_IObject            GABC_IObject;
    typedef GABC_NAMESPACE::GABC_OError	            GABC_OError;

    // Custom

    // Used for computing child bounds of output hierarchy. Transforms
    // need access to the existing XformSchema, geometry needs access to
    // its bounding box. Since we can't easily access these later, store
    // them at write time and access them after all packed Alembics have been
    // sampled for this frame.
    union Bounds {
        Bounds()
            : xform(NULL)
        {}
        Bounds(const Bounds &other)
        {
            this->xform = other.xform;
        }

        OXform *xform;
        Box3d  *box;
    };

    template<typename T>
    size_t  hash_value(const Alembic::Util::shared_ptr<T> &p)
    {
        boost::hash<int> hasher;
        return hasher((int)p.get());
    }

    typedef UT_Map<ObjectWriterPtr, Bounds>         BoundsMap;
    typedef std::pair<ObjectWriterPtr, Bounds>      BoundsMapInsert;
    typedef UT_Map<std::string, Record *>           RecordsMap;
    typedef std::pair<std::string, Record *>        RecordsMapInsert;
    typedef UT_Map<ObjectWriterPtr, OObjectType>    TypeMap;
    typedef std::pair<ObjectWriterPtr, OObjectType> TypeMapInsert;

    const static WrapExistingFlag   gabcWrapExisting
                                            = Alembic::Abc::kWrapExisting;

    // Record class used to store OObjects, their visibility
    // properties (if they have one), their arbitrary geometry parameters,
    // and their user properties. The Record object must store
    // a copy of the actual visibility property because that's what Alembic
    // gives us.
    class Record
    {
    public:
        explicit    Record(OObject *obj)
                        : myObject(obj)
                        , myVisProperty()
                        , myVisPointer(NULL)
                        , myArbGProperties(NULL)
                        , myUserProperties(NULL)
                    {}
                    Record(OObject *obj,
                            OVisibilityProperty vis,
                            PropertyMap *arbgprops,
                            PropertyMap *uprops)
                        : myObject(obj)
                        , myVisProperty(vis)
                        , myVisPointer(&myVisProperty)
                        , myArbGProperties(arbgprops)
                        , myUserProperties(uprops)
                    {}
        virtual     ~Record() {
                        if (myObject)
                            delete myObject;
                        if (myArbGProperties)
                            delete myArbGProperties;
                        if (myUserProperties)
                            delete myUserProperties;
                    }

        OObject *                   getObject() { return myObject; }
        OVisibilityProperty *       getVisibility() { return myVisPointer; }
        PropertyMap *               getArbGProperties()
                                    {
                                        return myArbGProperties;
                                    }
        PropertyMap *               getUserProperties()
                                    {
                                        return myUserProperties;
                                    }

    private:
        OObject             *myObject;
        OVisibilityProperty  myVisProperty;
        OVisibilityProperty *myVisPointer;
        PropertyMap         *myArbGProperties;
        PropertyMap         *myUserProperties;
    };

    // PropertyMap class is used to store pointers to Alembic Property
    // objects. Ideally we would use a regular map, but these 3 classes
    // are templated and so is their common ancestor.
    class PropertyMap
    {
    public:
        typedef std::pair<std::string, OScalarProperty *>   ScalarInsert;
        typedef std::pair<std::string, OArrayProperty *>    ArrayInsert;
        typedef std::pair<std::string, OCompoundProperty *> CompoundInsert;

        PropertyMap() {}
        ~PropertyMap()
        {
            clear();
        }

        void clear()
        {
            for (auto it = myScalarMap.begin(); it != myScalarMap.end(); ++it)
            {
                delete it->second;
            }
            myScalarMap.clear();

            for (auto it = myArrayMap.begin(); it != myArrayMap.end(); ++it)
            {
                delete it->second;
            }
            myArrayMap.clear();

            for (auto it = myCompoundMap.begin();
                    it != myCompoundMap.end();
                    ++it)
            {
                delete it->second;
            }
            myCompoundMap.clear();
        }

        void insert(const std::string &name, OScalarProperty *prop)
        {
            myScalarMap.insert(ScalarInsert(name, prop));
        }
        void insert(const std::string &name, OArrayProperty *prop)
        {
            myArrayMap.insert(ArrayInsert(name, prop));
        }
        void insert(const std::string &name, OCompoundProperty *prop)
        {
            myCompoundMap.insert(CompoundInsert(name, prop));
        }

        OScalarProperty *findScalar(const std::string &name)
        {
            auto    it = myScalarMap.find(name);

            if (it == myScalarMap.end())
            {
                return NULL;
            }

            return it->second;
        }
        OArrayProperty *findArray(const std::string &name)
        {
            auto    it = myArrayMap.find(name);

            if (it == myArrayMap.end())
            {
                return NULL;
            }

            return it->second;
        }
        OCompoundProperty *findCompound(const std::string &name)
        {
            auto    it = myCompoundMap.find(name);

            if (it == myCompoundMap.end())
            {
                return NULL;
            }

            return it->second;
        }

    private:
        UT_Map<std::string, OScalarProperty *>      myScalarMap;
        UT_Map<std::string, OArrayProperty *>       myArrayMap;
        UT_Map<std::string, OCompoundProperty *>    myCompoundMap;
    };

    // OReaderCollection class is a set that keeps track of how many copies
    // of a given ObjectReaderPtr are currently in the set.
    class OReaderCollection
    {
    public:
        typedef std::pair<ObjectReaderPtr, int>                 InsertType;
        typedef UT_Map<ObjectReaderPtr, int>::const_iterator    const_iterator;
        typedef UT_Map<ObjectReaderPtr, int>::iterator          iterator;

                        OReaderCollection() {}
        virtual         ~OReaderCollection() {}

        const_iterator  begin() const
                        {
                            return myMap.begin();
                        }
        iterator        begin()
                        {
                            return myMap.begin();
                        }
        const_iterator  end() const
                        {
                            return myMap.end();
                        }
        iterator        end()
                        {
                            return myMap.end();
                        }
        int             count(const ObjectReaderPtr &ptr) const
                        {
                            if (myMap.count(ptr))
                            {
                                return myMap.at(ptr);
                            }

                            return 0;
                        }
        void            insert(const ObjectReaderPtr &ptr)
                        {
                            if (myMap.count(ptr))
                            {
                                myMap[ptr] += 1;
                            }
                            else
                            {
                                myMap.insert(InsertType(ptr, 1));
                            }
                        }
        void            clear()
                        {
                            myMap.clear();
                        }

    private:
        UT_Map<ObjectReaderPtr, int>    myMap;
    };

                    ROP_AbcOutputWalker(const OObject *parent,
                            BoundsMap *bounds,
                            RecordsMap *records,
                            TypeMap *types,
                            GABC_OError &err,
                            fpreal time);

    virtual         ~ROP_AbcOutputWalker() {};

    virtual bool    process(const GABC_IObject &node,
                            const ROP_AbcContext &ctx,
                            const GT_TransformHandle &transform,
                            bool create_nodes,
                            bool visible_nodes,
                            int sample_limit) const = 0;

protected:
    //
    // Output helper classes
    //

    // Properties
    static void     sampleCompoundProperties(PropertyMap *p_map,
                            ICompoundProperty &in,
                            OCompoundProperty &out,
                            const ROP_AbcContext &ctx,
                            const ISampleSelector &iss,
                            const std::string &base_name);

    // Xforms
    static void     readXformSample(const GABC_IObject &node,
                            OXform *obj,
                            const ISampleSelector &iss,
                            UT_Matrix4D &transform,
                            bool &inherits);
    void            writeXformSample(const GABC_IObject &node,
                            OXform *obj,
                            PropertyMap *arb_map,
                            PropertyMap *p_map,
                            const ROP_AbcContext &ctx,
                            const ISampleSelector &iss,
                            UT_Matrix4D &iMatrix,
                            bool iInheritXform) const;
    void            writeSimpleXformSample(OXform *obj,
                            const ROP_AbcContext &ctx,
                            UT_Matrix4D &iMatrix,
                            bool iInheritXform) const;

    // Face Sets
    static void     createFaceSetsPolymesh(const GABC_IObject &node,
                            OPolyMesh *obj,
                            const ROP_AbcContext &ctx);
    static void     createFaceSetsSubd(const GABC_IObject &node,
                            OSubD *obj,
                            const ROP_AbcContext &ctx);
    template <typename ABC_SCHEMA_IN, typename ABC_SCHEMA_OUT>
    static void     sampleFaceSets(ABC_SCHEMA_IN &input,
                            Alembic::AbcGeom::OSchemaObject<ABC_SCHEMA_OUT> *output_o,
                            const ISampleSelector &iss,
                            std::vector<std::string> &names);

    // Geometry
    void     samplePolyMesh(const GABC_IObject &node,
                            OPolyMesh *obj,
                            PropertyMap *arb_map,
                            PropertyMap *p_map,
                            const ROP_AbcContext &ctx,
                            const ISampleSelector &iss) const;
    void     sampleSubD(const GABC_IObject &node,
                            OSubD *obj,
                            PropertyMap *arb_map,
                            PropertyMap *p_map,
                            const ROP_AbcContext &ctx,
                            const ISampleSelector &iss) const;
    void     samplePoints(const GABC_IObject &node,
                            OPoints *obj,
                            PropertyMap *arb_map,
                            PropertyMap *p_map,
                            const ROP_AbcContext &ctx,
                            const ISampleSelector &iss) const;
    void     sampleCurves(const GABC_IObject &node,
                            OCurves *obj,
                            PropertyMap *arb_map,
                            PropertyMap *p_map,
                            const ROP_AbcContext &ctx,
                            const ISampleSelector &iss) const;
    void     sampleNuPatch(const GABC_IObject &node,
                            ONuPatch *obj,
                            PropertyMap *arb_map,
                            PropertyMap *p_map,
                            const ROP_AbcContext &ctx,
                            const ISampleSelector &iss) const;
    static void     sampleCamera(const GABC_IObject &node,
                            OCamera *obj,
                            PropertyMap *arb_map,
                            PropertyMap *p_map,
                            const ROP_AbcContext &ctx,
                            const ISampleSelector &iss);

    // Functions for creating/sampling ancestor transforms
    bool            createAncestors(const GABC_IObject &node,
                            const ROP_AbcContext &ctx) const;
    virtual bool    processAncestors(const GABC_IObject &node,
                            const ROP_AbcContext &ctx,
                            int sample_limit,
                            bool visible) const = 0;

    // Reports errors/warnings
    GABC_OError                &myErrorHandler;
    // The parent OObject that was provided during construction
    OObject const              *const myOriginalParent;
    // The parent OObject that should be used currently when making new OObjects
    mutable OObject const      *myCurrentParent;
    // Stores Bounds objects used later for computing child bounds
    BoundsMap                  *const myBoundsMap;
    // Stores Records for IObjects from the current archive
    RecordsMap                 *const myRecordsMap;
    // Stores type information for created OObjects. This is used when computing
    // child bounds
    TypeMap                    *const myTypeMap;
    // Final matrix after performing all transforms in the hierarchy leading to
    // the current node. Used to compute the inverse transform of the hierarchy
    mutable UT_Matrix4D         myMatrix;
    // Flat time to add to sample time.
    // This is a consequence of us starting at frame 1
    fpreal const                myAdditionalSampleTime;
};

#endif