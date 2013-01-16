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
 * NAME:	GABC_OError.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#include "GABC_OError.h"
#include <UT/UT_WorkBuffer.h>
#include <UT/UT_Interrupt.h>
#include <stdarg.h>

static UT_Lock	theLock;

GABC_OError::~GABC_OError()
{
}

bool
GABC_OError::wasInterrupted() const
{
    return myInterrupt && myInterrupt->opInterrupt();
}

void
GABC_OError::clear()
{
    UT_AutoLock	lock(theLock);
    mySuccess = true;
    handleClear();
}

bool
GABC_OError::errorString(const char *msg)
{
    UT_AutoLock	lock(theLock);
    handleError(msg);
    mySuccess = false;
    return false;
}
void
GABC_OError::warningString(const char *msg)
{
    UT_AutoLock	lock(theLock);
    handleWarning(msg);
}

void
GABC_OError::infoString(const char *msg)
{
    UT_AutoLock	lock(theLock);
    handleInfo(msg);
}

bool
GABC_OError::error(const char *format, ...)
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
GABC_OError::warning(const char *format, ...)
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
GABC_OError::info(const char *format, ...)
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
GABC_OError::handleError(const char *msg)
{
#if UT_ASSERT_LEVEL > 0
    fprintf(stderr, "Abc error: %s\n", msg);
#endif
}

void
GABC_OError::handleWarning(const char *msg)
{
#if UT_ASSERT_LEVEL > 1
    fprintf(stderr, "Abc warning: %s\n", msg);
#endif
}

void
GABC_OError::handleInfo(const char *msg)
{
#if UT_ASSERT_LEVEL > 2
    fprintf(stderr, "Abc info: %s\n", msg);
#endif
}

void
GABC_OError::handleClear()
{
}
