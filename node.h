/*	This file is node.h
	Initial version written 08-07-2000 by Cubane Software
	Code is copyright 2000-2003 G. Michaels Consulting Ltd.
*/

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef _NODE_H
#define _NODE_H

#ifdef _MSC_VER
#pragma warning( disable : 4201 )			/* suppresses warning about nameless struct/union */
#endif

#define NODE_UNKNOWN 0
#define NODE_STRING  1
#define NODE_INT     2
#define NODE_REAL    3
#define NODE_LIST    4
#define NODE_HASH    5
#define NODE_NUMERIC 6 /* INT or REAL */
#define NODE_NODE	 7
#define NODE_DATA    8

#define NODE_TYPE_BITS 5
#define NODE_HASH_BITS 27

#define NODE_HASH_MASK 0x03FFFFFF

#define DO_DUMP      1 /* node debug options */

#define DEBUG_FLAG	 1	/* if this flag is set on a node, initiate various debug behaviours */

/* return values from node_parse */

#define NP_NODE		0	/* succesful read of node element */
#define NP_CPAREN	1	/* only thing on line is close paren */
#define NP_CBRACE	2	/* only thing on line is close brace */
#define NP_DUMP		3	/* only thing on line is hex dump */
#define NP_SERROR	4	/* syntax error reading line; skip this line */
#define NP_EOF		5	/* end of file */
#define NP_INVALID	6	/* invalid file stream or node pointer */


/********
 TYPEDEFS
 ********/

typedef unsigned char data_t;	
typedef struct node node_t;

#pragma pack(push)
#pragma pack(1)
struct node
{
    /* overhead */
    struct node* pnNext;		/* pointer to next node -- only used in lists/hashes */

    /* name */
    char* psName;               /* name of this node (may be NULL) */

    /* data */
    int nType:NODE_TYPE_BITS;	/* type of this node: NODE_xxx */
	int nHash:NODE_HASH_BITS;	/* partial hash value */

	union
	{
		struct
		{
			/* string data */
			char* psValue;              /* string value if string type or coerced to string type */

			/* real data */
			double dfValue;             /* double value if real type or coerced */

			/* int data */
			int nValue;                 /* int value if int type or coerced */
		};
		struct
		{

			/* binary data */
			data_t* pbValue;            /* binary data */
			int nDataLength;            /* length of data */
		};

		struct
		{
			/* list data */
			node_t* pnListHead;         /* head of list if list type */
			node_t* pnListTail;         /* tail of list if list type */
			int nListElements;			/* number of elements in list */
		};
	
		struct
		{
			/* hash data */
			node_t** ppnHashHeads;       /* array of buckets if hash type */
			node_t** ppnHashTails;       /* array of bucket tails if hash type */
			int nHashBuckets;            /* number of buckets if hash type */
			int nHashElements;          /* number of elements in hash */
		};
	};
};
#pragma pack(pop)

/****************
 Memory Functions 
 ****************/

/* allocate memory for a new node and return a pointer */
node_t * node_alloc();

/* free memory associated with a node, including children and neighbours*/
void node_free(node_t *pn);

/* allocate an empty list node */
node_t * node_list_alloc();

/* allocate an empty hash node */
node_t * node_hash_alloc();

/* allocate an empty hash node with a user-specified number of buckets*/
node_t * node_hash_alloc2( int nHashBuckets );

/*****************
 Setting Functions
 *****************/

/* node_set is a wrapper for the private function node_set_valist */ 
void node_set(node_t * pn, int nType, ...);	

/*****************
 Reading Functions
 *****************/

/* returns the int value from the node */
int node_get_int(node_t *pn);

/* returns the real value from the node */
double node_get_real(node_t *pn);

/* returns the string value from the node */
char * node_get_string(node_t *pn);

/* returns the data value from the node */
data_t * node_get_data(node_t *pn, int * pnLength);

/**************
 List Functions
 **************/

/* add a new node to the end of a list; similar variable arguments to node_set */
void node_list_add(node_t * pnList, int nType, ...);

/* delete a node from within a list */
void node_list_delete( node_t * pnList, node_t * pnToDelete );

/* returns the first node of a list */
node_t * node_first(node_t * pnList);

/* returns the next node in a list or hash bucket chain */
node_t * node_next(node_t * pn);

/* Stack functions: treating the list as a stack */
/* pushes a node onto the front of a list */
void node_push( node_t * pnList, int nType, ... );

/* pops a node off the front of a list */
node_t * node_pop( node_t * pnList );

/**************
 Hash Functions
 **************/

/* add a node to a hash; similar variable arguments to node_set */
void node_hash_add(node_t * pnHash, char * psKey, int nType, ...);

/* delete a node from within a hash */
void node_hash_delete( node_t * pnHash, node_t * pnToDelete );

/* get a node (by name) from a hash */
node_t * node_hash_get(node_t * pnHash, char * psKey);

/**************
 Name Functions
 **************/

/* set the name of a node */
void node_set_name(node_t * pn, char * psName);

/* get the name of node */
char * node_get_name(node_t * pn);

/*****************************
 Dumping and Parsing Functions
 *****************************/

/* dump the node to a file */
void node_dump(node_t * pn, FILE * pfOut, int nOptions);

/* read a node from a file */
int node_parse(FILE *pfIn, node_t ** ppn);

/*****************
 Utility Functions
 *****************/

/* deep copy a node */
node_t * node_copy(node_t *pn);

/* takes a byte and returns '.' if unprintable; else returns itself */
/* takes an int because the ctype function isprint() takes an int */
char printable(int c);

/* returns true if the node has a valid type and is suitable for adding
   to a list, etc. */
int node_is_valid( node_t * pn );

/* returns newly-allocated list node containing keys of hash */
node_t * node_hash_keys( node_t * pnHash );

#endif /* _NODE_H */
