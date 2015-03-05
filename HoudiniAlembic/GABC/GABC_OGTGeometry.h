/*
 * Copyright (c) 2015
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

#ifndef __GABC_OGTGeometry__
#define __GABC_OGTGeometry__

#include "GABC_API.h"
#include "GABC_Include.h"
#include "GABC_Util.h"
#include <Alembic/AbcGeom/All.h>
#include <GT/GT_Primitive.h>
#include <UT/UT_StringSet.h>

namespace GABC_NAMESPACE
{

class GABC_OError;
class GABC_OOptions;
class GABC_OProperty;

/// This class will translate and output a GT_Primitive to Alembic.
/// Each time the update function is called, this class will write a sample
/// to it's Alembic object. If updateFromPrevious is called, the most recent
/// sample will be reused. Before either of these functions can be called, the
/// start function must be called to setup the Alembic OObject. Start will also
/// call update for the first time.
class GABC_API GABC_OGTGeometry
{
public:
    typedef Alembic::Abc::OCompoundProperty         OCompoundProperty;
    typedef Alembic::Abc::OObject		    OObject;

    typedef Alembic::AbcGeom::ObjectVisibility	    ObjectVisibility;
    typedef Alembic::AbcGeom::OVisibilityProperty   OVisibilityProperty;
    typedef Alembic::AbcGeom::OPolyMesh		    OPolyMesh;
    typedef Alembic::AbcGeom::OSubD		    OSubD;
    typedef Alembic::AbcGeom::OCurves		    OCurves;
    typedef Alembic::AbcGeom::OPoints		    OPoints;
    typedef Alembic::AbcGeom::ONuPatch		    ONuPatch;

    typedef GABC_Util::PropertyMap                  PropertyMap;
    typedef GABC_Util::PropertyMapInsert            PropertyMapInsert;

    /// A simple set of strings
    class IgnoreList
	: public UT_StringSet
    {
    public:
        IgnoreList()
	    : UT_StringSet()
        {}
        IgnoreList(const char *arg0, ...);
        ~IgnoreList() {}

        void    addSkip(const UT_StringHolder &skip)
		    { insert(skip); }
        bool    deleteSkip(const UT_StringHolder &skip)
		    { return erase(skip); }
        bool	contains(const UT_StringHolder &token) const
		    { return count(token) > 0; }
        bool	contains(const char *token) const
		    { return count(UTmakeUnsafeRef(token)) > 0; }
    };

    /// The intrinsic cache is used to cache array values frame to frame when
    /// optimizing the .abc file for space.  Only arrays which change will be
    /// written to the file.  The cache has storage for most primitive types.
    class GABC_API IntrinsicCache
    {
    public:
	 IntrinsicCache() {}
	~IntrinsicCache() { clear(); }

	void	clear();

	/// @{
	/// Test whether topology arrays need to be written
	bool	needVertex(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &vertex_list)
		    { return needWrite(ctx, vertex_list, myVertexList); }
	bool	needCounts(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &counts)
		    { return needWrite(ctx, counts, myCounts); }
	/// @}

	/// Test to see if the attribute needs to be written (true) or whether
	/// we can use the sample from the previous frame (false)
	bool	needWrite(const GABC_OOptions &ctx,
			const char *name, const GT_DataArrayHandle &data);

	GT_DataArrayHandle	&vertexList()	{ return myVertexList; }
	GT_DataArrayHandle	&counts()	{ return myCounts; }
	GT_DataArrayHandle	&P()            { return myP; }
	GT_DataArrayHandle	&Pw()           { return myPw; }
	GT_DataArrayHandle	&N()            { return myN; }
	GT_DataArrayHandle	&uv()           { return myUV; }
	GT_DataArrayHandle	&v()            { return myVel; }
	GT_DataArrayHandle	&id()           { return myId; }
	GT_DataArrayHandle	&width()	{ return myWidth; }
	GT_DataArrayHandle	&uknots()	{ return myUKnots; }
	GT_DataArrayHandle	&vknots()	{ return myVKnots; }

    private:
	bool	needWrite(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &data,
			GT_DataArrayHandle &cache);
	GT_DataArrayHandle	myVertexList;
	GT_DataArrayHandle	myCounts;
	GT_DataArrayHandle	myP;
	GT_DataArrayHandle	myPw;
	GT_DataArrayHandle	myN;
	GT_DataArrayHandle	myUV;
	GT_DataArrayHandle	myVel;
	GT_DataArrayHandle	myId;
	GT_DataArrayHandle	myWidth;
	GT_DataArrayHandle	myUKnots;
	GT_DataArrayHandle	myVKnots;
    };

    /// Secondary cache is used to cache values for subdivision tags and trim
    /// curves.  This is an optional cache and is only created for some
    /// primitive types.
    class GABC_API SecondaryCache
    {
    public:
	 SecondaryCache() {}
	~SecondaryCache() { clear(); }

	/// @{
	/// Access trim curve information
	GT_DataArrayHandle	&trimNCurves()	{ return myData[0]; }
	GT_DataArrayHandle	&trimN()	{ return myData[1]; }
	GT_DataArrayHandle	&trimOrder()	{ return myData[2]; }
	GT_DataArrayHandle	&trimKnot()	{ return myData[3]; }
	GT_DataArrayHandle	&trimMin()	{ return myData[4]; }
	GT_DataArrayHandle	&trimMax()	{ return myData[5]; }
	GT_DataArrayHandle	&trimU()	{ return myData[6]; }
	GT_DataArrayHandle	&trimV()	{ return myData[7]; }
	GT_DataArrayHandle	&trimW()	{ return myData[8]; }
	/// @}

	/// Updates the cache with the current values and returns @c true if
	/// the values have changed or @c false if the previous values can be
	/// used.  There's no way in the Alembic API to set trim components
	/// individually.
	bool	needTrimNCurves(const GABC_OOptions &ctx,
				const GT_DataArrayHandle &data)
		{
		    { return needWrite(ctx, data, trimNCurves()); }
		}
	bool	needTrimN(const GABC_OOptions &ctx,
				const GT_DataArrayHandle &data)
		{
		    { return needWrite(ctx, data, trimN()); }
		}
	bool	needTrimOrder(const GABC_OOptions &ctx,
				const GT_DataArrayHandle &data)
		{
		    { return needWrite(ctx, data, trimOrder()); }
		}
	bool	needTrimKnot(const GABC_OOptions &ctx,
				const GT_DataArrayHandle &data)
		{
		    { return needWrite(ctx, data, trimKnot()); }
		}
	bool	needTrimMin(const GABC_OOptions &ctx,
				const GT_DataArrayHandle &data)
		{
		    { return needWrite(ctx, data, trimMin()); }
		}
	bool	needTrimMax(const GABC_OOptions &ctx,
				const GT_DataArrayHandle &data)
		{
		    { return needWrite(ctx, data, trimMax()); }
		}
	bool	needTrimU(const GABC_OOptions &ctx,
				const GT_DataArrayHandle &data)
		{
		    { return needWrite(ctx, data, trimU()); }
		}
	bool	needTrimV(const GABC_OOptions &ctx,
				const GT_DataArrayHandle &data)
		{
		    { return needWrite(ctx, data, trimV()); }
		}
	bool	needTrimW(const GABC_OOptions &ctx,
				const GT_DataArrayHandle &data)
		{
		    { return needWrite(ctx, data, trimW()); }
		}

	/// @{
	/// Access to fixed subdivision tags
	GT_DataArrayHandle	&creaseIndices()	{ return myData[0]; }
	GT_DataArrayHandle	&creaseLengths()	{ return myData[1]; }
	GT_DataArrayHandle	&creaseSharpnesses()	{ return myData[2]; }
	GT_DataArrayHandle	&cornerIndices()	{ return myData[3]; }
	GT_DataArrayHandle	&cornerSharpnesses()	{ return myData[4]; }
	GT_DataArrayHandle	&holeIndices()		{ return myData[5]; }
	/// @}

	/// @{
	/// Check whether subdivision tags need to be written
	bool	needCreaseIndices(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &data)
		    { return needWrite(ctx, data, creaseIndices()); }
	bool	needCreaseLengths(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &data)
		    { return needWrite(ctx, data, creaseLengths()); }
	bool	needCreaseSharpnesses(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &data)
		    { return needWrite(ctx, data, creaseSharpnesses()); }
	bool	needCornerIndices(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &data)
		    { return needWrite(ctx, data, cornerIndices()); }
	bool	needCornerSharpnesses(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &data)
		    { return needWrite(ctx, data, cornerSharpnesses()); }
	bool	needHoleIndices(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &data)
		    { return needWrite(ctx, data, holeIndices()); }
	/// @}

    private:
	void	clear();
	bool	needWrite(const GABC_OOptions &ctx,
			const GT_DataArrayHandle &data,
			GT_DataArrayHandle &cache);
	GT_DataArrayHandle	myData[9];
    };

    static IgnoreList &     getDefaultSkip();

     GABC_OGTGeometry(const std::string &name);
    ~GABC_OGTGeometry();

    /// Return true if the primitive can be processed
    static bool     isPrimitiveSupported(const GT_PrimitiveHandle &prim);

    // Create the output Alembic object, as well as it's attribute and user
    // properties (if it has them and they are to be output).
    bool            start(const GT_PrimitiveHandle &prim,
                            const OObject &parent,
                            const GABC_OOptions &ctx,
                            GABC_OError &err,
                            ObjectVisibility vis = Alembic::AbcGeom::kVisibilityDeferred);
    // Output geometry, attribute, and user property samples to Alembic for the
    // current frame.
    bool            update(const GT_PrimitiveHandle &prim,
                            const GABC_OOptions &ctx,
                            GABC_OError &err,
                            ObjectVisibility vis = Alembic::AbcGeom::kVisibilityDeferred);
    // Output samples to Alembic, reusing the samples for the previous frame.
    bool            updateFromPrevious(GABC_OError &err,
                            ObjectVisibility vis = Alembic::AbcGeom::kVisibilityHidden,
                            exint frames = 1);

    /// Return the OObject for this shape
    OObject         getOObject() const;

    /// Return the secondary cache (allocating if needed)
    SecondaryCache  &getSecondaryCache();

    /// Dump information
    void	dump(int indent=0) const;

protected:
    // Make Alembic OFaceSet objects from groups of polygons.
    void	makeFaceSets(const GT_PrimitiveHandle &prim,
			const GABC_OOptions &ctx);

    // Make Alembic arbGeomProperties from Houdini attributes.
    bool        makeArbProperties(const GT_PrimitiveHandle &prim,
                        GABC_OError &err,
			const GABC_OOptions &ctx);
    // Output samples of attribute data to Alembic for current frame.
    bool        writeArbProperties(const GT_PrimitiveHandle &prim,
                        GABC_OError &err,
			const GABC_OOptions &ctx);
    // Reuse previous samples of attribute data for current frame.
    void        writeArbPropertiesFromPrevious();

    // Make Alembic user properties.
    bool        makeUserProperties(const GT_PrimitiveHandle &prim,
                        OCompoundProperty *parent,
                        GABC_OError &err,
                        const GABC_OOptions &ctx);
    // Output user property samples to Alembic.
    bool        writeUserProperties(const GT_PrimitiveHandle &prim,
                        GABC_OError &err,
                        const GABC_OOptions &ctx);
    // Reuse previous user property sample for current frame.
    void        writeUserPropertiesFromPrevious();

    // Clear out existing data.
    void	clearProperties();
    void        clearArbProperties();
    void        clearUserProperties();
    void	clearShape();
    void	clearCache();

private:
    // User properties output states.
    //
    //
    //  NO_USER_PROPERTIES:         No user property data on the first frame.
    //                              Keep checking for errors on subsequent
    //                              frames.
    //  ERROR_READING_PROPERTIES:   An error occurred reading user property
    //                              data on the first frame or a subsequent
    //                              frame. Ignore user properties completely.
    //  WRITE_USER_PROPERTIES:      User property data output successfully for
    //                              first frame. Try to write user property
    //                              info for subsequent frames, but if there
    //                              is a problem fall back to the previous
    //                              existing samples.
    enum UserPropertiesState
    {
        NO_USER_PROPERTIES,
        ERROR_READING_PROPERTIES,
        WRITE_USER_PROPERTIES,

        UNSET=-1
    };
    // Attribute scopes.
    enum
    {
	VERTEX_PROPERTIES,
	POINT_PROPERTIES,
	UNIFORM_PROPERTIES,
	DETAIL_PROPERTIES,
	MAX_PROPERTIES
    };

    union {
	OPolyMesh	   *myPolyMesh;
	OSubD		   *mySubD;
	OCurves		   *myCurves;
	OPoints		   *myPoints;
	ONuPatch	   *myNuPatch;
	void		   *myVoidPtr;
    } myShape;

    IntrinsicCache          myCache; // Cache for space optimization
    OVisibilityProperty     myVisibility;
    PropertyMap             myArbProperties[MAX_PROPERTIES];
    PropertyMap             myUserProperties;
    SecondaryCache         *mySecondaryCache;
    UT_Array<std::string>   myFaceSetNames;
    UserPropertiesState     myUserPropState;
    std::string             myName;
    exint                   myElapsedFrames;
    int                     myType;
};

} // GABC_NAMESPACE

#endif
