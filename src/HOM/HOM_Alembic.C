/*
 * Copyright (c) 2018
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

//-*****************************************************************************
//
// Copyright (c) 2009-2012,
//  Sony Pictures Imageworks Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic, nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

// The python developers recommend that Python.h be included before any other
// header files.
#include <PY/PY_CPythonAPI.h>
// This file contains functions that will run arbitrary Python code
#include <PY/PY_Python.h>
#include <PY/PY_InterpreterAutoLock.h>
#include <GABC/GABC_Util.h>
#include <Alembic/AbcGeom/All.h>
#include <HOM/HOM_Module.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_StackBuffer.h>
#include <GT/GT_DataArray.h>
#include <CH/CH_Channel.h>
#include <OP/OP_Director.h>

using namespace GABC_NAMESPACE;

namespace
{
    // Basic types
    typedef Alembic::Abc::index_t		    index_t;
    typedef Alembic::Abc::chrono_t		    chrono_t;

    typedef Alembic::Abc::M44d			    M44d;
    typedef Alembic::Abc::V2d			    V2d;

    // Properties
    typedef Alembic::Abc::CompoundPropertyReaderPtr CompoundPropertyReaderPtr;
    typedef Alembic::Abc::ScalarPropertyReaderPtr   ScalarPropertyReaderPtr;
    typedef Alembic::Abc::ICompoundProperty         ICompoundProperty;

    // Sampling
    typedef Alembic::Abc::ISampleSelector	    ISampleSelector;
    typedef Alembic::Abc::TimeSamplingPtr	    TimeSamplingPtr;

    // Geometry
    typedef Alembic::AbcGeom::ICurves		    ICurves;
    typedef Alembic::AbcGeom::IFaceSet		    IFaceSet;
    typedef Alembic::AbcGeom::ILight		    ILight;
    typedef Alembic::AbcGeom::INuPatch		    INuPatch;
    typedef Alembic::AbcGeom::IPoints		    IPoints;
    typedef Alembic::AbcGeom::IPolyMesh		    IPolyMesh;
    typedef Alembic::AbcGeom::ISubD		    ISubD;
    typedef Alembic::AbcGeom::IXform		    IXform;
    typedef Alembic::AbcGeom::IXformSchema	    IXformSchema;

    // Camera
    typedef Alembic::AbcGeom::ICamera		    ICamera;
    typedef Alembic::AbcGeom::ICameraSchema	    ICameraSchema;
    typedef Alembic::AbcGeom::CameraSample	    CameraSample;
    typedef Alembic::AbcGeom::FilmBackXformOp	    FilmBackXformOp;

    // Wrap Existing
    typedef Alembic::Abc::WrapExistingFlag          WrapExistingFlag;
    static const WrapExistingFlag gabcWrapExisting = Alembic::Abc::kWrapExisting;

    static void
    appendFile(std::vector<std::string> &filenames, const char *name)
    {
	UT_String realname;

	// complete a path search in case it is in the geometry path
	UT_PathSearch::getInstance(UT_HOUDINI_GEOMETRY_PATH)->
		findFile(realname, name);
	filenames.push_back(realname.toStdString());
    }

    static void
    appendFileList(std::vector<std::string> &filenames, PY_PyObject *fileList)
    {
	if (!fileList)
	    return;

	if (PY_PyString_Check(fileList))
	{
	    const char *fileStr = PY_PyString_AsString(fileList);
	    if (fileStr && strlen(fileStr))
		appendFile(filenames, fileStr);
	}
	else if (PY_PySequence_Check(fileList))
	{
	    int numFiles = PY_PySequence_Size(fileList);
	    for (int i = 0; i < numFiles; ++i)
	    {
		PY_PyObject *fileObj = PY_PySequence_GetItem(fileList, i);
		if (PY_PyString_Check(fileObj))
		{
		    const char *fileStr = PY_PyString_AsString(fileObj);
		    if (fileStr && strlen(fileStr))
			appendFile(filenames, fileStr);
		}
	    }
	}
    }

    // Returns an Alembic objects transform, as well as if the transform
    // is constant, and if it inherits it's parent's transform.
    static PY_PyObject *
    alembicGetXform(PY_PyObject *self, PY_PyObject *args, int result_size,
	    bool localx)
    {
	UT_Matrix4D     xform;
	const char     *filename = NULL;
	const char     *objectPath = NULL;
	const double   *data = NULL;
	double          sampleTime = 0.0;
	bool            isConstant = true;
	bool            inheritsXform = true;
	bool            ok;

        if (!PY_PyArg_ParseTuple(args, "ssd", &filename, &objectPath,
                &sampleTime))
	{
	    return NULL;
	}

	if (localx)
	{
            ok = GABC_Util::getLocalTransform(filename, objectPath,
                    sampleTime, xform, isConstant, inheritsXform);
        }
	else
	{
            ok = GABC_Util::getWorldTransform(filename, objectPath,
                    sampleTime, xform, isConstant, inheritsXform);
        }

	if (!ok)
	{
	    xform.identity();
        }
	data = xform.data();

        PY_PyObject* matrixTuple = PY_PyTuple_New(16);
        for (PY_Py_ssize_t i = 0; i < 16; ++i)
        {
            PY_PyTuple_SET_ITEM(matrixTuple, i, PY_PyFloat_FromDouble(data[i]));
        }
        PY_PyObject *result = PY_PyTuple_New(result_size);
        PY_PyTuple_SET_ITEM(result, 0, matrixTuple);
        PY_PyTuple_SET_ITEM(result, 1, PY_PyInt_FromLong(isConstant));
	if (result_size > 2)
	    PY_PyTuple_SET_ITEM(result, 2, PY_PyInt_FromLong(inheritsXform));
        return result;
    }

    static const char	*Doc_AlembicGetLocalXform =
	"(xform, isConstant) = alembicGetLocalXform(abcPath, objectPath, sampleTime)\n"
	"\n"
	"Returns a tuple (xform, isConstant) containing the transform and a\n"
	"flag indicating whether the transform is animated or constant. The\n"
	"transform is a 16-tuple of floats.\n"
	"\n"
	"This is deprecated in favour of getLocalXform()";	// 12.5

    PY_PyObject *
    Py_AlembicGetLocalXform(PY_PyObject *self, PY_PyObject *args)
    {
	return alembicGetXform(self, args, 2, true);
    }

    static const char	*Doc_GetLocalXform =
	"(xform, isConstant, inherit) = getLocalXform(abcPath, objectPath, sampleTime)\n"
	"\n"
	"Returns a tuple (xform, isConstant, inherits) containing the transform\n"
	"and flags indicating whether the transform is animated/constant,\n"
	"along with whether the node inherits it's parent transform (or is\n"
	"disconnected from the transform hierarchy).";

    PY_PyObject *
    Py_GetLocalXform(PY_PyObject *self, PY_PyObject *args)
    {
	return alembicGetXform(self, args, 3, true);
    }

    static const char	*Doc_GetWorldXform =
	"(xform, isConstant, inherit) = getWorldXform(abcPath, objectPath, sampleTime)\n"
	"\n"
	"Returns a tuple (xform, isConstant, inherits) containing the object to\n"
	"world transform flags indicating whether the transform is animated\n"
	"or constant, along with whether the node inherit it's parent\n"
	"transform (or is disconnected from the transform hierarchy).";

    PY_PyObject *
    Py_GetWorldXform(PY_PyObject *self, PY_PyObject *args)
    {
	return alembicGetXform(self, args, 3, false);
    }

    // Extract the data for a single tuple element from a GT_DataArray.
    static PY_PyObject *
    extractTuple(const GT_DataArrayHandle &array, GT_Size tsize, exint idx)
    {
	PY_PyObject    *tuple = PY_PyTuple_New(tsize);

	if (GTisFloat(array->getStorage()))
	{
	    UT_StackBuffer<fpreal>	buf(tsize);
	    array->import(idx, buf, tsize);
	    for (GT_Size i = 0; i < tsize; ++i)
		PY_PyTuple_SetItem(tuple, i, PY_PyFloat_FromDouble(buf[i]));
	}
	else if (GTisInteger(array->getStorage()))
	{
	    UT_StackBuffer<int64>	buf(tsize);
	    array->import(idx, buf, tsize);
	    for (GT_Size i = 0; i < tsize; ++i)
		PY_PyTuple_SetItem(tuple, i, PY_PyInt_FromLong(buf[i]));
	}
	else
	{
	    for (GT_Size i = 0; i < tsize; ++i)
	    {
		PY_PyTuple_SetItem(tuple, i,
			PY_PyString_FromString(array->getS(idx, i)));
	    }
	}

	return tuple;
    }

    // Extract data from a GT_DataArray to a Python list.
    static PY_PyObject *
    dataFromArray(const GT_DataArrayHandle &array)
    {
	GT_Size         size = array ? array->entries() : 0;
	GT_Size         tsize = array ? array->getTupleSize() : 0;
	PY_PyObject    *list = PY_PyList_New(size);

	if (size)
	{
	    if (tsize > 1)
	    {
		for (exint i = 0; i < size; ++i)
		    PY_PyList_SetItem(list, i, extractTuple(array, tsize, i));
	    }
	    else if (GTisInteger(array->getStorage()))
	    {
		for (exint i = 0; i < size; ++i)
		    PY_PyList_SetItem(list, i, PY_PyInt_FromLong(array->getI64(i)));
	    }
	    else if (GTisFloat(array->getStorage()))
	    {
		for (exint i = 0; i < size; ++i)
		    PY_PyList_SetItem(list, i, PY_PyFloat_FromDouble(array->getF64(i)));
	    }
	    else
	    {
		for (exint i = 0; i < size; ++i)
		    PY_PyList_SetItem(list, i, PY_PyString_FromString(array->getS(i)));
	    }
	}

	return list;
    }

    // Match scope name to scope.
    static const char *
    scopeName(Alembic::AbcGeom::GeometryScope scope)
    {
	switch (scope)
	{
	    case Alembic::AbcGeom::kConstantScope:
		return "constant";
	    case Alembic::AbcGeom::kUniformScope:
		return "uniform";
	    case Alembic::AbcGeom::kVaryingScope:
		return "varying";
	    case Alembic::AbcGeom::kVertexScope:
		return "vertex";
	    case Alembic::AbcGeom::kFacevaryingScope:
		return "facevarying";
	    default:
		break;
	}
	return "unknown";
    }

        /// This class processes Alembic archives. Starting with the node passed
        /// to the process function as root, it creates a tree of nested tuples
        /// representing the structure of the Alembic archive. Each tuple contains
        /// the object name, type, and child tuples.
    class PyWalker : public GABC_Util::Walker
    {
        public:
    	PyWalker()
    	    : myRoot(NULL)
    	{}
    	~PyWalker() {}

    	virtual bool	process(const GABC_IObject &root)
    			{
    			    myRoot = walkNode(root);
    			    return false;
    			}

    	PY_PyObject	*walkNode(const GABC_IObject &obj)
    	{
    	    const char *otype = "<unknown>";

	    computeTimeRange(obj);

    	    switch (obj.nodeType())
    	    {
    		case GABC_XFORM:
    		    {
    			IXform	xform(obj.object(), Alembic::Abc::kWrapExisting);
			IXformSchema &ss = xform.getSchema();
			otype = "xform";
			if (ss.isConstant())
			{
			    if (ss.getInheritsXforms())
				otype = "cxform";
			    else
			    {
				GABC_IObject parent = obj.getParent();
				M44d xform;
				bool isConstant = true;
				bool inherits = true;
				if(!parent.worldTransform(0.0, xform, isConstant, inherits) || isConstant)
				    otype = "cxform";
			    }
			}
    		    }
    		    break;
    		case GABC_POLYMESH:
    		    otype = "polymesh";
    		    break;
    		case GABC_SUBD:
    		    otype = "subdmesh";
    		    break;
    		case GABC_CAMERA:
    		    otype = "camera";
    		    break;
    		case GABC_FACESET:
    		    otype = "faceset";
    		    break;
    		case GABC_CURVES:
    		    otype = "curves";
    		    break;
    		case GABC_POINTS:
    		    otype = "points";
    		    break;
    		case GABC_NUPATCH:
    		    otype = "nupatch";
    		    break;
    		default:
    		    otype = "unknown";
    		    break;
    	    }

    	    exint nkids = const_cast<GABC_IObject *>(&obj)->getNumChildren();
    	    PY_PyObject *result = PY_PyTuple_New(3);
    	    PY_PyObject *kids = PY_PyTuple_New(nkids);;

    	    PY_PyTuple_SET_ITEM(result, 0,
    		    PY_PyString_FromString(obj.getName().c_str()));
    	    PY_PyTuple_SET_ITEM(result, 1, PY_PyString_FromString(otype));
    	    PY_PyTuple_SET_ITEM(result, 2, kids);

	    UT_SortedMap<std::string, exint, UTnumberedStringCompare> child_map;
    	    for (exint i = 0; i < nkids; ++i)
		child_map.emplace(obj.getChild(i).getName(), i);

	    exint i = 0;
    	    for (auto &it : child_map)
    	    {
    		PY_PyTuple_SET_ITEM(kids, i++,
    			walkNode(const_cast<GABC_IObject *>(&obj)->getChild(it.second)));
    	    }
    	    return result;
    	}

    	PY_PyObject	*getObject() const	{ return myRoot; }

        private:
    	PY_PyObject	*myRoot;
    };

    static const char	*Doc_AlembicArbGeometry =
	"(value, isConstant, scope) = alembicArbGeometry(abcPath, objectPath, name, sampleTime)\n"
	"\n"
	"Returns None or a tuple (value, isConstant, scope).  The tuple\n"
	"contains the value for the attribute, it's scope ('varying',\n"
	"'vertex', 'facevarying', 'uniform', 'constant' or 'unknown') and\n"
	"a boolean flag indicating whether the attribute is constant over\n"
	"time or not.";

    PY_PyObject *
    Py_AlembicArbGeometry(PY_PyObject *self, PY_PyObject *args)
    {
	const char			*filename;
	const char			*objectPath;
	const char			*name;
	double				 sampleTime;
	Alembic::AbcGeom::GeometryScope	 scope;
	GEO_AnimationType		 atype;
	GT_DataArrayHandle		 data;

        if (!PY_PyArg_ParseTuple(args, "sssd", &filename, &objectPath,
                &name, &sampleTime))
	{
	    return NULL;
	}

	GABC_IObject	obj = GABC_Util::findObject(filename, objectPath);
	if (!obj.valid())
	{
	    PY_Py_RETURN_NONE;
	}
	data = obj.getGeometryProperty(name, sampleTime, scope, atype);
	PY_PyObject	*rcode = PY_PyTuple_New(3);
	PY_PyTuple_SetItem(rcode, 0, dataFromArray(data));
	PY_PyTuple_SetItem(rcode, 1, atype == GEO_ANIMATION_CONSTANT ? PY_Py_True() : PY_Py_False());
	PY_PyTuple_SetItem(rcode, 2, PY_PyString_FromString(scopeName(scope)));
	PY_Py_INCREF(rcode);
	return rcode;
    }

    static const char   *Doc_AlembicHasUserProperties =
        "alembicHasUserProperties(abcPath, objectPath)\n"
        "\n"
        "Returns None if the object has no user properties. Otherwise,\n"
        "returns whether the userProperties are constant over time or not.\n";

    PY_PyObject *
    Py_AlembicHasUserProperties(PY_PyObject *self, PY_PyObject *args)
    {
        const char         *filename;
        const char         *objectPath;

        if (!PY_PyArg_ParseTuple(args, "ss", &filename, &objectPath))
        {
            return NULL;
        }

        GABC_IObject obj = GABC_Util::findObject(filename, objectPath);
	ICompoundProperty uprops = obj.getUserProperties();
        if (!uprops || uprops.getNumProperties() == 0)
        {
            PY_Py_RETURN_NONE;
        }

        return GABC_Util::isABCPropertyConstant(uprops) ? PY_Py_True()
                : PY_Py_False();
    }

    static const char	*Doc_AlembicUserProperty =
	"(value, isConstant) = alembicUserProperty(abcPath, objectPath, name, sampleTime)\n"
	"\n"
	"Returns None or a tuple (value,isConstant).  The tuple contains the\n"
	"value for the attribute, and a boolean flag indicating whether the\n"
	"attribute is constant over time or not.\n";

    PY_PyObject *
    Py_AlembicUserProperty(PY_PyObject *self, PY_PyObject *args)
    {
	const char			*filename;
	const char			*objectPath;
	const char			*name;
	double				 sampleTime;
	GEO_AnimationType		 atype;
	GT_DataArrayHandle		 data;

        if (!PY_PyArg_ParseTuple(args, "sssd", &filename, &objectPath,
                &name, &sampleTime))
	{
	    return NULL;
	}

	GABC_IObject	obj = GABC_Util::findObject(filename, objectPath);
	if (!obj.valid())
	{
	    PY_Py_RETURN_NONE;
	}
	data = obj.getUserProperty(name, sampleTime, atype);
	PY_PyObject	*rcode = PY_PyTuple_New(2);
	PY_PyTuple_SetItem(rcode, 0, dataFromArray(data));
	PY_PyTuple_SetItem(rcode, 1, atype == GEO_ANIMATION_CONSTANT ? PY_Py_True() : PY_Py_False());
	PY_Py_INCREF(rcode);
	return rcode;
    }

    static const char   *Doc_AlembicUserPropertyValues =
        "alembicUserPropertyDictionary(abcPath, objectPath, sampleTime)\n"
        "\n"
        "Returns None or a JSON dictionary containing a map of user property \n"
        "names --> user property values.\n";

    PY_PyObject *
    Py_AlembicUserPropertyValues(PY_PyObject *self, PY_PyObject *args)
    {
        UT_JSONWriter      *data_writer;
        UT_WorkBuffer       data_dictionary;
        const char         *filename;
        const char         *objectPath;
        double              sampleTime;

        if (!PY_PyArg_ParseTuple(args,
                "ssd",
                &filename,
                &objectPath,
                &sampleTime))
        {
            return NULL;
        }

        GABC_IObject obj = GABC_Util::findObject(filename, objectPath);
        if (!obj.valid())
        {
            PY_Py_RETURN_NONE;
        }

        ICompoundProperty uprops = obj.getUserProperties();
        if (!uprops || uprops.getNumProperties() == 0)
        {
            PY_Py_RETURN_NONE;
        }

        data_writer = UT_JSONWriter::allocWriter(data_dictionary);
        if (!GABC_Util::importUserPropertyDictionary(data_writer,
                NULL,
                obj,
                sampleTime))
        {
            delete data_writer;
            PY_Py_RETURN_NONE;
        }

        delete data_writer;

        PY_PyObject    *result = PY_PyString_FromString(data_dictionary.buffer());

        return result;
    }

    static const char   *Doc_AlembicUserPropertyMetadata =
        "alembicUserPropertyMetadata(abcPath, objectPath, sampleTime)\n"
        "\n"
        "Returns None or a JSON dictionary containing a map of user property \n"
        "names --> user property metadata.\n";

    PY_PyObject *
    Py_AlembicUserPropertyMetadata(PY_PyObject *self, PY_PyObject *args)
    {
        UT_JSONWriter      *meta_writer;
        UT_WorkBuffer       meta_dictionary;
        const char         *filename;
        const char         *objectPath;
        double              sampleTime;

        if (!PY_PyArg_ParseTuple(args,
                "ssd",
                &filename,
                &objectPath,
                &sampleTime))
        {
            return NULL;
        }

        GABC_IObject obj = GABC_Util::findObject(filename, objectPath);
        if (!obj.valid())
        {
            PY_Py_RETURN_NONE;
        }

	ICompoundProperty uprops = obj.getUserProperties();
        if (!uprops || uprops.getNumProperties() == 0)
        {
            PY_Py_RETURN_NONE;
        }

        meta_writer = UT_JSONWriter::allocWriter(meta_dictionary);
        if (!GABC_Util::importUserPropertyDictionary(NULL,
                meta_writer,
                obj,
                sampleTime))
        {
            delete meta_writer;
            PY_Py_RETURN_NONE;
        }

        delete meta_writer;

        PY_PyObject    *result = PY_PyString_FromString(meta_dictionary.buffer());

        return result;
    }

    static const char   *Doc_AlembicUserPropertyValuesAndMetadata =
        "alembicUserPropertyValuesAndMetadata(abcPath, objectPath, sampleTime)\n"
        "\n"
        "Returns None or a Tuple containing two JSON dictionaries. The first \n"
        "dictionary contains a map of user properties to values. The second\n"
        "dictionary contains a map of user properties to metadata used to\n"
        "interpret the first dictionary.\n";

    PY_PyObject *
    Py_AlembicUserPropertyValuesAndMetadata(PY_PyObject *self, PY_PyObject *args)
    {
        UT_JSONWriter      *data_writer;
        UT_JSONWriter      *meta_writer;
        UT_WorkBuffer       data_dictionary;
        UT_WorkBuffer       meta_dictionary;
        const char         *filename;
        const char         *objectPath;
        double              sampleTime;

        if (!PY_PyArg_ParseTuple(args,
                "ssd",
                &filename,
                &objectPath,
                &sampleTime))
        {
            return NULL;
        }

        GABC_IObject obj = GABC_Util::findObject(filename, objectPath);
        if (!obj.valid())
        {
            PY_Py_RETURN_NONE;
        }

	ICompoundProperty uprops = obj.getUserProperties();
        if (!uprops || uprops.getNumProperties() == 0)
        {
            PY_Py_RETURN_NONE;
        }

        data_writer = UT_JSONWriter::allocWriter(data_dictionary);
        meta_writer = UT_JSONWriter::allocWriter(meta_dictionary);
        if (!GABC_Util::importUserPropertyDictionary(data_writer,
                meta_writer,
                obj,
                sampleTime))
        {
            delete data_writer;
            delete meta_writer;
            PY_Py_RETURN_NONE;
        }

        delete data_writer;
        delete meta_writer;

        PY_PyObject    *result = PY_PyTuple_New(2);
        PY_PyTuple_SetItem(result, 0, PY_PyString_FromString(data_dictionary.buffer()));
        PY_PyTuple_SetItem(result, 1, PY_PyString_FromString(meta_dictionary.buffer()));

        return result;
    }

    static const char	*Doc_AlembicTimeRange =
	"(start_time, end_time) = alembicTimeRange(abcPath, [objectPath=None])\n"
	"\n"
	"Returns None or a tuple (start_time, end_time). The tuple contains\n"
	"the global start and end times for the alembic archive, using the fps\n"
	"information in the archive. If an object path is provided, it computes\n"
	"the start and end times for the object.\n"
	"Returns None if the archive is constant.\n";

    PY_PyObject *
    Py_AlembicTimeRange(PY_PyObject *self, PY_PyObject *args)
    {
	const char		    *filename;
	const char		    *objectPath;
	UT_StringArray		    objects;

	objectPath = "";
	if (!PY_PyArg_ParseTuple(args, "s|s", &filename, &objectPath))
	    return NULL;

        PyWalker        walker;

	if (objectPath && *objectPath)
	{
	    objects.append(objectPath);
	    if (!GABC_Util::walk(filename, walker, objects))
		PY_Py_RETURN_NONE;
	}
	else
	{
	    if (!GABC_Util::walk(filename, walker))
		PY_Py_RETURN_NONE;
	}

	if (walker.computedValidTimeRange())
	{
	    PY_PyObject     *result = PY_PyTuple_New(2);
	    fpreal start            = walker.getStartTime();
	    fpreal end              = walker.getEndTime();

	    PY_PyTuple_SetItem(result, 0, PY_PyFloat_FromDouble(start));
	    PY_PyTuple_SetItem(result, 1, PY_PyFloat_FromDouble(end));

	    return result;
	}

	PY_Py_RETURN_NONE;
    }


    static const char	*Doc_AlembicVisibility =
	"(value, isConstant) = alembicVisibility(abcPath, objectPath, sampleTime, [check_ancestor=False])\n"
	"\n"
	"Returns None or a tuple (value,isConstant).  The tuple contains the\n"
	"visibility for the object, and a boolean flag indicating whether\n"
	"visibility is constant over the animation.  The visibility returned\n"
	"is an integer where 0 := hidden, 1 := visible, and -1 is deferred\n"
	"(dependent on parent visibility).\n";

    PY_PyObject *
    Py_AlembicVisibility(PY_PyObject *self, PY_PyObject *args)
    {
	const char			*filename;
	const char			*objectPath;
	double				 sampleTime;
	int				 check_parent;
	GT_DataArrayHandle		 data;

	// Usage: alembicVisiblity(file, object, time, [checkparent=False])
	check_parent = 0;
        if (!PY_PyArg_ParseTuple(args, "ssd|i", &filename, &objectPath,
                &sampleTime, &check_parent))
	{
	    return NULL;
	}

	GABC_IObject	obj = GABC_Util::findObject(filename, objectPath);
	if (!obj.valid())
	{
	    PY_Py_RETURN_NONE;
	}
	bool	animated = false;
	int	result = obj.visibility(animated, sampleTime,
					check_parent != 0);

	PY_PyObject	*rcode = PY_PyTuple_New(2);
	PY_PyTuple_SetItem(rcode, 0, PY_PyInt_FromLong(result));
	PY_PyTuple_SetItem(rcode, 1, animated ? PY_Py_False() : PY_Py_True());
	PY_Py_INCREF(rcode);
	return rcode;
    }

    //-*************************************************************************
    static const char	*Doc_AlembicGetSceneHierarchy =
	"alembicGetSceneHierarchy(abcPath, objectPath)\n"
	"\n"
	"Returns a tree of tuples.  Each tuple is constructed as:\n"
	"	node[0] := object_name\n"
	"	node[1] := object_type\n"
	"	node[2] := (children)\n"
	"Where (children) is a tuple containing the child nodes.\n"
	"Object types include (but may contain other types):\n"
	"  - cxform    A constant transform node\n"
	"  - xform     An animated transform node\n"
	"  - camera    A camera node\n"
	"  - polymesh  A Polygon Mesh shape node\n"
	"  - subdmesh  A Subdivision Surface Mesh shape node\n"
	"  - faceset   A Face Set shape node\n"
	"  - curves    A Curves shape node\n"
	"  - points    A Points shape node\n"
	"  - nupatch   A NuPatch shape node\n"
	"  - unknown   An unknown node";

    PY_PyObject *
    Py_AlembicGetSceneHierarchy(PY_PyObject *self, PY_PyObject *args)
    {
        PY_PyObject    *result;
        const char     *archivePath = NULL;
        const char     *objectPath = NULL;
        if (!PY_PyArg_ParseTuple(args, "ss", &archivePath, &objectPath))
	    PY_Py_RETURN_NONE;

	PyWalker	walker;
	UT_StringArray	objects;
	objects.append(objectPath);
	GABC_Util::walk(archivePath, walker, objects);

	result = walker.getObject();
	if (result)
	{
	    return walker.getObject();
	}

	PY_Py_RETURN_NONE;
    }

    //-*************************************************************************
    static const char	*Doc_AlembicClearArchiveCache =
	"alembicClearArchiveCache(abcPath)\n"
	"\n"
	"Clear the internal cache of Alembic files";

    PY_PyObject *
    Py_AlembicClearArchiveCache(PY_PyObject *self, PY_PyObject *args)
    {
	PY_PyObject *fileList = nullptr;
        if (!PY_PyArg_ParseTuple(args, "|O", &fileList))
	    PY_Py_RETURN_NONE;

	if(fileList)
	{
	    std::vector<std::string> filenames;
	    appendFileList(filenames, fileList);
	    for(auto &s : filenames)
		GABC_Util::clearCache(s.c_str());
	}
	else
	    GABC_Util::clearCache();

        PY_Py_RETURN_NONE;
    }

    static const char	*Doc_AlembicSetArchiveMaxCacheSize =
	"alembicSetArchiveMaxCacheSize(size)\n"
	"\n"
	"Set the maximum number of Alembic files cached at one time.";

    PY_PyObject *
    Py_AlembicSetArchiveMaxCacheSize(PY_PyObject *self, PY_PyObject *args)
    {
        unsigned int value;

        if (!PY_PyArg_ParseTuple(args, "I", &value))
	    return NULL;

	GABC_Util::setFileCacheSize(value);

        PY_Py_RETURN_NONE;
    }

    static const char	*Doc_AlembicGetArchiveMaxCacheSize =
	"alembicGetArchiveMaxCacheSize()\n"
	"\n"
	"Return the Alembic files cache size.";

    PY_PyObject *
    Py_AlembicGetArchiveMaxCacheSize(PY_PyObject *self, PY_PyObject *args)
    {
        return PY_PyInt_FromLong(GABC_Util::fileCacheSize());
    }

    //-*************************************************************************

    static const char	*Doc_AlembicGetObjectPathListForMenu =
	"alembicGetObjectPathListForMenu(abcPath)\n"
	"\n"
	"Returns a tuple of strings in the form expected for menu callbacks.\n"
	"That is, each object is duplicated as a token/label pair.";

    PY_PyObject *
    Py_AlembicGetObjectPathListForMenu(PY_PyObject *self, PY_PyObject *args)
    {
        const char *filename = NULL;
        if (!PY_PyArg_ParseTuple(args, "s", &filename))
	    return NULL;

	// TODO: Would be nice to have a tree instead of a flat list
	const GABC_Util::PathList &objects = GABC_Util::getObjectList(filename);
	exint		 nobj = objects.size();
	PY_PyObject	*result = PY_PyList_New(nobj*2);
	for (exint i = 0; i < nobj; ++i)
	{
	    PY_PyObject	*name = PY_PyString_FromString(objects[i].c_str());
	    PY_Py_INCREF(name);	// For second reference in list
	    PY_PyList_SetItem(result, i*2, name);	// Token
	    PY_PyList_SetItem(result, i*2+1, name);	// Label
	}

        return result;
    }

    //-*************************************************************************

    /// This class reads Alembic camera object samples. Alembic cameras
    /// are based off of Maya cameras, so this class processes the data into a
    /// form more suitable for Houdini cameras. It can also blend the current
    /// camera sample with another.
    class HoudiniCam
    {
    public:
	HoudiniCam(CameraSample &sample)
	{
            const CH_Manager *chman = OPgetDirector()->getChannelManager();

	    fpreal  winx = sample.getHorizontalFilmOffset()
	                    / sample.getHorizontalAperture();
	    fpreal  winy = sample.getVerticalFilmOffset()
	                    / sample.getVerticalAperture();

            myLensSqueezeRatio = sample.getLensSqueezeRatio();
	    myAperture = sample.getHorizontalAperture() * 10.0 * myLensSqueezeRatio;
	    myFocal = sample.getFocalLength();
	    myFocus = sample.getFocusDistance();
            myFStop = sample.getFStop();
	    myFilmAspectRatio = sample.getHorizontalAperture() / sample.getVerticalAperture();
	    myClipping[0] = sample.getNearClippingPlane();
	    myClipping[1] = sample.getFarClippingPlane();

            // For Alembic camera, shutter open/close time are stored in seconds.
            // For Houdini camera, shutter time is stored in frames.   
            myShutter = chman->getSampleDelta(sample.getShutterClose() - sample.getShutterOpen());

	    //TODO, full 2D transformations
	    V2d postScale(1.0, 1.0);
	    for ( size_t i = 0; i < sample.getNumOps(); ++i )
	    {
		const FilmBackXformOp	&op = sample.getOp(i);

		if (op.isScaleOp()
		    && ((op.getHint().compare(0, 7, "filmFit"))
		        || ((op.getHint().compare(7, 4, "Fill"))
		            && (op.getHint().compare(7, 4, "Horz"))
		            && (op.getHint().compare(7, 4, "Over"))
		            && (op.getHint().compare(7, 4, "Vert")))
		    ))
                {
                    postScale *= op.getScale();
                }
	    }

	    //TODO overscan

	    myWinOffset[0] = winx;
	    myWinOffset[1] = winy;
	    myWinSize[0] = 1.0 / postScale[0];
	    myWinSize[1] = 1.0 / postScale[1];
	}

	void            blend(const HoudiniCam &src, fpreal w)
	{
            myLensSqueezeRatio = SYSlerp(myLensSqueezeRatio, src.myLensSqueezeRatio, w);
	    myAperture = SYSlerp(myAperture, src.myAperture, w);
	    myFocal = SYSlerp(myFocal, src.myFocal, w);
	    myFocus = SYSlerp(myFocus, src.myFocus, w);
            myFStop = SYSlerp(myFStop, src.myFStop, w);
	    myFilmAspectRatio = SYSlerp(myFilmAspectRatio, src.myFilmAspectRatio, w);
	    myClipping[0] = SYSlerp(myClipping[0], src.myClipping[0], w);
	    myClipping[1] = SYSlerp(myClipping[1], src.myClipping[1], w);
	    myWinOffset[0] = SYSlerp(myWinOffset[0], src.myWinOffset[0], w);
	    myWinOffset[1] = SYSlerp(myWinOffset[1], src.myWinOffset[1], w);
	    myWinSize[0] = SYSlerp(myWinSize[0], src.myWinSize[0], w);
	    myWinSize[1] = SYSlerp(myWinSize[1], src.myWinSize[1], w);
            myShutter = SYSlerp(myShutter, src.myShutter, w);
	}

	static void	setItem(PY_PyObject *dict, const char *key, fpreal val)
	{
	    PY_PyObject	*v = PY_PyFloat_FromDouble(val);
	    PY_PyDict_SetItemString(dict, key, v);
	    PY_Py_DECREF(v);
	}

	void            setDict(PY_PyObject *dict) const
	{
            setItem(dict, "aspect", myLensSqueezeRatio);
	    setItem(dict, "aperture", myAperture);
	    setItem(dict, "focal", myFocal);
	    setItem(dict, "focus", myFocus);
            setItem(dict, "fstop", myFStop);
	    setItem(dict, "filmaspectratio", myFilmAspectRatio);
	    setItem(dict, "near", myClipping[0]);
	    setItem(dict, "far", myClipping[1]);
	    setItem(dict, "winx", myWinOffset[0]);
	    setItem(dict, "winy", myWinOffset[1]);
	    setItem(dict, "winsizex", myWinSize[0]);
	    setItem(dict, "winsizey", myWinSize[1]);
            setItem(dict, "shutter", myShutter);
	}

        fpreal  myLensSqueezeRatio;
	fpreal	myAperture;
	fpreal	myFocal;
	fpreal	myFocus;
        fpreal  myFStop;
	fpreal  myFilmAspectRatio;
	fpreal	myClipping[2];
	fpreal	myWinOffset[2];
	fpreal	myWinSize[2];
        fpreal  myShutter;
    };

    // Determine the two samples that should be blended to create the sample
    // for the given time, and the bias towards the first sample (if any) as a
    // factor of time.
    static const fpreal	theTimeBias = 0.0001;
    static fpreal
    blendTime(fpreal t,
	    const TimeSamplingPtr &itime,
	    exint nsamp,
	    index_t &i0,
	    index_t &i1)
    {
	nsamp = SYSmax(nsamp, 1);

	std::pair<index_t, chrono_t>	t0 = itime->getFloorIndex(t, nsamp);
	i0 = i1 = t0.first;
	if (nsamp == 1 || SYSisEqual(t, t0.second, theTimeBias))
	    return 0;

	std::pair<index_t, chrono_t>	t1 = itime->getCeilIndex(t, nsamp);
	i1 = t1.first;
	if (i0 == i1)
	    return 0;

	fpreal	bias = (t - t0.second) / (t1.second - t0.second);
	if (SYSisEqual(bias, 1, theTimeBias))
	{
	    i0 = i1;
	    return 0;
	}
	return bias;
    }

    static const char	*Doc_AlembicGetCameraDict =
	"alembicGetCameraDict(abcPath, objectPath, sampleTime)\n"
	"\n"
	"Returns a dictionary of camera parameters for the given object.";

    PY_PyObject *
    Py_AlembicGetCameraDict(PY_PyObject *self, PY_PyObject *args)
    {
	const char	*archivePath = NULL;
	const char	*objectPath = NULL;
	double		 sampleTime = 0.0;
	PY_PyObject	*resultDict = PY_PyDict_New();

        if (!PY_PyArg_ParseTuple(args, "ssd", &archivePath, &objectPath,
                &sampleTime)) return NULL;

	try
	{
	    GABC_IObject    obj = GABC_Util::findObject(archivePath, objectPath);

	    if (obj.valid() && ICamera::matches(obj.getHeader()))
	    {
		ICamera camera(obj.object(), Alembic::Abc::kWrapExisting);
		ICameraSchema	schema = camera.getSchema();
		index_t	i0, i1;
		fpreal	bias = blendTime(sampleTime,
				    schema.getTimeSampling(),
				    schema.getNumSamples(), i0, i1);
		CameraSample	sample0 = schema.getValue(ISampleSelector(i0));
		HoudiniCam	hcam0(sample0);
		if (i0 != i1)
		{
		    CameraSample s1 = schema.getValue(ISampleSelector(i1));
		    hcam0.blend(HoudiniCam(s1), bias);
		}
		hcam0.setDict(resultDict);
	    }
	}
	catch (const std::exception &e)
	{
	    PY_PyObject *val = PY_PyString_FromString(e.what());
	    PY_PyDict_SetItemString(resultDict, "error", val);
	    PY_Py_DECREF(val);
	}

        return resultDict;
    }

    static const char   *Doc_AlembicGetCameraResolution = 
        "alembicGetCameraResolution(abcPath, objectPath, sampleTime)\n"
        "\n"
        "Returns None or a Tuple containing two floats.\n"
        "The first value is resolution x of the Houdini Camera.\n"
        "The second value is resolution y of the Houdini Camera.\n"
        "Some camera (ex. Maya Camera) does not have a resolution,\n" 
        "in this case, None is returned.\n";

    PY_PyObject *
    Py_AlembicGetCameraResolution(PY_PyObject *self, PY_PyObject *args)
    {
        const char        *filename;
        const char        *objectPath;
        double             sampleTime;

        if (!PY_PyArg_ParseTuple(args, 
                "ssd", 
                &filename, 
                &objectPath, 
                &sampleTime))
        {
            return NULL;
        }

	GABC_IObject obj = GABC_Util::findObject(filename, objectPath);
        if (!obj.valid())
        {
            PY_Py_RETURN_NONE;
        }

        if(obj.nodeType() != GABC_CAMERA)
	{
            PY_Py_RETURN_NONE;
	}

	ICamera camera(obj.object(), gabcWrapExisting);
	ICameraSchema schema = camera.getSchema();
        ICompoundProperty uprops = schema.getUserProperties();

        if (!uprops || uprops.getNumProperties() == 0)
        {
            PY_Py_RETURN_NONE;
        }

        CompoundPropertyReaderPtr userPropPtr = GetCompoundPropertyReaderPtr(uprops);
        ScalarPropertyReaderPtr resxPtr = userPropPtr->getScalarProperty("resx");
        ScalarPropertyReaderPtr resyPtr = userPropPtr->getScalarProperty("resy");

        if (!resxPtr || !resyPtr)
        {
            PY_Py_RETURN_NONE;
        }

        Alembic::Util::float32_t resx = 0;
        Alembic::Util::float32_t resy = 0;
        resxPtr->getSample(resxPtr->getNearIndex(sampleTime).first, &resx);
        resyPtr->getSample(resyPtr->getNearIndex(sampleTime).first, &resy);

        PY_PyObject    *result = PY_PyTuple_New(2);
        PY_PyTuple_SetItem(result, 0, PY_PyFloat_FromDouble(resx));
        PY_PyTuple_SetItem(result, 1, PY_PyFloat_FromDouble(resy));

        return result;
    }
}

void
HOMextendLibrary()
{
    {
    // A PY_InterpreterAutoLock will grab the Python global interpreter
    // lock (GIL).  It's important that we have the GIL before making
    // any calls into the Python API.
    PY_InterpreterAutoLock interpreter_auto_lock;

    static PY_PyMethodDef alembic_hom_extension_methods[] =
    {
        {"getLocalXform", Py_GetLocalXform,
                PY_METH_VARARGS(), Doc_GetLocalXform},
        {"getWorldXform", Py_GetWorldXform,
                PY_METH_VARARGS(), Doc_GetWorldXform},
        {"alembicGetLocalXform", Py_AlembicGetLocalXform,
                PY_METH_VARARGS(), Doc_AlembicGetLocalXform},
        {"alembicGetSceneHierarchy", Py_AlembicGetSceneHierarchy,
                PY_METH_VARARGS(), Doc_AlembicGetSceneHierarchy},

        {"alembicClearArchiveCache", Py_AlembicClearArchiveCache,
                PY_METH_VARARGS(), Doc_AlembicClearArchiveCache},
        {"alembicSetArchiveMaxCacheSize", Py_AlembicSetArchiveMaxCacheSize,
                PY_METH_VARARGS(), Doc_AlembicSetArchiveMaxCacheSize},
        {"alembicGetArchiveMaxCacheSize", Py_AlembicGetArchiveMaxCacheSize,
                PY_METH_VARARGS(), Doc_AlembicGetArchiveMaxCacheSize}, 
        {"alembicGetObjectPathListForMenu", Py_AlembicGetObjectPathListForMenu,
                PY_METH_VARARGS(), Doc_AlembicGetObjectPathListForMenu },
        {"alembicGetCameraDict", Py_AlembicGetCameraDict,
                PY_METH_VARARGS(), Doc_AlembicGetCameraDict },
        {"alembicGetCameraResolution", Py_AlembicGetCameraResolution,
                PY_METH_VARARGS(), Doc_AlembicGetCameraResolution },   

	{ "alembicArbGeometry", Py_AlembicArbGeometry,
		PY_METH_VARARGS(), Doc_AlembicArbGeometry },
        { "alembicHasUserProperties", Py_AlembicHasUserProperties,
                PY_METH_VARARGS(), Doc_AlembicHasUserProperties},
	{ "alembicUserProperty", Py_AlembicUserProperty,
		PY_METH_VARARGS(), Doc_AlembicUserProperty },
        { "alembicUserPropertyValues", Py_AlembicUserPropertyValues,
                PY_METH_VARARGS(), Doc_AlembicUserPropertyValues},
        { "alembicUserPropertyMetadata", Py_AlembicUserPropertyMetadata,
                PY_METH_VARARGS(), Doc_AlembicUserPropertyMetadata},
        { "alembicUserPropertyValuesAndMetadata", Py_AlembicUserPropertyValuesAndMetadata,
                PY_METH_VARARGS(), Doc_AlembicUserPropertyValuesAndMetadata},

	{ "alembicVisibility",	Py_AlembicVisibility,
		PY_METH_VARARGS(), Doc_AlembicVisibility },
	{ "alembicTimeRange", Py_AlembicTimeRange,
		PY_METH_VARARGS(), Doc_AlembicTimeRange },

        { NULL, NULL, 0, NULL }
    };
    PY_Py_InitModule("_alembic_hom_extensions", alembic_hom_extension_methods);
    }


    PYrunPythonStatementsAndExpectNoErrors(
    "def _alembicGetCameraDict(self, archivePath, objectPath, sampleTime):\n"
    "    '''Return camera information.'''\n"
    "    import _alembic_hom_extensions\n"
    "    return _alembic_hom_extensions.alembicGetCameraDict(archivePath, objectPath, sampleTime)\n"
    "__import__('hou').ObjNode.alembicGetCameraDict = _alembicGetCameraDict\n"
    "del _alembicGetCameraDict\n");
}
