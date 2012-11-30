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

#include "GABC_IItem.h"
#include "GABC_IArchive.h"

#define SHOW_COUNTS
#include <UT/UT_ShowCounts.h>
UT_COUNTER(theCount, "GABC_IItem")

GABC_IItem::GABC_IItem(const GABC_IArchivePtr &arch)
    : myArchive(NULL)
{
    UT_INC_COUNTER(theCount);
    setArchive(arch);
}
GABC_IItem::GABC_IItem(const GABC_IItem &src)
    : myArchive(NULL)
{
    UT_INC_COUNTER(theCount);
    setArchive(src.myArchive);
}

GABC_IItem::~GABC_IItem()
{
    UT_DEC_COUNTER(theCount);
    setArchive(NULL);
}

void
GABC_IItem::setArchive(const GABC_IArchivePtr &arch)
{
    if (arch.get() != myArchive.get())
    {
	// Lock both archives
	GABC_AutoLock	l1(myArchive);
	GABC_AutoLock	l2(arch);
	if (myArchive)
	{
	    myArchive->unreference(this);
	}
	myArchive = arch;
	if (arch)
	{
	    arch->reference(this);
	}
    }
}
