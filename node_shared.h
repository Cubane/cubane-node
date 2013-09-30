/* header file for functions shared among other modules in the node library */
#ifndef NODE_SHARED_H
#define NODE_SHARED_H

/******************************
 Which Allocator Should We Use?
 ******************************/

#if defined(_DEBUG)
/* In debug mode, we want the MSFT allocator for its leak-checking code */
//#define USE_DL_MALLOC 0
#else
/* In release mode, we want the DL allocator for its superior efficiency */
#define USE_DL_MALLOC 0
#endif

//uncomment below to debug the dlmalloc-using code
//#define DEBUG_DL_MALLOC 1

#ifdef DEBUG_DL_MALLOC
#define USE_DL_MALLOC 1
#endif

/* if we have the DL allocator */
#if defined(USE_DL_MALLOC)

/* undefine the basic memory function macros (if any) */
#ifdef malloc
#undef malloc
#endif
#ifdef free
#undef free
#endif
#ifdef calloc
#undef calloc
#endif
#ifdef realloc
#undef realloc
#endif

/* define the basic memory function macros to refer to dlmalloc functions */
#define malloc(n)			dlmalloc((n))
#define free(p)				dlfree((p))
#define calloc(n,s)			dlcalloc((n),(s))
#define realloc(p,n)		dlrealloc((p),(n))

/* include the dlmalloc library header */
#include "node_dlmalloc.h"

#else

/* using the MSFT allocator */
#define _CRTDBG_MAP_ALLOC 1
#include <crtdbg.h>

#endif

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************
 What Internal calling convention are we using?
 **********************************************/

/* pass some args in registers */
#define WITH_FASTCALL 1

#ifdef WITH_FASTCALL
#define	NODE_INTERNAL_FUNC	__fastcall
#else
#define NODE_INTERNAL_FUNC
#endif

/* hash a string */
unsigned int NODE_INTERNAL_FUNC node_hashA( const char * psKey );
unsigned int NODE_INTERNAL_FUNC node_hashW( const wchar_t * psKey );

#include "node_lookup2.h"
 
#ifdef __cplusplus
}
#endif

#endif /* NODE_SHARED_H */