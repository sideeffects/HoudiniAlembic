/*
 * Copyright (c) 2017
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

#include "GABC_IItem.h"
#include "GABC_IArchive.h"

using namespace GABC_NAMESPACE;

//#define SHOW_COUNTS
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
	// Lock both archives - regardless of whether Alembic is threaded or not
	if (myArchive)
	{
	    GABC_AutoLock	l1(myArchive);
	    myArchive->unreference(this);
	    myArchive = NULL;
	}
	if (arch)
	{
	    GABC_AutoLock	l2(arch);
	    arch->reference(this);
	    myArchive = arch;
	}
    }
}
