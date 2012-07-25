/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *	Side Effects Software Inc
 *	123 Front Street West, Suite 1401
 *	Toronto, Ontario
 *	Canada   M5J 2M2
 *	416-504-9876
 *
 * NAME:	ROP_AbcCamera.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcCamera__
#define __ROP_AbcCamera__

#include "ROP_AlembicInclude.h"

class ROP_AbcContext;
class OBJ_Camera;
class ROP_AbcError;
class UT_WorkBuffer;

/// Stores an alembic transform (Houdini object)
class ROP_AbcCamera
{
public:
     ROP_AbcCamera(OBJ_Camera *obj,
		Alembic::AbcGeom::OObject parent,
		Alembic::AbcGeom::TimeSamplingPtr time_s,
		bool time_dep);
    ~ROP_AbcCamera();

    static bool		isCameraTimeDependent(OBJ_Camera *cam);

    OBJ_Camera			*getObject() const;
    const char			*getFullPath(UT_WorkBuffer &path) const;
    Alembic::AbcGeom::OCamera	*getNode() const	{ return myCamera; }
    bool			 isTimeDependent() const
					{ return myTimeDependent; }

    bool	 writeSample(ROP_AbcError &err, ROP_AbcContext &context);

private:
    Alembic::AbcGeom::CameraSample	 mySample;
    Alembic::AbcGeom::OCamera		*myCamera;
    OBJ_Camera				*myObject;
    bool				 myTimeDependent;
};

#endif
