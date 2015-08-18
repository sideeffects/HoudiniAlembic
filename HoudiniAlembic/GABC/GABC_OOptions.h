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

#ifndef __GABC_OOptions__
#define __GABC_OOptions__

#include "GABC_API.h"
#include "GABC_Include.h"
#include <UT/UT_String.h>
#include <GA/GA_Types.h>
#include <Alembic/AbcCoreAbstract/TimeSampling.h>
#include <Alembic/AbcGeom/GeometryScope.h>

namespace GABC_NAMESPACE
{

/// This class specifies the options used during Alembic archive export.
/// In practice, these options are passed by reference to const, so they are
/// set once during setup and never modified during the entrie output process.
class GABC_API GABC_OOptions
{
public:
    typedef Alembic::AbcCoreAbstract::TimeSamplingPtr	TimeSamplingPtr;

    /// Face set creation
    enum FaceSetMode
    {
	FACESET_NONE,		// Don't create face sets
	FACESET_NON_EMPTY,	// Only create face sets for non-empty groups
	FACESET_ALL_GROUPS,	// Create face sets for all primitive groups

	FACESET_DEFAULT = FACESET_NON_EMPTY,
    };

    GABC_OOptions();
    virtual ~GABC_OOptions() {};

    /// Method to return the time sampling pointer for output
    virtual const TimeSamplingPtr	&timeSampling() const = 0;

    /// @{
    /// Whether or not to create face sets, and if so for which groups.
    FaceSetMode	faceSetMode() const	{ return myFaceSetMode; }
    void	setFaceSetMode(FaceSetMode m)	{ myFaceSetMode = m; }
    /// @}

    /// @{
    /// The first frame of output
    exint       firstFrame() const      { return myFirstFrame; }
    void        setFirstFrame(exint f)  { myFirstFrame = f; }
    /// @}

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
    /// Set attribute masks and compare attributes against the masks
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

    /// @{
    /// Pattern matching for multiple uv sets
    const char *uvAttribPattern() const                 { return myUVAttribPattern.buffer(); }
    void        setUVAttribPattern(const char *pattern) { myUVAttribPattern.harden(pattern); }
    /// @}

private:
    void		checkAttributeStars();

    FaceSetMode		myFaceSetMode;
    UT_String		mySubdGroup;
    UT_String		myAttributePatterns[GA_ATTRIB_OWNER_N];
    UT_String           myUVAttribPattern;
    exint               myFirstFrame;
    bool		myAttributeStars;
    bool		mySaveAttributes;
    bool		myUseDisplaySOP;
    bool		myFullBounds;
};

}   // end GABC_NAMESPACE

#endif

