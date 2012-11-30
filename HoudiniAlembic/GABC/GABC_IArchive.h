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
 * NAME:	GABC_IArchive.h ( GABC Library, C++)
 *
 * COMMENTS:
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

    /// @{
    /// Open an archive.
    /// @note This method is @b not thread-safe.  You must lock around it.
    static GABC_IArchivePtr	open(const std::string &filename);
    /// @}

    /// @{
    /// Clear archives
    static void			closeArchive(const std::string &filename);
    static void			closeAll();
    /// @}


    /// Test validity
    bool		valid() const		{ return myArchive.valid(); }

    /// Error (set on creation)
    const std::string	&error() const		{ return myError; }

    /// Access the filename
    const std::string	&filename() const	{ return myFilename; }

    /// Access to the file lock - required for HDF5
    UT_Lock		&getLock() const	{ return *theLock; }

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

private:
    GABC_IArchive(const std::string &filename);

    // At the current time, HDF5 requires a *global* lock across all files.
    // Wouldn't it be nice if it could have a per-file lock?
    static UT_Lock	*theLock;

    SYS_AtomicInt32	 myRefCount;
    std::string		 myFilename;
    std::string		 myError;
    IArchive		 myArchive;
    SetType		 myObjects;
};

static inline void intrusive_ptr_add_ref(GABC_IArchive *i) { i->incref(); }
static inline void intrusive_ptr_release(GABC_IArchive *i) { i->decref(); }

class GABC_AutoLock
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
private:
    UT_Lock	*myLock;
};

#endif
