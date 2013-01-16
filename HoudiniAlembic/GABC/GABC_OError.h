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

#ifndef __ROP_AbcError__
#define __ROP_AbcError__

#include "GABC_API.h"
#include <SYS/SYS_Types.h>

class UT_Interrupt;

/// Class to handle error messages for output of Alembic geometry
class GABC_API GABC_OError
{
public:
    GABC_OError(UT_Interrupt *interrupt)
	: mySuccess(true)
	, myInterrupt(interrupt)
    {}
    virtual ~GABC_OError();

    void	clear();

    UT_Interrupt	*getInterrupt() const	{ return myInterrupt; }
    bool		 wasInterrupted() const;

    bool		 success() const	{ return mySuccess; }

    /// @c errorString() always returns false
    bool	errorString(const char *msg);
    void	warningString(const char *msg);
    void	infoString(const char *msg);

    /// @c error() always returns false
    bool	error(const char *format, ...)
		    SYS_PRINTF_CHECK_ATTRIBUTE(2, 3);
    void	warning(const char *format, ...)
		    SYS_PRINTF_CHECK_ATTRIBUTE(2, 3);
    void	info(const char *format, ...)
		    SYS_PRINTF_CHECK_ATTRIBUTE(2, 3);
protected:

    /// @{
    /// Callbacks to process error, warning, info and clear
    virtual void	handleError(const char *msg);
    virtual void	handleWarning(const char *msg);
    virtual void	handleInfo(const char *msg);
    virtual void	handleClear();
    /// @}
private:
    UT_Interrupt	*myInterrupt;
    bool		 mySuccess;
};

#endif
