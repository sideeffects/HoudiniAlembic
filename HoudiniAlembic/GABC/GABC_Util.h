/*
 * Copyright (c) 2013
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

#ifndef __GABC_Util__
#define __GABC_Util__

#include "GABC_API.h"
#include "GABC_Include.h"
#include "GABC_Types.h"

// Houdini includes
#include <SYS/SYS_Types.h>
#include <UT/UT_Matrix4.h>
#include <UT/UT_BoundingBox.h>
#include <UT/UT_SharedPtr.h>
#include "GABC_IArchive.h"
#include "GABC_IObject.h"

class UT_StringArray;

namespace GABC_NAMESPACE
{

class GABC_API GABC_Util
{
public:
    typedef Alembic::Abc::V3d			V3d;
    typedef Alembic::Abc::Box3d			Box3d;
    typedef Alembic::Abc::M44d			M44d;
    typedef Alembic::Abc::ICompoundProperty	ICompoundProperty;
    typedef std::vector<std::string>		PathList;

    static const char	*getAlembicCompileNamespace();

    /// Create a Box3d from a UT_BoundingBox
    static Box3d	getBox(const UT_BoundingBox &box)
    {
	return Box3d(V3d(box.xmin(), box.ymin(), box.zmin()),
		    V3d(box.xmax(), box.ymax(), box.zmax()));
    }
    /// Create a UT_BoundingBox from a Box3d
    static UT_BoundingBox	getBox(const Box3d &box)
    {
	return UT_BoundingBox(box.min[0], box.min[1], box.min[2],
		box.max[0], box.max[1], box.max[2]);
    }
    /// Create a UT_Matrix4D from an M44d
    static UT_Matrix4D		getM(const M44d &m)
    {
	return UT_Matrix4D(m.x);
    }
    /// Create an M44d from a UT_Matrix4D
    static M44d	getM(const UT_Matrix4D &m)
    {
	return M44d((const double (*)[4])m.data());
    }

    /// Class used in traversal of Alembic trees
    ///
    /// For standard walking of the tree, you'd likely have a process() method
    /// like this: @code
    ///	   bool process(const GABC_IObject &node)
    ///		{
    ///		    doSomething(node);
    ///		    return true;	// Process other nodes
    ///		}
    /// @endcode
    /// However, if you have to pass information to the child nodes, you might
    /// want to do something like: @code
    ///	    bool process(const GABC_IObject &node)
    ///		{
    ///		    doSomething(node);	// Process this node
    ///		    pushState();	// Set information for kids
    ///		    walkChildren(node);
    ///		    popState();		// Restore parent's state
    ///		    return false;	// Don't let parent walk kids
    ///		}
    /// @endcode
    class GABC_API Walker
    {
    public:
	Walker()
	    : myFilename()
	{}
	virtual ~Walker();

	/// @c preProcess() is called on the "root" of the walk.  The root may
	/// @b not be the root of the Alembic file (i.e. when walking lists of
	/// objects).  The @c preProcess() method will be called one time only.
	virtual bool	preProcess(const GABC_IObject &node);

	/// Return true to continue traveral and process the children of the
	/// given node.  Returning false will process the next sibling.  Use
	/// interrupted() to perform an early termination.
	virtual bool	process(const GABC_IObject &node) = 0;

	/// Allow for interruption of walk
	virtual bool	interrupted() const	{ return false; }

	/// Manually walk children of the given node.  Returns false if the
	/// walk was interrupted, true if the walk was completed.
	bool		walkChildren(const GABC_IObject &node);

	/// @{
	///  Access the current filename
	const std::string	&filename() const	{ return myFilename; }
	void			setFilename(const std::string &f)
					{ myFilename = f; }
	/// @}

    private:
	std::string	myFilename;
	friend class GABC_Util;
    };

    /// Event functor called when an archive is flushed from the cache
    class GABC_API ArchiveEventHandler
    {
    public:
	ArchiveEventHandler() {}
	virtual ~ArchiveEventHandler();

	/// Return whether the event handler is wired up to an archive
	bool		valid() const	{ return myArchive != NULL; }

	/// Call this method to no longer receive events
	void		stopReceivingEvents();

	/// This method is called when the archive is cleared.  The handler
	/// will no longer receive any events after the archive is cleared.
	virtual void	cleared() = 0;

	/// @{
	/// @private
	/// Methods used to access internal state
	void		 setArchivePtr(void *a) { myArchive = a; }
	const void	*archive() const { return myArchive; }
	/// @}
    private:
	void	*myArchive;
    };
    typedef UT_SharedPtr<ArchiveEventHandler>	ArchiveEventHandlerPtr;

    /// Add an event handler to be notified of events on the given filename
    /// The method returns false if the archive hasn't been loaded yet.
    static bool		addEventHandler(const std::string &filename,
				const ArchiveEventHandlerPtr &handler);

    /// Clear the cache.  If no filename is given, the entire cache will be
    /// cleared.
    static void		clearCache(const char *filename=NULL);

    /// Set the cache size
    static void		setFileCacheSize(int nfiles);

    /// Get the file cache size
    static int		fileCacheSize();

    /// Find a given GABC_IObject in an Alembic file.
    static GABC_IObject	findObject(const std::string &filename,
				const std::string &objectpath);


    /// Walk the tree in an alembic file.  Returns false if traversal was
    /// interrupted, otherwise returns true.
    static bool		walk(const std::string &filename, Walker &walker);

    /// Process a list of unique objects in an Alembic file (including their
    /// children)
    static bool		walk(const std::string &filename, Walker &walker,
				const UT_StringArray &objects);

    /// Get the local transform for a given node in an Alembic file.  The @c
    /// isConstant flag will be true if the local transform is constant (even
    /// if parent transforms are non-constant).
    static bool		getLocalTransform(
				const std::string &filename,
				const std::string &objectpath,
				fpreal sample_time,
				UT_Matrix4D &xform,
				bool &isConstant,
				bool &inheritsXform);

    /// Get the world transform for a given node in an Alembic file.  If the
    /// given node is a shape node, the transform up to its parent will be
    /// computed.  If the transform is constant (including all parents), the @c
    /// isConstant flag will be set.
    ///
    /// The method returns false if there was an error computing the transform.
    static bool		 getWorldTransform(
				const std::string &filename,
				const std::string &objectpath,
				fpreal sample_time,
				UT_Matrix4D &xform,
				bool &isConstant,
				bool &inheritsXform);

    /// Get the world transform for an GABC_IObject in an Alembic file.  If the
    /// given node is a shape node, the transform up to its parent will be
    /// returned.
    static bool		 getWorldTransform(
				const std::string &filename,
				const GABC_IObject &object,
				fpreal sample_time,
				UT_Matrix4D &xform,
				bool &isConstant,
				bool &inheritsXform);

    /// Test whether an object is static or has an animated transform
    static bool		isTransformAnimated(
				const std::string &filename,
				const GABC_IObject &object);

    /// Return a list of all the objects in an Alembic file
    static const PathList	&getObjectList(const std::string &filename,
					bool include_face_sets=false);
};
}

#endif
