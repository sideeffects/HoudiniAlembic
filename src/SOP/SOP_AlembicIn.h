/*
 * Copyright (c) 2022
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

#ifndef __SOP_ALEMBICIN_H__
#define __SOP_ALEMBICIN_H__

#include <UT/UT_Interrupt.h>
#include <SOP/SOP_Node.h>
#include <GABC/GABC_GEOWalker.h>
#include <GABC/GABC_Util.h>

/// SOP to read Alembic geometry
class SOP_AlembicIn : public SOP_Node
{
public:
    typedef GABC_NAMESPACE::GABC_Util		GABC_Util;
    typedef GABC_NAMESPACE::GABC_GEOWalker	GABC_GEOWalker;
    typedef GABC_Util::ArchiveEventHandler	ArchiveEventHandler;
    typedef GABC_Util::ArchiveEventHandlerPtr	ArchiveEventHandlerPtr;
    //--------------------------------------------------------------------------
    // Standard hdk declarations
    static OP_Node *myConstructor(OP_Network *net, const char *name,
		    OP_Operator *entry);
    static PRM_Template myTemplateList[];

    /// Reload callback
    static int reloadGeo(void *data, int index,
			float time, const PRM_Template *tplate);

    static void installSOP(OP_OperatorTable *table);

    /// Called when an archive gets cleared from the cache
    void	archiveClearEvent();
    void	appendFileNames(std::vector<std::string> &filenames, fpreal t);

    /// Return the label for the given input
    const char  *inputLabel(unsigned int idx) const override;

    SOP_ObjectAppearancePtr     getObjectAppearance() override;

    void	abcError(const char *message)
    		{
    		    addError(SOP_MESSAGE, message);
    		}
    void	abcWarning(const char *message)
    		{
    		    addWarning(SOP_MESSAGE, message);
    		}
    void	abcInfo(const char *message)
    		{
                    addMessage(SOP_MESSAGE, message);
    		}
protected:
    //--------------------------------------------------------------------------
    // Standard hdk declarations
    SOP_AlembicIn(OP_Network *net, const char *name, OP_Operator *op);
    ~SOP_AlembicIn() override;

    bool                updateParmsFlags() override;
    OP_ERROR            cookMySop(OP_Context &context) override;
    OP_ERROR            cookMyGuide1(OP_Context &ctx) override;
    void                syncNodeVersion(const char *old_version,
				const char *new_version,
				bool *node_deleted) override;
    void                getDescriptiveParmName(UT_String &name) const override
				{ name = "fileName"; }
    //--------------------------------------------------------------------------
    void                getNodeSpecificInfoText(OP_Context &context,
				OP_NodeInfoParms &iparms) override;

    void                fillInfoTreeNodeSpecific(UT_InfoTree &tree, 
				const OP_NodeInfoTreeParms &parms) override;

private:
    void	setupEventHandler(const std::vector<std::string> &filenames);
    void	clearEventHandler();

    GABC_GEOWalker::BoxCullMode	getCullingBox(UT_BoundingBox &box,
						OP_Context &context);
    GABC_GEOWalker::SizeCullMode	getSizeCullMode(GABC_GEOWalker::SizeCompare &cmp,
						fpreal &size, fpreal t);

    class Parms
    {
    public:
	Parms();
	Parms(const Parms &src);
	/// Compare this set of parameters with the other set of parameters to
	/// see if new geometry is needed (i.e. the filenames have changed, or
	/// the path attribute has changed, etc.)
	bool	needsNewGeometry(const Parms &parms);
	bool	needsPathAttributeUpdate(const Parms &parms);

	Parms	&operator=(const Parms &src);

	GABC_GEOWalker::AbcPolySoup		myPolySoup;
	GABC_GEOWalker::AbcPrimPointMode	myPointMode;
	GABC_GEOWalker::AFilter			myAnimationFilter;
        int                                     myGeometryFilter;
	GABC_GEOWalker::BoxCullMode		myBoundMode;
	GABC_GEOWalker::SizeCullMode		mySizeCullMode;
	GABC_GEOWalker::SizeCompare		mySizeCompare;
	GABC_GEOWalker::GroupMode		myGroupMode;
	GABC_GEOWalker::LoadMode		myLoadMode;
	GABC_GEOWalker::LoadUserPropsMode	myLoadUserProps;
	GEO_PackedNameMapPtr			myNameMapPtr;
	GEO_ViewportLOD				myViewportLOD;
	UT_BoundingBox				myBoundBox;
	UT_String				myFileNameAttribute;
	UT_String				myRootObjectPath;
	UT_String				myObjectPath;
	UT_String				myObjectPattern;
	UT_String				myExcludeObjectPath;
	UT_String				myPathAttribute;
	UT_String				mySubdGroupName;
        UT_String                               myFacesetAttribute;
	std::vector<std::string>		myFileNames;
	fpreal					mySize;
	bool					myMissingFileError;
	bool					myBuildAbcShape;
	bool					myBuildAbcXform;
	bool					myIncludeXform;
	bool					myUseVisibility;
	bool					myStaticTimeZero;
	bool					myBuildLocator;
    };

    class EventHandler : public ArchiveEventHandler
    {
    public:
	EventHandler(SOP_AlembicIn *sop)
	    : mySOP(sop)
	{
	}
	~EventHandler() override {}

	void	setSOP(SOP_AlembicIn *sop)	{ mySOP = sop; }

        void    cleared(bool purged) override
	{
	    if (mySOP)
	    {
		if(purged)
		    mySOP->unloadData();
		mySOP->archiveClearEvent();
		mySOP = nullptr;
	    }
	}
    private:
	SOP_AlembicIn	*mySOP;
    };

    void	evaluateParms(Parms &parms, OP_Context &context);
    void	setPathAttributes(GABC_GEOWalker &walk, const Parms &parms);
    void	setPointMode(GABC_GEOWalker &walk, const Parms &parms);
    void	unpack(GU_Detail &dest, const GU_Detail &src, const Parms &parms);

    ArchiveEventHandlerPtr	myEventHandler;
    Parms			myLastParms;
    bool			myTopologyConstant;
    bool			myEntireSceneIsConstant;
    int				myConstantUniqueId; // Detail id for constant topology
    bool			myComputedFrameRange;

    // Global and end frame in the alembic archive.
    fpreal		        myStartFrame;
    fpreal			myEndFrame;

    UT_UniquePtr<GU_Detail>	myPackedGdp;
};

#endif
