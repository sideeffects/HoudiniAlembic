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

#ifndef __GABC_GEOWalker__
#define __GABC_GEOWalker__

#include "GABC_API.h"
#include "GABC_IError.h"
#include "GABC_Include.h"
#include "GABC_Util.h"
#include <GA/GA_Handle.h>
#include <GU/GU_Detail.h>
#include <stack>

class GU_PrimPacked;
class UT_StringArray;

namespace GABC_NAMESPACE
{
/// @brief Walk an Alembic tree to create Houdini geometry
///
/// To convert an Alembic file to Houdini geometry, the code would look
/// something like this: @code
///	GU_Detail	gdp;
///	GABC_GEOWalker	walker(gdp);
///	
///	walker.setFrame(frame, fps);	// Specify the time sample
///	// Set any other parameters you might care about
///	walker.setBuildAbcPrim(true);
///	
///	// Now, walker the Alembic file
///	if (!GABC_Util::walker(filename, walker))
///	    addError();
/// @endcode
class GABC_API GABC_GEOWalker : public GABC_Util::Walker
{
public:
    using M44d = Alembic::Abc::M44d;
    using ISampleSelector = Alembic::Abc::ISampleSelector;

    /// Test by loading "test.abc" in the current directory and saving
    /// "test.geo" as output.
    static void		test();

    enum GroupMode
    {
	ABC_GROUP_NONE,		        // No geometry groups
	ABC_GROUP_SHAPE_NODE,	        // Name geometry group based on shape node
	ABC_GROUP_XFORM_NODE,	        // Name geometry group based on transform node
	ABC_GROUP_SHAPE_BASENAME,	// Group by last path component
	ABC_GROUP_XFORM_BASENAME,	// Group by last xform node path component
    };

    enum BoxCullMode
    {
	BOX_CULL_IGNORE,	// Ignore bounding box
	BOX_CULL_ANY_INSIDE,	// Add if boxes overlap
	BOX_CULL_INSIDE,	// Add if box is entirely inside
	BOX_CULL_ANY_OUTSIDE,	// Add if any of object is outside filter box
	BOX_CULL_OUTSIDE,	// Add if object is entirely outside bounds
    };

    enum SizeCullMode
    {
	SIZE_CULL_IGNORE,	// Ignore size constraint
	SIZE_CULL_AREA,		// Add if bounding box area passes size test
	SIZE_CULL_RADIUS,	// Add if bounding box radius passes size test
	SIZE_CULL_VOLUME,	// Add if bounding box volume passes size test
    };

    enum SizeCompare
    {
	SIZE_COMPARE_LESSTHAN,		// Add if bounding box is smaller than the reference size
	SIZE_COMPARE_GREATERTHAN,	// Add if bounding box is larger than the reference size
    };

    enum LoadMode
    {
	LOAD_ABC_PRIMITIVES,		// Load Alembic primitives
	LOAD_ABC_UNPACKED,		// Load ABC then unpack
	LOAD_HOUDINI_PRIMITIVES,	// Load houdini primitives
	LOAD_HOUDINI_POINTS,		// Load point cloud for objects
	LOAD_HOUDINI_BOXES,		// Load Bounds as Houdini geometry
    };

    /// Animating object filter
    enum AFilter
    {
	ABC_AFILTER_STATIC,            // Only static geometry
	ABC_AFILTER_DEFORMING,         // Only deforming geometry
        ABC_AFILTER_TRANSFORMING,      // Only transforming geometry
        ABC_AFILTER_ANIMATING,         // Only animating geometry
	ABC_AFILTER_ALL                // All geometry
    };

    /// Geometry type filter
    enum GFilter
    {
        ABC_GFILTER_POLYMESH = 0x01,                    // Load primitives that are polygons
        ABC_GFILTER_CURVES = 0x02,                      // Load primitives that are curves
        ABC_GFILTER_NUPATCH = 0x04,                     // Load primitives that are NURBS
        ABC_GFILTER_POINTS = 0x08,                      // Load primitives that are points
        ABC_GFILTER_SUBD = 0x10,                        // Load primitives that are subdivision surfaces
        ABC_GFILTER_ALL = (ABC_GFILTER_POLYMESH |       // Load all primitives 
                           ABC_GFILTER_CURVES |
                           ABC_GFILTER_NUPATCH |
                           ABC_GFILTER_POINTS |
                           ABC_GFILTER_SUBD)                  
    };

    /// How Alembic delayed load primitives are attached to GA points
    enum AbcPrimPointMode
    {
	ABCPRIM_SHARED_POINT,		// All primitives share a GA point
	ABCPRIM_UNIQUE_POINT,		// Each prim has its own point
	ABCPRIM_CENTROID_POINT,		// Place point at centroid
    };

    /// Whether to build polysoup primitives when it's possible
    enum AbcPolySoup
    {
	ABC_POLYSOUP_NONE,	// Build polygons only
	ABC_POLYSOUP_POLYMESH,	// Polygon Mesh primitives only
	ABC_POLYSOUP_SUBD,	// Polygon soups & subdivision primitives
    };

    /// How to load user properties
    enum LoadUserPropsMode
    {
        UP_LOAD_NONE,  // Don't load user properties
        UP_LOAD_DATA,  // Load only user property values
        UP_LOAD_ALL,   // Load user property values and metadata
    };

    // Helper class stores world transform state.
    class TransformState
    {
    public:
	 TransformState() {}
	~TransformState() {}

    private:
	friend class GABC_GEOWalker;

	void	push(const M44d &m, bool c)
		{
		    myM = m;
		    myC = c;
		}
	void	pop(M44d &m, bool &c) const
		{
		    m = myM;
		    c = myC;
		}

	M44d	myM;
	bool	myC;
    };

    GABC_GEOWalker(GU_Detail &gdp, GABC_IError &err,
		   bool record_time_range=false);
    virtual ~GABC_GEOWalker();

    virtual bool	preProcess(const GABC_IObject &node);

    virtual bool	process(const GABC_IObject &node);
    virtual bool	interrupted() const;

    /// Quickly update ABC primitives with the new time
    void		updateAbcPrims();

    /// @{
    /// Get state
    GU_Detail			&detail() const
				    { return myDetail; }
    GABC_IError                 &errorHandler()
                                    { return myErrorHandler; }
    const UT_String		&objectPattern() const
				    { return myObjectPattern; }
    const UT_StringArray	&excludeObjects() const
				    { return myExcludeObjects; }
    const GEO_PackedNameMapPtr	&nameMapPtr() const
				    { return myNameMapPtr; }
    const UT_String             &facesetAttribute() const
                                    { return myFacesetAttribute; }

    fpreal	time() const		{ return myTime; }
    bool	includeXform() const	{ return myIncludeXform; }
    bool	useVisibility() const	{ return myUseVisibility; }
    bool	staticTimeZero() const	{ return myStaticTimeZero; }
    bool	reusePrimitives() const	{ return myReusePrimitives; }
    bool	buildLocator() const	{ return myBuildLocator; }
    LoadUserPropsMode   loadUserProps() const   { return myLoadUserProps; }
    LoadMode	loadMode() const	{ return myLoadMode; }
    bool	buildAbcPrim() const
			{ return myLoadMode == LOAD_ABC_PRIMITIVES; }
    bool	buildAbcShape() const	{ return myBuildAbcShape; }
    bool	buildAbcXform() const	{ return myBuildAbcXform; }
    bool	pathAttributeChanged() const { return myPathAttributeChanged; }
    /// @}

    /// Get a sample selector for the given time
    ISampleSelector		timeSample() const
				    { return ISampleSelector(myTime); }


    /// Keeps track of the number of geometry points added in traversal
    GA_Offset	pointCount() const	{ return myPointCount; }
    /// Keeps track of the number of geometry vertices added in traversal
    GA_Offset	vertexCount() const	{ return myVertexCount; }
    /// Keeps track of the number of geometry primitives added in traversal
    GA_Offset	primitiveCount() const	{ return myPrimitiveCount; }
    /// True if *all* shapes and transforms are constant
    bool	isConstant() const	{ return myIsConstant; }
    /// True if *all* shapes have constant topology
    bool	topologyConstant() const { return myTopologyConstant; }
    /// True if the transform from the root to the current node is constant
    bool	transformConstant() const { return myTransformConstant; }
    /// True if *all* transforms are constant
    bool	allTransformConstant() const { return myAllTransformConstant; }
    bool	rebuiltNURBS() const	{ return myRebuiltNURBS; }

    /// @{
    /// Set state
    void	setExcludeObjects(const char *s);
    void	setObjectPattern(const char *s)
		    { myObjectPattern.harden(s); }
    void        setFacesetAttribute(const char *s)
                    { myFacesetAttribute.harden(s); }
    void	setNameMapPtr(const GEO_PackedNameMapPtr &ptr)
		    { myNameMapPtr = ptr; }
    void	setPathAttribute(const GA_RWAttributeRef &a);
    void	setTime(fpreal t)		{ myTime = t; }
    void	setFrame(fpreal f, fpreal fps)	{ myTime = f/fps; }
    void	setIncludeXform(bool v)		{ myIncludeXform = v; }
    void	setUseVisibility(bool v)	{ myUseVisibility = v; }
    void	setStaticTimeZero(bool v)	{ myStaticTimeZero = v; }
    void	setReusePrimitives(bool v)	{ myReusePrimitives = v; }
    void	setBuildLocator(bool v)		{ myBuildLocator = v; }
    void	setLoadMode(LoadMode mode)	{ myLoadMode = mode; }
    void	setBuildAbcShape(bool v)	{ myBuildAbcShape = v; }
    void	setBuildAbcXform(bool v)	{ myBuildAbcXform = v; }
    void	setPathAttributeChanged(bool v)	{ myPathAttributeChanged = v; }
    void	setUserProps(LoadUserPropsMode m) { myLoadUserProps = m; }
    void	setGroupMode(GroupMode m)	{ myGroupMode = m; }
    void	setAnimationFilter(AFilter m)	{ myAnimationFilter = m; }
    void        setGeometryFilter(int m)    { myGeometryFilter = m; }
    void	setBounds(BoxCullMode mode, const UT_BoundingBox &box);
    void	setPointMode(AbcPrimPointMode mode,
			GA_Offset shared_point = GA_INVALID_OFFSET);
    void	setPolySoup(AbcPolySoup soup)	{ myPolySoup = soup; }
    void	setViewportLOD(GEO_ViewportLOD v)	{ myViewportLOD = v; }
    void	setSizeCullMode(SizeCullMode mode, SizeCompare cmp, fpreal size)
		    { mySizeCullMode = mode; mySizeCompare = cmp; mySize = size; }
    /// @}

    /// @{
    /// State accessors
    AbcPolySoup		polySoup() const	{ return myPolySoup; }
    GEO_ViewportLOD	viewportLOD() const	{ return myViewportLOD; }
    /// @}

    /// @{
    /// Primitive group to store subdivision primitives
    GA_PrimitiveGroup	*subdGroup() const	{ return mySubdGroup; }
    void		setSubdGroup(GA_PrimitiveGroup *g) { mySubdGroup = g; }
    /// @}

    /// @{
    /// State modified during traversal
    void	setNonConstant()		{ myIsConstant = false; }
    void	setNonConstantTopology()	{ myTopologyConstant = false; }
    /// @}

    /// Keep track of added points/vertices/primitives.  This should be called
    /// after the primitives have been added to the detail.
    void	trackPtVtxPrim(const GABC_IObject &obj,
				exint npoints, exint nvertex, exint nprim,
				bool do_transform);

    /// Push transform during traversal.  If the walker is set to include the
    /// full transforms, the transform passed in will be combined with the
    /// current transform, otherwise the transform passed in will replace the
    /// transform state in the walker.  The @c const_xform flag indicates
    /// whether the transform node is constant or not.  The current state will
    /// be stashed in @c stash_prev and should be passed to popTransform().
    void	pushTransform(const M44d &xform, bool const_xform,
				TransformState &prev_state,
				bool inheritXforms);
    /// Pop transform in traveral, pass in the value stored in pushTransform().
    void	popTransform(const TransformState &prev_state);

    /// Get the current transform
    const M44d	&getTransform() const	{ return myMatrix; }

    /// Get the group name associated with an GABC_IObject
    bool	getGroupName(UT_String &str, const GABC_IObject &obj) const;

    /// Translate attribute names.  This will return false if the attribute is
    /// not allowed.
    bool	translateAttributeName(GA_AttributeOwner own, UT_String &name);

    /// @{
    /// Access information about last poly/subd/curve mesh loaded
    GA_Size	lastFaceCount() const	{ return myLastFaceCount; }
    GA_Offset	lastFaceStart() const	{ return myLastFaceStart; }
    void	trackLastFace(GA_Size nfaces);
    void	trackSubd(GA_Size nfaces);
    /// @}

    /// Get a GA_Offset to which the Alembic delayed load primitive should be
    /// attached.  This may return an invalid offset
    GA_Offset	getPointForAbcPrim();
    void	setPointLocation(GU_PrimPacked *abc, GA_Offset offset) const;

protected:
    /// Verify the object matches filters before generating geometry
    bool		 filterObject(const GABC_IObject &obj) const;

private:
    bool		 matchObjectName(const GABC_IObject &obj) const;
    bool		 matchAnimationFilter(const GABC_IObject &obj) const;
    bool                 matchGeometryFilter(const GABC_IObject &obj) const;
    bool		 matchBounds(const GABC_IObject &obj) const;
    bool		 matchSize(const GABC_IObject &obj) const;
    bool		 abcPrimPointMode() const
				{ return myAbcPrimPointMode; }
    GA_Offset		 abcSharedPoint() const
				{ return myAbcSharedPoint; }
    void		 recordTimeRange(const GABC_IObject &node);

    // Enum values
    AbcPolySoup             myPolySoup;
    AbcPrimPointMode        myAbcPrimPointMode;
    AFilter                 myAnimationFilter;      // Animating object filter
    int                     myGeometryFilter;       // Geometry type filter
    BoxCullMode             myBoxCullMode;
    SizeCullMode	    mySizeCullMode;
    SizeCompare		    mySizeCompare;
    GroupMode               myGroupMode;            // How to construct group names
    LoadMode                myLoadMode;             // Build Alembic primitives
    LoadUserPropsMode       myLoadUserProps;        // How to load user properties

    GA_Offset               myAbcSharedPoint;
    GA_Offset               myLastFaceStart;        // Start of faces in last mesh
    GA_PrimitiveGroup      *mySubdGroup;
    GA_RWHandleS            myPathAttribute;
    GA_Size                 myLastFaceCount;        // Number of faces in last mesh
    GABC_IError             &myErrorHandler;
    GEO_PackedNameMapPtr    myNameMapPtr;           // Attribute map for ABC primitives
    GEO_ViewportLOD         myViewportLOD;
    GU_Detail              &myDetail;
    M44d                    myMatrix;
    UT_BoundingBox          myCullBox;
    UT_Interrupt           *myBoss;
    UT_String               myFacesetAttribute;
    UT_String               myObjectPattern;
    UT_StringArray	    myExcludeObjects;
    std::stack<GABC_VisibilityType> myVisibilityStack;
    UT_Set<std::string>	    myVisited;

    fpreal	myTime; // Alembic evaluation time
    fpreal	mySize;

    GA_Offset	myPointCount;		// Points added
    GA_Offset	myPrimitiveCount;	// Primitive's added count
    GA_Offset	myVertexCount;		// Vertices added
    int		myBossId;
    bool	myBuildAbcXform;	// Build primitives for transforms
    bool	myBuildAbcShape;	// Build primitives for transforms
    bool	myBuildLocator;		// Whether to build Maya locators
    bool	myIncludeXform;		// Transform geometry
    bool	myPathAttributeChanged;	// Whether path attrib name changed
    bool	myReusePrimitives;	// Reuse primitives in input geometry
    bool	myUseVisibility;	// Use visibility
    bool	myStaticTimeZero;	// All static objects have frame=0
    bool	myRecordTimeRange;

    // Modified during traversal
    bool	myIsConstant;		// Whether all objects are constant
    bool	myTopologyConstant;	// Whether topology is constant
    bool	myTransformConstant;	// All xforms down the tree are const
    bool	myAllTransformConstant;	// All transforms in scene are const
    bool	myRebuiltNURBS;		// Whether NURBS were rebuilt
};
}

#endif
