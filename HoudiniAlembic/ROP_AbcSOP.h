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

#ifndef __ROP_AbcSOP__
#define __ROP_AbcSOP__

#include "ROP_AbcContext.h"
#include "ROP_AbcGTCompoundShape.h"
#include "ROP_AbcObject.h"
#include <GA/GA_Range.h>
#include <GA/GA_OffsetList.h>
#include <GABC/GABC_OGTAbc.h>
#include <GT/GT_Handles.h>
#include <UT/UT_Array.h>
#include <UT/UT_StringHolder.h>

class SOP_Node;
class ROP_AbcGTShape;
class ROP_AbcGTCompoundShape;

/// Alembic geometry contained within a Houdini SOP node. The geometry contained
/// in the SOP node is partitioned here into multiple child nodes if the
/// "Partition Mode" or "Build Hierarchy From Attribute" options are enabled.
/// Otherwise, a ROP_AbcSOP object should have only one child.
class ROP_AbcSOP : public ROP_AbcObject
{
public:
    class abc_PrimContainer;

    typedef Alembic::Abc::OObject                       OObject;

    typedef ROP_AbcGTCompoundShape::InverseMap          InverseMap;
    typedef ROP_AbcGTCompoundShape::InverseMapInsert    InverseMapInsert;
    typedef ROP_AbcGTCompoundShape::GeoSet              GeoSet;
    typedef ROP_AbcGTCompoundShape::XformMap            XformMap;
    typedef ROP_AbcGTCompoundShape::XformMapInsert      XformMapInsert;
    typedef ROP_AbcGTCompoundShape::XformUserPropsMap	XformUserPropsMap;
    typedef ROP_AbcGTCompoundShape::XformUserPropsMapInsert	XformUserPropsMapInsert;

    typedef std::vector<std::string>                    NameList;
    typedef UT_Map<std::string, int>                    NameMap;
    typedef std::pair<std::string, int>                 NameMapInsert;
    typedef UT_Map<std::string, int>                    PartitionMap;
    typedef std::pair<std::string, int>                 PartitionMapInsert;
    typedef UT_Array<abc_PrimContainer>                 PrimitiveList;

    /// Helper class stores a GT_PrimitiveHandle to a GT_GEODetail containing
    /// all geometry in a single partition, as well as additional information
    /// about how to output that geometry.
    class abc_PrimContainer
    {
    public:
	abc_PrimContainer(const GT_PrimitiveHandle &prim,
	        const std::string &identifier,
	        bool has_alembic,
	        bool is_partition,
                bool subd_mode,
                bool show_unused_points)
	    : myPrim(prim)
	    , myIdentifier(identifier)
	    , myHasAlembic(has_alembic)
	    , myIsPartition(is_partition)
	    , mySubdMode(subd_mode)
	    , myShowPts(show_unused_points)
	{}

	GT_PrimitiveHandle	myPrim;
	std::string             myIdentifier;
	bool                    myHasAlembic;
	bool                    myIsPartition;
	bool			mySubdMode;
	bool			myShowPts;
    };

    ROP_AbcSOP(SOP_Node *node);
    virtual ~ROP_AbcSOP();

    /// @{
    /// Interface defined on ROP_AbcObject
    virtual const char	*className() const	{ return "SOP"; }
    virtual bool	start(const OObject &parent,
				GABC_OError &err,
				const ROP_AbcContext &ctx,
				UT_BoundingBox &box);
    virtual bool	update(GABC_OError &err,
				const ROP_AbcContext &ctx,
				UT_BoundingBox &box);
    virtual bool	selfTimeDependent() const;
    virtual bool	getLastBounds(UT_BoundingBox &box) const;
    /// @}

    virtual void	dump(int indent) const;

private:
    /// Clear all stored data and reset the object.
    void		clear();
    /// Router method for partitionGeometryRange, determines what the
    /// arguments to it should be.
    void                partitionGeometry(PrimitiveList &primitives,
                                const SOP_Node *sop,
                                const GU_ConstDetailHandle &gdh,
				const GU_Detail &gdp,
                                const ROP_AbcContext &ctx,
                                GABC_OError &err);
    /// Partitions the geometry in the range into one or more groups. A child
    /// ROP_AbcGTCompoundShape will be created for each group.
    void                partitionGeometryRange(PrimitiveList &primitives,
                                const GU_ConstDetailHandle &gdh,
				const GU_Detail &gdp,
                                const GA_Range &range,
                                const ROP_AbcContext &ctx,
                                GABC_OError &err,
                                bool force_subd_mode,
                                bool show_pts);

    ROP_AbcGTCompoundShape	*newShape(const abc_PrimContainer &prim,
				    const ROP_AbcContext &ctx,
				    GABC_OError &err,
				    Alembic::AbcGeom::ObjectVisibility vis = Alembic::AbcGeom::kVisibilityDeferred);

    InverseMap                          myInverseMap;
    GeoSet                              myGeoSet;
    NameList                            myPartitionNames;
    NameMap                             myNameMap;
    OObject				myParent;
    PartitionMap                        myPartitionMap;
    UT_Array<GA_OffsetList>             myPartitionIndices;
    UT_Array<ROP_AbcGTCompoundShape *>	myShapes;
    UT_BoundingBox			myBox;
    XformMap                            myXformMap;
    XformUserPropsMap			myXformUserPropsMap;
    int					myGeoLock;
    int					mySopId;
    UT_StringHolder                     myPathAttribName;
    bool				myTimeDependent;
};

#endif
