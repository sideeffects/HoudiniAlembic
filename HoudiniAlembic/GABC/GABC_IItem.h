/*
 * Copyright (c) 2016
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

#ifndef __GABC_IItem__
#define __GABC_IItem__

#include "GABC_API.h"
#include "GABC_Include.h"
#include "GABC_Types.h"
#include <UT/UT_IntrusivePtr.h>
#include <UT/UT_String.h>

namespace GABC_NAMESPACE
{
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
}

#endif
