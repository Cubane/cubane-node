/* header file for functions shared among other modules in the node library */
#ifndef NODE_SHARED_H
#define NODE_SHARED_H

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
