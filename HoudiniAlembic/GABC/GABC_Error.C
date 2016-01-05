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

#include "GABC_Error.h"
#include <UT/UT_WorkBuffer.h>
#include <UT/UT_Interrupt.h>
#include <stdarg.h>

using namespace GABC_NAMESPACE;

static UT_Lock	theLock;

bool
GABC_Error::wasInterrupted() const
{
    return myInterrupt && myInterrupt->opInterrupt();
}

void
GABC_Error::clear()
{
    UT_AutoLock	lock(theLock);
    mySuccess = true;
    handleClear();
}

bool
GABC_Error::errorString(const char *msg)
{
    UT_AutoLock	lock(theLock);
    handleError(msg);
    mySuccess = false;
    return false;
}
void
GABC_Error::warningString(const char *msg)
{
    UT_AutoLock	lock(theLock);
    handleWarning(msg);
}

void
GABC_Error::infoString(const char *msg)
{
    UT_AutoLock	lock(theLock);
    handleInfo(msg);
}

bool
GABC_Error::error(const char *format, ...)
{
    UT_WorkBuffer	wbuf;
    va_list		args;

    va_start(args, format);
    wbuf.vsprintf(format, args);
    va_end(args);

    UT_AutoLock	lock(theLock);
    handleError(wbuf.buffer());
    mySuccess = false;
    return false;
}

void
GABC_Error::warning(const char *format, ...)
{
    UT_WorkBuffer	wbuf;
    va_list		args;

    va_start(args, format);
    wbuf.vsprintf(format, args);
    va_end(args);

    UT_AutoLock	lock(theLock);
    handleWarning(wbuf.buffer());
}

void
GABC_Error::info(const char *format, ...)
{
    UT_WorkBuffer	wbuf;
    va_list		args;

    va_start(args, format);
    wbuf.vsprintf(format, args);
    va_end(args);

    UT_AutoLock	lock(theLock);
    handleInfo(wbuf.buffer());
}

void
GABC_Error::handleError(const char *msg)
{
#if UT_ASSERT_LEVEL > 0
    fprintf(stderr, "Abc error: %s\n", msg);
#endif
}

void
GABC_Error::handleWarning(const char *msg)
{
#if UT_ASSERT_LEVEL > 1
    fprintf(stderr, "Abc warning: %s\n", msg);
#endif
}

void
GABC_Error::handleInfo(const char *msg)
{
#if UT_ASSERT_LEVEL > 2
    fprintf(stderr, "Abc info: %s\n", msg);
#endif
}

void
GABC_Error::handleClear()
{
}
