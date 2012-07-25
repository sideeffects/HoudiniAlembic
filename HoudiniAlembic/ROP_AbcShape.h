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
 * NAME:	ROP_AbcShape.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcShape__
#define __ROP_AbcShape__

#include "ROP_AlembicInclude.h"
#include <UT/UT_PtrArray.h>

class ROP_AbcGeometry;
class ROP_AbcError;
class SOP_Node;
class ROP_AbcContext;
class GU_Detail;

/// Class to store shapes for a SOP's geometry
class ROP_AbcShape
{
public:
     ROP_AbcShape(SOP_Node *sop, Alembic::AbcGeom::OXform *parent);
    ~ROP_AbcShape();

    void	 clear();
    void	 create(ROP_AbcError &err,
			const char *name,
			ROP_AbcContext &context,
			const Alembic::AbcGeom::TimeSamplingPtr &timeRange);

    bool	 writeSample(ROP_AbcError &err, ROP_AbcContext &context);
    bool	 isTimeDependent() const	{ return myTimeDependent; }

    int		 getCount() const	{ return myGeos.entries(); }
    ROP_AbcGeometry	*getGeometry(int i) const	{ return myGeos(i); }

private:
    UT_PtrArray<ROP_AbcGeometry *>	 myGeos;
    Alembic::AbcGeom::OXform		*myParent;
    SOP_Node				*mySop;
    bool				 myTimeDependent;
    bool				 myTopologyWarned;
};

#endif
