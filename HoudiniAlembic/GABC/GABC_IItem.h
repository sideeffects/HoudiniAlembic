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
 * NAME:	GABC_IItem.h (GABC Library, C++)
 *
 * COMMENTS:
 */

#ifndef __GABC_IItem__
#define __GABC_IItem__

#include "GABC_API.h"
#include "GABC_Include.h"
#include "GABC_Types.h"
#include <UT/UT_IntrusivePtr.h>
#include <UT/UT_String.h>

/// Objects stored in GABC_IArchives
///
/// Since IArchives keep references to objects, holding onto a reference will
/// prevent the archive from closing.  When we try to close an archive (i.e. so
/// that we can write over it), we can't have any references dangling.  The
/// GABC_IArchive maintains a set of all the items and can force them to purge
/// their data.
class GABC_API GABC_IItem
{
public:
    GABC_IItem(const GABC_IArchivePtr &arch=NULL);
    GABC_IItem(const GABC_IItem &src);
    virtual ~GABC_IItem();

    GABC_IItem	&operator=(const GABC_IItem &src)
		{
		    setArchive(src.myArchive);
		    return *this;
		}

    /// Purge any references to objects in the Alembic archive.  Once the
    /// object is purged, it should be considered to be an invalid object.  The
    /// object does not have to re-resolve itself.
    /// @warning The purge() method should not alter the base class archive.
    virtual void	purge() = 0;

    /// @{
    /// Access the archive
    const GABC_IArchivePtr	&archive() const { return myArchive; }
    void			 setArchive(const GABC_IArchivePtr &a);
    /// @}

private:
    GABC_IArchivePtr	myArchive;
};

#endif
