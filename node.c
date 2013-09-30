/* node.c -- implementation of node module
 * Initial version written 2000-08-07 Cubane Software
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <crtdbg.h>

#include "node.h"
#include "message.h"
#include "regex.h"
#include "mv_hash.h"

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC 1
#endif

/****************
 Module Variables
 ****************/

static int node_nSpaces = 0;		/* the current level of indentation */
static int node_nParseLine = 0;		/* the current line of the file we are parsing */
static int node_fRegExCompiled = 0;	/* flag is TRUE if regexp's have been compiled */
static regex_t node_reLine;			/* regular expression for the current line */

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

	pnNew = malloc( sizeof(node_t) );
	if( pnNew == NULL ) 
	{
		errexit("Out of memory: can't allocate new node\n");
	}

	assert( pnNew != NULL );

	pnNew->pnNext = NULL;

	pnNew->nType = NODE_UNKNOWN;
	pnNew->nFlags = 0;

	pnNew->psName = NULL;

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
	int i;
	node_t * pnSaved = NULL;

	while( pn != NULL ) 
	{

		pnSaved = pn->pnNext;

		if( pn->psName != NULL ) 
		{
			free( pn->psName );
			pn->psName = NULL;
		}

		if( pn->psValue != NULL ) 
		{
			free( pn->psValue );
			pn->psValue = NULL;
		}

		if( pn->pbValue != NULL ) 
		{
			free( pn->pbValue );
			pn->pbValue = NULL;
		}

		node_free( pn->pnListHead );
		pn->pnListHead = NULL;

		/* no need to node_free pnListTail, because the recursive call to node_free on
		 * pnListHead will free the whole list in the while loop
		 */

		for( i = 0; i < pn->nHashBuckets; i++ ) 
		{
			node_free( pn->ppnHashHeads[i] );
			pn->ppnHashHeads[i] = NULL;
		}

		if( pn->ppnHashHeads != NULL ) 
		{
			free( pn->ppnHashHeads );
			pn->ppnHashHeads = NULL;
		}

		if( pn->ppnHashTails != NULL ) 
		{
			free( pn->ppnHashTails );
			pn->ppnHashTails = NULL;
		}

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

	/* if it's a list already */
	if( pn->nType == NODE_LIST ) 
	{

		/* do nothing */
		return;

	} 
	else 
	{
	/* else: it's not a list */

		/* set the type */
		pn->nType = NODE_LIST;

		/* free the old list head (if any) */
		node_free( pn->pnListHead );

		/* null the list head */
		pn->pnListHead = NULL;

		/* do the tail magic */
		pn->pnListTail = (node_t*)&(pn->pnListHead);

	}

	return;
}

/* initialize a hash node */
static void node_hash_init( node_t * pn, int nHashBuckets )
{
	int i;

	assert( pn != NULL );
	
	/* if it's a hash, do nothing */
	if(pn->nType == NODE_HASH)
	{
		return;
	}

	/* otherwise, */
	
	/* if ppnHashHeads is set, clean it up: */
	if(pn->ppnHashHeads != NULL)
	{
		/* make sure no-one is corrupting our memory */
		assert(pn->nHashBuckets != 0);
		assert(pn->ppnHashTails != NULL);

		/* loop for 1..nHashBuckets */
		for(i=0; i < pn->nHashBuckets; i++)
		{
			/* call node_free on each element of ppnHashHeads */
			node_free(pn->ppnHashHeads[i]);

		}
		
		/* if we're resizing this hash, we must free the has head and tail and reallocate storage */
		if( pn->nHashBuckets != nHashBuckets )
		{
			free( pn->ppnHashHeads );
			free( pn->ppnHashTails );

			/* set the number of buckets to the requested number */
			pn->nHashBuckets = nHashBuckets;

			/* allocate ppnHashHeads */
			pn->ppnHashHeads = malloc( sizeof(node_t*)*pn->nHashBuckets );
			
			if( pn->ppnHashHeads == NULL )
			{
				errexit("Out of memory: can't allocate ppnHashHeads.\n");
			}

			/* allocate ppnHashTails */
			pn->ppnHashTails = malloc( sizeof(node_t*)*pn->nHashBuckets );

			if( pn->ppnHashTails == NULL )
			{
				errexit("Out of memory: can't allocate ppnHashTails.\n");
			}

		}
	}
	else	/* pn->ppnHashHeads == NULL */
	{
		/* again, make sure no-one is corrupting our information */
		assert(pn->nHashBuckets == 0);
		assert(pn->ppnHashTails == NULL);
		
		/* set nHashBuckets */
		pn->nHashBuckets = nHashBuckets;

		/* allocate ppnHashHeads */
		pn->ppnHashHeads = malloc( sizeof(node_t*)*pn->nHashBuckets );
		
		if( pn->ppnHashHeads == NULL )
		{
			errexit("Out of memory: can't allocate ppnHashHeads.\n");
		}

		/* allocate ppnHashTails */
		pn->ppnHashTails = malloc( sizeof(node_t*)*pn->nHashBuckets );

		if( pn->ppnHashTails == NULL )
		{
			errexit("Out of memory: can't allocate ppnHashTails.\n");
		}

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
		pn->psValue = malloc( strlen("-2147483648") + 1 );
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
		pn->psValue = malloc( strlen("-1000000000.00000") + 1 );
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
		pn->psValue = safe_copy("");

	}

	return pn->psValue;
}

/* returns the data value from the node */
data_t * node_get_data(node_t *pn, int * pnLength)
{
	assert( pn != NULL );

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

	return;

}

/* deletes a node from a list */
void node_list_delete( node_t * pnList, node_t * pnToDelete )
{
	node_t * pnPrevious = NULL;
	node_t * pnScroll = NULL;

	assert( pnList != NULL );
	assert( pnToDelete != NULL );
	
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

/**************
 Hash Functions
 **************/

/* add a node to a hash{} similar variable arguments to node_set */
void node_hash_add(node_t * pnHash, char * psKey, int nType, ...)
{
	node_t * pnElement = NULL;
	node_t * pnNew = NULL;
	node_t * pnSaveNext = NULL;
	va_list valist;
	int nBucket;
	unsigned long nHash;

	assert( pnHash != NULL );
	assert( psKey != NULL );

	/* make sure hash is initialized */
	node_hash_init( pnHash, DEFAULT_HASHBUCKETS );

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
	node_t * pnElement = NULL;

	assert( pnHash != NULL );
	assert( psKey != NULL );

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

	while( pnElement != NULL)
	{
		if( strcmp( pnElement->psName, psKey ) == 0 )
		{
			/* if found, return a pointer to the found element */
			return pnElement;
		}
		pnElement = node_next( pnElement );
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
	
	if( pnHash->nType != NODE_HASH )
	{
		assert(0);
		return;
	}

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
	char * psTemp;

	assert( pn != NULL );

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
	pn->psName = safe_copy(psName);

	/* if there is a ':' in the name, assert and kill it */
	psTemp = strchr( pn->psName, ':' );
	
	if( psTemp != NULL )	/* there is a colon in the name */
	{
		assert(0);	/* the character ':' is not permitted in a node name */
		
		/* terminate the name at the colon */
		*psTemp = '\0';

	}

	return;
}

/* get the name of node */
char * node_get_name(node_t * pn)
{
	assert( pn != NULL );
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

	assert( pn != NULL );
	assert( pfOut != NULL );

	/* if the Debug flag is set in the node, don't write out the 
		node unless nOptions includes the DEBUG option */
	if( (pn->nFlags & DEBUG_FLAG) && !(nOptions & DO_DUMP) )
	{
		return;
	}
	
	/* write node_nSpaces spaces */
	for( i=0; i < node_nSpaces; i++ )
	{
		fprintf( pfOut, " " );
	}

	/* write the name (if any) */
	if( pn->psName != NULL)
	{
		fprintf( pfOut, "%s", pn->psName );
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
		fprintf( pfOut, "\"%s\"\n", pn->psValue );
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
 *  NP_DUMP   : only thing on line is hex dump
 *  NP_SERROR : syntax error reading line; skip this line
 *  NP_EOF    : end of file
 *
 *	for a return of NP_NODE, ppn is set to the node read; otherwise, it is set
 *	to NULL.
 ***********************************************************************/
int node_parse(FILE *pfIn, node_t ** ppn)
{
	char psLineBuffer[1024];
	regmatch_t rmMatches[5];
	int nResult;
	unsigned int nNotBlank;
	int nRows;
	int i;
	node_t * pnNew = NULL;
	node_t * pnChild = NULL;
	char * psQuote = NULL;
	
	assert(pfIn != NULL);
	assert(ppn != NULL);

	if(!node_fRegExCompiled )
	{
		/* compile all regular expressions and set the compiled flag */

		/* a line is spaces, an optional name, a colon, spaces, a type character, 
		   and the rest */

		nResult = regcomp( &node_reLine, "^([0-9]+#)?[[:blank:]]*([^:]*):[[:blank:]]*(.)(.*)$", 
							REG_EXTENDED );
#define MATCH_WHOLE     0
#define MATCH_SEQNO     1
#define MATCH_NAME      2
#define MATCH_TYPECHAR  3
#define MATCH_REST      4    

		if(nResult != 0)
		{
			errexit("Error compiling regular expression reLine: %s.\n", regerror(nResult) );
		}

		/* set the compiled flag */
		node_fRegExCompiled = 1;
	}

	/* while not EOF, read lines from the file*/
read_line:
	if( fgets(psLineBuffer, sizeof(psLineBuffer), pfIn) == NULL )
	{
		/* re-initialize line counter */
		node_nParseLine = 0;

		*ppn = NULL;
		return NP_EOF;
	}

	/* increment the line counter */
	node_nParseLine++;
	
	/* if the line is completely blank, read another */
	if( ( nNotBlank = strspn(psLineBuffer, " \t") ) == strlen(psLineBuffer) )
	{
		goto read_line;
	}

	nResult = regexec( &node_reLine, psLineBuffer, sizeof(rmMatches)/sizeof(regmatch_t), rmMatches, 0 );

	/* if no match: */
	if( nResult == REG_NOMATCH )
	{
		/* set the returned node to null because we are not a node */
		*ppn = NULL;

		/* determine what type of line it is and return it */
		switch( psLineBuffer[nNotBlank] )
		{
		case ')':
			return NP_CPAREN;
		case '}':
			return NP_CBRACE;
		case '$':
			return NP_DUMP;
		default:
			errmsg( "Illegal first character ('%c') on non-node line %d.\n", 
				psLineBuffer[nNotBlank], node_nParseLine );					
			goto parse_err;

		} /* switch */
	} 
	else if( nResult != 0 ) 
	{
		/* regular expression error */
		errmsg("Regular expression error trying to match line %d: %s\n",
				node_nParseLine, regerror( nResult ) );
		goto parse_err;
	}

	/* we matched the line regular expression */

	/* allocate a new node */
	pnNew = node_alloc();

	/* \1 is the name, if any */
	if( rmMatches[MATCH_NAME].rm_so != rmMatches[MATCH_NAME].rm_eo )
	{
		/* we found a name of more than 0 length */

		/* terminate the name at the colon */
		psLineBuffer[ rmMatches[MATCH_NAME].rm_eo ] = '\0';

		/* set the name */
		node_set_name( pnNew, psLineBuffer + rmMatches[MATCH_NAME].rm_so ) ;

	}

	/* \2 tells us the node type */
	switch( psLineBuffer[ rmMatches[MATCH_TYPECHAR].rm_so ] )
	{
		/* if it's digit -> numeric */
	case '0': case '1': case '2': case '3': case '4': case '.':
	case '5': case '6': case '7': case '8': case '9': case '-':
		pnNew->nType = NODE_NUMERIC;
		break;

		/* if it's a 'D' => data */
	case 'D':
		pnNew->nType = NODE_DATA;
		break;

		/* if it's a '"' => string */
	case '"':
		pnNew->nType = NODE_STRING;
		break;

		/* if it's a '(' => list */
	case '(':
		node_list_init( pnNew );
		break;

		/* if it's a '{' => hash */
	case '{':
		node_hash_init( pnNew, DEFAULT_HASHBUCKETS );
		break;

		/* this is an unknown kind of node */
	default:
		errmsg( "Unknown node type: '%c' on line %d.\n",  psLineBuffer[ rmMatches[MATCH_TYPECHAR].rm_so ], 
				node_nParseLine );
		goto parse_err;
	}

	/* \3 may provide additional data */
	switch( pnNew->nType )
	{
		/* if type is numeric: */
	case NODE_NUMERIC:

		/* a '.' in \2 tells us it's real */
		if( strchr( psLineBuffer + rmMatches[MATCH_TYPECHAR].rm_so, '.' ))
		{
			/* if it's real: dfValue = atof( \3 ) */
			pnNew->nType = NODE_REAL;
			pnNew->dfValue = atof( psLineBuffer + rmMatches[MATCH_TYPECHAR].rm_so );
		}
		else
		{
			/* if it's int : nValue = atoi( \3 ) */
			pnNew->nType = NODE_INT;
			pnNew->nValue = atoi( psLineBuffer + rmMatches[MATCH_TYPECHAR].rm_so  );
		}
		break;

		/* if it's data: */
	case NODE_DATA:

		/* \3 is 'ATA\s*(\d*)' where the digits tell how many bytes of data follow */
		pnNew->nDataLength = atoi(psLineBuffer + rmMatches[MATCH_REST].rm_so + 3 );
		break;

		/* if it's a string: */
	case NODE_STRING:

		/* \3 is '(.*)"' where the string data is everything up to the quote */
		psQuote = strrchr(psLineBuffer + rmMatches[MATCH_REST].rm_so, '"' );

		if(psQuote == NULL)
		{
			errmsg( "Unterminated string on line %d.\n", node_nParseLine );
			goto parse_err;
		}
		
		/* Replace the end quote with a '\0' to terminate the string  */
		*psQuote = '\0';

		/* Copy the string value into pnNew */
		pnNew->psValue = safe_copy( psLineBuffer + rmMatches[MATCH_REST].rm_so );

		break;
	
	}

	/* More parsing for lists, hashes and data elements: */
	switch(pnNew->nType)
	{
		/* if it's a list: */
	case NODE_LIST:

		/* loop: call node_parse() until it returns the correct closing element */
		while( ( nResult = node_parse(pfIn, &pnChild ) ) == NP_NODE )
		{
			/* put the newly-read element into the list */
			node_list_add( pnNew, NODE_NODE, pnChild );
			node_free( pnChild );
		}

		/* if the buffer contains the required closing element, good */
		/* if not, node_free() the node and report a syntax error upwards */
		if( nResult != NP_CPAREN )
		{
			errmsg( "No close paren for list on line %d.\n", node_nParseLine );
			goto parse_err;
		}

		break;

		/* if it's a hash: */
	case NODE_HASH:

		/* loop: call node_parse() until it returns the correct closing element */
		while( ( nResult = node_parse(pfIn, &pnChild ) ) == NP_NODE )
		{
			/* put the newly-read element into the hash */
			if( pnChild->psName == NULL )
			{
				errmsg( "Child of hash on line %d has no name -- skipping.\n", 
						node_nParseLine );
			}
			else
			{
				node_hash_add( pnNew, pnChild->psName, NODE_NODE, pnChild );
			}

			node_free( pnChild );
		}

		/* if the buffer contains the required closing element, good */
		/* if not, node_free() the node and report a syntax error upwards */
		if( nResult != NP_CBRACE )
		{
			errmsg( "No close brace for hash on line %d.\n", node_nParseLine );
			goto parse_err;
		}
		break;

		/* if it's a data element: */
	case NODE_DATA:

		/* allocate nDataLength storage in pbData */
		assert( pnNew->nDataLength > 0 );
		if( (size_t)pnNew->nDataLength > LOTS_OF_MEMORY )
		{
			wrnmsg("Suspicious amount of memory (%d) allocated for data element on line %d\n",
					pnNew->nDataLength, node_nParseLine );
		}

		pnNew->pbValue = malloc( pnNew->nDataLength );

		if(pnNew->pbValue == NULL )
		{
			errexit("Out of memory to allocate %d bytes for data element on line %d.\n", 
				pnNew->nDataLength, node_nParseLine );
		}

		/* loop over the rows of the binary data in pbValue */
		for( nRows=0; nRows < ( pnNew->nDataLength + 15 )/16; nRows ++ )
		{
			int nNextChar;

			/* peek at the first character on the next line */
			if( (nNextChar = fgetc(pfIn)) == '$')
			{
			/* if it's a $, we're in a hex dump */

				char* psCursor;
				int b;

				/* read a line of hex dump */
				fgets( psLineBuffer, sizeof(psLineBuffer), pfIn );
				node_nParseLine++;

				psCursor = psLineBuffer;

				/* loop over the hex representations of the bytes */
				for( i = nRows*16; i < pnNew->nDataLength && i < (nRows+1)*16; i++)
				{
					/* skip whitespace */
					psCursor += strspn(psCursor, " ");

					/* convert the hex data into a byte */
					nResult = sscanf(psCursor,"%2x",&b);
					if( nResult != 1 ) 
					{
						errmsg("Unable to read hex data: stopped at %s on line %d.\n",
								psCursor, node_nParseLine );
						goto parse_err;
					}

					/* advance cursor */
					psCursor += 2;

					/* 2 nybbles of hex data should never be more than 255
					 * and *never* be negative */
					assert( b >= 0 );
					assert( b <= 255 );

					/* store the byte in pbValue */
					pnNew->pbValue[ i ] = (data_t)b;
				}
			}
			else /* first character is not a '$' */
			{
			/* otherwise, it's corrupt, since we haven't reached the last row */

				/* put the character back */
				ungetc( nNextChar, pfIn );

				errmsg( "Expected more lines of hex data at line %d.\n", node_nParseLine );

				goto parse_err;
			}

		}
		break;
	}

	/* Return NP_NODE and *ppn = pn, the node we read. */
	*ppn = pnNew;
	return NP_NODE;

parse_err:
	if( pnNew != NULL ) 
	{
		node_free( pnNew );
	}

	*ppn = NULL;
	node_nParseLine = 0;

	return NP_SERROR;

}

/*****************
 Utility Functions
 *****************/

/* deep copy a node */
node_t * node_copy(node_t *pn)
{
	int i;
	node_t * pnCopy = NULL;

	/* if it's NULL, return NULL */
	if( pn == NULL )
	{
		return NULL;
	}

	/* allocate new node */
	pnCopy = node_alloc();

	/* copy the type */
	pnCopy->nType = pn->nType;

	/* copy the Flags */
	pnCopy->nFlags = pn->nFlags;

	/* copy the name */
	pnCopy->psName = safe_copy( pn->psName );
	
	/* if there's a pnNext, deep copy it */
	pnCopy->pnNext = node_copy( pn->pnNext );

	/* copy the data */
	switch( pnCopy->nType )
	{
	case NODE_INT:
		pnCopy->nValue = pn->nValue;
		break;

	case NODE_REAL:
		pnCopy->dfValue = pn->dfValue;
		break;

	case NODE_STRING:
		pnCopy->psValue = safe_copy( pn->psValue );
		break;

	case NODE_DATA:

		/* copy the data length */
		pnCopy->nDataLength = pn->nDataLength;

		/* malloc and memcpy the data */
		pnCopy->pbValue = malloc( pnCopy->nDataLength );
		
		if( pnCopy->pbValue == NULL )
		{
			errexit( "Out of memory: can't allocate DATA buffer for copy.\n" );
		}

		memcpy( pnCopy->pbValue, pn->pbValue, pnCopy->nDataLength );	
		break;

		/* LIST : deep copy the list */
	case NODE_LIST:

		/* copy the list */
		pnCopy->pnListHead = node_copy( pn->pnListHead );

		/* start the tail (use magic) */
		pnCopy->pnListTail= (node_t *)&(pnCopy->pnListHead);

		/* scroll the tail to the end */
		while( pnCopy->pnListTail->pnNext != NULL )
		{
			pnCopy->pnListTail = pnCopy->pnListTail->pnNext;
		}
		break;

		/* HASH: deep copy the hash */
	case NODE_HASH:

		/* copy the number of buckets */
		pnCopy->nHashBuckets = pn->nHashBuckets;

		/* copy the number of elements */
		pnCopy->nHashElements = pn->nHashElements;

		/* allocate buckets: heads */
		pnCopy->ppnHashHeads = malloc( sizeof(node_t *)*pnCopy->nHashBuckets );

		if( pnCopy->ppnHashHeads == NULL)
		{
			errexit( "Out of memory: can't allocate storage for a hash copy.\n" );
		}

		/* ... tails */
		pnCopy->ppnHashTails= malloc( sizeof(node_t *)*pnCopy->nHashBuckets );

		if( pnCopy->ppnHashTails == NULL)
		{
			errexit( "Out of memory: can't allocate storage for a hash copy.\n" );
		}

		/* copy the elements */
		for(i=0; i < pnCopy->nHashBuckets; i++)
		{
			/* copy the bucket */
			pnCopy->ppnHashHeads[i] = node_copy( pn->ppnHashHeads[i] );

			/* start the tail */
			pnCopy->ppnHashTails[i] = (node_t *)&(pnCopy->ppnHashHeads[i]);

			/* scroll the tail */
			while( pnCopy->ppnHashTails[i]->pnNext != NULL )
			{
				pnCopy->ppnHashTails[i] = pnCopy->ppnHashTails[i]->pnNext;
			}

		}
	
		break;

	default:
		errmsg( "Attempted to copy illegal node type (value %d).\n", pn->nType );
	}

	/* return the copied node */		
	return pnCopy;

}

/* safely copy a string */
char * safe_copy(char * ps)
{
	char * psCopy;

	/* you shouldn't be sending null pointers to safe_copy */
	if( ps == NULL ) 
	{
		return NULL;
	}

	/* allocate memory */
	psCopy = malloc( strlen(ps) + 1);
	if(psCopy == NULL)
	{
		errexit("Out of memory: can't allocate storage for safe_copy.\n");
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
	
	switch (nType)
	{
	case NODE_INT:
		pn->nValue = va_arg(valist, int);
		break;

	case NODE_REAL:
		pn->dfValue = va_arg(valist, double);
		break;

	case NODE_STRING:
		psValue = va_arg(valist, char *);
		if(psValue == NULL)
		{
			assert(0);
			errmsg("Attempted to set node value to null string.\n");
			return;
		}

        /* if setting to same as current, done */
        if( psValue == pn->psValue )
        {
            return;
        }

		/* if set, free */
		if( pn->psValue != NULL )
		{
			free( pn->psValue );
		}

		/* safe_copy psValue */
		pn->psValue = safe_copy( psValue );

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

		if( pn->pbValue != NULL )
		{
			free( pn->pbValue );
		}

		/* allocate the buffer */
		pn->pbValue = malloc( pn->nDataLength );
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
     if( pn->nType == NODE_UNKNOWN )
     {
          return 0;
     }

     /* else, it's valid */
     return 1;
}


/*
   Local variables:
   tab-width: 4
   eval: (c-set-style "msvc")
   end:
 */
