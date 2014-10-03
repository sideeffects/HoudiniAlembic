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

#ifndef __SOP_ALEMBICIN_H__
#define __SOP_ALEMBICIN_H__

#include <UT/UT_Interrupt.h>
#include <SOP/SOP_Node.h>
#include <GABC/GABC_GEOWalker.h>
#include <GABC/GABC_Util.h>


/// SOP to read Alembic geometry
class SOP_AlembicIn2 : public SOP_Node
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

    /// Return the label for the given input
    virtual const char	*inputLabel(unsigned int idx) const;

    virtual SOP_ObjectAppearancePtr	getObjectAppearance();

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
    SOP_AlembicIn2(OP_Network *net, const char *name, OP_Operator *op);
    virtual ~SOP_AlembicIn2();

    virtual bool	updateParmsFlags();
    virtual OP_ERROR	cookMySop(OP_Context &context);
    virtual OP_ERROR	cookMyGuide1(OP_Context &ctx);
    virtual void	syncNodeVersion(const char *old_version,
				const char *new_version, bool *node_deleted);
    //--------------------------------------------------------------------------

private:
    void	setupEventHandler(const std::string &filename);
    void	clearEventHandler();

    GABC_GEOWalker::BoxCullMode	getCullingBox(UT_BoundingBox &box,
						OP_Context &context);

    class Parms
    {
    public:
	Parms();
	Parms(const Parms &src);
	/// Compare this set of parameters with the other set of parameters to
	/// see if new geometry is needed (i.e. the filename has changed, or the
	/// path attribute has changed, etc.)
	bool	needsNewGeometry(const Parms &parms);
	bool	needsPathAttributeUpdate(const Parms &parms);

	Parms	&operator=(const Parms &src);

	GABC_GEOWalker::AbcPolySoup         myPolySoup;
	GABC_GEOWalker::AbcPrimPointMode    myPointMode;
	GABC_GEOWalker::AFilter             myAnimationFilter;
	GABC_GEOWalker::BoxCullMode         myBoundMode;
	GABC_GEOWalker::GroupMode           myGroupMode;
	GABC_GEOWalker::LoadMode            myLoadMode;
	GABC_GEOWalker::LoadUserPropsMode   myLoadUserProps;
	GEO_PackedNameMapPtr                myNameMapPtr;
	GEO_ViewportLOD                     myViewportLOD;
	UT_BoundingBox                      myBoundBox;
	UT_String                           myFilenameAttribute;
	UT_String                           myObjectPath;
	UT_String                           myObjectPattern;
	UT_String                           myPathAttribute;
	UT_String                           mySubdGroupName;
	std::string                         myFilename;
	bool                                myBuildAbcShape;
	bool                                myBuildAbcXform;
	bool                                myIncludeXform;
	bool                                myUseVisibility;
	bool                                myBuildLocator;
    };

    class EventHandler : public ArchiveEventHandler
    {
    public:
	EventHandler(SOP_AlembicIn2 *sop)
	    : mySOP(sop)
	{
	}
	virtual ~EventHandler() {}

	void	setSOP(SOP_AlembicIn2 *sop)	{ mySOP = sop; }

	virtual void	cleared()
	{
	    if (mySOP)
	    {
		mySOP->archiveClearEvent();
		mySOP = NULL;
	    }
	}
    private:
	SOP_AlembicIn2	*mySOP;
    };

    void	evaluateParms(Parms &parms, OP_Context &context);
    void	setPathAttributes(GABC_GEOWalker &walk, const Parms &parms);

    ArchiveEventHandlerPtr	myEventHandler;
    Parms			myLastParms;
    bool			myTopologyConstant;
    bool			myEntireSceneIsConstant;
    int				myConstantUniqueId; // Detail id for constant topology
};

#endif
