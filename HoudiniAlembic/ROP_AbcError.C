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
 * NAME:	ROP_AbcError.h ( ROP Library, C++)
 *
 * COMMENTS:
 */

#include "ROP_AbcError.h"
#include <UT/UT_WorkBuffer.h>
#include <UT/UT_Interrupt.h>
#include <stdarg.h>

ROP_AbcError::~ROP_AbcError()
{
}

bool
ROP_AbcError::wasInterrupted() const
{
    return myInterrupt && myInterrupt->opInterrupt();
}

bool
ROP_AbcError::error(const char *format, ...)
{
    UT_WorkBuffer	wbuf;
    va_list		args;

    va_start(args, format);
    wbuf.vsprintf(format, args);
    va_end(args);
    handleError(wbuf.buffer());
    mySuccess = false;
    return false;
}

void
ROP_AbcError::warning(const char *format, ...)
{
    UT_WorkBuffer	wbuf;
    va_list		args;

    va_start(args, format);
    wbuf.vsprintf(format, args);
    va_end(args);
    handleWarning(wbuf.buffer());
}

void
ROP_AbcError::info(const char *format, ...)
{
    UT_WorkBuffer	wbuf;
    va_list		args;

    va_start(args, format);
    wbuf.vsprintf(format, args);
    va_end(args);
    handleInfo(wbuf.buffer());
}

void
ROP_AbcError::handleError(const char *msg)
{
#if UT_ASSERT_LEVEL > 0
    fprintf(stderr, "Abc error: %s\n", msg);
#endif
}

void
ROP_AbcError::handleWarning(const char *msg)
{
#if UT_ASSERT_LEVEL > 1
    fprintf(stderr, "Abc warning: %s\n", msg);
#endif
}

void
ROP_AbcError::handleInfo(const char *msg)
{
#if UT_ASSERT_LEVEL > 2
    fprintf(stderr, "Abc info: %s\n", msg);
#endif
}

void
ROP_AbcError::handleClear()
{
}
