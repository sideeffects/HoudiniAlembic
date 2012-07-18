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
 * NAME:	GABC_Include.h ( GABC Library, C++)
 *
 * COMMENTS:
 */

#ifndef __GABC_Include__
#define __GABC_Include__

// When compiling under MSVC for DLLs, we need to set some defines to ensure
// that the proper linking pragma's in boost/config/auto_link.hpp are done
// correctly.
//
// This header must be included before the inclusion of boost or openexr
// headers.
#if defined(WIN32) && !defined(MAKING_STATIC)
    #if !defined(BOOST_THREAD_DYN_DLL)
	#define BOOST_THREAD_DYN_DLL
    #endif
    #if !defined(BOOST_DATE_TIME_DYN_LINK)
	#define BOOST_DATE_TIME_DYN_LINK
    #endif
    #if !defined(OPENEXR_DLL)
	#define OPENEXR_DLL
    #endif
#endif

#endif
