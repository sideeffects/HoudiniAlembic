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
 * NAME:	GABC_GEOWalker.h ( GABC Library, C++)
 *
 * COMMENTS:
 */

#ifndef __GABC_GEOWalker__
#define __GABC_GEOWalker__

#include "GABC_API.h"
#include "GABC_Include.h"
#include "GABC_Util.h"
#include "GABC_GEOPrim.h"
#include <GA/GA_Handle.h>
#include <GU/GU_Detail.h>

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
    typedef Alembic::Abc::M44d			M44d;
    typedef Alembic::Abc::ISampleSelector	ISampleSelector;

    /// Test by loading "test.abc" in the current directory and saving
    /// "test.geo" as output.
    static void		test();

    enum GroupMode
    {
	ABC_GROUP_NONE,		// No geometry groups
	ABC_GROUP_SHAPE_NODE,	// Name geometry group based on shape node
	ABC_GROUP_XFORM_NODE,	// Name geometry group based on transform node
    };

    enum BoxCullMode
    {
	BOX_CULL_IGNORE,	// Ignore bounding box
	BOX_CULL_ANY_INSIDE,	// Add if boxes overlap
	BOX_CULL_INSIDE,	// Add if box is entirely inside
	BOX_CULL_ANY_OUTSIDE,	// Add if any of object is outside filter box
	BOX_CULL_OUTSIDE,	// Add if object is entirely outside bounds
    };

    enum LoadMode
    {
	LOAD_ABC_PRIMITIVES,		// Load Alembic primitives
	LOAD_HOUDINI_PRIMITIVES,	// Load houdini primitives
	LOAD_HOUDINI_POINTS,		// Load point cloud for objects
	LOAD_HOUDINI_BOXES		// Load Bounds as Houdini geometry
    };

    /// Animating object filter
    enum AFilter
    {
	ABC_AFILTER_STATIC	= 0x01,		// Only static geometry
	ABC_AFILTER_ANIMATING	= 0x02,		// Only animating
	ABC_AFILTER_ALL		= (ABC_AFILTER_STATIC|ABC_AFILTER_ANIMATING),
    };

    GABC_GEOWalker(GU_Detail &gdp);
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
    const UT_String		&objectPattern() const
				    { return myObjectPattern; }
    const UT_String		*attributePatterns() const
				    { return myAttributePatterns; }
    const GABC_NameMapPtr	&nameMapPtr() const
				    { return myNameMapPtr; }

    fpreal	time() const		{ return myTime; }
    bool	includeXform() const	{ return myIncludeXform; }
    bool	reusePrimitives() const	{ return myReusePrimitives; }
    bool	buildLocator() const	{ return myBuildLocator; }
    LoadMode	loadMode() const	{ return myLoadMode; }
    bool	buildAbcPrim() const
			{ return myLoadMode == LOAD_ABC_PRIMITIVES; }
    bool	buildAbcXform() const	{ return myBuildAbcXform; }
    bool	pathAttributeChanged() const { return myPathAttributeChanged; }
    /// @}

    /// Get a sample selector for the given time
    ISampleSelector		timeSample() const
				    { return ISampleSelector(myTime); }


    /// Keeps track of the number of geometry points added in traversal
    exint	pointCount() const	{ return myPointCount; }
    /// Keeps track of the number of geometry vertices added in traversal
    exint	vertexCount() const	{ return myVertexCount; }
    /// Keeps track of the number of geometry primitives added in traversal
    exint	primitiveCount() const	{ return myPrimitiveCount; }
    /// True if *all* shapes and transforms are constant
    bool	isConstant() const	{ return myIsConstant; }
    /// True if *all* shapes have constant topology
    bool	topologyConstant() const { return myTopologyConstant; }
    /// True if the transform from the root to the current node is constant
    bool	transformConstant() const { return myTransformConstant; }
    /// True if *all* transforms are constant
    bool	allTransformConstant() const { return myAllTransformConstant; }
    bool	rebuiltNURBS() const	{ return myRebuiltNURBS; }
    /// @}

    /// @{
    /// Set state
    void	setObjectPattern(const char *s)
		    { myObjectPattern.harden(s); }
    void	setAttributePattern(GA_AttributeOwner owner, const char *s)
		    { myAttributePatterns[owner].harden(s); }
    void	setVertexPattern(const char *s)
		    { setAttributePattern(GA_ATTRIB_VERTEX, s); }
    void	setPointPattern(const char *s)
		    { setAttributePattern(GA_ATTRIB_POINT, s); }
    void	setPrimitivePattern(const char *s)
		    { setAttributePattern(GA_ATTRIB_PRIMITIVE, s); }
    void	setDetailPattern(const char *s)
		    { setAttributePattern(GA_ATTRIB_DETAIL, s); }
    void	setNameMapPtr(const GABC_NameMapPtr &ptr)
		    { myNameMapPtr = ptr; }
    void	setPathAttribute(const GA_RWAttributeRef &a);
    void	setTime(fpreal t)		{ myTime = t; }
    void	setFrame(fpreal f, fpreal fps)	{ myTime = f/fps; }
    void	setIncludeXform(bool v)		{ myIncludeXform = v; }
    void	setReusePrimitives(bool v);
    void	setBuildLocator(bool v)		{ myBuildLocator = v; }
    void	setLoadMode(LoadMode mode)	{ myLoadMode = mode; }
    void	setBuildAbcXform(bool v)	{ myBuildAbcXform = v; }
    void	setPathAttributeChanged(bool v)	{ myPathAttributeChanged = v; }
    void	setGroupMode(GroupMode m)	{ myGroupMode = m; }
    void	setAnimationFilter(AFilter m)	{ myAnimationFilter = m; }
    void	setBounds(BoxCullMode mode, const UT_BoundingBox &box);
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

    /// Access information about last poly/subd/curve mesh loaded
    GA_Size	lastFaceCount() const	{ return myLastFaceCount; }
    GA_Offset	lastFaceStart() const	{ return myLastFaceStart; }
    void	trackLastFace(GA_Size nfaces);

protected:
    /// Verify the object matches filters before generating geometry
    bool		 filterObject(const GABC_IObject &obj) const;

private:
    bool		 matchObjectName(const GABC_IObject &obj) const;
    bool		 matchAnimationFilter(const GABC_IObject &obj) const;
    bool		 matchBounds(const GABC_IObject &obj) const;

    GU_Detail		&myDetail;
    UT_String		 myObjectPattern;
    UT_String		 myAttributePatterns[GA_ATTRIB_OWNER_N];
    GABC_NameMapPtr	 myNameMapPtr;	// Attribute map for ABC primitives
    GA_RWHandleS	 myPathAttribute;
    UT_BoundingBox	 myCullBox;
    UT_Interrupt	*myBoss;
    int			 myBossId;
    M44d		 myMatrix;
    GA_Size		 myLastFaceCount;	// Number of faces in last mesh
    GA_Offset		 myLastFaceStart;	// Start of faces in last mash

    fpreal	myTime;			// Alembic evaluation time
    GroupMode	myGroupMode;		// How to construct group names
    BoxCullMode	myBoxCullMode;
    AFilter	myAnimationFilter;	// Animating object filter
    bool	myIncludeXform;		// Transform geometry
    bool	myBuildLocator;		// Whether to build Maya locators
    bool	myPathAttributeChanged;	// Whether path attrib name changed
    LoadMode	myLoadMode;		// Build Alembic primitives
    bool	myBuildAbcXform;	// Build primitives for transforms

    exint	myPointCount;		// Points added
    exint	myVertexCount;		// Vertices added
    exint	myPrimitiveCount;	// Primitive's added count
    bool	myReusePrimitives;	// Reuse primitives in input geometry

    // Modified during traversal
    bool	myIsConstant;		// Whether all objects are constant
    bool	myTopologyConstant;	// Whether topology is constant
    bool	myTransformConstant;	// All xforms down the tree are const
    bool	myAllTransformConstant;	// All transforms in scene are const
    bool	myRebuiltNURBS;		// Whether NURBS were rebuilt
};

#endif
