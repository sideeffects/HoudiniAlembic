
#ifndef __GABC_API_h__
#define __GABC_API_h__

#include <SYS/SYS_Visibility.h>

#ifdef GABC_EXPORTS
#define GABC_API SYS_VISIBILITY_EXPORT
#else
#define GABC_API SYS_VISIBILITY_IMPORT
#endif

#endif
