/* node.c -- implementation of node module
 * Initial version written 2000-08-07 Cubane Software
 * Copyright 2000-2003 G. Michaels Consulting Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "assert.h"
#include <crtdbg.h>

#define __STDC__	1

#include "node.h"

#include "mv_hash.h"

#include "message.h"

#ifndef _DEBUG
#define USE_DL_MALLOC
#endif
#include "malloc.h"

/****************
 Module Variables
 ****************/

static int node_nSpaces = 0;		/* the current level of indentation */

/***********************
 Private Named Constants
 ***********************/

#define DEFAULT_HASHBUCKETS 17	
#define LOTS_OF_MEMORY 65536

/*****************************
 Private Function Declarations
 *****************************/

/* set the node to a value using variable arguments. nType determines how 
arguments are processed.*/
static void node_set_valist(node_t *pn, int nType, va_list valist);

/* hash a string */
static unsigned long node_hash( char * psKey );

/* initialize a list node */
static void node_list_init(node_t * pn);	

/* initialize a hash node */
static void node_hash_init(node_t * pn, int nHashBuckets );

/* clean up overlaid structures in a node changing type */
static void node_cleanup( node_t * pn );

/* dlmalloc a new string */
static char * node_safe_copy(char * ps);

/* read a line from a file into malloc'ed storage */
static char * read_line( FILE * pfIn );

/* escape and unescape pesky characters */
static char * node_escape( char * psUnescaped );
static char * node_unescape( char * psEscaped );

/*******************************
 Public Function Implementations
 *******************************/

/****************
 Memory Functions 
 ****************/

/* allocate memory for a new node and return a pointer */
node_t * node_alloc()
{
	node_t * pnNew = NULL;

	pnNew = (node_t *)malloc( sizeof(node_t) );
	if( pnNew == NULL ) 
	{
		errexit("Out of memory: can't allocate new node\n");
	}

	assert( pnNew != NULL );

	pnNew->pnNext = NULL;

	pnNew->nType = NODE_UNKNOWN;

	pnNew->psName = NULL;
	pnNew->nHash = 0;

	pnNew->nValue  = 0;
	pnNew->dfValue = 0.0;
	pnNew->psValue = NULL;

	pnNew->pbValue = NULL;
	pnNew->nDataLength = 0;

	pnNew->pnListHead = NULL;
	pnNew->pnListTail = NULL;

	pnNew->ppnHashHeads  = NULL;
	pnNew->ppnHashTails  = NULL;
	pnNew->nHashBuckets  = 0;
	pnNew->nHashElements = 0;

	return pnNew;
}

/* free memory associated with a node, including children and neighbours*/
void node_free(node_t *pn)
{
	node_t * pnSaved = NULL;

	while( pn != NULL ) 
	{

		pnSaved = pn->pnNext;

		if( pn->psName != NULL ) 
		{
			free( pn->psName );
			pn->psName = NULL;
		}
		
		/* free and NULL all members of pn (type specific) */
		node_cleanup( pn );

		free( pn );
		pn = pnSaved;

	} /* while (pn is not null) */

	return;
}

/* initialize a list node:
 *
 * This function converts a non-list node to an empty list node
 * and leaves list nodes untouched.
 * 
 */
static void node_list_init(node_t * pn)
{
	assert( pn != NULL );

	if( pn == NULL )
		return;

	/* if it's a list already */
	if( pn->nType == NODE_LIST ) 
	{

		/* do nothing */
		return;

	} 
	else 
	{
	/* else: it's not a list */

		/* free and null the previous occupants of the union */
		node_cleanup( pn );

		/* set the type */
		pn->nType = NODE_LIST;

		/* do the tail magic */
		pn->pnListTail = (node_t*)&(pn->pnListHead);

		/* initialize the element count */
		pn->nListElements = 0;
	}

	return;
}

/* initialize a hash node */
static void node_hash_init( node_t * pn, int nHashBuckets )
{
	int i;

	assert( pn != NULL );
	if( pn == NULL )
		return;
	
	/* if it's a hash, do nothing */
	if(pn->nType == NODE_HASH)
	{
		return;
	}

	/* otherwise, */
	
	/* free and null the previous occupants of the union */
	node_cleanup( pn );

	/* again, make sure no-one is corrupting our information */
	assert(pn->nHashBuckets == 0);
	assert(pn->ppnHashTails == NULL);
	
	/* set nHashBuckets */
	pn->nHashBuckets = nHashBuckets;

	/* allocate ppnHashHeads */
	pn->ppnHashHeads = ( node_t ** )malloc( sizeof(node_t*)*pn->nHashBuckets );
	
	if( pn->ppnHashHeads == NULL )
	{
		errexit("Out of memory: can't allocate ppnHashHeads.\n");
	}

	/* allocate ppnHashTails */
	pn->ppnHashTails = (node_t **)malloc( sizeof(node_t*)*pn->nHashBuckets );

	if( pn->ppnHashTails == NULL )
	{
		errexit("Out of memory: can't allocate ppnHashTails.\n");
	}
	
	for( i=0; i < pn->nHashBuckets; i++ )
	{
		/* null each element of ppnHashHeads */
		pn->ppnHashHeads[i] = NULL;

		/* do the tail magic with each element of ppnHashHeads */
		pn->ppnHashTails[i] = (node_t*)&(pn->ppnHashHeads[i]);
	
	}
	
	/* set nHashElements to 0 */
	pn->nHashElements = 0;
	
	/* set nType to NODE_HASH */
	pn->nType = NODE_HASH;

	return;

}

node_t * node_list_alloc()
{
    node_t * pn = NULL;

    pn = node_alloc();

    node_list_init( pn );

    return pn;
}

node_t * node_hash_alloc()
{
    node_t * pn = NULL;

    pn = node_alloc();

    node_hash_init( pn, DEFAULT_HASHBUCKETS );

    return pn;
}

node_t * node_hash_alloc2( int nHashBuckets )
{
    node_t * pn = NULL;

    pn = node_alloc();

    node_hash_init( pn, nHashBuckets );

    return pn;
}

/*****************
 Setting Functions
 *****************/

/* node_set is a wrapper for the private function node_set_valist */ 
void node_set(node_t * pn, int nType, ...)
{
	va_list valist;

	assert( pn != NULL );
    if( pn == NULL )
        return;

	/* va_start starts processing the variable arguments */
	va_start(valist, nType);

	/* pass the variable arguments to node_set_valist */
	node_set_valist(pn, nType, valist);

	/* clean up */
	va_end(valist);

	return;

}	

/*****************
 Reading Functions
 *****************/

/* returns the int value from the node */
int node_get_int(node_t *pn)
{
	assert( pn != NULL );

	if( pn == NULL )
		return 0;

	/* switch nType */
	switch( pn->nType )
	{
	case NODE_INT:
		return pn->nValue;

	case NODE_REAL:	
		/* convert the real to int and return that */
		return (int)pn->dfValue;

	case NODE_STRING:
		/* convert the string to int and return that */
		return atoi(pn->psValue);

	case NODE_LIST:
		/* if the list has at least one element */
		if( node_first(pn) != NULL)
		{
			/* return the int value of the first element */
			return node_get_int(node_first(pn));
		}
		else	/* empty list */
		{
			assert(0);	/* called node_get_int on empty list node */
		}
		break;
		
	default:
		/* all other types */
		assert(0);	/* called node_get_int on invalid type */
	}	/* end switch */

	return 0;		/* fail gracefully on invalid input */

}

/* returns the real value from the node */
double node_get_real(node_t *pn)
{
	assert( pn != NULL );

	if( pn == NULL )
		return 0.0;

	/* switch nType */
	switch( pn->nType )
	{
	case NODE_INT:
		/* convert the int to real and return that */
		return (double)pn->nValue;

	case NODE_REAL:	
		return pn->dfValue;

	case NODE_STRING:
		/* convert the string to real and return that */
		return atof(pn->psValue);

	case NODE_LIST:
		/* if the list has at least one element */
		if( node_first(pn) != NULL )
		{
			/* return the real value of the first element */
			return node_get_real( node_first(pn) );
		}
		else	/* empty list */
		{
			assert(0);	/* called node_get_real on empty list node */
		}
		break;
		
	default:
		/* all other types */
		assert(0);	/* called node_get_real on invalid type */
	}	/* end switch */

	return 0.0;		/* fail gracefully on invalid input */

}

/* returns the string value from the node */
char * node_get_string(node_t *pn)
{
	assert( pn != NULL );
	
	if( pn == NULL )
		return "";

	switch (pn->nType) 
	{
	case NODE_STRING:
		return pn->psValue;

	case NODE_INT:
		/* free string value if set */
		if( pn->psValue != NULL ) 
		{
			free( pn->psValue );
		}

		/* allocate enough memory in psValue to store string rep. of longest int */
		pn->psValue = (char *)malloc( strlen("-2147483648") + 1 );
		if( pn->psValue == NULL ) 
		{
			errexit( "Out of memory: can't allocate memory for int to string conversion\n" );
		}

		/* convert nValue into psValue */
		pn->psValue = _itoa( pn->nValue, pn->psValue, 10 );

		/* return psValue */
		return pn->psValue;

	case NODE_REAL:
		/* free string value if set */
		if( pn->psValue != NULL ) 
		{
			free( pn->psValue );
		}

		/* allocate enough memory in psValue to store string rep. of real */
		pn->psValue = (char *)malloc( strlen("-1000000000.00000") + 1 );
		if( pn->psValue == NULL ) 
		{
			errexit( "Out of memory: can't allocate memory for real to string conversion\n" );
		}

		/* convert nValue into psValue */
		sprintf( pn->psValue, "%-16.5f", pn->dfValue );

		/* return psValue */
		return pn->psValue;

	case NODE_LIST:
		/* if has at least one element */
		if( node_first( pn ) != NULL ) 
		{
			/* call node_get_string on first element */
			return node_get_string( node_first( pn ) );
		}
		else	/* pn is an empty list */
		{
			/* treat it as if it had an invalid type */
			/*EMPTY*/;
		}
		/*FALLTHROUGH*/

	default:	/* invalid */
		assert(0); /* invalid node type: fail gracefully by returning empty string */

		/* free psValue if set */
		if( pn->psValue != NULL ) 
		{
			free( pn->psValue );
		}

		/* set psvalue to empty string */
		pn->psValue = node_safe_copy("");

	}

	return pn->psValue;
}

/* returns the data value from the node */
data_t * node_get_data(node_t *pn, int * pnLength)
{
	assert( pn != NULL );

	if( pn == NULL )
	{
		*pnLength = 0;
		return (data_t*)"";
	}

	switch( pn->nType )
	{
	case NODE_DATA:

		/* set pnLength to the length of data stored in pn */
		*pnLength = pn->nDataLength;
		
		/* return the pointer to data */
		return pn->pbValue;
		
	case NODE_LIST:

		/* if the list has more than one element */
		if( node_first( pn ) != NULL ) 
		{
			/* call node_get_data on first element */
			return node_get_data( node_first( pn ), pnLength );
		}
		else	/* pn is an empty list */
		{
			/* treat it as if it had an invalid type */
			/*EMPTY*/;
		}
		/*FALLTHROUGH*/
			
	default:

		/* otherwise, since no type is convertible to data */
		assert(0);	/* fail gracefully on invalid types */	
		*pnLength = 0;	/* set the length to 0 */
	}

	return NULL;

}

/**************
 List Functions
 **************/

/* add a new node to the end of a list; similar variable arguments to node_set */
void node_list_add(node_t * pnList, int nType, ...)
{
	node_t * pnElement = NULL;
	node_t * pnNew = NULL;
	node_t * pnSaveNext = NULL;
	va_list valist;

	assert( pnList != NULL );

	if( pnList == NULL )
		return;

	/* make sure list is initialized */
	node_list_init( pnList );

	/* grab the variable arguments */
	va_start(valist, nType);

	switch( nType)
	{
	/* if it's a list, hash or node */
	case NODE_LIST:
	case NODE_HASH:
	case NODE_NODE:
	
		/* get and check the variable argument */
		pnElement = va_arg( valist, node_t * );

		if( pnElement == NULL ) 
		{
			assert( 0 );
			errmsg( "Attempted to add NULL node to list.\n" );
			return;
		}

		/* if the node we're adding is in a list or hash, it may have
		 * pnNext set; temporarily set pnNext to NULL while we copy the
		 * node so we don't take the rest of its list or hash along
		 */

		/* save the pointer to next */
		pnSaveNext = pnElement->pnNext;
		pnElement->pnNext = NULL;

		/* make a deep copy of the node to be added (without its neighbors) */
		pnNew = node_copy( pnElement );

		/* restore the pointer to next */
		pnElement->pnNext = pnSaveNext;

		/* assert that the new node is terminated */
		assert(pnNew->pnNext == NULL);
		break;

	/* otherwise */
	default:

		/* create a new node with node_alloc */
		pnNew = node_alloc();


		/* pass the variable arguments to node_set_valist */
		node_set_valist(pnNew, nType, valist);


		break;
	}

	/* common code */

	/* clean up the variable arguments*/
	va_end(valist);

	/* add the new node to the end of the list */
	pnList->pnListTail->pnNext = pnNew;

	/* advance the tail pointer */
	pnList->pnListTail = pnList->pnListTail->pnNext;

	/* increase the element count */
	pnList->nListElements++;

	return;

}

/* deletes a node from a list */
void node_list_delete( node_t * pnList, node_t * pnToDelete )
{
	node_t * pnPrevious = NULL;
	node_t * pnScroll = NULL;

	assert( pnList != NULL );
	assert( pnToDelete != NULL );
	
	if( pnList == NULL || pnToDelete == NULL )
		return;

	if( pnList->nType != NODE_LIST )
	{
		assert(0);
		return;
	}
	
	/* if the node to delete is the first in the list */
	if( pnToDelete == node_first( pnList ) )
	{
		/* cut the head off the list */
		pnList->pnListHead = node_next( pnToDelete );
		pnToDelete->pnNext = NULL;

		/* if it's a one-membered list */
		if( pnToDelete == pnList->pnListTail )
		{
			/* fix the tail */
			pnList->pnListTail = (node_t*)&(pnList->pnListHead);
		}

		assert( pnList->nListElements > 0 );
		pnList->nListElements--;

		return;
	}

	for( pnPrevious = node_first( pnList ), pnScroll = node_next( pnPrevious );
		 pnScroll != NULL; pnPrevious = pnScroll, pnScroll = node_next( pnPrevious ) )
	{
		/* if the current node is the node to delete */
		if( pnToDelete == pnScroll )
		{
			/* cut it out of the list */
			pnPrevious->pnNext = node_next( pnToDelete );
			pnToDelete->pnNext = NULL;

			/* if it was the end of the list */
			if( pnToDelete == pnList->pnListTail )
			{
				/* make the tail pointer point to the new tail */
				pnList->pnListTail = pnPrevious;
			}

			assert( pnList->nListElements > 0 );
			pnList->nListElements--;

			return;
		}
	}

	/* if we got here, then the node was not found in the list! */
	assert(0);

}

/* returns the first node of a list */
node_t * node_first(node_t * pnList)
{
	assert( pnList != NULL );

	if( pnList == NULL )
		return NULL;

	/* half of a simple iterator: returns pnListHead */
	if( pnList->nType == NODE_LIST )
	{
		return pnList->pnListHead;
	}
	else	/* not currently a list, but may have been in the past... */
	{
		assert(0);	/* invalid type for node_first */
		return NULL;	/* return explicit NULL instead of pnListHead */
	}

}

/* returns the next node in a list or hash bucket chain */
node_t * node_next(node_t * pn)
{
	assert( pn != NULL );

	return pn->pnNext;
}

/* Stack functions: treating the list as a stack */
/* pushes a node onto the front of a list */
void node_push( node_t * pnList, int nType, ... )
{
	va_list valist;

    node_t * pnElement = NULL;
    node_t * pnNew = NULL;
    node_t * pnSaveNext = NULL;

	assert( pnList != NULL );

	if( pnList == NULL )
		return;

	/* make sure list is initialized */
	node_list_init( pnList );

	/* grab the variable arguments */
	va_start(valist, nType);

	switch( nType)
	{
	/* if it's a list, hash or node */
	case NODE_LIST:
	case NODE_HASH:
	case NODE_NODE:
	
		/* get and check the variable argument */
		pnElement = va_arg( valist, node_t * );

		if( pnElement == NULL ) 
		{
			assert( 0 );
			errmsg( "Attempted to add NULL node to list.\n" );
			return;
		}

		/* if the node we're adding is in a list or hash, it may have
		 * pnNext set; temporarily set pnNext to NULL while we copy the
		 * node so we don't take the rest of its list or hash along
		 */

		/* save the pointer to next */
		pnSaveNext = pnElement->pnNext;
		pnElement->pnNext = NULL;

		/* make a deep copy of the node to be added (without its neighbors) */
		pnNew = node_copy( pnElement );

		/* restore the pointer to next */
		pnElement->pnNext = pnSaveNext;

		/* assert that the new node is terminated */
		assert(pnNew->pnNext == NULL);
		break;

	/* otherwise */
	default:

		/* create a new node with node_alloc */
		pnNew = node_alloc();

		/* pass the variable arguments to node_set_valist */
		node_set_valist(pnNew, nType, valist);

		break;
	}

    /* put the node at the head of the list */
    pnNew->pnNext = pnList->pnListHead;

    /* put the new list-chain in the list */
    pnList->pnListHead = pnNew;

    /* if the list was empty */
    if( pnList->pnListHead->pnNext == NULL )
    {
        /* advance the tail pointer */
        pnList->pnListTail = pnList->pnListTail->pnNext;
    }

	pnList->nListElements++;

    return;
}

/* pops a node off the front of a list */
node_t * node_pop( node_t * pnList )
{
    node_t * pnPopped = NULL;

    assert( pnList != NULL );
    assert( pnList->nType == NODE_LIST );

	if( pnList == NULL || pnList->nType != NODE_LIST )
		return NULL;

    /* if the list is empty */
    if( pnList->pnListHead == NULL )
    {
        /* return NULL */
        return NULL;
    }

    /* get a pointer to the list head */
    pnPopped = node_first( pnList );

    /* delete the node from the list */
    node_list_delete( pnList, pnPopped );

    /* return the old head */
    return pnPopped;
}

/**************
 Hash Functions
 **************/

/* add a node to a hash{} similar variable arguments to node_set */
void node_hash_add(node_t * pnHash, char * psKey, int nType, ...)
{
	node_t * pnElement = NULL;
	node_t * pnNew = NULL;
	node_t * pnSaveNext = NULL;

	node_t * pnOld = NULL;

	va_list valist;
	int nBucket;
	unsigned long nHash;

	assert( pnHash != NULL );
	assert( psKey != NULL );

	if( pnHash == NULL || psKey == NULL )
		return;

	/* make sure hash is initialized */
	node_hash_init( pnHash, DEFAULT_HASHBUCKETS );

	/* if the item already exists in the hash */
	pnOld = node_hash_get( pnHash, psKey );
	if( pnOld != NULL )
	{
		/* delete it */
		node_hash_delete( pnHash, pnOld );

		/* free it */
		node_free( pnOld );
	}

	/* grab the variable arguments */
	va_start(valist, nType);

	switch( nType )
	{

	/* if it's a list, hash or node */
	case NODE_LIST:
	case NODE_HASH:
	case NODE_NODE:
		
		/* get and check the variable argument */
		pnElement = va_arg( valist, node_t * );

		if( pnElement == NULL ) 
		{
			assert( 0 );
			errmsg( "Attempted to add NULL node to hash.\n" );
			return;
		}

		/* if the node we're adding is in a list or hash, it may have
		 * pnNext set; temporarily set pnNext to NULL while we copy the
		 * node so we don't take the rest of its list or hash along
		 */

		/* save the pointer to next */
		pnSaveNext = pnElement->pnNext;
		pnElement->pnNext = NULL;

		/* make a deep copy of the node to be added (without its neighbors) */
		pnNew = node_copy( pnElement );

		/* restore the pointer to next */
		pnElement->pnNext = pnSaveNext;

		/* assert that the new node is terminated */
		assert(pnNew->pnNext == NULL);
		break;

	default:
		/* create a new node */
		pnNew = node_alloc();

		/* call node_set_valist on the input*/
		node_set_valist(pnNew, nType, valist);

		break;
	}

	/* common code */

	/* clean up after variable argument processing*/
	va_end(valist);

	/* assert that psKey is valid */
	assert( psKey != NULL );

	/* set the node name to psKey */
	node_set_name( pnNew, psKey );

	/* hash the name */
	nHash = node_hash( pnNew->psName );
	
	/* get a bucket number */
	nBucket = nHash % pnHash->nHashBuckets;

	/* add the element to the tail of that bucket */
	pnHash->ppnHashTails[nBucket]->pnNext = pnNew;

	/* advance the bucket tail pointer */
	pnHash->ppnHashTails[nBucket] = pnHash->ppnHashTails[nBucket]->pnNext;

	/* increment the number of hash elements */
	pnHash->nHashElements++;

	return;
}

/* get a node (by name) from a hash */
node_t * node_hash_get(node_t * pnHash, char * psKey)
{
	unsigned long nHash;
	int nBucket;
	int nMasked;
	node_t * pnElement = NULL;

	assert( pnHash != NULL );
	assert( psKey != NULL );

	if( pnHash == NULL || psKey == NULL )
		return NULL;

	/* if nType is not NODE_HASH, assert and return NULL */
	if( pnHash->nType != NODE_HASH )
	{
		assert(0);	/* tried to get a hash out of a non-hash node! */
		return NULL;
	}

	/* hash psKey */
	nHash = node_hash( psKey );

	/* get a bucket number */
	nBucket = nHash % pnHash->nHashBuckets;

	/* search the bucket for value associated with psKey */
	pnElement = pnHash->ppnHashHeads[nBucket];

	/* get the masked hash value */
	nMasked = nHash & NODE_HASH_MASK;

	while( pnElement != NULL)
	{
		if( nMasked == pnElement->nHash &&
			 stricmp( pnElement->psName, psKey ) == 0 )
		{
			/* if found, return a pointer to the found element */
			return pnElement;
		}
		pnElement = pnElement->pnNext;
	}

	/* if not found, return NULL */
	return NULL;
}

void node_hash_delete( node_t * pnHash, node_t * pnToDelete )
{
	node_t * pnPrevious = NULL;
	node_t * pnScroll = NULL;
	
	unsigned long nHash = 0;
	unsigned long nBucket = 0;

	assert( pnHash != NULL );
	assert( pnToDelete != NULL );
	assert( pnToDelete->psName != NULL );
	
	if( pnHash == NULL || pnToDelete == NULL || pnToDelete->psName == NULL )
		return;

	if( pnHash->nType != NODE_HASH )
	{
		assert(0);
		return;
	}

	/* decrement the count of hash members */
	assert( pnHash->nHashElements > 0 );
	pnHash->nHashElements--;

	/* hash the name of the node to delete */
	nHash = node_hash( pnToDelete->psName );

	/* get the bucket it's in */
	nBucket = nHash % pnHash->nHashBuckets;

	/* if the node to delete is the first in the bucket */
	if( pnToDelete == pnHash->ppnHashHeads[nBucket] )
	{
		/* cut the head off the list in the bucket*/
		pnHash->ppnHashHeads[nBucket] = node_next( pnToDelete );
		pnToDelete->pnNext = NULL;

		/* if it's a one-membered list */
		if( pnToDelete == pnHash->ppnHashTails[nBucket] )
		{
			/* fix the tail */
			pnHash->ppnHashTails[nBucket] = (node_t*)&( pnHash->ppnHashHeads[nBucket] );
		}
		return;
	}

	for( pnPrevious = pnHash->ppnHashHeads[nBucket], pnScroll = node_next( pnPrevious );
		 pnScroll != NULL; pnPrevious = pnScroll, pnScroll = node_next( pnPrevious ) )
	{
		/* if the current node is the node to delete */
		if( pnToDelete == pnScroll )
		{
			/* cut it out of the list */
			pnPrevious->pnNext = node_next( pnToDelete );
			pnToDelete->pnNext = NULL;

			/* if it was the end of the list */
			if( pnToDelete == pnHash->ppnHashTails[nBucket] )
			{
				/* make the tail pointer point to the new tail */
				pnHash->ppnHashTails[nBucket] = pnPrevious;
			}

			return;
		}
	}

	/* if we got here, then the node was not found in the bucket */
	assert(0);

}

/**************
 Name Functions
 **************/

/* set the name of a node */
void node_set_name (node_t * pn, char * psName)
{
	assert( pn != NULL );
	if( pn == NULL )
		return;

	/* test psName for validity */
	assert( psName != NULL );
	if(psName == NULL)
	{
		errmsg("Internal error: attempt to set a node name to NULL.\n");
		return;
	}

    /* if pn->psName is the same as the passed-in name, we're done */
    if( pn->psName == psName )
    {
        return;
    }

	/* if pn->psName is set, free it */
	if( pn->psName != NULL )
	{
		free(pn->psName);
	}

	/* copy psName onto pn->psName */
	pn->psName = node_safe_copy(psName);
	pn->nHash = node_hash( psName ) & NODE_HASH_MASK;

	return;
}

/* get the name of node */
char * node_get_name(node_t * pn)
{
	assert( pn != NULL );

	if( pn == NULL )
		return NULL;

	return pn->psName;
}

/*****************************
 Dumping and Parsing Functions
 *****************************/

/* dump the node to a file */
void node_dump(node_t * pn, FILE * pfOut, int nOptions)
{
	int i;
	int nRows;
	node_t * pnElt;
    float fTemp;

	char * psEscaped = NULL;

	assert( pn != NULL );
	assert( pfOut != NULL );
	
	if( pn == NULL || pfOut == NULL )
		return;

	/* write node_nSpaces spaces */
	for( i=0; i < node_nSpaces; i++ )
	{
		fprintf( pfOut, " " );
	}

	/* write the name (if any) -- curently booby-trapped if there are ':'s in the name */
	if( pn->psName != NULL)
	{
		psEscaped = node_escape( pn->psName );
		fprintf( pfOut, "%s", psEscaped );
		free( psEscaped );
	}

	/* write ':' */
	fprintf( pfOut, ": " );
	
	switch( pn->nType )
	{
	case NODE_INT:
		fprintf( pfOut, "%d  (0x%08X)\n", pn->nValue, pn->nValue );
		break;

	case NODE_REAL:
        fTemp = (float)pn->dfValue;
		fprintf( pfOut, "%f  (0x%08X)\n", pn->dfValue, *((int*)&fTemp) );
		break;

	case NODE_STRING:
		psEscaped = node_escape( pn->psValue );
		fprintf( pfOut, "\"%s\"\n", psEscaped );
		free( psEscaped );
		break;

	case NODE_DATA:
		/* DATA: write 'DATA ', the data length, and a newline, 
		   then write a hex dump of the data like this:
		   $ 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 ................ */
		fprintf(pfOut, "DATA %d\n", pn->nDataLength );
		
		/* 16 bytes per line */
		for( nRows=0; nRows < ( pn->nDataLength + 15 )/16; nRows++ )
		{
			/* a dollar for every row... */
			fprintf(pfOut, "$" );

			/* output the hex values of the data for this row */
			for( i=0; i < 16; i++)
			{
				/* insert an extra space in the middle for readability */
				if( i == 8 )
				{
					fprintf(pfOut, " ");
				}

				/* while there's data, spit it out */
				if( (nRows*16 + i) < pn->nDataLength )
				{
					fprintf(pfOut," %02x", pn->pbValue[nRows*16 + i] );
				}
				else
				{
					/* we're done with the data, so pad with spaces */
					fprintf(pfOut, "   " );
				}
			}	/* for hex values */

			/* add some spaces between the hex and character info */
			fprintf(pfOut,"   ");

			/* now output the character values for the row */
			for( i=0; i < 16; i++ )
			{
				/* while there's data, spit it out */
				if( (nRows*16 + i) < pn->nDataLength )
				{
					fprintf(pfOut,"%c", printable(pn->pbValue[nRows*16 + i]) );
				}
				else
				{
					/* we're done with the data, so pad with spaces */
					fprintf(pfOut, " " );
				}

			}	/* for character values */
			
			fprintf(pfOut, "\n");

		} /* for nRows */
		break;

	case NODE_LIST:

		fprintf(pfOut, "(\n" );

		/* increase the indentation */
		node_nSpaces += 2;

		/* for each element in the list, call node_dump */
		for(pnElt = node_first(pn); pnElt != NULL; pnElt = node_next(pnElt))
		{
            node_dump( pnElt, pfOut, nOptions );
		}

		/* restore the previous level of indentation */
		node_nSpaces -= 2;

		for(i=0; i < node_nSpaces;i++)
		{
			fprintf(pfOut, " " );
		}

		fprintf(pfOut, ")\n");
		break;

	case NODE_HASH:

		/* HASH: write '{' and newline */
		fprintf(pfOut, "{\n");

		/* increase the indentation */
		node_nSpaces += 2;
		
		/* for each element in the hash, call node_dump */
		if(pn->ppnHashHeads != NULL)
		{
			/* make sure no-one is corrupting our memory */
			assert(pn->nHashBuckets != 0);
			assert(pn->ppnHashTails != NULL);

			/* loop for 1..nHashBuckets */
			for(i=0; i < pn->nHashBuckets; i++)
			{
				for(pnElt = pn->ppnHashHeads[i];pnElt != NULL; pnElt = node_next(pnElt) )
				{
					/* call node_dump on each element of ppnHashHeads */
					node_dump( pnElt, pfOut, nOptions );
				}

			}
		} /* if pn->ppnHashHeads != NULL */

		/* restore the previous level of indentation */
		node_nSpaces -= 2;

		for(i=0; i < node_nSpaces;i++)
		{
			fprintf(pfOut, " " );
		}

		fprintf(pfOut, "}\n");
		break;

	/* everything else */
	default:
		assert(0);	/* tried to dump node of unknown or invalid type */
	}
	
	return;
}

/********************************************************************** 
 * read a node from a file 
 * return values: 
 *
 *  NP_NODE   : succesful read of node element
 *  NP_CPAREN : only thing on line is close paren
 *  NP_CBRACE : only thing on line is close brace
 *  NP_SERROR : syntax error reading line; skip this line
 *  NP_EOF    : end of file
 *
 *	for a return of NP_NODE, ppn is set to the node read; otherwise, it is set
 *	to NULL.
 ***********************************************************************/
int node_parse(FILE *pfIn, node_t ** ppn)
{
	char * psLine = NULL;
	char * psPos = NULL;

	char * psColon = NULL;
	char * psEscapedName = NULL;
	char * psName = NULL;

	node_t * pn = NULL;
	node_t * pnChild = NULL;
	char * psType = NULL;

	char * psTrailingQuote = NULL;

	int nDataLength = 0;
	data_t * pb = NULL;
	int nRows = 0;
	int i = 0;
	char * psData = NULL;
	char * psCursor = NULL;
	int b = 0;

	int nResult = 0;

	char * psUnescaped = NULL;

	/* initialize the returned node */
	*ppn = NULL;

	/* read a line */
READ_LINE:
	psLine = read_line( pfIn );

	/* if it fails */
	if( psLine == NULL )
		return NP_EOF;

	/* skip initial white-space */
	for( psPos = psLine; isspace( *psPos ); psPos++ )
		/*empty*/;

	/* what kind of line is this? */

	/* if it's blank -- read another line */
	if( *psPos == '\0' )
	{
		free( psLine );
		goto READ_LINE;
	}

	/* if it's a close paren */
	if( *psPos == ')' )
	{
		free( psLine );
		return NP_CPAREN;
	}

	/* if it's a close brace */
	if( *psPos == '}' )
	{
		free( psLine );
		return NP_CBRACE;
	}

	/* it's a normal node */

	/* unescape the name */
	psColon = strchr( psPos, ':' );
	if( psColon == NULL )
		return NP_INVALID;

	if( psColon != psPos )
	{
		psEscapedName = (char *)malloc( psColon - psPos + 1);
		if( psEscapedName == NULL )
			exit(1);
		strncpy( psEscapedName, psPos, psColon - psPos );
		psEscapedName[psColon-psPos] = '\0';
		psName = node_unescape( psEscapedName );
		if( psEscapedName != NULL )
			free( psEscapedName );
	}

	/* figure out the node type */
	for( psType = psColon+1; isspace( *psType ); psType++ )
		/* empty */;

	/* allocate a new node */
	pn = node_alloc();
	if( psName != NULL )
	{
		node_set_name( pn, psName );
		free( psName );
	}

	/* switch the node type */
	switch( *psType )
	{
	case '.': case '0': case '1': case '2': case '3': case '4':
	case '-': case '5': case '6': case '7': case '8': case '9':
		if( strchr( psType, '.' ) != NULL )
			node_set( pn, NODE_REAL, atof( psType ) );
		else
			node_set( pn, NODE_INT, atoi( psType ) );
		break;
	case '"':
		/* remove trailing quote */
		psTrailingQuote = strrchr( psType, '"' );
		if( psTrailingQuote == NULL )
		{
			errmsg( "Unterminated string.\n" );
			goto PARSE_ERROR;
		}

		*psTrailingQuote = '\0';

		psUnescaped = node_unescape( psType+1 );
		node_set( pn, NODE_STRING, psUnescaped );
		free( psUnescaped );
		break;
	case 'D':
		nDataLength = atoi( psType + 4 );
		pb = (data_t *)malloc( nDataLength );
		if( pb == NULL )
			exit(1);

		/* loop over the rows of the binary data */
		for( nRows = 0; nRows < (nDataLength+15)/16; nRows++ )
		{
			/* read a new line */
			psData = read_line( pfIn );
			psCursor = psData+1;

			for( i = nRows*16; i < nDataLength && i < (nRows+1)*16; i++ )
			{
				/* skip whitespace */
				psCursor += strspn( psCursor, " " );

				/* convert the hex data to a byte */
				sscanf( psCursor, "%2x", &b );
				psCursor += 2;

				/* set the data */
				pb[i] = (data_t)b;
			}
			free( psData );
		}

		node_set( pn, NODE_DATA, nDataLength, pb );
		free( pb );
		break;
	case '(':
		node_list_init( pn );
		while( (nResult = node_parse( pfIn, &pnChild ) ) == NP_NODE )
		{
			node_list_add( pn, NODE_NODE, pnChild );
			node_free( pnChild );
		}

		if( nResult != NP_CPAREN )
		{
			errmsg( "No close paren for list.\n" );
			goto PARSE_ERROR;
		}

		break;
	case '{':
		node_hash_init( pn, DEFAULT_HASHBUCKETS );
		while( (nResult = node_parse( pfIn, &pnChild ) ) == NP_NODE )
		{
			if( node_get_name( pnChild ) != NULL )
				node_hash_add( pn, node_get_name( pnChild ), NODE_NODE, pnChild );
			else
				errmsg( "Child of hash has no name -- skipping.\n" );

			node_free( pnChild );
		}

		if( nResult != NP_CBRACE )
		{
			errmsg( "No close brace for hash.\n" );
			goto PARSE_ERROR;
		}

		break;
	}

	free( psLine );
	*ppn = pn;
	return NP_NODE;

PARSE_ERROR:
	if( pn != NULL )
		node_free( pn );
	free( psLine );
	
	return NP_SERROR;
}

/*****************
 Utility Functions
 *****************/

/* deep copy a node */
node_t * node_copy(node_t *pnSource)
{
	node_t * pnCopy = NULL;
	node_t * pn = NULL;
	node_t * pnKeys = NULL;

	/* if it's NULL, return NULL */
	if( pnSource == NULL )
	{
		return NULL;
	}

	/* allocate new node */
	if( pnSource->nType == NODE_HASH )
		pnCopy = node_hash_alloc2( pnSource->nHashBuckets );
	else if( pnSource->nType == NODE_LIST )
		pnCopy = node_list_alloc();
	else
		pnCopy = node_alloc();

	/* copy the type */
	pnCopy->nType = pnSource->nType;

	/* copy the name */
	pnCopy->psName = node_safe_copy( pnSource->psName );
	pnCopy->nHash = pnSource->nHash;
	
	/* ignore the next node */
	pnCopy->pnNext = NULL;

	/* copy the data */
	switch( pnCopy->nType )
	{
	case NODE_INT:
		pnCopy->nValue = pnSource->nValue;
		break;

	case NODE_REAL:
		pnCopy->dfValue = pnSource->dfValue;
		break;

	case NODE_STRING:
		pnCopy->psValue = node_safe_copy( pnSource->psValue );
		break;

	case NODE_DATA:

		/* copy the data length */
		pnCopy->nDataLength = pnSource->nDataLength;

		/* malloc and memcpy the data */
		pnCopy->pbValue = (data_t * )malloc( pnCopy->nDataLength );
		
		if( pnCopy->pbValue == NULL )
		{
			errexit( "Out of memory: can't allocate DATA buffer for copy.\n" );
		}

		memcpy( pnCopy->pbValue, pnSource->pbValue, pnCopy->nDataLength );	
		break;

		/* LIST : deep copy the list */
	case NODE_LIST:

		/* deep copy the list */
		for( pn = node_first( pnSource ); pn != NULL; pn = node_next( pn ) )
		{
			node_list_add( pnCopy, NODE_NODE, pn );
		}
		break;

		/* HASH: deep copy the hash */
	case NODE_HASH:

		/* get the keys */
		pnKeys = node_hash_keys( pnSource );

		/* copy the elements */
		for( pn = node_first( pnKeys ); pn != NULL; pn = node_next( pn ) )
		{
			char * psKey = node_get_string( pn );
			node_hash_add( pnCopy, psKey, NODE_NODE, node_hash_get( pnSource, psKey ) );
		}
		node_free( pnKeys );
	
		break;

	default:
		errmsg( "Attempted to copy illegal node type (value %d).\n", pnSource->nType );
	}

	/* return the copied node */		
	return pnCopy;

}

/* safely copy a string */
static char * node_safe_copy(char * ps)
{
	char * psCopy;

	/* you shouldn't be sending null pointers to node_safe_copy */
	if( ps == NULL ) 
	{
		return NULL;
	}

	/* allocate memory */
	psCopy = (char *)malloc( strlen(ps) + 1);
	if(psCopy == NULL)
	{
		errexit("Out of memory: can't allocate storage for node_safe_copy.\n");
	}

	/* copy the string */
	strcpy( psCopy, ps );

	return psCopy;

}

/* takes a byte and returns '.' if unprintable; else returns the byte */
char printable(int c)
{
	assert(c >= 0);
	assert(c <= 255);

	if(isprint(c))
	{
		return (char)c;
	}
	else
	{
		return '.';
	}
}

/********************************
 Private Function Implementations
 ********************************/

/* set the node to a value using variable arguments. nType determines how 
arguments are processed.*/
static void node_set_valist(node_t *pn, int nType, va_list valist)
{
	char * psValue = NULL;
	data_t * pbValue = NULL;

	assert( pn != NULL );

	if( pn == NULL )
		return;
	
	switch (nType)
	{
	case NODE_INT:
		/* clean up */
		node_cleanup( pn );

		pn->nValue = va_arg(valist, int);
		break;

	case NODE_REAL:
		/* clean up */
		node_cleanup( pn );

		pn->dfValue = va_arg(valist, double);
		break;

	case NODE_STRING:
		psValue = va_arg(valist, char *);
		if(psValue == NULL)
		{
			errmsg("Attempted to set node value to null string.\n");
			return;
		}

        /* if setting to same as current, done */
        if( psValue == pn->psValue )
        {
            return;
        }

		/* clean up */
		node_cleanup( pn );

		/* node_safe_copy psValue */
		pn->psValue = node_safe_copy( psValue );

		break;

	case NODE_DATA:
		/* store the length */
		pn->nDataLength = va_arg( valist, int );

		assert( pn->nDataLength > 0 );
		if( (size_t)pn->nDataLength > LOTS_OF_MEMORY ) 
		{
			wrnmsg("Suspicious amount of memory (%d) allocated for data element\n",
					pn->nDataLength );
		}

		pbValue = va_arg( valist, data_t * );

		if( pbValue == NULL )
		{
			assert(0);
			errmsg( "Attempted to set data with NULL pointer.\n");
			return;
		}

        /* if setting to same as current, return */
        if( pbValue == pn->pbValue )
        {
            return;
        }

		/* clean up */
		node_cleanup( pn );

		/* allocate the buffer */
		pn->pbValue = (data_t *)malloc( pn->nDataLength );
		if( pn->pbValue == NULL ) 
		{
			errexit( "Out of memory: can't allocate data buffer in node_set.\n" );
		}

		/* copy the data into the buffer */
		memcpy( pn->pbValue, pbValue, pn->nDataLength );
		break;

	default:
	/* otherwise */
		assert(0);
		errmsg( "Tried to set node with invalid type (%d).\n", nType );
	}

	/* copy the type */
	pn->nType = nType;
	return;

}	

/* hash a string */
static unsigned long node_hash( char * psKey)
{
	return mv_hash( psKey );
}

int node_is_valid( node_t* pn )
{
	assert( pn != NULL );

	if( pn == NULL )
		return 0;

     if( pn->nType == NODE_UNKNOWN )
     {
          return 0;
     }

     /* else, it's valid */
     return 1;
}

static void node_cleanup( node_t * pn )
{
	int i = 0;

	assert( pn != NULL );
	if( pn == NULL )
		return;

	/* on node_cleanup, make sure all members are freed and zeroed */
	switch( pn->nType )
	{
	case NODE_STRING:
	case NODE_INT:
	case NODE_REAL:
		/* because of implicit conversion, must clear all scalar values together */
		if( pn->psValue != NULL )
			free( pn->psValue );
		pn->psValue = NULL;
		pn->nValue = 0;
		pn->dfValue = 0.0;
		break;
	case NODE_DATA:
		if( pn->pbValue != NULL )
			free( pn->pbValue );
		pn->pbValue = NULL;
		pn->nDataLength = 0;
		break;
	case NODE_LIST:
		node_free( pn->pnListHead );
		pn->pnListHead = NULL;
		pn->pnListTail = NULL;
		break;
	case NODE_HASH:
		for( i = 0; i < pn->nHashBuckets; i ++ )
		{
			node_free( pn->ppnHashHeads[i] );
		}
		free( pn->ppnHashHeads );
		free( pn->ppnHashTails );

		pn->ppnHashHeads = NULL;
		pn->ppnHashTails = NULL;

		pn->nHashBuckets = 0;
		pn->nHashElements = 0;
		break;
	}
}

/* node_hash_keys 
 * This function returns a newly-allocated list node containing 
 * a list of the key strings for hash pnHash. 
 */
node_t * node_hash_keys( node_t * pnHash ) 
{
	node_t * pnList = NULL;
	node_t * pn = NULL;

	int i = 0;

	assert( pnHash != NULL );
	if( pnHash == NULL )
		return NULL;

	/* allocate a list node */
	pnList = node_list_alloc();

	/* if pnHash is not a hash node */
	assert( pnHash->nType == NODE_HASH );
	if( pnHash->nType != NODE_HASH )
	{
		/* return the list */
		return pnList;
	}

	/* for each hash bucket */
	for( i = 0; i < pnHash->nHashBuckets; i++ )
	{
		/* for each node in the bucket */
		for( pn = pnHash->ppnHashHeads[i]; pn != NULL; pn = node_next( pn ) )
		{
			/* add the node's name to the list */
			node_list_add( pnList, NODE_STRING, node_get_name( pn ) );
		}
	}

	/* return the list */
	return pnList;
}

static char * read_line( FILE * pfIn )
{
	int nBufferSize = 40;
	char * psBuffer = NULL;
	int nBytesRead = 0;
	int nChar = 0;

	/* check for end-of-file */
	nChar = fgetc( pfIn );
	if( feof( pfIn ) )
		return NULL;
	else
		ungetc( nChar, pfIn );

	/* allocate initial buffer */
	psBuffer = (char *)malloc( nBufferSize );
	if( psBuffer == NULL )
		exit(1);

	for(;;)
	{
		/* read a character */
		nChar = fgetc( pfIn );

		/* if the character was EOF */
		if( nChar == EOF )
			break;

		/* increment the read count */
		nBytesRead++;

		/* if we're past the length of the buffer */
		if( nBytesRead >= nBufferSize )
		{
			char * psNewBuffer = NULL;

			/* increase the buffer size */
			nBufferSize *= 2;

			/* reallocate the buffer */
			psNewBuffer = (char *)malloc( nBufferSize );
			if( psNewBuffer == NULL )
				exit(1);

			/* copy the old buffer onto the new one */
			memcpy( psNewBuffer, psBuffer, (nBufferSize/2) );

			free( psBuffer );
			psBuffer = psNewBuffer;
		}

		/* store the character in the buffer */
		psBuffer[ nBytesRead-1 ] = (char)nChar;

		/* if the character was a newline */
		if( nChar == '\n' )
			break;
	}

	/* terminate the buffer */
	psBuffer[ nBytesRead ] = '\0';

	/* return the read string */
	return psBuffer;
}

static char * node_escape( char * psUnescaped )
{
	char * psEscaped = NULL;
	char * psE = NULL;
	char * psU = NULL;

	if( psUnescaped == NULL )
		return NULL;

	psEscaped = (char *)malloc( 3*strlen(psUnescaped) + 1 );
	if( psEscaped == NULL )
		exit(1);

	psE = psEscaped;
	psU = psUnescaped;
	while( *psU != '\0' )
	{
		switch( *psU )
		{
		case '%':
		case ':':
		case 0x0D:		/* <CR> */
		case 0x0A:		/* <LF> */
			/* write out a percent */
			*psE++ = '%';

			/* write out the hex code */
			sprintf( psE, "%02X", *psU );
			psU++;
			psE += 2;

			break;
		default:
			*psE++ = *psU++;
		}
	}
	*psE = '\0';

	return psEscaped;
}

static char * node_unescape( char * psEscaped )
{
	char * psUnescaped = node_safe_copy( psEscaped );
	char * psE = psEscaped;
	char * psU = psUnescaped;

	int n = 0;

	while( *psE != '\0' )
	{
		switch( *psE )
		{
		case '%':
			/* step over the percent */
			psE++;

			/* convert the hex code */
			sscanf( psE, "%2X", &n );
			*psU++ = (char)n;
			psE += 2;

			break;
		default:
			*psU++ = *psE++;
		}
	}
	*psU = '\0';

	return psUnescaped;
}


/*
   Local variables:
   tab-width: 4
   eval: (c-set-style "msvc")
   end:
 */
