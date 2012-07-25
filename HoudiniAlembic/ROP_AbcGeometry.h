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
 * NAME:	ROP_AbcGeometry.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcGeometry__
#define __ROP_AbcGeometry__

#include <SYS/SYS_Types.h>
#include "ROP_AlembicInclude.h"
#include <GT/GT_Handles.h>
#include <GT/GT_PrimitiveTypes.h>
#include <UT/UT_SymbolTable.h>

class UT_WorkBuffer;
class SOP_Node;
class ROP_AbcError;
class ROP_AbcContext;

/// Container for Alembic shape nodes
///
/// Since a SOP can create multiple shape nodes, the ROP_AbcShape node needs to
/// store multiple geometry nodes.  Each geometry node corresponds to a given
/// Alembic shape node.
class ROP_AbcGeometry
{
public:
     ROP_AbcGeometry(ROP_AbcError &err,
		    SOP_Node *sop,
		    const char *name,
		    const GT_PrimitiveHandle &prim,
		    Alembic::AbcGeom::OXform *parent,
		    Alembic::AbcGeom::TimeSamplingPtr timesampling,
		    const ROP_AbcContext &context);
    ~ROP_AbcGeometry();

    bool	isValid() const;
    bool	writeSample(ROP_AbcError &err, const GT_PrimitiveHandle &prim);

private:
    void	setElementCounts(const GT_PrimitiveHandle &prim);

    bool	writePolygonMesh(ROP_AbcError &err,
			const GT_PrimitiveHandle &prim);
    bool	writeSubdMesh(ROP_AbcError &err,
			const GT_PrimitiveHandle &prim);
    bool	writeCurveMesh(ROP_AbcError &err,
			const GT_PrimitiveHandle &prim);
    bool	writePointMesh(ROP_AbcError &err,
			const GT_PrimitiveHandle &prim);
    bool	writeNuPatch(ROP_AbcError &err,
			const GT_PrimitiveHandle &prim);
    bool	writeLocator(ROP_AbcError &err,
			const GT_PrimitiveHandle &prim);

    void	writeProperties(const GT_PrimitiveHandle &prim);
    void	makeProperties(ROP_AbcError &err,
			const GT_PrimitiveHandle &prim,
			Alembic::AbcGeom::TimeSamplingPtr ts);
    void	clearProperties();

    Alembic::AbcGeom::OCurves	*myCurves;
    Alembic::AbcGeom::OPolyMesh	*myPolyMesh;
    Alembic::AbcGeom::OSubD	*mySubD;
    Alembic::AbcGeom::OPoints	*myPoints;
    Alembic::AbcGeom::ONuPatch	*myNuPatch;
    Alembic::AbcGeom::OXform	*myLocatorTrans;
    Alembic::AbcGeom::OXform	*myLocator;

    enum {
	VERTEX_PROPERTIES,
	POINT_PROPERTIES,
	UNIFORM_PROPERTIES,
	DETAIL_PROPERTIES,
	MAX_PROPERTIES
    };

    UT_String		 myName;
    UT_SymbolTable	 myProperties[MAX_PROPERTIES];

    exint		 myPointAttribs;	// Shared point attribute count
    exint		 myVertexAttribs;	// Vertex attribute count
    exint		 myUniformAttribs;	// Uniform attribute count
    exint		 myDetailAttribs;	// Uniform attribute count
    exint		 myPointCount;		// Shared point element count
    exint		 myVertexCount;		// Vertex element count
    exint		 myUniformCount;	// Uniform element count
    exint		 myDetailCount;		// Uniform element count

    GT_PrimitiveType	 myPrimitiveType;	// GT primitive type
    bool		 myTopologyWarned;	// Warned about topology
};

#endif

