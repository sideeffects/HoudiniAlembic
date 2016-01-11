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
 * NAME:	ROP_AbcGTInstance.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#ifndef __ROP_AbcGTInstance__
#define __ROP_AbcGTInstance__

#include "ROP_AbcObject.h"
#include "ROP_AbcContext.h"
#include <GT/GT_Handles.h>

class ROP_AbcGTCompoundShape;

/// The single GT_Primitive owned by a ROP_AbcGTShape may represent instanced
/// geometry. If so, the ROP_AbcGTShape will create an instance of this class
/// as a child. This class creates a child ROP_AbcGTCompoundShape for the
/// geometry (since the packed geometry can itself be a heterogeneous mix
/// of primitives) and one or more Alembic Xform objects which reference the
/// geometry as a child object.
class ROP_AbcGTInstance
{
public:
    typedef GABC_NAMESPACE::GABC_OError             GABC_OError;

    typedef Alembic::Abc::M44d                      M44d;

    typedef Alembic::Abc::OObject	            OObject;

    typedef Alembic::AbcGeom::ObjectVisibility      ObjectVisibility;
    typedef Alembic::AbcGeom::OVisibilityProperty   OVisibilityProperty;
    typedef Alembic::AbcGeom::OXform                OXform;
    typedef Alembic::AbcGeom::XformSample           XformSample;

    /// Helper class that stores an OXform object and visibility property for
    /// each instance of the geometry.
    class Instance
    {
    public:
	 Instance(bool geo_lock)
	     : myOXform()
	     , myGeoLock(geo_lock)
	 {}

	~Instance() {}

	void	                first(const OObject &parent,
			                GABC_OError &err,
			                const ROP_AbcContext &ctx,
			                const UT_Matrix4D &xform,
			                const std::string &name,
			                ObjectVisibility vis);
	ROP_AbcGTCompoundShape *setGeometry(GABC_OError &err,
					const ROP_AbcContext &ctx,
					const GT_PrimitiveHandle &g,
					const std::string &name,
					bool subd_mode,
					bool add_unused_pts);
	void                    setGeometry(const OObject &child,
	                                const std::string &name);
	void                    update(const UT_Matrix4D &xform,
                                        ObjectVisibility vis);
	void                    updateFromPrevious(ObjectVisibility vis);

	OXform	               &oxform() { return myOXform; }

    private:
        OVisibilityProperty myVisibility;
	OXform              myOXform;
	bool                myGeoLock;
    };

    ROP_AbcGTInstance(const std::string &name, bool geo_lock);
    ~ROP_AbcGTInstance();

    /// Output the first frame to Alembic. Does most of the setup.
    bool	first(const OObject &parent,
			GABC_OError &err,
			const ROP_AbcContext &ctx,
			const GT_PrimitiveHandle &prim,
			bool subd_mode,
			bool add_unused_pts,
                        ObjectVisibility vis = Alembic::AbcGeom::kVisibilityDeferred);
    /// Output an additional frame to Alembic.
    bool	update(const GT_PrimitiveHandle &prim,
			const ROP_AbcContext &ctx,
                        GABC_OError &err,
                        ObjectVisibility vis = Alembic::AbcGeom::kVisibilityDeferred);
    /// Output additional frames, reusing the data from the previous sample.
    bool	updateFromPrevious(GABC_OError &err,
                        int primType,
                        ObjectVisibility vis = Alembic::AbcGeom::kVisibilityHidden,
                        exint frames = 1);

    OObject	getOObject() const;
    exint	entries() const	{ return myInstances.entries(); }

    void	dump(int indent=0) const;

private:
    std::string             myName;
    OObject		    myParent;
    exint		    myElapsedFrames;
    ROP_AbcGTCompoundShape *myGeometry;
    UT_Array<Instance>      myInstances;
    const bool              myGeoLock;
};

#endif

