#ifndef GOOGLE_SPARSEHASH_WINDOWS_CONFIG_H_
#define GOOGLE_SPARSEHASH_WINDOWS_CONFIG_H_

/* src/config.h.in.  Generated from configure.ac by autoheader.  */

/* Namespace for Google classes */
#define GOOGLE_NAMESPACE  ::google

/* the location of the header defining hash functions */
#define HASH_FUN_H  <hash_map>

/* the location of <unordered_map> or <hash_map> */
#define HASH_MAP_H  <hash_map>

/* the namespace of the hash<> function */
#define HASH_NAMESPACE  stdext

/* the location of <unordered_set> or <hash_set> */
#define HASH_SET_H  <hash_set>

/* Define to 1 if you have the <google/malloc_extension.h> header file. */
#undef HAVE_GOOGLE_MALLOC_EXTENSION_H

/* define if the compiler has hash_map */
#define HAVE_HASH_MAP  1

/* define if the compiler has hash_set */
#define HAVE_HASH_SET  1

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define to 1 if the system has the type `long long'. */
#define HAVE_LONG_LONG  1

/* Define to 1 if you have the `memcpy' function. */
#define HAVE_MEMCPY  1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE  1

/* Define to 1 if you have the <memory.h> header file. */
#undef HAVE_MEMORY_H

/* define if the compiler implements namespaces */
#define HAVE_NAMESPACES  1

/* Define if you have POSIX threads libraries and header files. */
#undef HAVE_PTHREAD

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H  1

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H  1

/* Define to 1 if you have the <sys/resource.h> header file. */
#undef HAVE_SYS_RESOURCE_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H  1

/* Define to 1 if you have the <sys/time.h> header file. */
#undef HAVE_SYS_TIME_H

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H  1

/* Define to 1 if you have the <sys/utsname.h> header file. */
#undef HAVE_SYS_UTSNAME_H

/* Define to 1 if the system has the type `uint16_t'. */
#undef HAVE_UINT16_T

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* define if the compiler supports unordered_{map,set} */
#undef HAVE_UNORDERED_MAP

/* Define to 1 if the system has the type `u_int16_t'. */
#undef HAVE_U_INT16_T

/* Define to 1 if the system has the type `__uint16'. */
#define HAVE___UINT16  1

/* Name of package */
#undef PACKAGE

/* Define to the address where bug reports for this package should be sent. */
#undef PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#undef PACKAGE_NAME

/* Define to the full name and version of this package. */
#undef PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#undef PACKAGE_TARNAME

/* Define to the version of this package. */
#undef PACKAGE_VERSION

/* Define to necessary symbol if this constant uses a non-standard name on
   your system. */
#undef PTHREAD_CREATE_JOINABLE

/* The system-provided hash function including the namespace. */
#define SPARSEHASH_HASH  HASH_NAMESPACE::hash_compare

/* The system-provided hash function, in namespace HASH_NAMESPACE. */
#define SPARSEHASH_HASH_NO_NAMESPACE  hash_compare

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS  1

/* the namespace where STL code like vector<> is defined */
#define STL_NAMESPACE  std

/* Version number of package */
#undef VERSION

/* Stops putting the code inside the Google namespace */
#define _END_GOOGLE_NAMESPACE_  }

/* Puts following code inside the Google namespace */
#define _START_GOOGLE_NAMESPACE_   namespace google {


// ---------------------------------------------------------------------
// Extra stuff not found in config.h.in

#define HAVE_WINDOWS_H  1   // used in time_hash_map

// This makes sure the definitions in config.h and sparseconfig.h match
// up.  If they don't, the compiler will complain about redefinition.
#include <google/sparsehash/sparseconfig.h>

// TODO(csilvers): include windows/port.h in every relevant source file instead?
#include "windows/port.h"

#endif  /* GOOGLE_SPARSEHASH_WINDOWS_CONFIG_H_ */
