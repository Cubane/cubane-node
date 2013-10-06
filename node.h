/*	This file is node.h
	Initial version written 08-07-2000 by Cubane Software

	Revised into Unicode-capable DLL form 10-03-2005 by Cubane Software
	Performance enhancements and unit tests added 06-2006 by Cubane Software
*/

#ifndef _NODE_H
#define _NODE_H

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NODE_STATIC
#define NODE_API
#else /* not static */
#ifdef NODE_DLL
#define NODE_API	__declspec(dllexport)
#else
#define NODE_API	__declspec(dllimport)
#endif
#endif

// specify library to link
#ifdef _DEBUG 
#define NL_RELMODE "debug"
#else
#define NL_RELMODE "release"
#endif

#if _WIN64
#define NL_PLATFORM "-x64"
#else
#define NL_PLATFORM ""
#endif

#ifdef NODE_STATIC
#define NL_LIB		"node-" NL_RELMODE "-static" NL_PLATFORM ".lib"
#pragma comment( lib, NL_LIB )
#undef NL_LIB
#endif

#undef NL_PLATFORM
#undef NL_RELMODE

#ifdef NODE_USE_CONST
#define NODE_CONSTOUT const
#else
#define NODE_CONSTOUT 
#endif

/********************
 Node Library Version
 ********************/

#define NODE_HEADER_VERSION_MAJOR		3
#define NODE_HEADER_VERSION_MINOR		0

NODE_API extern const int NODE_VERSION_MAJOR;
NODE_API extern const int NODE_VERSION_MINOR;

#define node_version()		node_version_3_0()

NODE_API const char * node_version_3_0();

/*******************************
 Node Library #defined Constants
 *******************************/

/* Node Types */

#define NODE_UNKNOWN	0  /* node is invalid or uninitialized */
#define NODE_STRINGA	1  /* node contains a string value (encoding unspecified) */
#define NODE_INT		2  /* node contains a 32-bit integer value */
#define NODE_REAL		3  /* node contains a double value */
#define NODE_LIST		4  /* node contians a linked list of nodes */
#define NODE_HASH		5  /* node contains a hash of name->value (could be either A or W) */
#define NODE_INT64		6  /* node contains a 64-bit integer value */
#define NODE_OLD_COPY	7  /* add flag, not a node type: means "add a copy of this node" */
#define NODE_DATA		8  /* node contains arbitrary binary data with length */
#define NODE_STRINGW	9  /* node contains a UTF-16 string */
#define NODE_OLD_REF	10 /* add flag, not a node type: means "add this node, not a copy" */
#define NODE_PTR		11 /* store arbitrary pointer */
/* unused: 12-31 */

#define NODE_ADD_COPY	64	/* be outside the legal node type range */
#define NODE_ADD_REF	128

#define NODE_COPY_DATA	(NODE_ADD_COPY|NODE_DATA)
#define NODE_REF_DATA	(NODE_ADD_REF|NODE_DATA)

/* Node Type Synonyms */
#define NODE_NODE		NODE_ADD_COPY
#define NODE_REF		NODE_ADD_REF

/* Dump Options */
#define DO_DUMP      1 /* node debug options */
#define DO_NOESCAPE	 2 /* don't escape string data */

/* Return Values From node_parse */
#define NP_NODE		0	/* succesful read of node element */
#define NP_CPAREN	1	/* only thing on line is close paren */
#define NP_CBRACE	2	/* only thing on line is close brace */
#define NP_DUMP		3	/* only thing on line is hex dump */
#define NP_SERROR	4	/* syntax error reading line; skip this line */
#define NP_EOF		5	/* end of file */
#define NP_INVALID	6	/* invalid file stream or node pointer */

/* Node Debugging Options */
#define NODE_DEBUG_INTERN		0x01	/* check for problems with intern table */
#define NODE_DEBUG_UNICODE		0x02	/* check for Unicode strings passed when ASCII expected, and vice-versa */
#define NODE_DEBUG_REF			0x04	/* check that nodes already in a collection are not added to another collection */
#define NODE_DEBUG_HASHPERF		0x08	/* check hash performance */

#define NODE_DEBUG_ALL			0x0F	/* convenience macro to turn on common debugging options */

/********
 TYPEDEFS
 ********/

typedef unsigned char data_t;
typedef struct __node node_t;
typedef void * node_arena_t; 

/* most client apps should not define NODE_TRANSPARENT */
#ifdef NODE_TRANSPARENT

#define NODE_TYPE_BITS				5							/* number of bits for nType - up to 32 nodes */
#define NODE_COLLECTIONFLAG_BITS	1							/* am I in a list/hash? */
#define NODE_BAG_BITS				1							/* is the bag in use? */
#define NODE_HASH_BITS				25							/* how many hash bits we keep */
#define NODE_HASH_MASK				((1<<NODE_HASH_BITS)-1)

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4201 )			/* suppresses warning about nameless struct/union */
#endif

struct node_arena;

struct __node
{
	/* overhead */
	node_t * pnNext;			/* pointer to next node -- only used in lists/hashes */
	
	/* name */
	char * psAName; 			/* name of this node (may be NULL) */
	wchar_t * psWName;			/* name of this node (may be NULL) */

	struct node_arena * pArena;
	/* Win32 - 16 bytes */
	/* Win64 - 32 bytes */
	
	/* data */
	unsigned int nType:NODE_TYPE_BITS;		/* type of this node: NODE_xxx */
	unsigned int bInCollection:NODE_COLLECTIONFLAG_BITS;
	unsigned int bBagUsed:NODE_BAG_BITS;
	unsigned int nHash:NODE_HASH_BITS;
	/* Win32 - 20 bytes */
	/* Win64 - 36 bytes, padded to 40 */
	
	union
	{
		struct
		{
			/* string data */
			char * psAValue;			/* string value if string type or coerced to string type */
			wchar_t * psWValue; 		/* string value if string type or coerced to string type */

			/* numeric data */
			union
			{
			double dfValue; 			/* double value if real type or coerced */

			__int64 n64Value;			/* 64-bit integer value */

			/* int data */
			int nValue; 				/* int value if int type or coerced */
			};

			/* Win32 - 16 bytes */
			/* Win64 - 24 bytes */
		};
		
		struct
		{
			data_t * pbValue;			/* binary data */
			int nDataLength;			/* length of data */
			/* Win32 - 8 bytes */
			/* Win64 - 12 bytes, padded to 16 */
		};
		
		struct
		{
			/* pointer data */
			void * pvValue; 			/* arbitrary pointer */
			/* Win32 - 4 bytes */
			/* Win64 - 8 bytes */
		};
		
		struct
		{
			/* list data */
			node_t* pnListHead; 		/* head of list if list type */
			node_t* pnListTail; 		/* tail of list if list type */

			int nListElements;			/* number of elements in list */
			/* Win32 - 12 bytes */
			/* Win64 - 20 bytes, padded to 24 */
		};
		
		struct
		{
			/* hash data */
			node_t** ppnHashHeads;		/* array of buckets if hash type */
#ifdef _DEBUG
			char * psHashAllocated;		/* where hash allocated from: debug only */
#endif
			int nHashBuckets;			/* number of buckets if hash type */
			int nHashFlags:2; 			/* debugging/data flags */
			int nHashElements:29;		/* number of elements in hash */

			/* Win32 - 16 bytes(D), 12 bytes padded to 16 (R) */
			/* Win64 - 16 bytes(D), 24 bytes (R) */
		};
	};
};

#ifdef _MSC_VER
#pragma warning( pop )
#endif

#else /* not NODE_TRANSPARENT */

/* hide details of node implementation */
struct __node;

#endif

/****************
 Memory Functions 
 ****************/

/** allocate memory for a new node and return a pointer */
NODE_API node_t * node_alloc();

/** free memory associated with a node, including children and neighbours */
NODE_API void node_free(node_t *pn);

/** allocate an empty list node */
NODE_API node_t * node_list_alloc();

/** allocate an empty hash node */
NODE_API node_t * node_hash_alloc();

/** allocate an empty hash node with a user-specified number of buckets */
NODE_API node_t * node_hash_alloc2( int nHashBuckets );

/*****************
 Setting Functions
 *****************/

/** set the value of an existing node */ 
NODE_API node_t * node_set( node_t * pn, int nType, ... );

/** set the value of an existing node to NODE_DATA type */ 
NODE_API node_t * node_set_data( node_t * pn, int nLength, const void * pb );

/*****************
 Reading Functions
 *****************/

/** returns the node's type */
NODE_API int node_get_type( const node_t * pn );

/** returns the int value from the node */
NODE_API int node_get_int( const node_t * pn );

/** returns the real value from the node */
NODE_API double node_get_real( const node_t * pn );

/** returns the int64 value from the node */
NODE_API __int64 node_get_int64( const node_t * pn );

/** returns the 'A' (narrow) string value from the node */
NODE_API NODE_CONSTOUT char * node_get_stringA( node_t * pn );

/** returns the 'W' (UTF-16) string value from the node */
NODE_API NODE_CONSTOUT wchar_t * node_get_stringW( node_t * pn );

/** returns the data value from the node */
NODE_API NODE_CONSTOUT data_t * node_get_data( const node_t * pn, int * pnLength );

/** returns the number of elements in the node (list/hash only) */
NODE_API int node_get_elements( const node_t * pn );

/** returns the pointer value from the node */
NODE_API void * node_get_ptr( const node_t * pn );

/**************
 List Functions
 **************/

/** add a new node to the end of a list; similar variable arguments to node_set */
NODE_API node_t * node_list_add( node_t * pnList, int nType, ... );

/** delete a node from within a list */
NODE_API void node_list_delete( node_t * pnList, node_t * pnToDelete );

/** returns the first node of a list */
NODE_API node_t * node_first( const node_t * pnList );

/** returns the next node in a list or hash bucket chain */
NODE_API node_t * node_next( const node_t * pn );

/* Stack functions: treating the list as a stack */
/** pushes a node onto the front of a list */
NODE_API node_t * node_push( node_t * pnList, int nType, ... );

/** pops a node off the front of a list */
NODE_API node_t * node_pop( node_t * pnList );

/**************
 Hash Functions
 **************/

/** add a node to a hash; similar variable arguments to node_set */
NODE_API node_t * node_hash_addA( node_t * pnHash, const char * psKey, int nType, ... );
/** add a node to a hash; similar variable arguments to node_set */
NODE_API node_t * node_hash_addW( node_t * pnHash, const wchar_t * psKey, int nType, ... );

/** delete a node from within a hash */
NODE_API void node_hash_delete( node_t * pnHash, node_t * pnToDelete );

/** get a node (by name) from a hash */
NODE_API node_t * node_hash_getA( const node_t * pnHash, const char * psKey );
/** get a node (by name) from a hash */
NODE_API node_t * node_hash_getW( const node_t * pnHash, const wchar_t * psKey );

/**************
 Name Functions
 **************/

/** set the name of a node */
NODE_API void node_set_nameA( node_t * pn, const char * psName );
/** set the name of a node */
NODE_API void node_set_nameW( node_t * pn, const wchar_t * psName );

/** get the name of node */
NODE_API NODE_CONSTOUT char * node_get_nameA( const node_t * pn);
/** get the name of node */
NODE_API NODE_CONSTOUT wchar_t * node_get_nameW( const node_t * pn);

/*****************************
 Dumping and Parsing Functions
 *****************************/

/** dump the node to a file */
NODE_API void node_dumpA( const node_t * pn, FILE * pfOut, int nOptions);

/** dump the node to a file */
NODE_API void node_dumpW( const node_t * pn, FILE * pfOut, int nOptions);

/** read a node from a file */
NODE_API int node_parseA(FILE *pfIn, node_t ** ppn);

/** read a node from a file */
NODE_API int node_parseW(FILE *pfIn, node_t ** ppn);

/** read a node from a string */
NODE_API int node_parse_from_stringA( const char * ps, node_t ** ppn );

/** read a node from a string */
NODE_API int node_parse_from_stringW( const wchar_t * ps, node_t ** ppn );

/** read a node from an unterminated string, length supplied */
NODE_API int node_parse_from_dataA( const void * pv, size_t nBytes, node_t ** ppn );

/** read a node from an unterminated string, length supplied */
NODE_API int node_parse_from_dataW( const void * pv, size_t nBytes, node_t ** ppn );

/*****************
 Utility Functions
 *****************/

/** deep copy a node */
NODE_API node_t * node_copy( const node_t * pn );

/** returns true if the node has a valid type and is suitable for adding
   to a list, etc. */
NODE_API int node_is_valid( const node_t * pn );

/** returns newly-allocated list node containing keys of hash */
NODE_API node_t * node_hash_keysA( const node_t * pnHash );

/** returns newly-allocated list node containing keys of hash */
NODE_API node_t * node_hash_keysW( const node_t * pnHash );

/** set codepage for internal ASCII/wide character conversions */
NODE_API void node_set_codepage( int nCodePage );

/** get codepage for internal ASCII/wide character conversions */
NODE_API int node_get_codepage();

/** set debug state */
NODE_API void node_set_debug( int nDebug );

/** get debug state */
NODE_API int node_get_debug();

#ifdef _DEBUG
/** set hash load factor limit */
NODE_API void node_set_loadlimit( double dfLoadLimit );

/** get hash load factor limit */
NODE_API double node_get_loadlimit();

/* for profiling hash performance */
#define node_hash_alloc_d()		node_hash_alloc_dbg( __FILE__, __LINE__ )
#define node_hash_alloc2_d( n )		node_hash_alloc2_dbg( __FILE__, __LINE__, n )

#endif

/** clean up all module storage */
NODE_API void node_finalize();

NODE_API node_arena_t node_create_arena( size_t size );

NODE_API node_arena_t node_get_arena();

NODE_API node_arena_t node_set_arena( node_arena_t pNewArena );

NODE_API size_t node_delete_arena( node_arena_t pToDelete );
/**************************************
 Debugging analogues of above functions
 **************************************/

#ifdef _DEBUG

NODE_API node_t * node_alloc_dbg( const char *psFile, int nLine );

NODE_API node_t * node_list_alloc_dbg( const char * psFile, int nLine );

NODE_API node_t * node_hash_alloc_dbg( const char * psFile, int nLine );
NODE_API node_t * node_hash_alloc2_dbg( int nHashBuckets, const char * psFile, int nLine );

NODE_API node_t * node_set_dbg( const char *psFile, int nLine, node_t * pn, int nType, ... );
NODE_API node_t * node_set_data_dbg( const char *psFile, int nLine, node_t * pn, int nLength, const void * pb );

NODE_API NODE_CONSTOUT char * node_get_string_dbgA( const char *psFile, int nLine, node_t * pn );
NODE_API NODE_CONSTOUT wchar_t * node_get_string_dbgW( const char *psFile, int nLine, node_t * pn );

NODE_API node_t * node_list_add_dbg( const char *psFile, int nLine, node_t * pnList, int nType, ... );
NODE_API node_t * node_push_dbg( const char *psFile, int nLine, node_t * pnList, int nType, ... );

NODE_API node_t * node_hash_add_dbgA( const char *psFile, int nLine, node_t * pnHash, const char * psKey, int nType, ... );
NODE_API node_t * node_hash_add_dbgW( const char *psFile, int nLine, node_t * pnHash, const wchar_t * psKey, int nType, ... );

NODE_API void node_set_name_dbgA( const char *psFile, int nLine, node_t * pn, const char * psName );
NODE_API void node_set_name_dbgW( const char *psFile, int nLine, node_t * pn, const wchar_t * psName );

NODE_API int node_parse_dbgA( const char *psFile, int nLine, FILE *pfIn, node_t ** ppn);
NODE_API int node_parse_dbgW( const char *psFile, int nLine, FILE *pfIn, node_t ** ppn);

NODE_API int node_parse_from_string_dbgA( const char *psFile, int nLine, const char * ps, node_t ** ppn );
NODE_API int node_parse_from_string_dbgW( const char *psFile, int nLine, const wchar_t * ps, node_t ** ppn );

NODE_API int node_parse_from_data_dbgA( const char *psFile, int nLine, const void * pv, size_t nBytes, node_t ** ppn );
NODE_API int node_parse_from_data_dbgW( const char *psFile, int nLine, const void * pv, size_t nBytes, node_t ** ppn );

NODE_API node_t * node_copy_dbg( const char *psFile, int nLine, const node_t * pn );

NODE_API node_t * node_hash_keys_dbgA( const char *psFile, int nLine, const node_t * pnHash );
NODE_API node_t * node_hash_keys_dbgW( const char *psFile, int nLine, const node_t * pnHash );

#endif

/************************
 Error-Handling Functions
 ************************/

typedef void (*node_error_func_t)( char * psError );
typedef int (*node_memory_func_t)( size_t cb );
typedef void (*node_assert_func_t)( void * psExpr, void * psFile, unsigned int nLine );

#define NODE_MEMORY_FAIL	0
#define NODE_MEMORY_RETRY	1

NODE_API void node_set_error_funcs( node_error_func_t pErrFunc, node_memory_func_t pMemFunc, 
								    node_assert_func_t pAssertFunc );

/***********************************
 Definitions to support generic char
 ***********************************/

#ifndef _UNICODE

#define NODE_STRING						NODE_STRINGA
#define node_get_string					node_get_stringA
#define node_hash_add					node_hash_addA
#define node_hash_get					node_hash_getA
#define node_hash_keys					node_hash_keysA
#define node_get_name					node_get_nameA
#define node_set_name					node_set_nameA
#define node_parse						node_parseA
#define node_parse_from_string			node_parse_from_stringA
#define node_parse_from_data			node_parse_from_dataA
#define node_dump						node_dumpA

#else

#define NODE_STRING						NODE_STRINGW
#define node_get_string					node_get_stringW
#define node_hash_add					node_hash_addW
#define node_hash_get					node_hash_getW
#define node_hash_keys					node_hash_keysW
#define node_get_name					node_get_nameW
#define node_set_name					node_set_nameW
#define node_parse						node_parseW
#define node_parse_from_string			node_parse_from_stringW
#define node_parse_from_data			node_parse_from_dataW
#define node_dump						node_dumpW

#endif

/*******************************************************
 Definitions to support debug (must follow generic char)
 *******************************************************/
#if defined(_DEBUG) && defined(NODE_DEBUG_MEMORY_LEAKS)

#define node_list_alloc()			node_list_alloc_dbg( __FILE__, __LINE__ )
#define node_hash_alloc()			node_hash_alloc_dbg( __FILE__, __LINE__ )

/* note: this fn preceded the node-debug branch, keeping arg order for backward compatibility */
#define node_hash_alloc2(n)			node_hash_alloc2_dbg( n, __FILE__, __LINE__ )

#define node_alloc()				node_alloc_dbg( __FILE__, __LINE__ )
#define node_hash_alloc()			node_hash_alloc_dbg( __FILE__, __LINE__ )
#define node_set(n,t,v)				node_set_dbg( __FILE__, __LINE__, n, t, v )
#define node_list_add(n,t,v)		node_list_add_dbg( __FILE__, __LINE__, n, t, v )
#define node_push(n,t,v)			node_push_dbg( __FILE__, __LINE__, n, t, v )
#define node_copy(n)				node_copy_dbg( __FILE__, __LINE__, n )
#define node_set_data(n,l,d)		node_set_data_dbg( __FILE__, __LINE__, n, l, d )

#define node_get_stringA(n)				node_get_string_dbgA( __FILE__, __LINE__, n )
#define node_get_stringW(n)				node_get_string_dbgW( __FILE__, __LINE__, n )
#define node_hash_addA(n,na,t,v)		node_hash_add_dbgA( __FILE__, __LINE__, n, na, t, v )
#define node_hash_addW(n,na,t,v)		node_hash_add_dbgW( __FILE__, __LINE__, n, na, t, v )
#define node_set_nameA(n,na)			node_set_name_dbgA( __FILE__, __LINE__, n, na )
#define node_set_nameW(n,na)			node_set_name_dbgW( __FILE__, __LINE__, n, na )

#define node_parseA(f,pn)				node_parse_dbgA( __FILE__, __LINE__, f, pn )
#define node_parseW(f,pn)				node_parse_dbgW( __FILE__, __LINE__, f, pn )
#define node_parse_from_stringA(f,pn)	node_parse_from_string_dbgA( __FILE__, __LINE__, f, pn )
#define node_parse_from_stringW(f,pn)	node_parse_from_string_dbgW( __FILE__, __LINE__, f, pn )

#define node_parse_from_dataA(pv,n,pn)	node_parse_from_data_dbgA( __FILE__, __LINE__, pv, n, pn )
#define node_parse_from_dataW(pv,n,pn)	node_parse_from_data_dbgW( __FILE__, __LINE__, pv, n, pn )

#define node_hash_keysA(n)				node_hash_keys_dbgA( __FILE__, __LINE__, n )
#define node_hash_keysW(n)				node_hash_keys_dbgW( __FILE__, __LINE__, n )

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _NODE_H */
