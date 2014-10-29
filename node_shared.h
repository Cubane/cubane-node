/* header file for functions shared among other modules in the node library */
#ifndef NODE_SHARED_H
#define NODE_SHARED_H

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************
 What Internal calling convention are we using?
 **********************************************/

/* hash a string */
unsigned int node_hashA( const char * psKey );
unsigned int node_hashW( const wchar_t * psKey );

#include "node_lookup2.h"
 
#ifdef __cplusplus
}
#endif

#endif /* NODE_SHARED_H */
