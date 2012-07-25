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

#ifndef __ROP_AbcError__
#define __ROP_AbcError__

#include <SYS/SYS_Types.h>

class UT_Interrupt;

class ROP_AbcError
{
public:
    ROP_AbcError(UT_Interrupt *interrupt)
	: mySuccess(true)
	, myInterrupt(interrupt)
    {}
    virtual ~ROP_AbcError();

    void	clear()
		{
		    mySuccess = true;
		    handleClear();
		}

    UT_Interrupt	*getInterrupt() const	{ return myInterrupt; }
    bool		 wasInterrupted() const;

    bool	success() const
		{
		    return mySuccess;
		}

    /// Error always returns false
    bool	errorString(const char *msg)
		{
		    mySuccess = false;
		    handleError(msg);
		    return false;
		}
    void	warningString(const char *msg)	{ handleWarning(msg); }
    void	infoString(const char *msg)		{ handleInfo(msg); }

    /// Error always returns false
    bool	error(const char *format, ...)
		    SYS_PRINTF_CHECK_ATTRIBUTE(2, 3);
    void	warning(const char *format, ...)
		    SYS_PRINTF_CHECK_ATTRIBUTE(2, 3);
    void	info(const char *format, ...)
		    SYS_PRINTF_CHECK_ATTRIBUTE(2, 3);
protected:

    virtual void	handleError(const char *msg);
    virtual void	handleWarning(const char *msg);
    virtual void	handleInfo(const char *msg);
    virtual void	handleClear();
private:
    UT_Interrupt	*myInterrupt;
    bool		 mySuccess;
};

#endif

