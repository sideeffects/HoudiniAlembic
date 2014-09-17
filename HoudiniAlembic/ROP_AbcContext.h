/*
 * Copyright (c) 2014
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

#ifndef __ROP_AbcContext__
#define __ROP_AbcContext__

#include <Alembic/AbcGeom/All.h>
#include <GABC/GABC_Include.h>
#include <GABC/GABC_OOptions.h>
#include <OP/OP_Context.h>
#include <UT/UT_Array.h>
#include <UT/UT_String.h>

class UT_WorkBuffer;
class SOP_Node;

/// Alembic evaluation context
class ROP_AbcContext : public GABC_NAMESPACE::GABC_OOptions
{
public:
    typedef Alembic::Abc::TimeSamplingPtr	TimeSamplingPtr;

     ROP_AbcContext();
    ~ROP_AbcContext();

    /// @{
    /// Time sampling
    virtual const TimeSamplingPtr	&timeSampling() const
					    { return myTimeSampling; }
    const UT_Array<fpreal>	&blurTimes() const { return myBlurTimes; }
    void			setTimeSampling(fpreal tstart,
						fpreal tstep,
						int mb_samples = 1,
						fpreal shutter_open = 0,
						fpreal shutter_close = 0);
    /// @}

    /// @{
    /// Manage sub-frame time sampling.  To write a single frame, you'll want
    /// to have code like:
    /// @code
    ///   void writeFrame(fpreal time)
    ///   {
    ///       for (exint i = 0; i < ctx.samplesPerFrame(); ++i)
    ///       {
    ///           ctx.setTime(time, i);
    ///           ...
    ///       }
    ///   }
    /// @endcode
    exint		samplesPerFrame() const
			{
			    return myBlurTimes.entries();
			}
    void		setTime(fpreal base_time, exint samp);
    /// @}

    /// @{
    /// Access to the cook context
    fpreal	  cookTime() const
		    { return myCookContext.getTime(); }
    OP_Context	 &cookContext() const
		    { return const_cast<ROP_AbcContext *>(this)->myCookContext; }
    /// @}

    /// @{
    /// Whether to collapse leaf node identity transforms
    enum
    {
	COLLAPSE_NONE,		// Don't collapse identity geometry nodes
	COLLAPSE_IDENTITY,	// Collapse non-animated identity nodes
	COLLAPSE_GEOMETRY,	// Collapse geometry nodes, regardless of xform
	COLLAPSE_ALL,           // Collapse all transform nodes (used for
	                        // roundtripping packed Alembics)
    };
    int		 collapseIdentity() const	{ return myCollapseIdentity; }
    void	 setCollapseIdentity(int v)	{ myCollapseIdentity = v; }
    /// @}

    /// @{
    /// Whether to use instancing
    bool	useInstancing() const		{ return myUseInstancing; }
    void	setUseInstancing(bool v)	{ myUseInstancing = v; }
    /// @}

    /// @{
    /// Whether to save out objects that aren't displayed.  In this case,
    /// objects will be written to the .abc file with their visibility set to
    /// kHidden.
    bool	saveHidden() const		{ return mySaveHidden; }
    void	setSaveHidden(bool v)		{ mySaveHidden = v; }
    /// @}

    /// @{
    /// Set path attribute for writing geometry to a specific hierarchy.
    bool        buildFromPath() const           { return myBuildFromPath; }
    void        setBuildFromPath(bool v)        { myBuildFromPath = v; }
    const char *pathAttribute() const           { return myPathAttribute; }
    void        setPathAttribute(const char *s) { myPathAttribute.harden(s); }
    void        clearPathAttribute()            { myPathAttribute.clear(); }

    enum
    {
        // When multiple packed Alembics are present under the same parent
        // transform, we can modify the path to keep their unique transformation
        // or we can ignore the transformation to keep their path unmodified.
        PRIORITY_HIERARCHY,     // Ignore transform, keep hierarchy path
        PRIORITY_TRANSFORM      // Keep transform, modify hierarchy path
    };
    int         packedAlembicPriority() const   { return myPackedAbcPriority; }
    void        serPackedAlembicPriority(int p) { myPackedAbcPriority = p; }
    /// @}

    /// @{
    /// Set partition attribute for splitting geometry into different shapes
    enum
    {
	// When partitioning using a string attribute that represents the full
	// path for shape nodes, there are different ways we can name the
	// resulting shape node.  All of these will have invalid characters
	// (i.e. '/') replaced with '_'.
	PATHMODE_FULLPATH,	// Full string (default mode)
	PATHMODE_SHAPE,		// The tail of the path (shape node)
	PATHMODE_XFORM,		// The second last path component
	PATHMODE_XFORM_SHAPE,	// The last two components of the path
    };
    int		 partitionMode() const		{ return myPartitionMode; }
    void	 setPartitionMode(int m)	{ myPartitionMode = m; }
    const char	*partitionAttribute() const	{ return myPartitionAttribute; }
    void	 setPartitionAttribute(const char *s)
		    { myPartitionAttribute.harden(s); }
    void	 clearPartitionAttribute()
		 {
		     myPartitionMode = PATHMODE_FULLPATH;
		     myPartitionAttribute.clear();
		 }
    /// @}

    /// @{
    /// Utility method to take the value of the partition attribute and
    /// generate the name according to the partition mode.  Any slashes ('/')
    /// will be replaced with the replace_char.
    const char 		*partitionModeValue(const char *value,
				UT_WorkBuffer &storage,
				char replace_char=':') const
			{
			    return partitionModeValue(myPartitionMode,
				    value, storage, replace_char);
			}
    static const char	*partitionModeValue(int mode, const char *value,
				UT_WorkBuffer &storage,
				char replace_char=':');
    /// @}

    /// @{
    /// A singleton SOP is used when rendering from a SOP network
    SOP_Node		*singletonSOP() const	{ return mySingletonSOP; }
    void		 setSingletonSOP(SOP_Node *s)	{ mySingletonSOP = s; }
    /// @}

private:
    TimeSamplingPtr     myTimeSampling;
    UT_Array<fpreal>    myBlurTimes; // Sub-frame offsets for motion blur
    OP_Context          myCookContext;
    SOP_Node           *mySingletonSOP;
    UT_String           myPartitionAttribute;
    UT_String           myPathAttribute;
    int                 myPackedAbcPriority;
    int                 myPartitionMode;
    int                 myCollapseIdentity;
    bool                myUseInstancing;
    bool                mySaveHidden;
    bool                myBuildFromPath;
};

#endif
