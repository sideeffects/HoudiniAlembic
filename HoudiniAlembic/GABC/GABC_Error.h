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

#ifndef __GABC_Error__
#define __GABC_Error__

#include "GABC_API.h"
#include <SYS/SYS_Types.h>

class UT_Interrupt;

namespace GABC_NAMESPACE
{

/// This class reports info/warning/error messages that occur during Alembic
/// import/export.
class GABC_API GABC_Error
{
public:
    GABC_Error(UT_Interrupt *interrupt)
	: mySuccess(true)
	, myInterrupt(interrupt)
    {}
    virtual ~GABC_Error() {}

    void            clear();

    UT_Interrupt   *getInterrupt() const    { return myInterrupt; }
    bool            wasInterrupted() const;

    bool            success() const         { return mySuccess; }

    /// @c errorString() always returns false
    bool            errorString(const char *msg);
    void            warningString(const char *msg);
    void            infoString(const char *msg);

    /// @c error() always returns false
    bool            error(const char *format, ...)
		            SYS_PRINTF_CHECK_ATTRIBUTE(2, 3);
    void            warning(const char *format, ...)
		            SYS_PRINTF_CHECK_ATTRIBUTE(2, 3);
    void            info(const char *format, ...)
		            SYS_PRINTF_CHECK_ATTRIBUTE(2, 3);
protected:
    /// @{
    /// Callbacks to process error, warning, info and clear
    virtual void    handleError(const char *msg);
    virtual void    handleWarning(const char *msg);
    virtual void    handleInfo(const char *msg);
    virtual void    handleClear();
    /// @}

private:
    UT_Interrupt   *myInterrupt;
    bool            mySuccess;
};

} // end GABC_NAMESPACE

#endif
