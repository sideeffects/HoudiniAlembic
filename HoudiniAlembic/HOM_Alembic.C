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

// The python developers recomment that Python.h be includede before any other
// header files.
#include <PY/PY_CPythonAPI.h>
// This file contains functions that will run arbitrary Python code
#include <PY/PY_Python.h>
#include <PY/PY_InterpreterAutoLock.h>
#include <GABC/GABC_Util.h>
#include <Alembic/AbcGeom/All.h>
#include <HOM/HOM_Module.h>
#include <UT/UT_DSOVersion.h>

namespace
{
    typedef Alembic::Abc::IObject		IObject;
    typedef Alembic::Abc::V2d			V2d;
    typedef Alembic::Abc::ObjectHeader		ObjectHeader;
    typedef Alembic::Abc::ISampleSelector	ISampleSelector;
    typedef Alembic::AbcGeom::IXform		IXform;
    typedef Alembic::AbcGeom::ICamera		ICamera;
    typedef Alembic::AbcGeom::CameraSample	CameraSample;
    typedef Alembic::AbcGeom::FilmBackXformOp	FilmBackXformOp;

    static PY_PyObject *
    alembicGetXform(PY_PyObject *self, PY_PyObject *args, int result_size,
	    bool localx)
    {
	const char	*filename = NULL;
	const char	*objectPath = NULL;
	double		 sampleTime = 0.0;
	bool		 isConstant = true;
	bool		 inheritsXform = true;
	UT_Matrix4D	 xform;

        if (!PY_PyArg_ParseTuple(args, "ssd", &filename, &objectPath,
                &sampleTime))
	{
	    return NULL;
	}
	bool	ok;
	if (localx)
	    ok = GABC_Util::getLocalTransform(filename, objectPath,
		    sampleTime, xform, isConstant, inheritsXform);
	else
	    ok = GABC_Util::getWorldTransform(filename, objectPath,
		    sampleTime, xform, isConstant, inheritsXform);
	if (!ok)
	    xform.identity();

	const double	*data = xform.data();
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
	"Returns a tuple (xform,isConstant) containing the transform and a\n"
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
	"Returns a tuple (xform,isConstant,inherits) containing the transform\n"
	"and flags indicating whether the transform is animated/constant,\n"
	"along with whether the node inherit it's parent transform (or is\n"
	"disconnected from the transform hierarchy).";

    PY_PyObject *
    Py_GetLocalXform(PY_PyObject *self, PY_PyObject *args)
    {
	return alembicGetXform(self, args, 3, true);
    }

    static const char	*Doc_GetWorldXform =
	"(xform, isConstant, inherit) = getWorldXform(abcPath, objectPath, sampleTime)\n"
	"\n"
	"Returns a tuple (xform,isConstant,inherits) containing the object to\n"
	"world transform flags indicating whether the transform is animated\n"
	"or constant, along with whether the node inherit it's parent\n"
	"transform (or is disconnected from the transform hierarchy).";

    PY_PyObject *
    Py_GetWorldXform(PY_PyObject *self, PY_PyObject *args)
    {
	return alembicGetXform(self, args, 3, false);
    }


    class PyWalker : public GABC_Util::Walker
    {
    public:
	PyWalker()
	    : myRoot(NULL)
	{
	}
	~PyWalker()
	{
	}
	virtual bool	process(const IObject &root)
			{
			    myRoot = walkNode(root);
			    return false;
			}

	PY_PyObject	*walkNode(const IObject &obj)
	{
	    const char		*otype = "<unknown>";
	    switch (GABC_Util::getNodeType(obj))
	    {
		case GABC_XFORM:
		    {
			IXform	xform(obj, Alembic::Abc::kWrapExisting);
			if (xform.getSchema().isConstant())
			    otype = "cxform";
			else
			    otype = "xform";
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

	    exint nkids = const_cast<IObject *>(&obj)->getNumChildren();
	    PY_PyObject *result = PY_PyTuple_New(3);
	    PY_PyObject *kids = PY_PyTuple_New(nkids);;

	    PY_PyTuple_SET_ITEM(result, 0,
		    PY_PyString_FromString(obj.getName().c_str()));
	    PY_PyTuple_SET_ITEM(result, 1, PY_PyString_FromString(otype));
	    PY_PyTuple_SET_ITEM(result, 2, kids);

	    for (exint i = 0; i < nkids; ++i)
	    {
		PY_PyTuple_SET_ITEM(kids, i,
			walkNode(const_cast<IObject *>(&obj)->getChild(i)));
	    }
	    return result;
	}
	PY_PyObject	*getObject() const	{ return myRoot; }

    private:
	PY_PyObject	*myRoot;
    };

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
        const char * archivePath = NULL;
        const char * objectPath = NULL;
        if (!PY_PyArg_ParseTuple(args, "ss", &archivePath, &objectPath))
	    return NULL;

	PyWalker	walker;
	UT_StringArray	objects;
	objects.append(objectPath);
	GABC_Util::walk(archivePath, walker, objects);
	return walker.getObject();
    }

    //-*************************************************************************
    static const char	*Doc_AlembicClearArchiveCache =
	"alembicClearArchiveCache()\n"
	"\n"
	"Clear the internal cache of Alembic files";

    PY_PyObject *
    Py_AlembicClearArchiveCache(PY_PyObject *self, PY_PyObject *args)
    {
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
	bool		 isConstant = true;
	PY_PyObject	*resultDict = PY_PyDict_New();

        if (!PY_PyArg_ParseTuple(args, "ssd", &archivePath, &objectPath,
                &sampleTime)) return NULL;

	IObject	obj = GABC_Util::findObject(archivePath, objectPath);

	if (obj.valid() && ICamera::matches(obj.getHeader()))
	{
	    ICamera camera(obj, Alembic::Abc::kWrapExisting);
	    isConstant = camera.getSchema().isConstant();
	    CameraSample cameraSample = camera.getSchema().getValue(
		    ISampleSelector(sampleTime));

	    //Express in houdini terms?
	    PY_PyObject * val = NULL;

	    val = PY_PyFloat_FromDouble(cameraSample.getFocalLength());
	    PY_PyDict_SetItemString(resultDict, "focal", val);
	    PY_Py_DECREF(val);

	    val = PY_PyFloat_FromDouble(cameraSample.getNearClippingPlane());
	    PY_PyDict_SetItemString(resultDict, "near", val);
	    PY_Py_DECREF(val);

	    val = PY_PyFloat_FromDouble(cameraSample.getFarClippingPlane());
	    PY_PyDict_SetItemString(resultDict, "far", val);
	    PY_Py_DECREF(val);

	    val = PY_PyFloat_FromDouble(cameraSample.getFocusDistance());
	    PY_PyDict_SetItemString(resultDict, "focus", val);
	    PY_Py_DECREF(val);

	    double top, bottom, left, right;
	    cameraSample.getScreenWindow(top, bottom, left, right);

	    double winx = cameraSample.getHorizontalFilmOffset() *
		    cameraSample.getLensSqueezeRatio() /
			    cameraSample.getHorizontalAperture();

	    double winy = cameraSample.getVerticalFilmOffset() *
		    cameraSample.getLensSqueezeRatio() /
			    cameraSample.getVerticalAperture();

	    //TODO, full 2D transformations
	    V2d postScale(1.0, 1.0);
	    for ( size_t i = 0; i < cameraSample.getNumOps(); ++i )
	    {
		const FilmBackXformOp	&op = cameraSample.getOp(i);

		if ( op.isScaleOp() )
		{
		    postScale *= op.getScale();
		}
	    }

	    //TODO overscan
	    double winsizex = cameraSample.getLensSqueezeRatio() / postScale[0];
	    double winsizey = cameraSample.getLensSqueezeRatio() / postScale[1];

	    val = PY_PyFloat_FromDouble(winx);
	    PY_PyDict_SetItemString(resultDict, "winx", val);
	    PY_Py_DECREF(val);

	    val = PY_PyFloat_FromDouble(winy);
	    PY_PyDict_SetItemString(resultDict, "winy", val);
	    PY_Py_DECREF(val);

	    val = PY_PyFloat_FromDouble(winsizex);
	    PY_PyDict_SetItemString(resultDict, "winsizex", val);
	    PY_Py_DECREF(val);

	    val = PY_PyFloat_FromDouble(winsizey);
	    PY_PyDict_SetItemString(resultDict, "winsizey", val);
	    PY_Py_DECREF(val);

	    val = PY_PyFloat_FromDouble(
		    cameraSample.getHorizontalAperture()*10.0);
	    PY_PyDict_SetItemString(resultDict, "aperture", val);
	    PY_Py_DECREF(val);
	}

        return resultDict;
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
