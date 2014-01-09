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

#ifndef __GABC_IArchive__
#define __GABC_IArchive__

#include "GABC_API.h"
#include "GABC_Include.h"
#include "GABC_IObject.h"
#include <SYS/SYS_AtomicInt.h>
#include <UT/UT_Lock.h>
#include <UT/UT_Set.h>
#include <Alembic/Abc/IArchive.h>

// Change this when Ogawa is supported
#if ALEMBIC_LIBRARY_VERSION >= 10200
    #define GABC_OGAWA	1
#endif

// If you're using a thread-safe version of Alembic (thread-safe HDF5 or Ogawa
// for example), you can set this define.
#define GABC_ALEMBIC_THREADSAFE

namespace GABC_NAMESPACE
{
class GABC_IItem;

/// Wrapper around an Alembic archive.  This provides thread-safe access to
/// Alembic data.
class GABC_API GABC_IArchive
{
public:
    typedef Alembic::Abc::IArchive	IArchive;
    typedef UT_Set<GABC_IItem *>	SetType;

    /// Destructor
    ~GABC_IArchive();

    /// Test validity
    bool		valid() const		{ return myArchive.valid(); }

    /// Check to see if the archive has been purged (invalid)
    bool		purged() const		{ return myPurged; }

    /// Error (set on creation)
    const std::string	&error() const		{ return myError; }

    /// Access the filename
    const std::string	&filename() const	{ return myFilename; }

    /// Get the root object
    GABC_IObject	getTop() const;

    /// Purge all objects references
    void		purgeObjects();

    /// @{
    /// @private
    /// Called by GABC_IObject to resolve the object
    void		resolveObject(GABC_IObject &obj);

    /// Called to maintain references to GABC_IItem objects
    void		reference(GABC_IItem *item);
    void		unreference(GABC_IItem *item);
    /// @}

    /// @{
    /// Reference counting
    void	incref()	{ myRefCount.add(1); }
    void	decref()
		{
		    if (!myRefCount.add(-1))
			delete this;
		}
    /// @}

    /// @{
    /// Open an archive.  Please use GABC_Util::open instead
    /// This method is @b not thread-safe.  You must lock around it.
    /// @private
    static GABC_IArchivePtr	open(const std::string &filename);
    /// @}

private:
    /// Access to the file lock - required for non-thread safe HDF5
    UT_Lock		&getLock() const	{ return *theLock; }
    friend class	 GABC_AutoLock;

    GABC_IArchive(const std::string &filename);

    // At the current time, HDF5 requires a *global* lock across all files.
    // Wouldn't it be nice if it could have a per-file lock?
    static UT_Lock	*theLock;

    SYS_AtomicInt32	 myRefCount;
    std::string		 myFilename;
    std::string		 myError;
    IArchive		 myArchive;
    SetType		 myObjects;
    bool		 myPurged;
};

static inline void intrusive_ptr_add_ref(GABC_IArchive *i) { i->incref(); }
static inline void intrusive_ptr_release(GABC_IArchive *i) { i->decref(); }

/// When running with a thread safe Alembic implementation, the fake lock is
/// used (since the underlying library is safe).
class GABC_API GABC_AutoFakeLock
{
public:
     GABC_AutoFakeLock(const GABC_IArchive &) {}
     GABC_AutoFakeLock(const GABC_IArchivePtr &) {}
    ~GABC_AutoFakeLock() {}
    void	unlock() {}
};
/// The true lock is an implementation of a UT_AutoLock which is used when
/// locking internal library structures.
class GABC_API GABC_AutoLock
{
public:
    GABC_AutoLock(const GABC_IArchive &arch)
	: myLock(&(arch.getLock()))
    {
	if (myLock)
	    myLock->lock();
    }
    GABC_AutoLock(const GABC_IArchivePtr &arch)
	: myLock(arch ? &(arch->getLock()) : NULL)
    {
	if (myLock)
	    myLock->lock();
    }
    ~GABC_AutoLock()
    {
	if (myLock)
	    myLock->unlock();
    }
    void	unlock()	{ myLock->unlock(); myLock = NULL; }
private:
    UT_Lock	*myLock;
};

#if defined(GABC_ALEMBIC_THREADSAFE)
typedef GABC_AutoFakeLock	GABC_AlembicLock;
#else
typedef GABC_AutoLock	GABC_AlembicLock;
#endif

}

#endif
