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
 * NAME:	ROP_AbcXform.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcXform__
#define __ROP_AbcXform__

#include "ROP_AlembicInclude.h"
#include <UT/UT_SymbolTable.h>

class ROP_AbcContext;
class OBJ_Node;
class ROP_AbcCamera;
class ROP_AbcShape;
class ROP_AbcError;
class UT_WorkBuffer;

/// Stores an alembic transform (Houdini object)
class ROP_AbcXform
{
public:
     ROP_AbcXform(OBJ_Node *obj,
		const char *name,
		Alembic::AbcGeom::OObject parent,
		Alembic::AbcGeom::TimeSamplingPtr time_s,
		bool time_dep);
     // For place-holder (non-animated identity) nodes in the graph
     ROP_AbcXform(OBJ_Node *obj, ROP_AbcXform *parent);
    ~ROP_AbcXform();

    OBJ_Node			*getObject() const;
    const char			*getFullPath(UT_WorkBuffer &path) const;
    Alembic::AbcGeom::OXform	*getNode() const	{ return myXform; }
    ROP_AbcCamera		*getCamera() const	{ return myCamera; }
    ROP_AbcShape		*getShape() const	{ return myShape; }
    bool			 isTimeDependent() const
					{ return myTimeDependent; }

    ROP_AbcCamera	*addCamera(ROP_AbcError &err,
				    ROP_AbcContext &context,
				    Alembic::AbcGeom::TimeSamplingPtr trange);
    ROP_AbcShape	*addShape(ROP_AbcError &err,
				    ROP_AbcContext &context,
				    Alembic::AbcGeom::TimeSamplingPtr trange);

    bool		 writeSample(ROP_AbcError &err, ROP_AbcContext &context);

    const char		*getUniqueChildName(const char *base,
				UT_WorkBuffer &storage)
			{ return getUniqueName(*myChildNames, base, storage); }
    const char		*getUniqueShapeName(const char *base,
				UT_WorkBuffer &storage)
			{ return getUniqueName(*myChildNames, base, storage); }

private:
    const char		*getUniqueName(UT_SymbolTable &nametable,
				const char *base,
				UT_WorkBuffer &storage);

    Alembic::AbcGeom::OXform	*myXform;
    ROP_AbcCamera		*myCamera;
    ROP_AbcShape		*myShape;
    OBJ_Node			*myObject;
    UT_SymbolTable		*myChildNames;
    bool			 myTimeDependent;
    bool			 myIdentity;
};

#endif
