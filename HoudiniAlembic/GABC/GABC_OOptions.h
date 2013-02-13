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
 * NAME:	GABC_OOptions.h (GABC Library, C++)
 *
 * COMMENTS:
 */

#ifndef __GABC_OOptions__
#define __GABC_OOptions__

#include "GABC_API.h"
#include "GABC_Include.h"
#include <UT/UT_String.h>
#include <GA/GA_Types.h>
#include <Alembic/AbcCoreAbstract/TimeSampling.h>
#include <Alembic/AbcGeom/GeometryScope.h>

/// Class to specify options for output of Alembic geometry
class GABC_API GABC_OOptions
{
public:
    typedef Alembic::AbcCoreAbstract::TimeSamplingPtr	TimeSamplingPtr;

    GABC_OOptions();
    virtual ~GABC_OOptions();

    /// Method to return the time sampling pointer for output
    virtual const TimeSamplingPtr	&timeSampling() const = 0;

    /// Space optimization will bypass sending unchanged data down to the
    /// Alembic library.  There may be some cost to these checks.
    enum SpaceOptimize
    {
	OPTIMIZE_FAST,		// Only peform non-costly optimizations
	OPTIMIZE_TOPOLOGY,	// Check for constant topology
	OPTIMIZE_ATTRIBUTES,	// Check topology and all attributes

	OPTIMIZE_DEFAULT = OPTIMIZE_TOPOLOGY
    };

    SpaceOptimize	optimizeSpace() const
			    { return myOptimizeSpace; }
    void		setOptimizeSpace(SpaceOptimize v)
			    { myOptimizeSpace = v; }

    /// Face set creation
    enum FaceSetMode
    {
	FACESET_NONE,		// Don't create face sets
	FACESET_NON_EMPTY,	// Only create face sets for non-empty groups
	FACESET_ALL_GROUPS,	// Create face sets for all primitive groups

	FACESET_DEFAULT = FACESET_NON_EMPTY,
    };
    FaceSetMode	faceSetMode() const	{ return myFaceSetMode; }
    void	setFaceSetMode(FaceSetMode m)	{ myFaceSetMode = m; }


    /// @{
    /// Whether or not to save attributes along with the geometry.  Default true
    bool	saveAttributes() const		{ return mySaveAttributes; }
    void	setSaveAttributes(bool f)	{ mySaveAttributes = f; }
    /// @}

    /// @{
    /// Whether to use the display or render SOP.  Default is to choose render.
    bool	useDisplaySOP() const		{ return myUseDisplaySOP; }
    void	setUseDisplaySOP(bool f)	{ myUseDisplaySOP = f; }
    /// @}

    /// @{
    /// Whether to cook full bounding boxes for all nodes
    bool	fullBounds() const	{ return myFullBounds; }
    void	setFullBounds(bool f)	{ myFullBounds = f; }
    /// @}

    /// @{
    /// Group name to identify subdivision surfaces
    const char	*subdGroup() const		{ return mySubdGroup.buffer(); }
    void	 setSubdGroup(const char *g)	{ mySubdGroup.harden(g); }
    /// @}

    /// @{
    /// Set attribute masks
    bool	 matchAttribute(Alembic::AbcGeom::GeometryScope scope,
			const char *name) const;
    bool	 matchAttribute(GA_AttributeOwner own, const char *name) const;
    const char	*getAttributePattern(GA_AttributeOwner own) const
			{ return myAttributePatterns[own].buffer(); }
    void	 setAttributePattern(GA_AttributeOwner own, const char *pattern)
		 {
		     myAttributePatterns[own].harden(pattern);
		     checkAttributeStars();
		 }
    /// @}

private:
    void		checkAttributeStars();

    SpaceOptimize	myOptimizeSpace;
    FaceSetMode		myFaceSetMode;
    UT_String		mySubdGroup;
    UT_String		myAttributePatterns[GA_ATTRIB_OWNER_N];
    bool		myAttributeStars;
    bool		mySaveAttributes;
    bool		myUseDisplaySOP;
    bool		myFullBounds;
};

#endif

