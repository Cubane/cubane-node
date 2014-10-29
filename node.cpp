/* node.cpp -- implementation of node module
 * Initial version written 2000-08-07 Cubane Software
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stddef.h>

#define NODE_TRANSPARENT 1

#include "node.h"
#include "node_shared.h"

#include "git-version.h"

/**************************************
 Node Library Configuration Definitions
 **************************************/

/* define exactly one of the next two */
//#define HASH_USES_MOD				/* use % operator and prime-number bucket counts */
#define HASH_USES_BITMASK			/* use & operator and power-of-two bucket counts (slightly faster) */

/* optional, only valid if HASH_USES_BITMASK */
//#define HASH_AUTO_RESIZE			/* auto-resize hashes if they get very large */

/* optional: reasonable speed improvement */
//#define WITH_FASTCALL				/* internal functions use __fastcall calling convention */
// needs to be defined at project level to affect all files -- set in DSP file

/* optional: small speed improvement when hashes used frequently */
#define USE_BAGS					/* allocate extra space after node for (small) string storage instead of using free store */

/***********************************
 Error checking on above definitions
 ***********************************/

#ifndef HASH_USES_MOD
#ifndef HASH_USES_BITMASK
#error Must define either HASH_USES_BITMASK or HASH_USES_MOD
#endif
#endif

#ifdef HASH_USES_MOD
#ifdef HASH_USES_BITMASK
#error Must define only one of HASH_USES_BITMASK or HASH_USES_MOD
#endif
#endif

#ifdef HASH_USES_MOD
#ifdef HASH_AUTO_RESIZE
#error Hash is only resizable if HASH_USES_BITMASK
#endif
#endif

#define HASH_LOAD_FACTOR	16
#define NEEDS_REBALANCE(pn)		( (pn)->nHashBuckets < (1<<NODE_HASH_BITS) && (pn)->nHashElements > HASH_LOAD_FACTOR * (pn)->nHashBuckets )

#define DIMENSION( A ) ( (sizeof A)/(sizeof A[0]) )

#if defined(_MSC_VER)
#define node_mutex CRITICAL_SECTION
int node_mutex_init(node_mutex *nm) {
  return InitializeCriticalSection(nm);
}
void node_mutex_lock(node_mutex *nm) {
  EnterCriticalSection( nm );
}
void node_mutex_unlock(node_mutex *nm) {
  LeaveCriticalSection( nm );
}
#else
#define node_mutex pthread_mutex_t
int node_mutex_init(node_mutex * nm) {
  return pthread_mutex_init(nm);
}
void node_mutex_lock(node_mutex *nm) {
  pthread_mutex_lock( nm );
}
void node_mutex_unlock(node_mutex *nm) {
  pthread_mutex_unlock( nm );
}
#endif

/* Malloc specials */
#ifdef USE_DL_MALLOC
extern "C" extern struct malloc_state _gm_;

struct node_arena
{
	struct va
	{
		va( va * p ) : next(p) {}
		va * next;
	};

	mspace pSpace;
	node_t * pnFreeList;
	node_mutex csFreeList;
	va * pVA;
};

struct node_arena g_GlobalArena = { &_gm_, NULL, {0}, 0 };

static void inline nfree( node_arena * pArena, void * pv )
{
	mspace_free( pArena->pSpace, pv );
}
#else
struct node_arena
{
	int nDummy;
};

struct node_arena g_GlobalArena = {0};
static void inline nfree( node_arena * , void * pv )
{
	free( pv );
}
#endif

/*************************************
 Debug/Release adjustments to #defines
 *************************************/

#if defined(_DEBUG) && !defined(DEBUG_DL_MALLOC)	/* Debug build */

#define DEBUG_MEMORY_LEAKS

#endif

typedef void (*node_assert_func_internal_t)( void * psExpr, const char * psFile, unsigned int nLine );

/**********************
 Module Classes for TLS
 **********************/

struct node_tls
{
	node_tls() : nCodePage(CP_UTF8), pfError(NULL), pfMemory(NULL), pfAssert(NULL), pArena(&g_GlobalArena), psSourceFile(NULL), nSourceLine(-1) {}
	int nCodePage;
	node_error_func_t pfError;
	node_memory_func_t pfMemory;
	node_assert_func_internal_t pfAssert;
	node_arena * pArena;
	const char * psSourceFile;
	int nSourceLine;
};

static int m_dwTLSIndex = -1;

node_tls * GetTLS()
{
	if( m_dwTLSIndex < 0 )
    {
		m_dwTLSIndex = TlsAlloc();
    }
    node_tls* data = reinterpret_cast<node_tls*>(TlsGetValue(m_dwTLSIndex));
    if(!data)
    {
		data = new node_tls();
		if( data == NULL )
		{
			OutputDebugStringA( "node library - unable to allocate TLS data structure" );
			exit(1);
		}
		
		TlsSetValue( m_dwTLSIndex, data );
    }
	return data;
}

static inline int& GetTLS_nCodePage()
{
	return GetTLS()->nCodePage;
}

static inline node_error_func_t& GetTLS_pfError()
{
	return GetTLS()->pfError;
}

static inline node_memory_func_t& GetTLS_pfMemory()
{
	return GetTLS()->pfMemory;
}

static inline node_assert_func_internal_t& GetTLS_pfAssert()
{
	return GetTLS()->pfAssert;
}

static inline node_arena*& GetTLS_pArena()
{
	return GetTLS()->pArena;
}

static inline const char *& GetTLS_source_file()
{
	return GetTLS()->psSourceFile;
}

static inline int& GetTLS_source_line()
{
	return GetTLS()->nSourceLine;
}

#define node_nCodePage		GetTLS_nCodePage()
#define node_pfError		GetTLS_pfError()
#define node_pfMemory		GetTLS_pfMemory()
#define node_pfAssert		GetTLS_pfAssert()
#define node_pArena			GetTLS_pArena()
#define node_source_file	GetTLS_source_file()
#define node_source_line	GetTLS_source_line()

#ifdef _DEBUG
class set_debug_allocator
{
	const char * psFileOld_;
	int nLineOld_;
public:
	set_debug_allocator( const char * psFile, int nLine ) : psFileOld_( node_source_file ), nLineOld_( node_source_line )
	{
		node_source_file = psFile;
		node_source_line = nLine;
	}
	~set_debug_allocator()
	{
		node_source_file = psFileOld_;
		node_source_line = nLineOld_;
	}
};
#else
class set_debug_allocator
{
public:
	set_debug_allocator( const char * , int ) {}
};
#endif

/************************************
 Module Classes for Critical Sections 
 ************************************/

class node_lock
{
	node_mutex * m_cs;
public:
  node_lock( node_mutex *cs ) : m_cs(cs) {node_mutex_lock(m_)cs);}
  ~node_lock() { node_mutex_unlock(m_cs); }
};

/****************
 Module Variables
 ****************/

/* explicitly synchronize with header so client apps can assert( NODE_HEADER_VERSION_MAJOR == NODE_VERSION_MAJOR ) */
extern const int NODE_VERSION_MAJOR = NODE_HEADER_VERSION_MAJOR;
extern const int NODE_VERSION_MINOR = NODE_HEADER_VERSION_MINOR;

#ifdef _DEBUG
#define VER_SUFFIX " (debug,static)"
#else
#define VER_SUFFIX " (static)"
#endif

const char * node_version_3_0() { return "Node version " GIT_VERSION_SHORT VER_SUFFIX; }

static const char * node_cpp_revision = GIT_VERSION_SHORT;
static const char * node_cpp_revision_long = GIT_VERSION_LONG;

#ifdef _DEBUG
/* debug mode - turn on all debugging options */
static int node_nDebug = NODE_DEBUG_UNICODE|NODE_DEBUG_REF;

static int node_nDebugIntern = 0;
static int node_nDebugUnicode = NODE_DEBUG_UNICODE;
static int node_nDebugRef = NODE_DEBUG_REF;
static int node_nDebugHashPerf = 0;

static double node_dfLoadLimit = 10.0;
#else
static int node_nDebug = 0;

static int node_nDebugIntern = 0;
static int node_nDebugUnicode = 0;
static int node_nDebugRef = 0;
static int node_nDebugHashPerf = 0;
#endif

/* spaces - to avoid fputc               1234567890123456 */
static const char    node_acSpaces[] =  "                ";
static const wchar_t node_wcSpaces[] = L"                ";
#define NODE_SPACES_COUNT 16

static const char    node_acHex[] =  "0123456789abcdef";
static const wchar_t node_wcHex[] = L"0123456789abcdef";

static inline void byte_to_strA( char *& ps, unsigned int b )
{
	*ps++ = node_acHex[ (b>>4)&0x0F ];
	*ps++ = node_acHex[  b    &0x0F ];
}

static inline void byte_to_strW( wchar_t *& ps, unsigned int b )
{
	*ps++ = node_wcHex[ (b>>4)&0x0F ];
	*ps++ = node_wcHex[  b    &0x0F ];
}

/***********************
 Private Named Constants
 ***********************/

#ifdef HASH_USES_BITMASK
#define DEFAULT_HASHBUCKETS 8
#endif

#ifdef HASH_USES_MOD
#define DEFAULT_HASHBUCKETS 17
#endif

#define LOTS_OF_MEMORY 0x10000000 /* 256MB */
#define GRATUITOUSLY_MUCH_MEMORY 0x7FFFFFFF /* >2GB NODE_DATA allocations are not supported */

#define NODE_A		1
#define NODE_W		2

#define NODE_BOM	0xFEFF

#define HASH_CONTAINS_AKEYS		0x01
#define HASH_CONTAINS_WKEYS		0x02

#if _WIN64 
#define NODE_SIZE		64		/* not sizeof(node_t), which is 64 */
#else 
#define NODE_SIZE		48		/* not sizeof(node_t), which is 48 */
#endif


#ifdef USE_BAGS
#if _WIN64 
#define BAG_SIZE		64
#else 
#define BAG_SIZE		32
#endif

#define GET_BAG( pn )		(((unsigned char *)pn) + NODE_SIZE)
#define IS_BAG( pn, ps )	( (void*)(ps) == (void*)GET_BAG(pn) )
#else
#define BAG_SIZE		0
#define GET_BAG( pn ) (assert(0))
#pragma warning( disable: 4127 )
#define IS_BAG( pn, ps ) 0
#endif

#define NOT_IN_COLLECTION	0
#define IN_COLLECTION		1

/***********************
 Private Data Structures
 ***********************/

struct node_dump
{
	FILE * pfOut;
	int nOptions;
	int nSpaces;
};

/*****************************
 Private Function Declarations
 *****************************/

/* set the node to a value using variable arguments. nType determines how 
arguments are processed.*/
static void node_set_valist(node_t *pn, int nType, va_list valist);
static node_t * node_add_common( node_arena * pArena, int nType, va_list valist );

static void node_set_int( node_t * pn, int nValue );
static void node_set_int64( node_t * pn, int64_t nValue );
static void node_set_real( node_t * pn, double dfValue );
static void node_set_stringA( node_t * pn, const char * psAValue );
static void node_set_stringW( node_t * pn, const wchar_t * psWValue );
static void node_set_ptr( node_t * pn, void * pv );

/* versions with less arg checking */
static void node_set_stringA_internal( node_t * pn, const char * psAValue );
static void node_set_stringA_internal( node_t * pn, const char * psAValue, const char * psEnd );
static void node_set_stringA_internal( node_t * pn, const char * psAValue, size_t nLength );

static void node_set_stringW_internal( node_t * pn, const wchar_t * psWValue );
static void node_set_stringW_internal( node_t * pn, const wchar_t * psWValue, const wchar_t * psEnd );
static void node_set_stringW_internal( node_t * pn, const wchar_t * psWValue, size_t cch );

static void node_set_data_internal( node_t * pn, int nLength, const data_t * pb );

/* initialize a list node */
static void node_list_init(node_t * pn);	

/* initialize a hash node */
static void node_hash_init( node_t * pn, int nHashBuckets );

/* clean up overlaid structures in a node changing type */
static void node_cleanup( node_t * pn );

/* internal analogs of external functions */
static node_t * node_alloc_internal( node_arena * pArena );
static void node_free_internal( node_t * pn, unsigned int bInCollection );

static node_t * node_list_add_valist( node_t * pnList, int nType, va_list valist );
static void node_list_add_internal( node_t * pnList, node_t * pnNew );
static void node_list_delete_internal( node_t * pnList, node_t * pnToDelete );

static node_t * node_push_valist( node_t * pnList, int nType, va_list valist );
static node_t * node_push_internal( node_t * pnList, node_t * pnNew );
static node_t * node_pop_internal( node_t * pnList );

static node_t * node_copy_internal( node_arena * pArena, const node_t * pnSource );

static node_t * node_hash_addA_valist( node_t * pnHash, const char * psKey, int nType, va_list valist );
static node_t * node_hash_addW_valist( node_t * pnHash, const wchar_t * psKey, int nType, va_list valist );
static void node_hash_add_internal( node_t * pnHash, node_t * pnNew );
static void node_hash_delete_internal( node_t * pnHash, node_t * pnToDelete );

static node_t * node_hash_getA_internal( const node_t * pnHash, const char * psKey );
static node_t * node_hash_getW_internal( const node_t * pnHash, const wchar_t * psKey );

static void node_dumpA_internal( const node_t * pn, struct node_dump * pd );
static void node_dumpW_internal( const node_t * pn, struct node_dump * pd );

static void node_set_nameA_internal( node_t * pn, const char * psName );
static void node_set_nameW_internal( node_t * pn, const wchar_t * psName );

/* dlmalloc a new string */
static char * node_safe_copyA( node_arena * pArena, const char * ps );
static wchar_t * node_safe_copyW( node_arena * pArena, const wchar_t * ps );

/* read a line from a file into malloc'ed storage */
static char * read_lineA( node_arena * pArena, FILE * pfIn, char ** ppsEnd );
static wchar_t * read_lineW( node_arena * pArena, FILE * pfIn, wchar_t ** ppsEnd );

/* escape and unescape pesky characters */
static char * node_escapeA( node_arena * pArena, const char * psUnescaped );
static wchar_t * node_escapeW( node_arena * pArena, const wchar_t * psUnescaped );

static char * node_unescapeA( node_arena * pArena, const char * psEscaped, const char * psEnd );
static wchar_t * node_unescapeW( node_arena * pArena, const wchar_t * psEscaped, const wchar_t * psEnd );

static char * WToA( node_arena * pArena, const wchar_t * psW );
static char * WToA( node_arena * pArena, const wchar_t * psW, const wchar_t * psWEnd );
static char * WToA( node_arena * pArena, const wchar_t * psW, size_t cch );

static wchar_t * AToW( node_arena * pArena, const char * psA );
static wchar_t * AToW( node_arena * pArena, const char * psA, const char * psAEnd );
static wchar_t * AToW( node_arena * pArena, const char * psA, size_t nLength );

class NodeReader;
static int node_parse_internalA(NodeReader *pnr, node_t ** ppn, int nOutputStyle );
static int node_parse_internalW(NodeReader *pnr, node_t ** ppn, int nOutputStyle );

static int node_parse_internal(FILE *pfIn, node_t ** ppn, int nOutputStyle );

/* takes a byte and returns '.' if unprintable; else returns itself */
/* takes an int because the ctype function isprint() takes an int */
static __inline int printable(int c);

static inline void node_write_spacesA( FILE * pfOut, int nSpaces );
static inline void node_write_spacesW( FILE * pfOut, int nSpaces );

static int node_memory( size_t cb );
static void node_error( char * psError, ... );

static void _node_assert( const char *, const char *, unsigned int );
#define node_assert(exp) (void)( (exp) || (_node_assert(#exp, __FILE__, __LINE__), 0) )

static int __inline hash_to_bucket( const node_t * pnHash, int nHash );

#ifdef HASH_AUTO_RESIZE
static void hash_rebalance( node_t * pnHash, int nNewBuckets );
#endif

/* debug checking functions */
static void node_check_ascii_string( const char * psValue, const char * psContext );
static void node_check_unicode_string( const wchar_t * psValue, const char * psContext );

#define NODE_STRING_CANTTELL	1
#define NODE_STRING_ASCII		2
#define NODE_STRING_UNICODE		3

static int node_string_type( const unsigned char * ps );

static void * node_valloc( node_arena * pArena, size_t nSize );

void * node_malloc( struct node_arena * pArena, size_t cb );

/***********************************************
 Node Parsing Classes - Interface & Declarations 
 ***********************************************/

/* interface class */
class NodeReader
{
public:
	node_arena * m_pArena;

	NodeReader( node_arena * pArena ) : m_pArena(pArena) {}
	virtual void read_lineA( const char ** ppsStart, const char ** ppsEnd ) = 0;
	virtual void read_lineW( const wchar_t ** ppsStart, const wchar_t ** ppsEnd ) = 0;
	virtual void free_line( const void * psLine ) = 0;
};

class NodeFileReader;
class NodeStringAReader;
class NodeStringWReader;

/*******************************
 Public Function Implementations
 *******************************/

/*********
 Iterators
 *********/

/* returns the first node of a list */
node_t * node_first( const node_t * pnList )
{
	if( pnList == NULL )
	{
		node_assert( pnList != NULL );
		return NULL;
	}

	/* half of a simple iterator: returns pnListHead */
	if( pnList->nType == NODE_LIST )
	{
		return pnList->pnListHead;
	}
	else	/* not currently a list, but may have been in the past... */
	{
		node_assert( pnList->nType == NODE_LIST );	/* invalid type for node_first */
		return NULL;	/* return explicit NULL instead of pnListHead */
	}

}

/* returns the next node in a list or hash bucket chain */
node_t * node_next( const node_t * pn )
{
	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return NULL;
	}

	return pn->pnNext;
}

/*********************************************
 Local-to-node.dll Macro Versions of Iterators 
 *********************************************/

#define node_first( pn )	((pn)->pnListHead)
#define node_next( pn )		((pn)->pnNext)
 
/****************
 Memory Functions 
 ****************/

/* allocate memory for a new node and return a pointer */
node_t * node_alloc()
{
	return node_alloc_internal( node_pArena );
}

node_t * node_alloc_dbg( const char * psFile, int nLine )
{
	set_debug_allocator s( psFile, nLine );
	return node_alloc_internal( node_pArena );
}

static node_t * node_alloc_internal( node_arena * pArena )
{
	node_t * pnNew = NULL;

#ifdef USE_DL_MALLOC
	{
		node_lock l( &pArena->csFreeList );
		if( pArena->pnFreeList != NULL )
		{
			pnNew = pArena->pnFreeList;
			pArena->pnFreeList = pnNew->pnNext;
		}

		if( pnNew == NULL )
		{
			/* allocate some more space */

#ifdef USE_BAGS
			const size_t nNodeSize = NODE_SIZE + BAG_SIZE;
#else
			const size_t nNodeSize = NODE_SIZE;
#endif
			/* reserve 16 bytes for dlmalloc page overhead */
			const size_t nArenaSize = 65536;
			const size_t nCount = (nArenaSize-sizeof(node_arena::va))/ nNodeSize;

			char * pcArena = reinterpret_cast<char*>(node_valloc( pArena, nArenaSize ) );

			/* build freelist */
			node_t * pnLast = reinterpret_cast<node_t*>( pcArena + (nCount-1) * nNodeSize );
			pnLast->pnNext = pArena->pnFreeList;

			
			for( unsigned int i = nCount-2; i > 0; --i )
			{
				node_t * pn = reinterpret_cast<node_t*>( pcArena + i * nNodeSize );
				pn->pnNext = pnLast;
				pnLast = pn;
			}

			pArena->pnFreeList = pnLast;

			pnNew = reinterpret_cast<node_t*>(pcArena);

		}
	}
#endif /* USE_DL_MALLOC */

	// BAG_SIZE == 0 if not USE_BAGS, so below is OK
	if( pnNew == NULL )
		pnNew = reinterpret_cast<node_t *>(node_malloc( pArena, NODE_SIZE + BAG_SIZE ));

	memset( pnNew, 0, NODE_SIZE );
	pnNew->pArena = pArena;

	return pnNew;
}

/* free memory associated with a node, including children and neighbours */
void node_free(node_t *pn)
{
	if( pn == NULL )
		return;

	node_free_internal( pn, NOT_IN_COLLECTION );
}

static void node_free_internal( node_t * pn, unsigned int bInCollection )
{
	node_t * pnSaved = NULL;

	while( pn != NULL ) 
	{
		node_arena * pArena = pn->pArena;
		pnSaved = pn->pnNext;

		if( pn->bInCollection != bInCollection )
		{
			node_assert( pn->bInCollection == bInCollection );
			return;
		}

		if( pn->psAName != NULL ) 
		{
			nfree( pArena, pn->psAName );
			pn->psAName = NULL;
		}
		
		if( pn->psWName != NULL ) 
		{
			nfree( pArena, pn->psWName );
			pn->psWName = NULL;
		}

		/* free and NULL all members of pn (type specific) */
		node_cleanup( pn );

#ifdef USE_DL_MALLOC
		{
			node_lock l(&pArena->csFreeList );
			pn->pnNext = pArena->pnFreeList;
			pArena->pnFreeList = pn;
		}
#else
		nfree( pArena, pn );
#endif
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
	/* if it's a list already */
	if( pn->nType == NODE_LIST ) 
	{

		/* do nothing */
		return;

	} 

	/* free and null the previous occupants of the union */
	node_cleanup( pn );

	/* set the type */
	pn->nType = NODE_LIST;

	/* do the tail magic */
	pn->pnListTail = (node_t*)&(pn->pnListHead);

	/* initialize the element count */
	pn->nListElements = 0;

	return;
}

/* initialize a hash node */
static void node_hash_init( node_t * pn, int nHashBuckets )
{
	/* if it's a hash, do nothing */
	if(pn->nType == NODE_HASH)
	{
		return;
	}

	/* otherwise, */
	
	/* free and null the previous occupants of the union */
	node_cleanup( pn );

	/* again, make sure no-one is corrupting our information */
	node_assert(pn->nHashBuckets == 0);

#ifdef HASH_USES_BITMASK
	/* for bitmask - hash fails if nHashBuckets is not a power of 2 */
	const int nNearbyPower2[33] = 
		{ 2,  2,  2,  4,  4,  4,  4,  8,  8,  8,  8, 16, 16, 16, 16, 16,
		 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 32, 32, 32, 32, 32, 32, 32 };

	if( nHashBuckets <= 32 )
		pn->nHashBuckets = nNearbyPower2[nHashBuckets];
	else
	{
		int v = nHashBuckets;

		v--;
		v |= v >> 1;
		v |= v >> 2;
		v |= v >> 4;
		v |= v >> 8;
		v |= v >> 16;
		v++;

		/* set nHashBuckets */
		pn->nHashBuckets = v;
	}

#endif

#ifdef HASH_USES_MOD
	/* for mod - hash is less efficient if nHashBuckets is not prime */
	
	/* TODO: consider checking this */
	pn->nHashBuckets = nHashBuckets;
#endif

	size_t nSize = sizeof(node_t*) * pn->nHashBuckets;

#ifdef USE_BAGS
	if( !pn->bBagUsed && nSize <= BAG_SIZE )
	{
		pn->ppnHashHeads = (node_t**)GET_BAG( pn );
		pn->bBagUsed = TRUE;
	}
	else
#endif
	{
		/* allocate ppnHashHeads */
		pn->ppnHashHeads = ( node_t ** )node_malloc( pn->pArena, nSize );
	}
	
	memset( pn->ppnHashHeads, 0, pn->nHashBuckets*sizeof(node_t*) );
	
	/* set nHashElements to 0 */
	pn->nHashElements = 0;
	
	/* set nType to NODE_HASH */
	pn->nType = NODE_HASH;

	return;

}

node_t * node_list_alloc()
{
	node_t * pn = NULL;
	
	pn = node_alloc_internal( node_pArena );
	
	node_list_init( pn );
	
	return pn;
}

node_t * node_list_alloc_dbg( const char * psFile, int nLine )
{
	set_debug_allocator s( psFile, nLine );

	return node_list_alloc();
}

node_t * node_hash_alloc()
{
	node_t * pn = node_alloc_internal( node_pArena );
	
	node_hash_init( pn, DEFAULT_HASHBUCKETS );

	return pn;
}

node_t * node_hash_alloc2( int nHashBuckets )
{
	node_t * pn = node_alloc_internal( node_pArena );
	
	node_hash_init( pn, nHashBuckets );
	
	return pn;
}

#ifdef _DEBUG
static void node_hash_store_debug( node_t * pn, const char * psFile, int nLine )
{
	char acFile[1024];
	_snprintf( acFile, sizeof(acFile), "%s(%d) :", psFile, nLine );
	acFile[ sizeof(acFile)-1 ] = '\0';
	pn->psHashAllocated = node_safe_copyA( pn->pArena, acFile );
}

node_t * node_hash_alloc_dbg( const char * psFile, int nLine )
{
	set_debug_allocator s( psFile, nLine );

	node_t * pn = node_alloc_internal( node_pArena );
	
	node_hash_init( pn, DEFAULT_HASHBUCKETS );

	if( node_nDebugHashPerf )
		node_hash_store_debug( pn, psFile, nLine );

	return pn;
}

node_t * node_hash_alloc2_dbg( int nHashBuckets, const char * psFile, int nLine )
{
	set_debug_allocator s( psFile, nLine );

	node_t * pn = node_alloc_internal( node_pArena );
	
	node_hash_init( pn, nHashBuckets );
	
	if( node_nDebugHashPerf )
		node_hash_store_debug( pn, psFile, nLine );

	return pn;
}

/** set hash load factor limit */
void node_set_loadlimit( double dfLoadLimit )
{
	node_dfLoadLimit = dfLoadLimit;
}

/** get hash load factor limit */
double node_get_loadlimit()
{
	return node_dfLoadLimit;
}

#endif

/*****************
 Setting Functions
 *****************/

node_t * node_set_dbg( const char * psFile, int nLine, node_t * pn, int nType, ... )
{
	set_debug_allocator s( psFile, nLine );

	va_list valist;

	/* va_start starts processing the variable arguments */
	va_start(valist, nType);

	/* pass the variable arguments to node_set_valist */
	node_set_valist(pn, nType, valist);

	/* clean up */
	va_end(valist);

	return pn;
}

/* node_set is a wrapper for the private function node_set_valist */ 
node_t * node_set(node_t * pn, int nType, ...)
{
	va_list valist;

	/* va_start starts processing the variable arguments */
	va_start(valist, nType);

	/* pass the variable arguments to node_set_valist */
	node_set_valist(pn, nType, valist);

	/* clean up */
	va_end(valist);

	return pn;
}	

/*****************
 Reading Functions
 *****************/

/* returns the node's type */
int node_get_type( const node_t * pn )
{
	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return NODE_UNKNOWN;
	}

	return pn->nType;
}

/* returns the int value from the node */
int node_get_int( const node_t *pn)
{
	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return 0;
	}

	/* switch nType */
	switch( pn->nType )
	{
	case NODE_INT:
		return pn->nValue;

	case NODE_INT64:
		return (int)pn->n64Value;

	case NODE_REAL:	
		/* convert the real to int and return that */
		return (int)pn->dfValue;

	case NODE_STRINGA:
		/* convert the string to int and return that */
		return atoi(pn->psAValue);

	case NODE_STRINGW:
		/* convert the string to int and return that */
		return wcstol(pn->psWValue,NULL,10);

	case NODE_LIST:
		/* if the list has at least one element */
		if( node_first(pn) != NULL)
		{
			/* return the int value of the first element */
			return node_get_int(node_first(pn));
		}
		else	/* empty list */
		{
			node_assert( pn->pnListHead != NULL );	/* called node_get_int on empty list node */
		}
		break;
		
	default:
		/* all other types */
		node_assert(!"Node cannot be converted to int");	/* called node_get_int on invalid type */
	}	/* end switch */

	return 0;		/* fail gracefully on invalid input */

}

/* returns the int value from the node */
int64_t node_get_int64( const node_t *pn)
{
	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return 0;
	}

	/* switch nType */
	switch( pn->nType )
	{
	case NODE_INT:
		return pn->nValue;

	case NODE_INT64:
		return pn->n64Value;

	case NODE_REAL:	
		/* convert the real to int and return that */
		return (int64_t)pn->dfValue;

	case NODE_STRINGA:
		/* convert the string to int and return that */
		return _atoi64(pn->psAValue);

	case NODE_STRINGW:
		/* convert the string to int and return that */
		return _wtoi64(pn->psWValue);

	case NODE_LIST:
		/* if the list has at least one element */
		if( node_first(pn) != NULL)
		{
			/* return the int value of the first element */
			return node_get_int(node_first(pn));
		}
		else	/* empty list */
		{
			node_assert( pn->pnListHead != NULL );	/* called node_get_int on empty list node */
		}
		break;
		
	default:
		/* all other types */
		node_assert(!"Node cannot be converted to int");	/* called node_get_int on invalid type */
	}	/* end switch */

	return 0;		/* fail gracefully on invalid input */

}

/* returns the real value from the node */
double node_get_real( const node_t *pn )
{
	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return 0.0;
	}

	/* switch nType */
	switch( pn->nType )
	{
	case NODE_INT:
		/* convert the int to real and return that */
		return (double)pn->nValue;

	case NODE_INT64:
		return (double)pn->n64Value;

	case NODE_REAL:	
		return pn->dfValue;

	case NODE_STRINGA:
		/* convert the string to real and return that */
		return atof( pn->psAValue );

	case NODE_STRINGW:
		/* convert the string to real and return that */
		return wcstod( pn->psWValue, NULL );

	case NODE_LIST:
		/* if the list has at least one element */
		if( node_first(pn) != NULL )
		{
			/* return the real value of the first element */
			return node_get_real( node_first(pn) );
		}
		else	/* empty list */
		{
			node_assert(pn->pnListHead != NULL);	/* called node_get_real on empty list node */
		}
		break;
		
	default:
		/* all other types */
		node_assert(!"Node cannot be converted to real");	/* called node_get_real on invalid type */
	}	/* end switch */

	return 0.0;		/* fail gracefully on invalid input */

}

/* returns the A string value from the node */
NODE_CONSTOUT char * node_get_string_dbgA( const char * psFile, int nLine, node_t * pn )
{
	set_debug_allocator s( psFile, nLine );

	return node_get_stringA( pn );
}

NODE_CONSTOUT char * node_get_stringA( node_t *pn )
{
	char acBuffer[32] = {0};

	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return "";
	}

	switch (pn->nType) 
	{
	case NODE_STRINGA:
		return pn->psAValue;

	case NODE_STRINGW:
		/* convert W to A */
		if( pn->psAValue == NULL )
		{
			pn->psAValue = WToA( pn->pArena, pn->psWValue );
		}

		return pn->psAValue;

	case NODE_INT:
		/* free string value if set */
		if( pn->psAValue != NULL ) 
		{
			nfree( pn->pArena, pn->psAValue );
		}

		/* convert nValue into psValue */
		_itoa( pn->nValue, acBuffer, 10 );
		pn->psAValue = node_safe_copyA( pn->pArena, acBuffer );

		/* return psValue */
		return pn->psAValue;

	case NODE_INT64:
		/* free string value if set */
		if( pn->psAValue != NULL ) 
		{
			nfree( pn->pArena, pn->psAValue );
		}

		/* convert nValue into psValue */
		_i64toa( pn->n64Value, acBuffer, 10 );
		pn->psAValue = node_safe_copyA( pn->pArena, acBuffer );

		/* return psValue */
		return pn->psAValue;

	case NODE_REAL:
		/* free string value if set */
		if( pn->psAValue != NULL ) 
		{
			nfree( pn->pArena, pn->psAValue );
		}

		/* convert nValue into psValue */
		sprintf( acBuffer, "%-16.5f", pn->dfValue );

		pn->psAValue = node_safe_copyA( pn->pArena, acBuffer );

		/* return psValue */
		return pn->psAValue;

	case NODE_LIST:
		/* if has at least one element */
		if( node_first( pn ) != NULL ) 
		{
			/* call node_get_string on first element */
			return node_get_stringA( node_first( pn ) );
		}
		else	/* empty list */
		{
			node_assert(pn->pnListHead != NULL);	/* called node_get_stringA on empty list node */
		}
		break;

	default:	/* invalid */
		node_assert(!"Node cannot be converted to string"); /* invalid node type: fail gracefully by returning empty string */
	}

	return "";
}

/* returns the W string value from the node */
NODE_CONSTOUT wchar_t * node_get_string_dbgW( const char * psFile, int nLine, node_t * pn )
{
	set_debug_allocator s( psFile, nLine );

	return node_get_stringW( pn );
}

NODE_CONSTOUT wchar_t * node_get_stringW(node_t *pn)
{
	wchar_t acBuffer[32] = {0};

	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return L"";
	}

	switch (pn->nType) 
	{
	case NODE_STRINGA:
		if( pn->psWValue == NULL )
			pn->psWValue = AToW( pn->pArena, pn->psAValue );

		return pn->psWValue;

	case NODE_STRINGW:
		return pn->psWValue;

	case NODE_INT:
		/* free string value if set */
		if( pn->psWValue != NULL ) 
		{
			nfree( pn->pArena, pn->psWValue );
		}

		/* convert nValue into psValue */
		_itow( pn->nValue, acBuffer, 10 );
		pn->psWValue = node_safe_copyW( pn->pArena, acBuffer );

		/* return psValue */
		return pn->psWValue;

	case NODE_INT64:
		/* free string value if set */
		if( pn->psWValue != NULL ) 
		{
			nfree( pn->pArena, pn->psWValue );
		}

		/* convert nValue into psValue */
		_i64tow( pn->n64Value, acBuffer, 10 );
		pn->psWValue = node_safe_copyW( pn->pArena, acBuffer );

		/* return psValue */
		return pn->psWValue;

	case NODE_REAL:
		/* free string value if set */
		if( pn->psWValue != NULL ) 
		{
			nfree( pn->pArena, pn->psWValue );
		}

		/* convert nValue into psValue */
		swprintf( acBuffer, DIMENSION(acBuffer), L"%-16.5f", pn->dfValue );
		pn->psWValue = node_safe_copyW( pn->pArena, acBuffer );

		/* return psValue */
		return pn->psWValue;

	case NODE_LIST:
		/* if has at least one element */
		if( node_first( pn ) != NULL ) 
		{
			/* call node_get_string on first element */
			return node_get_stringW( node_first( pn ) );
		}
		else	/* empty list */
		{
			node_assert(pn->pnListHead != NULL);	/* called node_get_stringW on empty list node */
		}
		break;

	default:	/* invalid */
		node_assert(!"Node cannot be converted to string"); /* invalid node type: fail gracefully by returning empty string */
	}

	return L"";
}


/* returns the data value from the node */
NODE_CONSTOUT data_t * node_get_data( const node_t *pn, int * pnLength )
{
	if( pn == NULL )
	{
		node_assert( pn != NULL );
	
		if( pnLength != NULL )
			*pnLength = 0;
		
		return (data_t*)"";
	}

	switch( pn->nType )
	{
	case NODE_DATA:

		/* set pnLength to the length of data stored in pn */
		if( pnLength != NULL )
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
		node_assert(!"Node cannot be converted to data");	/* fail gracefully on invalid types */	
	
		if( pnLength != NULL )
			*pnLength = 0;	/* set the length to 0 */
	}

	return (data_t*)"";
}


/* returns the number of elements in the node (list/hash only) */
int node_get_elements( const node_t * pn )
{
	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return 0;
	}

	switch( pn->nType )
	{
	case NODE_LIST:
		return pn->nListElements;
		break;

	case NODE_HASH:
		return pn->nHashElements;
		break;

	default:
		node_assert( !"Incorrect type for node_get_elements." );
		return 0;
	}
}

/* returns the pointer value from the node */
void * node_get_ptr( const node_t * pn )
{
	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return NULL;
	}

	switch( pn->nType )
	{
	case NODE_PTR:

		/* return the pointer */
		return pn->pvValue;
		
	case NODE_LIST:

		/* if the list has more than one element */
		if( node_first( pn ) != NULL ) 
		{
			/* call node_get_data on first element */
			return node_get_ptr( node_first( pn ) );
		}
		else	/* pn is an empty list */
		{
			/* treat it as if it had an invalid type */
			/*EMPTY*/;
		}
		/*FALLTHROUGH*/
			
	default:

		/* otherwise, since no type is convertible to ptr */
		node_assert(!"Node cannot be converted to pointer");	/* fail gracefully on invalid types */	
	}

	return NULL;
}

/**************
 List Functions
 **************/

/* add a new node to the end of a list; similar variable arguments to node_set */
node_t * node_list_add_dbg( const char * psFile, int nLine, node_t * pnList, int nType, ...)
{
	set_debug_allocator s( psFile, nLine );

	node_t * pnNew = NULL;
	va_list valist;

	/* grab the variable arguments */
	va_start(valist, nType);

	pnNew = node_list_add_valist( pnList, nType, valist );

	/* clean up the variable arguments*/
	va_end(valist);

	return pnNew;
}


node_t * node_list_add(node_t * pnList, int nType, ...)
{
	node_t * pnNew = NULL;
	va_list valist;

	/* grab the variable arguments */
	va_start(valist, nType);

	pnNew = node_list_add_valist( pnList, nType, valist );

	/* clean up the variable arguments*/
	va_end(valist);

	return pnNew;
}

static node_t * node_list_add_valist( node_t * pnList, int nType, va_list valist )
{
	if( pnList == NULL )
	{
		node_assert( pnList != NULL );
		return NULL;
	}

	/* make sure list is initialized */
	node_list_init( pnList );

	node_t * pnNew = node_add_common( pnList->pArena, nType, valist );
	if( pnNew == NULL )
		return NULL;

	node_list_add_internal( pnList, pnNew );

	return pnNew;
}

static void node_list_add_internal( node_t * pnList, node_t * pnNew )
{
	node_assert( pnNew->bInCollection == NOT_IN_COLLECTION );

	/* this node is now in a collection */
	pnNew->bInCollection = IN_COLLECTION;

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
	if( pnList == NULL || pnToDelete == NULL )
	{
		node_assert( pnList != NULL );
		node_assert( pnToDelete != NULL );
		return;
	}

	if( pnList->nType != NODE_LIST )
	{
		node_assert(pnList->nType == NODE_LIST);
		return;
	}

	if( pnList->nListElements <= 0 )
	{
		node_assert( pnList->nListElements > 0 );
		return;
	}

	node_assert( pnToDelete->bInCollection == IN_COLLECTION );

	node_list_delete_internal( pnList, pnToDelete );
}

static void node_list_delete_internal( node_t * pnList, node_t * pnToDelete )
{
	node_t * pnPrevious = NULL;
	node_t * pnScroll = NULL;
	
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

		/* decrease the element count */
		node_assert( pnList->nListElements > 0 );
		pnList->nListElements--;

		/* this node is no longer in a collection */
		pnToDelete->bInCollection = NOT_IN_COLLECTION;

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

			/* decrease the element count */
			node_assert( pnList->nListElements > 0 );
			pnList->nListElements--;

			/* this node is no longer in a collection */
			pnToDelete->bInCollection = NOT_IN_COLLECTION;

			return;
		}
	}

	/* if we got here, then the node was not found in the list! */
	node_assert(!"node_list_delete: node to delete was not found in list");

}

/* Stack functions: treating the list as a stack */
/* pushes a node onto the front of a list */
node_t * node_push_dbg( const char * psFile, int nLine, node_t * pnList, int nType, ... )
{
	set_debug_allocator s(psFile, nLine);

	va_list valist;

	/* grab the variable arguments */
	va_start(valist, nType);

	node_t * pnNew = node_push_valist( pnList, nType, valist );;

	va_end(valist);

	return pnNew;
}

node_t * node_push( node_t * pnList, int nType, ... )
{
	va_list valist;

	/* grab the variable arguments */
	va_start(valist, nType);

	node_t * pnNew = node_push_valist( pnList, nType, valist );;

	va_end(valist);

	return pnNew;
}

static node_t * node_push_valist( node_t * pnList, int nType, va_list valist )
{
	node_t * pnNew = NULL;

	if( pnList == NULL )
	{
		node_assert( pnList != NULL );
		return NULL;
	}

	/* make sure list is initialized */
	node_list_init( pnList );

	pnNew = node_add_common( pnList->pArena, nType, valist );
	if( pnNew == NULL )
		return NULL;

	return node_push_internal( pnList, pnNew );
}

node_t * node_push_internal( node_t * pnList, node_t * pnNew )
{
	/* this node is now in a collection */
	node_assert( pnNew->bInCollection == NOT_IN_COLLECTION );
	pnNew->bInCollection = IN_COLLECTION;

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

	return pnNew;
}

/* pops a node off the front of a list */
node_t * node_pop( node_t * pnList )
{
	if( pnList == NULL )
	{
		node_assert( pnList != NULL );
		return NULL;
	}

	if( pnList->nType != NODE_LIST )
	{
		node_assert( pnList->nType == NODE_LIST );
		return NULL;
	}

	return node_pop_internal( pnList );
}

node_t * node_pop_internal( node_t * pnList )
{
	node_t * pnPopped = NULL;

	/* if the list is empty */
	if( pnList->pnListHead == NULL )
	{
		/* don't assert so pop() can be used to probe for elements */

		/* return NULL */
		return NULL;
	}

	/* get a pointer to the list head */
	pnPopped = node_first( pnList );

	/* delete the node from the list */
	node_list_delete_internal( pnList, pnPopped );

	/* return the old head */
	return pnPopped;
}

/**************
 Hash Functions
 **************/

static node_t * node_add_common( node_arena * pArena, int nType, va_list valist )
{
	node_t * pnElement = NULL;
	node_t * pnNew = NULL;

	switch( nType )
	{

	/* if it's a list, hash or node */
	case NODE_LIST:
	case NODE_HASH:
	case NODE_OLD_COPY:
	case NODE_ADD_COPY:
	case NODE_COPY_DATA:
		
		/* get and check the variable argument */
		pnElement = va_arg( valist, node_t * );

		if( pnElement == NULL ) 
		{
			node_assert( pnElement != NULL );
			node_error( "Attempted to add NULL node to collection.\n" );
			return NULL;
		}

		/* make a deep copy of the node to be added (without its neighbors) */
		pnNew = node_copy_internal( pArena, pnElement );
		break;

	/* if we're supposed to add _this_ node and not a copy */
	/* WARNING: if this is abused, it is possible to create circular data structures 
	 * that cannot be freed-- the node library is not designed to handle this sort of thing */
	case NODE_OLD_REF:
	case NODE_ADD_REF:
	case NODE_REF_DATA:

		/* get and check the variable argument */
		pnElement = va_arg( valist, node_t * );

		if( pnElement == NULL ) 
		{
			node_assert( pnElement != NULL );
			node_error( "Attempted to add NULL node to collection.\n" );
			return NULL;
		}

		pnNew = pnElement;
		if( node_nDebugRef )
		{
			node_assert( pnElement->bInCollection == NOT_IN_COLLECTION );
			if( pnElement->bInCollection != NOT_IN_COLLECTION )
			{
				node_error( "Attempted to add node with NODE_REF when node was in another list/hash - copying!\n" );
				pnNew = node_copy_internal( pArena, pnElement );
			}
		}

		// if node_nDebugArena??
		if( pnNew->pArena != pArena )
		{
			/* error only if heavyweight */
			if( ( pnNew->nType == NODE_LIST || pnNew->nType == NODE_HASH ) && node_get_elements( pnNew ) > 1  )
				node_error( "Attempting to add node with NODE_REF when nodes are from different arenas - copying!\n" );
			pnNew = node_copy_internal( pArena, pnElement );
		}

		break;

	default: /* some scalar type *./
		/* create a new node */
		pnNew = node_alloc_internal( pArena );

		/* call node_set_valist on the input*/
		node_set_valist(pnNew, nType, valist);

		break;
	}

	return pnNew;
}

/* add a node to a hash{} similar variable arguments to node_set */
node_t * node_hash_add_dbgA( const char * psFile, int nLine, node_t * pnHash, const char * psKey, int nType, ...)
{
	set_debug_allocator s(psFile, nLine);

	va_list valist;

	/* grab the variable arguments */
	va_start( valist, nType );

	node_t * pn = node_hash_addA_valist( pnHash, psKey, nType, valist );

	/* clean up after variable argument processing*/
	va_end( valist );

	return pn;
}

node_t * node_hash_addA(node_t * pnHash, const char * psKey, int nType, ...)
{
	va_list valist;

	/* grab the variable arguments */
	va_start( valist, nType );

	node_t * pn = node_hash_addA_valist( pnHash, psKey, nType, valist );

	/* clean up after variable argument processing*/
	va_end( valist );

	return pn;
}

static node_t * node_hash_addA_valist(node_t * pnHash, const char * psKey, int nType, va_list valist )
{
	node_t * pnNew = NULL;

	node_t * pnOld = NULL;

	if( pnHash == NULL || psKey == NULL )
	{
		node_assert( pnHash != NULL );
		node_assert( psKey != NULL );
		return NULL;
	}

	/* make sure hash is initialized */
	node_hash_init( pnHash, DEFAULT_HASHBUCKETS );

	if( node_nDebugUnicode )
	{
		/* check that there are no W keys in this hash */
		if( pnHash->nHashFlags & HASH_CONTAINS_WKEYS )
		{
			node_error( "Attempting to add A key to hash which contains W keys.\n" );
			node_assert( (pnHash->nHashFlags & HASH_CONTAINS_WKEYS) == 0 );
			return NULL;
		}

		/* set the A flag */
		pnHash->nHashFlags |= HASH_CONTAINS_AKEYS;
	}

	pnNew = node_add_common( pnHash->pArena, nType, valist );
	if( pnNew == NULL )
		return NULL;

	/* if the item already exists in the hash */
	pnOld = node_hash_getA_internal( pnHash, psKey );
	if( pnOld != NULL )
	{
		/* delete it */
		node_hash_delete_internal( pnHash, pnOld );

		/* free it */
		node_free_internal( pnOld, NOT_IN_COLLECTION );
	}

	/* set the node name to psKey */
	node_set_nameA_internal( pnNew, psKey );

	node_hash_add_internal( pnHash, pnNew );

	return pnNew;
}

static void node_hash_add_internal( node_t * pnHash, node_t * pnNew )
{
	int nBucket;

	/* this node is now in a collection */
	node_assert( pnNew->bInCollection == NOT_IN_COLLECTION );
	pnNew->bInCollection = IN_COLLECTION;

	/* get a bucket number */
	nBucket = hash_to_bucket( pnHash, pnNew->nHash );

	/* add the element to that bucket */
	pnNew->pnNext = pnHash->ppnHashHeads[nBucket];
	pnHash->ppnHashHeads[nBucket] = pnNew;

	/* increment the number of hash elements */
	pnHash->nHashElements++;
#ifdef _DEBUG
	if( node_nDebugHashPerf )
	{
		int nMaxElts = pnHash->nHashFlags >> 8;

		nMaxElts = __max( nMaxElts, pnHash->nHashElements );

		pnHash->nHashFlags = (pnHash->nHashFlags&0xFF) | (nMaxElts<<8);
	}
#endif

#ifdef HASH_AUTO_RESIZE
	if( NEEDS_REBALANCE( pnHash ) )
	{
		hash_rebalance( pnHash );
	}
#endif

	return;
}

/* add a node to a hash{} similar variable arguments to node_set */
node_t * node_hash_add_dbgW( const char * psFile, int nLine, node_t * pnHash, const wchar_t * psKey, int nType, ...)
{
	set_debug_allocator s(psFile, nLine);

	va_list valist;

	va_start( valist, nType );

	node_t * pnNew = node_hash_addW_valist( pnHash, psKey, nType, valist );

	va_end( valist );

	return pnNew;
}

node_t * node_hash_addW( node_t * pnHash, const wchar_t * psKey, int nType, ... )
{
	va_list valist;

	va_start( valist, nType );

	node_t * pnNew = node_hash_addW_valist( pnHash, psKey, nType, valist );

	va_end( valist );

	return pnNew;
}

static node_t * node_hash_addW_valist( node_t * pnHash, const wchar_t * psKey, int nType, va_list valist )
{
	node_t * pnNew = NULL;

	node_t * pnOld = NULL;

	if( pnHash == NULL || psKey == NULL )
	{
		node_assert( pnHash != NULL );
		node_assert( psKey != NULL );
		return NULL;
	}

	/* make sure hash is initialized */
	node_hash_init( pnHash, DEFAULT_HASHBUCKETS );

	if( node_nDebugUnicode )
	{
		/* check that there are no A keys in this hash */
		if( pnHash->nHashFlags & HASH_CONTAINS_AKEYS )
		{
			node_error( "Attempting to add W key to hash which contains A keys.\n" );
			node_assert( (pnHash->nHashFlags & HASH_CONTAINS_AKEYS) == 0 );
			return NULL;
		}

		/* set the W flag */
		pnHash->nHashFlags |= HASH_CONTAINS_WKEYS;
	}

	pnNew = node_add_common( pnHash->pArena, nType, valist );
	if( pnNew == NULL )
		return NULL;

	/* if the item already exists in the hash */
	pnOld = node_hash_getW_internal( pnHash, psKey );
	if( pnOld != NULL )
	{
		/* delete it */
		node_hash_delete_internal( pnHash, pnOld );

		/* free it */
		node_free_internal( pnOld, NOT_IN_COLLECTION );
	}

	/* set the node name to psKey */
	node_set_nameW_internal( pnNew, psKey );

	node_hash_add_internal( pnHash, pnNew );

	return pnNew;
}

#ifdef HASH_AUTO_RESIZE
static void hash_rebalance( node_t * pnHash )
{
	int nOldBuckets = pnHash->nHashBuckets;
	int nNewBuckets = 0;

	node_t ** ppnOldHeads = pnHash->ppnHashHeads;

	node_t * pn = NULL;

	int i = 0;

	/* create new hash arrays */
	for( pnHash->nHashBuckets*=2; NEEDS_REBALANCE( pnHash ); pnHash->nHashBuckets*=2 )
		/* empty */;

	nNewBuckets = pnHash->nHashBuckets;

	pnHash->ppnHashHeads = reinterpret_cast<node_t **>(node_malloc( pn->pArena, sizeof(node_t *) * nNewBuckets ));
	
	for( i=0; i < nNewBuckets; i++ )
	{
		/* null each element of ppnHashHeads */
		pnHash->ppnHashHeads[i] = NULL;
	}

	/* add all the old elements in */
	for( i = 0; i < nOldBuckets; i++ )
	{
		node_t * pnNext = NULL;
		int nBucket = 0;

		for( pn = ppnOldHeads[i]; pn != NULL; pn = pnNext )
		{
			/* save the next */
			pnNext = pn->pnNext;
			pn->pnNext = NULL;

			/* get a bucket number */
			nBucket = hash_to_bucket( pnHash, pn->nHash );

			/* add the element to that bucket */
			pn->pnNext = pnHash->ppnHashHeads[nBucket];
			pnHash->ppnHashHeads[nBucket] = pn;
		}
	}

#ifdef USE_BAGS
	if( IS_BAG( pnHash, ppnOldHeads ) )
		pnHash->bBagUsed = 0;
	else
#endif
	{
		nfree( pnHash->pArena, ppnOldHeads );
	}
}
#endif

/* get a node (by name) from a hash */
node_t * node_hash_getA( const node_t * pnHash, const char * psKey)
{
	if( pnHash == NULL || psKey == NULL )
	{
		node_assert( pnHash != NULL );
		node_assert( psKey != NULL );
		return NULL;
	}

	/* if nType is not NODE_HASH, assert and return NULL */
	if( pnHash->nType != NODE_HASH )
	{
		node_assert(pnHash->nType == NODE_HASH);	/* tried to get a hash out of a non-hash node! */
		return NULL;
	}

	if( node_nDebugUnicode )
	{
		/* check that there are no W keys in this hash */
		if( pnHash->nHashFlags & HASH_CONTAINS_WKEYS )
		{
			node_error( "Attempting to look for an A key in a hash which contains W keys.\n" );
			node_assert( (pnHash->nHashFlags & HASH_CONTAINS_WKEYS) == 0  );
		}
	}

	return node_hash_getA_internal( pnHash, psKey );
}

static node_t * node_hash_getA_internal( const node_t * pnHash, const char * psKey )
{
	unsigned long nHash;
	int nBucket;
	node_t * pnElement = NULL;

	/* hash psKey */
	nHash = node_hashA( psKey );

	/* get a bucket number */
	nBucket = hash_to_bucket( pnHash, nHash );

	/* search the bucket for value associated with psKey */
	pnElement = pnHash->ppnHashHeads[nBucket];

	while( pnElement != NULL)
	{
		if( nHash == pnElement->nHash && _stricmp( pnElement->psAName, psKey ) == 0 )
		{
			/* if found, return a pointer to the found element */
			return pnElement;
		}
		pnElement = node_next( pnElement );
	}

	/* if not found, return NULL */
	return NULL;
}

/* get a node (by name) from a hash */
node_t * node_hash_getW( const node_t * pnHash, const wchar_t * psKey )
{
	if( pnHash == NULL || psKey == NULL )
	{
		node_assert( pnHash != NULL );
		node_assert( psKey != NULL );
		return NULL;
	}

	/* if nType is not NODE_HASH, assert and return NULL */
	if( pnHash->nType != NODE_HASH )
	{
		node_assert(pnHash->nType == NODE_HASH);	/* tried to get a hash out of a non-hash node! */
		return NULL;
	}

	if( node_nDebugUnicode )
	{
		/* check that there are no A keys in this hash */
		if( pnHash->nHashFlags & HASH_CONTAINS_AKEYS )
		{
			node_error( "Attempting to look for a W key in a hash which contains A keys.\n" );
			node_assert( (pnHash->nHashFlags & HASH_CONTAINS_AKEYS) == 0 );
		}
	}

	return node_hash_getW_internal( pnHash, psKey );
}

static node_t * node_hash_getW_internal( const node_t * pnHash, const wchar_t * psKey )
{
	unsigned long nHash;
	int nBucket;
	node_t * pnElement = NULL;

	/* hash psKey */
	nHash = node_hashW( psKey );

	/* get a bucket number */
	nBucket = hash_to_bucket( pnHash, nHash );

	/* search the bucket for value associated with psKey */
	pnElement = pnHash->ppnHashHeads[nBucket];

	while( pnElement != NULL)
	{
		if( nHash == pnElement->nHash && _wcsicmp( pnElement->psWName, psKey ) == 0 )
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
	if( pnHash == NULL || pnToDelete == NULL )
	{
		node_assert( pnHash != NULL );
		node_assert( pnToDelete != NULL );
		return;
	}

	if( pnHash->nType != NODE_HASH )
	{
		node_assert(pnHash->nType == NODE_HASH);
		return;
	}

	node_hash_delete_internal( pnHash, pnToDelete );
}

static void node_hash_delete_internal( node_t * pnHash, node_t * pnToDelete )
{
	node_t * pnPrevious = NULL;
	node_t * pnScroll = NULL;
	
	unsigned long nBucket = 0;

	/* decrement the count of hash members */
	if( pnHash->nHashElements <= 0 )
	{
		node_assert( pnHash->nHashElements > 0 );
		return;
	}

	node_assert( pnToDelete->bInCollection == IN_COLLECTION );

	/* get the bucket it's in */
	nBucket = hash_to_bucket( pnHash, pnToDelete->nHash );

	/* if nonempty bucket */
	if( pnHash->ppnHashHeads[nBucket] != NULL )
	{
		/* if the node to delete is the first in the bucket */
		if( pnToDelete == pnHash->ppnHashHeads[nBucket] )
		{
			/* cut the head off the list in the bucket*/
			pnHash->ppnHashHeads[nBucket] = node_next( pnToDelete );
			pnToDelete->pnNext = NULL;

			/* this node is no longer in a collection */
			pnToDelete->bInCollection = NOT_IN_COLLECTION;
			pnHash->nHashElements--;

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

				/* this node is no longer in a collection */
				pnToDelete->bInCollection = NOT_IN_COLLECTION;			
				pnHash->nHashElements--;

				return;
			}
		}
	}

	/* if we got here, then the node was not found in the bucket */
	node_assert(!"node_hash_delete: node to delete was not found in list");

}

/**************
 Name Functions
 **************/

/* set the name of a node */
void node_set_name_dbgA( const char * psFile, int nLine, node_t * pn, const char * psName)
{
	set_debug_allocator s(psFile, nLine);
	node_set_nameA( pn, psName );
}

void node_set_nameA(node_t * pn, const char * psName)
{
	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return;
	}

	/* test psName for validity */
	if(psName == NULL)
	{
		node_assert( psName != NULL );
		node_error("Internal error: attempt to set a node name to NULL.\n");
		return;
	}

	node_set_nameA_internal( pn, psName );
}

static void node_set_nameA_internal( node_t * pn, const char * psName )
{
	/* if pn->psName is the same as the passed-in name, we're done */
	if( pn->psAName == psName )
	{
		return;
	}
	
	/* if pn->psName is set, free it */
	if( pn->psAName != NULL )
	{
		nfree( pn->pArena, pn->psAName );
	}
	
	/* copy psName onto pn->psName */
	pn->psAName = node_safe_copyA( pn->pArena, psName );
	
	pn->nHash = node_hashA( psName );
	
	return;
}

/* set the name of a node */
void node_set_name_dbgW( const char * psFile, int nLine, node_t * pn, const wchar_t * psName)
{
	set_debug_allocator s(psFile, nLine);

	node_set_nameW( pn, psName );
}

void node_set_nameW(node_t * pn, const wchar_t * psName)
{
	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return;
	}

	/* test psName for validity */
	if(psName == NULL)
	{
		node_assert( psName != NULL );
		node_error("Internal error: attempt to set a node name to NULL.\n");
		return;
	}

	node_set_nameW_internal( pn, psName );

}

static void node_set_nameW_internal( node_t * pn, const wchar_t * psName )
{
	/* if pn->psName is the same as the passed-in name, we're done */
	if( pn->psWName == psName )
	{
		return;
	}

	/* if pn->psName is set, free it */
	if( pn->psWName != NULL )
	{
		nfree( pn->pArena, pn->psWName );
	}

	/* copy psName onto pn->psName */
	pn->psWName = node_safe_copyW( pn->pArena, psName );

	pn->nHash = node_hashW( psName );

	return;
}

/* get the name of node */
NODE_CONSTOUT char * node_get_nameA( const node_t * pn )
{
	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return NULL;
	}

	return pn->psAName;
}

NODE_CONSTOUT wchar_t * node_get_nameW( const node_t * pn)
{
	if( pn == NULL )
	{
		node_assert( pn != NULL );
		return NULL;
	}

	return pn->psWName;
}

/*****************************
 Dumping and Parsing Functions
 *****************************/

/* dump the node to a file */
void node_dumpA( const node_t * pn, FILE * pfOut, int nOptions )
{
	if( pn == NULL || pfOut == NULL )
	{
		node_assert( pn != NULL );
		node_assert( pfOut != NULL );	
		return;
	}

	struct node_dump d = {0};
	d.pfOut = pfOut;
	d.nOptions = nOptions;
	d.nSpaces = 0;

	node_dumpA_internal( pn, &d );

	fflush( pfOut );
}

static void node_dumpA_internal( const node_t * pn, struct node_dump * pd )
{
	node_t * pnElt;
	float fTemp;
	char * psEscaped = NULL;

	data_t * pbBase = NULL;
	size_t nLength = 0;

	int nOptions = pd->nOptions;
	int nSpaces = pd->nSpaces;
	FILE * pfOut = pd->pfOut;
	node_arena * pArena = pn->pArena;

	/* write spaces */
	node_write_spacesA( pfOut, nSpaces );

	/* write the name (if any) */
	if( pn->psWName != NULL && pn->psAName == NULL )
	{
		char *psA = WToA( pArena, pn->psWName );
		psEscaped = node_escapeA( pArena, psA );
		nfree( pArena, psA );

		fputs( psEscaped, pfOut );
		nfree( pArena, psEscaped );
		psEscaped = NULL;
	}
	else if( pn->psAName != NULL )
	{
		psEscaped = node_escapeA( pArena, pn->psAName );
		fputs( psEscaped, pfOut );
		nfree( pArena, psEscaped );
		psEscaped = NULL;
	}

	/* write ':' */
	fputs( ": ",  pfOut );
	
	switch( pn->nType )
	{
	case NODE_INT:
		fprintf( pfOut, "%d  (0x%08X)\r\n", pn->nValue, pn->nValue );
		break;

	case NODE_INT64:
		fprintf( pfOut, "%I64dL  (0x%016I64X)\r\n", pn->n64Value, pn->n64Value );
		break;

	case NODE_REAL:
		fTemp = (float)pn->dfValue;
		fprintf( pfOut, "%f  (0x%08X)\r\n", pn->dfValue, *((int*)&fTemp) );
		break;

	case NODE_STRINGW:
		/* TODO: maybe warn if data lost through conversion */
		{
			char * psA = NULL;
			
			if( pn->psAValue == NULL )
			{	
				psA = WToA( pArena, pn->psWValue );
			}
			else
			{
				psA = pn->psAValue;
			}

			if( nOptions & DO_NOESCAPE )
			{
				fprintf( pfOut, "\"%s\"\r\n", psA );
			}
			else
			{
				psEscaped = node_escapeA( pArena, psA );
				fprintf( pfOut, "'%s'\r\n", psEscaped );
				nfree( pn->pArena, psEscaped );
			}
			
			if( pn->psAValue == NULL )
			{	
				nfree( pArena, psA );
			}
		}
		break;

	case NODE_STRINGA:
		if( nOptions & DO_NOESCAPE )
		{
			fprintf( pfOut, "\"%s\"\r\n", pn->psAValue );
		}
		else
		{
			psEscaped = node_escapeA( pArena, pn->psAValue );
			fprintf( pfOut, "'%s'\r\n", psEscaped );
			nfree( pArena, psEscaped );
		}
		break;

	case NODE_PTR:
		/* dump out the pointer value */
		fprintf( pfOut, "PTR 0x%p\r\n", pn->pvValue );
		break;

	case NODE_DATA:
		/* DATA: write 'DATA ', the data length, and a newline, 
		   then write a hex dump of the data like this:
		   $ 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 ................ */
		fprintf(pfOut, "DATA %d\r\n", pn->nDataLength );
		pbBase = pn->pbValue;
		nLength = pn->nDataLength;
		
		/* 16 bytes per line AND remainder */
		while( nLength > 0 )
		{
			char acBuffer[80];
			char * ps = acBuffer;
			size_t l;
			data_t * pb;
			
			size_t nChunk = __min( 16, nLength );

			/* a dollar for every row... */
			*ps++ = '$';

			pb = pbBase;
			size_t nSmallChunk = __min( 8, nChunk );
			switch( nSmallChunk )
			{
			case  8: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  7: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  6: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  5: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  4: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  3: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  2: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  1: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			}
			l = 3 * (8-nSmallChunk) + 1;
			memset( ps, ' ', l );
			ps += l;

			nSmallChunk = nChunk - nSmallChunk;
			switch( nSmallChunk )
			{
			case  8: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  7: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  6: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  5: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  4: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  3: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  2: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			case  1: *ps++ = ' '; byte_to_strA( ps, *pb++ );
			}

			/* add the spaces */
			l = 3 * (8-nSmallChunk);
			memset( ps, ' ', l );
			ps += l;

			/* add some spaces between the hex and character info */
			*ps++ = ' '; *ps++ = ' '; *ps++ = ' ';

			/* now output the character values for the row */
			pb = pbBase;
			switch( nChunk )
			{
			case 16: *ps++ = (char)printable(*pb++);
			case 15: *ps++ = (char)printable(*pb++);
			case 14: *ps++ = (char)printable(*pb++);
			case 13: *ps++ = (char)printable(*pb++);
			case 12: *ps++ = (char)printable(*pb++);
			case 11: *ps++ = (char)printable(*pb++);
			case 10: *ps++ = (char)printable(*pb++);
			case  9: *ps++ = (char)printable(*pb++);
			case  8: *ps++ = (char)printable(*pb++);
			case  7: *ps++ = (char)printable(*pb++);
			case  6: *ps++ = (char)printable(*pb++);
			case  5: *ps++ = (char)printable(*pb++);
			case  4: *ps++ = (char)printable(*pb++);
			case  3: *ps++ = (char)printable(*pb++);
			case  2: *ps++ = (char)printable(*pb++);
			case  1: *ps++ = (char)printable(*pb++);
			}

			l = 16-nChunk;
			memset( ps, ' ', l );
			ps += l;
			
			*ps++ = '\r'; 
			*ps++ ='\n';
			fwrite( acBuffer, ps-acBuffer, sizeof(*ps), pfOut );

			nLength -= nChunk;
			pbBase += nChunk;
		}

		break;

	case NODE_LIST:

		fputs( "(\r\n", pfOut );

		/* increase the indentation */
		pd->nSpaces += 2;

		/* for each element in the list, call node_dump */
		for(pnElt = node_first(pn); pnElt != NULL; pnElt = node_next(pnElt))
		{
			node_dumpA_internal( pnElt, pd );
		}

		/* restore the previous level of indentation */
		pd->nSpaces -= 2;

		node_write_spacesA( pfOut, nSpaces );

		fputs( ")\r\n", pfOut );
		break;

	case NODE_HASH:

		/* HASH: write '{' and newline */
		fputs( "{\r\n", pfOut );

		/* increase the indentation */
		pd->nSpaces += 2;
		
		/* for each element in the hash, call node_dump */
		if(pn->ppnHashHeads != NULL)
		{
			/* loop for 1..nHashBuckets */
			for(int i=0; i < pn->nHashBuckets; i++)
			{
				for(pnElt = pn->ppnHashHeads[i];pnElt != NULL; pnElt = node_next(pnElt) )
				{
					/* call node_dump on each element of ppnHashHeads */
					node_dumpA_internal( pnElt, pd );
				}

			}
		} /* if pn->ppnHashHeads != NULL */

		/* restore the previous level of indentation */
		pd->nSpaces -= 2;

		node_write_spacesA( pfOut, nSpaces );

		fputs( "}\r\n", pfOut );
		break;

	/* everything else */
	default:
		node_assert(!"Node type unknown in node_dump");	/* tried to dump node of unknown or invalid type */
	}
	
	return;
}

static inline void node_write_spacesA( FILE * pfOut, int nSpaces )
{
	int nChunk = 0;
	for( int n = nSpaces; n > 0; n -= nChunk )
	{
		nChunk = __min( n, 16 );
		fwrite( node_acSpaces, nChunk, 1, pfOut );
	}
}

/* dump the node to a file */
void node_dumpW( const node_t * pn, FILE * pfOut, int nOptions )
{
	if( pn == NULL || pfOut == NULL )
	{
		node_assert( pn != NULL );
		node_assert( pfOut != NULL );
		return;
	}

	struct node_dump d = {0};
	d.pfOut = pfOut;
	d.nOptions = nOptions;
	d.nSpaces = 0;

	node_dumpW_internal( pn, &d );

	fflush( pfOut );
}


static void node_dumpW_internal( const node_t * pn, struct node_dump * pd )
{
	int i;
	node_t * pnElt;
	float fTemp;
	wchar_t * psEscaped = NULL;

	data_t * pbBase;
	int nLength;

	FILE * pfOut = pd->pfOut;
	int nSpaces = pd->nSpaces;
	int nOptions = pd->nOptions;
	node_arena * pArena = pn->pArena;

	/* write node_nSpaces spaces */
	node_write_spacesW( pfOut, nSpaces );

	/* write the name (if any) */
	if( pn->psAName != NULL && pn->psWName == NULL )
	{
		wchar_t * psW = AToW( pArena, pn->psAName );
		psEscaped = node_escapeW( pArena, psW );
		nfree( pArena, psW );

		fputws( psEscaped, pfOut );
		nfree( pArena, psEscaped );
		psEscaped = NULL;

	}
	else if( pn->psWName != NULL)
	{
		psEscaped = node_escapeW( pArena, pn->psWName );
		fputws( psEscaped, pfOut );
		nfree( pArena,  psEscaped );
		psEscaped = NULL;
	}

	/* write ':' */
	fputws( L": ", pfOut );
	
	switch( pn->nType )
	{
	case NODE_INT:
		fwprintf( pfOut, L"%d  (0x%08X)\r\n", pn->nValue, pn->nValue );
		break;

	case NODE_INT64:
		fwprintf( pfOut, L"%I64dL  (0x%016I64X)\r\n", pn->n64Value, pn->n64Value );
		break;

	case NODE_REAL:
		fTemp = (float)pn->dfValue;
		fwprintf( pfOut, L"%f  (0x%08X)\r\n", pn->dfValue, *((int*)&fTemp) );
		break;

	case NODE_STRINGA:
		{
			wchar_t * psW = NULL;

			if( pn->psWValue == NULL )
				psW = AToW( pArena, pn->psAValue );
			else
				psW = pn->psWValue;

			if( nOptions & DO_NOESCAPE )
			{
				fwprintf( pfOut, L"\"%s\"\r\n", psW );
			}
			else
			{
				psEscaped = node_escapeW( pArena, psW );
				fwprintf( pfOut, L"'%s'\r\n", psEscaped );
				nfree( pArena, psEscaped );
			}

			if( pn->psWValue == NULL )
				nfree( pArena, psW );
		}
		break;

	case NODE_STRINGW:
		if( nOptions & DO_NOESCAPE )
		{
			fwprintf( pfOut, L"\"%s\"\r\n", pn->psWValue );
		}
		else
		{
			psEscaped = node_escapeW( pArena, pn->psWValue );
			fwprintf( pfOut, L"'%s'\r\n", psEscaped );
			nfree( pArena, psEscaped );
		}
		break;

	case NODE_PTR:
		/* dump out the pointer value */
		fwprintf( pfOut, L"PTR 0x%p\r\n", pn->pvValue );
		break;

	case NODE_DATA:
		/* DATA: write 'DATA ', the data length, and a newline, 
		   then write a hex dump of the data like this:
		   $ 00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00 ................ */
		fwprintf(pfOut, L"DATA %d\r\n", pn->nDataLength );
		
		pbBase = pn->pbValue;
		nLength = pn->nDataLength;
		
		/* 16 bytes per line AND remainder */
		while( nLength > 0 )
		{
			wchar_t acBuffer[80];
			wchar_t * ps = acBuffer;
			int l, i;
			data_t * pb;
			
			int nChunk = __min( 16, nLength );

			/* a dollar for every row... */
			*ps++ = '$';

			pb = pbBase;
			int nSmallChunk = __min( 8, nChunk );
			switch( nSmallChunk )
			{
			case  8: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  7: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  6: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  5: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  4: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  3: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  2: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  1: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			}
			l = 3 * (8-nSmallChunk) + 1;
			for( i = 0; i <l; i++ )
				*ps++ = L' ';

			nSmallChunk = nChunk - nSmallChunk;
			switch( nSmallChunk )
			{
			case  8: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  7: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  6: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  5: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  4: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  3: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  2: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			case  1: *ps++ = ' '; byte_to_strW( ps, *pb++ );
			}

			/* add the spaces */
			l = 3 * (8-nSmallChunk);
			for( i = 0; i <l; i++ )
				*ps++ = L' ';

			/* add some spaces between the hex and character info */
			*ps++ = ' '; *ps++ = ' '; *ps++ = ' ';

			/* now output the character values for the row */
			pb = pbBase;
			switch( nChunk )
			{
			case 16: *ps++ = (wchar_t)printable(*pb++);
			case 15: *ps++ = (wchar_t)printable(*pb++);
			case 14: *ps++ = (wchar_t)printable(*pb++);
			case 13: *ps++ = (wchar_t)printable(*pb++);
			case 12: *ps++ = (wchar_t)printable(*pb++);
			case 11: *ps++ = (wchar_t)printable(*pb++);
			case 10: *ps++ = (wchar_t)printable(*pb++);
			case  9: *ps++ = (wchar_t)printable(*pb++);
			case  8: *ps++ = (wchar_t)printable(*pb++);
			case  7: *ps++ = (wchar_t)printable(*pb++);
			case  6: *ps++ = (wchar_t)printable(*pb++);
			case  5: *ps++ = (wchar_t)printable(*pb++);
			case  4: *ps++ = (wchar_t)printable(*pb++);
			case  3: *ps++ = (wchar_t)printable(*pb++);
			case  2: *ps++ = (wchar_t)printable(*pb++);
			case  1: *ps++ = (wchar_t)printable(*pb++);
			}

			l = 16-nChunk;
			for( i = 0; i <l; i++ )
				*ps++ = L' ';
			
			*ps++ = '\r'; 
			*ps++ ='\n';
			fwrite( acBuffer, ps-acBuffer, sizeof(*ps), pfOut );

			nLength -= nChunk;
			pbBase += nChunk;
		}

		break;

	case NODE_LIST:

		fputws( L"(\r\n", pfOut );

		/* increase the indentation */
		pd->nSpaces += 2;

		/* for each element in the list, call node_dump */
		for(pnElt = node_first(pn); pnElt != NULL; pnElt = node_next(pnElt))
		{
			node_dumpW_internal( pnElt, pd );
		}

		/* restore the previous level of indentation */
		pd->nSpaces -= 2;

		node_write_spacesW( pfOut, nSpaces );

		fputws( L")\r\n", pfOut );
		break;

	case NODE_HASH:

		/* HASH: write '{' and newline */
		fputws( L"{\r\n", pfOut );

		/* increase the indentation */
		pd->nSpaces += 2;
		
		/* for each element in the hash, call node_dump */
		if(pn->ppnHashHeads != NULL)
		{
			/* loop for 1..nHashBuckets */
			for(i=0; i < pn->nHashBuckets; i++)
			{
				for(pnElt = pn->ppnHashHeads[i];pnElt != NULL; pnElt = node_next(pnElt) )
				{
					/* call node_dump on each element of ppnHashHeads */
					node_dumpW_internal( pnElt, pd );
				}

			}
		} /* if pn->ppnHashHeads != NULL */

		/* restore the previous level of indentation */
		pd->nSpaces -= 2;

		node_write_spacesW( pfOut, nSpaces );

		fputws( L"}\r\n", pfOut );
		break;

	/* everything else */
	default:
		node_assert(!"Node type unknown in node_dump");	/* tried to dump node of unknown or invalid type */
	}
	
	return;
}

static inline void node_write_spacesW( FILE * pfOut, int nSpaces )
{
	int nChunk = 0;
	for( int n = nSpaces; n > 0; n -= nChunk )
	{
		nChunk = __min( n, 16 );
		fwrite( node_wcSpaces, nChunk, sizeof(wchar_t), pfOut );
	}
}


/*********************************************
 Node Parsing Helper Classes - Implementations
 *********************************************/

/* get A or W lines from a file */
class NodeFileReader : public NodeReader
{
	FILE * m_pfIn;
public:
	NodeFileReader( FILE * pfIn, node_arena * pArena ) : NodeReader(pArena), m_pfIn(pfIn)  {}
	void read_lineA( const char ** psStart, const char ** ppsEnd ) 
	{ 
		char * psEnd;
		char * psLine = ::read_lineA( m_pArena, m_pfIn, &psEnd ); 
		*psStart = psLine;
		*ppsEnd = psEnd;
	}
	void read_lineW( const wchar_t ** psStart, const wchar_t ** ppsEnd ) 
	{ 
		wchar_t * psEnd;
		wchar_t * psLine = ::read_lineW( m_pArena, m_pfIn, &psEnd ); 
		*psStart = psLine; 
		*ppsEnd = psEnd; 
	}
	void free_line( const void * psLine ) { nfree( m_pArena, (void*)psLine ); }
};

/* get A lines from an A string */
class NodeStringAReader : public NodeReader
{
	const char * m_psAOrig;
	const char * m_psA;
	const char * m_psAEnd;
public:
	/* construct from zero-terminated string: copy */
	NodeStringAReader( struct node_arena * p, const char * psA ) : ::NodeReader(p) { m_psAOrig = m_psA = psA; m_psAEnd = m_psAOrig + strlen(m_psAOrig); }
	NodeStringAReader( struct node_arena * p, const char * psA, size_t nChars ) : ::NodeReader(p) 
	{
		m_psAOrig = m_psA = psA;
		m_psAEnd = m_psA + nChars;
	}
	~NodeStringAReader() { }

	void read_lineA( const char ** ppsStart, const char ** ppsEnd ) 
	{ 
		const char * psStart = NULL;
		const char * psEnd = NULL;
		
		if( m_psA < m_psAEnd )
		{
			psStart = m_psA;

			for( psEnd = psStart; psEnd < m_psAEnd; ++psEnd )
				if( *psEnd == '\n' )
					break;
			m_psA = psEnd+1;
		}

		*ppsStart = psStart;
		*ppsEnd = psEnd;
	}
	void read_lineW( const wchar_t ** ppsStart, const wchar_t ** ppsEnd ) { node_assert(!"NodeStringAReader::read_lineW() is not defined."); *ppsStart = *ppsEnd = NULL; }

	/* free_line is a no-op because the line was not independently allocated */
	void free_line( const void * psLine ) { psLine = psLine; }
};

/* get W lines from a W string */
class NodeStringWReader : public NodeReader
{
	const wchar_t * m_psWOrig;
	const wchar_t * m_psW;
	const wchar_t * m_psWEnd;
public:
	/* construct from zero-terminated string: copy */
	NodeStringWReader( struct node_arena * p, const wchar_t * psW ) : ::NodeReader(p) { m_psWOrig = m_psW = psW; m_psWEnd = m_psW + wcslen( m_psW ); }
	NodeStringWReader( struct node_arena * p, const wchar_t * psW, size_t nChars ) : ::NodeReader(p)
	{ 
		m_psWOrig = m_psW = psW;
		m_psWEnd = m_psW + nChars;
	}
	~NodeStringWReader() {  }

	void read_lineW( const wchar_t ** ppsStart, const wchar_t ** ppsEnd ) 
	{ 
		const wchar_t * psStart = NULL;
		const wchar_t * psEnd = NULL;
		
		if( m_psW < m_psWEnd )
		{
			psStart = m_psW;

			for( psEnd = psStart; psEnd < m_psWEnd; ++psEnd )
				if( *psEnd == '\n' )
					break;
				
			m_psW = psEnd+1;
		}

		*ppsStart = psStart;
		*ppsEnd = psEnd;
	}
	void read_lineA( const char **, const char ** ) { node_assert(!"NodeStringWReader:read_lineA() is not defined."); }

	/* free_line is a no-op because the line was not independently allocated */
	void free_line( const void * psLine ) { psLine = psLine; }
};


/***************************
 Parsing Interface Functions
 ***************************/

int node_parseA( FILE * pfIn, node_t ** ppn )
{
	if( pfIn == NULL || ppn == NULL )
		return NP_INVALID;

	return node_parse_internal( pfIn, ppn, NODE_A );
}

int node_parse_dbgA( const char * psFile, int nLine, FILE * pfIn, node_t ** ppn )
{
	set_debug_allocator s(psFile, nLine);

	if( pfIn == NULL || ppn == NULL )
		return NP_INVALID;

	return node_parse_internal( pfIn, ppn, NODE_A );
}

int node_parseW( FILE * pfIn, node_t ** ppn )
{
	if( pfIn == NULL || ppn == NULL )
		return NP_INVALID;

	return node_parse_internal( pfIn, ppn, NODE_W );
}

int node_parse_dbgW( const char * psFile, int nLine, FILE * pfIn, node_t ** ppn )
{
	set_debug_allocator s(psFile, nLine);

	if( pfIn == NULL || ppn == NULL )
		return NP_INVALID;

	return node_parse_internal( pfIn, ppn, NODE_W );
}

int node_parse_from_stringA( const char * ps, node_t ** ppn )
{
	if( ps == NULL || ppn == NULL )
		return NP_INVALID;

	NodeStringAReader nr( node_pArena, ps );
	return node_parse_internalA( &nr, ppn, NODE_A );
}

int node_parse_from_string_dbgA( const char * psFile, int nLine, const char * ps, node_t ** ppn )
{
	set_debug_allocator s(psFile, nLine);

	if( ps == NULL || ppn == NULL )
		return NP_INVALID;

	NodeStringAReader nr( node_pArena, ps );
	return node_parse_internalA( &nr, ppn, NODE_A );
}

int node_parse_from_stringW( const wchar_t * ps, node_t ** ppn )
{
	if( ps == NULL || ppn == NULL )
		return NP_INVALID;

	NodeStringWReader nr( node_pArena, ps );
	return node_parse_internalW( &nr, ppn, NODE_W );
}

int node_parse_from_string_dbgW( const char * psFile, int nLine, const wchar_t * ps, node_t ** ppn )
{
	set_debug_allocator s(psFile, nLine);

	if( ps == NULL || ppn == NULL )
		return NP_INVALID;

	NodeStringWReader nr( node_pArena, ps);
	return node_parse_internalW( &nr, ppn, NODE_W );
}

static int node_parse_from_data_internal( const void * pv, size_t nBytes, node_t ** ppn, int nType )
{
	int nLength = 128;
	if( nBytes < 128 )
		nLength = (int)nBytes;

	int iFlags = 0xFFDD;
	int bUnicode = IsTextUnicode( pv, nLength, &iFlags );

	if( bUnicode )
	{
		NodeStringWReader nr( node_pArena, (wchar_t *)pv, nBytes/2 );
		return node_parse_internalW( &nr, ppn, nType );
	}
	else
	{
		NodeStringAReader nr( node_pArena, (char *)pv, nBytes );
		return node_parse_internalA( &nr, ppn, nType );
	}
}

int node_parse_from_dataA( const void * pv, size_t nBytes, node_t ** ppn )
{
	if( pv == NULL || ppn == NULL )
		return NP_INVALID;

	return node_parse_from_data_internal( pv, nBytes, ppn, NODE_A );
}

int node_parse_from_data_dbgA( const char * psFile, int nLine, const void * pv, size_t nBytes, node_t ** ppn )
{
	set_debug_allocator s(psFile, nLine);

	if( pv == NULL || ppn == NULL )
		return NP_INVALID;

	return node_parse_from_data_internal( pv, nBytes, ppn, NODE_A );
}

int node_parse_from_dataW( const void * pv, size_t nBytes, node_t ** ppn )
{
	if( pv == NULL || ppn == NULL )
		return NP_INVALID;

	return node_parse_from_data_internal( pv, nBytes, ppn, NODE_W );
}

int node_parse_from_data_dbgW( const char * psFile, int nLine, const void * pv, size_t nBytes, node_t ** ppn )
{
	set_debug_allocator s(psFile, nLine);

	if( pv == NULL || ppn == NULL )
		return NP_INVALID;

	return node_parse_from_data_internal( pv, nBytes, ppn, NODE_W );
}

static int node_parse_internal( FILE * pfIn, node_t ** ppn, int nOutputStyle )
{
	/* save the file position */
	fpos_t fpt = {0};
	fgetpos( pfIn, &fpt );

	/* read a block from pfIn */
	data_t abData[512] = {0};
	size_t nBytes = fread( abData, sizeof(data_t), sizeof(abData), pfIn );

	/* evaluate its type */
	int iFlags = 0xFFDD;
	int nResult = IsTextUnicode( abData, (int)nBytes, &iFlags );

	/* restore the file position */
	fsetpos( pfIn, &fpt );

	NodeFileReader nr( pfIn, node_pArena );

	/* if the file is Unicode */
	if( nResult > 0 )
		return node_parse_internalW( &nr, ppn, nOutputStyle );
	else
		return node_parse_internalA( &nr, ppn, nOutputStyle );
}

template <class T> static const T * unterminated_strchr( const T * psStart, const T * psEnd, const int nChar )
{
	const T * psChar;

	for( psChar = psStart; psChar < psEnd; ++psChar )
		if( *psChar == nChar )
			return psChar;

	return NULL;
}

template <class T> static const T * unterminated_strrchr( const T * psStart, const T * psEnd, const int nChar )
{
	const T * psChar;

	for( psChar = psEnd-1; psChar >= psStart; --psChar )
		if( *psChar == nChar )
			return psChar;

	return NULL;
}

template <class T> class TempSZ
{
	node_arena * pArena;
	T m_ac[32];
	T* m_ps;

public:
	TempSZ( node_arena * p, const T * psStart, const T * psEnd ) : pArena(p), m_ps(0)
	{
		size_t nLength = psEnd-psStart;
		T* ps = NULL;
		
		if( nLength < sizeof( m_ac ) / sizeof(T) )
		{
			ps = m_ac;
		}
		else
		{
			ps = m_ps = (T*)node_malloc( pArena, (nLength + 1)*sizeof(T) );
		}

		memcpy( ps, psStart, nLength * sizeof(T) );
		ps[ nLength ] = 0;
	}
	~TempSZ()
	{
		if( m_ps )
			nfree( pArena, m_ps );
	}

	operator const T *()
	{
		return m_ps ? m_ps : m_ac;
	}
};

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
static int node_parse_internalA( NodeReader * pnr, node_t ** ppn, int nOutputStyle )
{
	const char * psLine = NULL;
	const char * psEnd = NULL;

	const char * psPos = NULL;

	const char * psColon = NULL;
	char * psName = NULL;
	char * psUnescaped = NULL;

	node_t * pn = NULL;
	node_t * pnChild = NULL;
	const char * psType = NULL;

	const char * psTrailingQuote = NULL;

	int nDataLength = 0;
	data_t * pb = NULL;
	int nRows = 0;
	int i = 0;
	int b = 0;

	int nResult = 0;
	node_arena * pArena = pnr->m_pArena;

	/* initialize the returned node */
	*ppn = NULL;

	/* read a line */
READ_LINE:
	pnr->read_lineA( &psLine, &psEnd );

	/* if it fails */
	if( psLine == NULL )
		return NP_EOF;

	/* skip initial white-space */
	for( psPos = psLine; psPos < psEnd; psPos++ )
	{
		if( !isspace( static_cast<unsigned char>( *psPos ) ) )
			break;
	}

	/* what kind of line is this? */

	/* if it's blank -- read another line */
	if( psPos >= psEnd )
	{
		pnr->free_line( psLine );
		goto READ_LINE;
	}

	/* if it's a close paren */
	if( *psPos == ')' )
	{
		pnr->free_line( psLine );
		return NP_CPAREN;
	}

	/* if it's a close brace */
	if( *psPos == '}' )
	{
		pnr->free_line( psLine );
		return NP_CBRACE;
	}

	/* it's a normal node */

	/* unescape the name */
	psColon = unterminated_strchr<char>( psPos, psEnd, ':' );

	if( psColon == NULL )
	{
		node_error( "No colon on line.\n" );
		goto PARSE_ERROR;
	}

	if( psColon != psPos )
	{
		psName = node_unescapeA( pArena, psPos, psColon );
	}

	/* figure out the node type */
	for( psType = psColon+1; psType < psEnd; psType++ )
		if( !isspace( *psType ) )
			break;

	/* allocate a new node */
	pn = node_alloc_internal( pArena );
	if( psName != NULL )
	{
		if( nOutputStyle == NODE_A )
		{
			node_set_nameA_internal( pn, psName );
		}
		else
		{
			wchar_t * psW = AToW( pArena, psName );
			node_set_nameW_internal( pn, psW );
			nfree( pArena, psW );
		}

		nfree( pArena, psName );
	}

	/* switch the node type */
	switch( *psType )
	{
	case '.': case '0': case '1': case '2': case '3': case '4':
	case '-': case '5': case '6': case '7': case '8': case '9':
		{
			TempSZ<char> psValue( pArena, psType, psEnd );

			if( strchr( psValue, '.' ) != NULL )
				node_set_real( pn, strtod( psValue, NULL ) );
			else if( strchr( psValue, 'L' ) != NULL )
				node_set_int64( pn, _atoi64( psValue ) );
			else
				node_set_int( pn, atoi( psValue ) );
		}
		break;

	case '"':
		/* remove trailing quote */
		psTrailingQuote = unterminated_strrchr<char>( psType, psEnd, '"' );
		if( psTrailingQuote == NULL || psTrailingQuote <= psType )
		{
			node_error( "Unterminated string.\n" );
			goto PARSE_ERROR;
		}

		if( nOutputStyle == NODE_A )
			node_set_stringA_internal( pn, psType+1, psTrailingQuote );
		else
		{
			wchar_t * psW = AToW( pArena, psType+1, psTrailingQuote );
			node_set_stringW_internal( pn, psW );
			nfree( pArena, psW );
		}

		break;

	case '\'':
		/* remove trailing quote */
		psTrailingQuote = unterminated_strrchr<char>( psType, psEnd, '\'' );
		if( psTrailingQuote == NULL || psTrailingQuote <= psType )
		{
			node_error( "Unterminated string.\n" );
			goto PARSE_ERROR;
		}

		psUnescaped = node_unescapeA( pArena, psType+1, psTrailingQuote );

		if( nOutputStyle == NODE_A )
			node_set_stringA_internal( pn, psUnescaped );
		else
		{
			wchar_t * psW = AToW( pArena, psUnescaped );
			node_set_stringW_internal( pn, psW );
			nfree( pArena, psW );
		}

		nfree( pArena, psUnescaped );
		break;

	case 'P':
		/* handle reading in pointer value: use NULL */
		node_set_ptr( pn, NULL );
		break;
	
	case 'D':
		nDataLength = atoi( psType + 4 );
		pb = (data_t *)node_malloc( pArena, nDataLength );

		/* loop over the rows of the binary data */
		for( nRows = 0; nRows < (nDataLength+15)/16; nRows++ )
		{
			/* read a new line */
			const char * psData = NULL;;
			const char * psDataEnd = NULL;
			
			pnr->read_lineA( &psData, &psDataEnd );
			const char * psCursor = psData+1;

			for( i = nRows*16; i < nDataLength && i < (nRows+1)*16; i++ )
			{
				/* skip whitespace */
				psCursor += strspn( psCursor, " " );

				/* convert the hex data to a byte */
				b = strtoul( psCursor, NULL, 16 );
				psCursor += 2;

				/* set the data */
				pb[i] = (data_t)b;
			}
			pnr->free_line( psData );
		}

		node_set_data( pn, nDataLength, pb );
		nfree( pArena, pb );
		break;

	case '(':
		node_list_init( pn );
		while( (nResult = node_parse_internalA( pnr, &pnChild, nOutputStyle ) ) == NP_NODE )
		{
			node_list_add_internal( pn, pnChild );
		}

		if( nResult != NP_CPAREN )
		{
			node_error( "No close paren for list.\n" );
			goto PARSE_ERROR;
		}

		break;
	case '{':
		{
			node_t * pnList = node_list_alloc();
			while( (nResult = node_parse_internalA( pnr, &pnChild, nOutputStyle ) ) == NP_NODE )
			{
				if( nOutputStyle == NODE_A )
				{
					if( pnChild->psAName != NULL )
						node_push_internal( pnList, pnChild );
					else
					{
						node_free( pnChild );
						node_error( "Child of hash has no name -- skipping.\n" );
					}
				}
				else
				{
					if( pnChild->psWName != NULL )
						node_push_internal( pnList, pnChild );
					else
					{
						node_free( pnChild );
						node_error( "Child of hash has no name -- skipping.\n" );
					}
				}
			}

			if( nResult != NP_CBRACE )
			{
				node_error( "No close brace for hash.\n" );
				node_free( pnList );
				goto PARSE_ERROR;
			}

			node_hash_init( pn, __max( DEFAULT_HASHBUCKETS, pnList->nListElements>>3 )  );

			while( pnList->nListElements != 0 )
				node_hash_add_internal( pn, node_pop_internal( pnList ) );

			node_free( pnList );

		}
		break;
	}

	pnr->free_line( psLine );
	*ppn = pn;
	return NP_NODE;

PARSE_ERROR:
	if( pn != NULL )
		node_free_internal( pn, NOT_IN_COLLECTION );
	pnr->free_line( psLine );
	
	return NP_SERROR;
}

static int node_parse_internalW( NodeReader *pnr, node_t ** ppn, int nOutputStyle )
{
	const wchar_t * psLine = NULL;
	const wchar_t * psEnd = NULL;
	const wchar_t * psPos = NULL;

	const wchar_t * psColon = NULL;
	wchar_t * psName = NULL;
	wchar_t * psUnescaped = NULL;

	node_t * pn = NULL;
	node_t * pnChild = NULL;
	const wchar_t * psType = NULL;

	const wchar_t * psTrailingQuote = NULL;

	int nDataLength = 0;
	data_t * pb = NULL;
	int nRows = 0;
	int i = 0;
	const wchar_t * psData = NULL;
	const wchar_t * psCursor = NULL;
	int b = 0;

	node_arena * pArena = pnr->m_pArena;

	int nResult = 0;

	/* initialize the returned node */
	*ppn = NULL;

	/* read a line */
READ_LINE:
	pnr->read_lineW( &psLine, &psEnd );

	/* if it fails */
	if( psLine == NULL )
		return NP_EOF;

	/* skip initial white-space */
	for( psPos = psLine; psPos < psEnd; ++psPos )
		if( !iswspace( *psPos ) )
			break;

	/* what kind of line is this? */

	/* if it's blank -- read another line */
	if( psPos >= psEnd )
	{
		pnr->free_line( psLine );
		goto READ_LINE;
	}

	/* if it's a close paren */
	if( *psPos == ')' )
	{
		pnr->free_line( psLine );
		return NP_CPAREN;
	}

	/* if it's a close brace */
	if( *psPos == '}' )
	{
		pnr->free_line( psLine );
		return NP_CBRACE;
	}

	/* it's a normal node */

	/* unescape the name */
	psColon = unterminated_strchr<wchar_t>( psPos, psEnd, ':' );
	if( psColon == NULL )
		goto PARSE_ERROR;

	if( psColon != psPos )
	{
		psName = node_unescapeW( pArena, psPos, psColon );
	}

	/* figure out the node type */
	for( psType = psColon+1; psType < psEnd; psType++ )
		if( !iswspace( *psType ) )
			break;

	/* allocate a new node */
	pn = node_alloc_internal( pArena );
	if( psName != NULL )
	{
		if( nOutputStyle == NODE_W )
		{
			node_set_nameW_internal( pn, psName );
		}
		else
		{
			/* TODO: warn possible loss of data */
			char * psA = WToA( pArena, psName );
			node_set_nameA_internal( pn, psA );
			nfree( pArena, psA );
		}

		nfree( pArena, psName );
	}

	/* switch the node type */
	switch( *psType )
	{
		/* TODO: possibly worry about localized numbers output */
	case '.': case '0': case '1': case '2': case '3': case '4':
	case '-': case '5': case '6': case '7': case '8': case '9':
		{
			TempSZ<wchar_t> psValue( pArena, psType, psEnd );

			if( wcschr( psValue, '.' ) != NULL )
				node_set_real( pn, wcstod( psValue, NULL ) );
			else if( wcschr( psValue, 'L' ) != NULL )
				node_set_int64( pn, _wtoi64( psValue ) );
			else
				node_set_int( pn, wcstol( psValue, NULL, 10 ) );
		}
		break;

	case '"':
		/* remove trailing quote */
		psTrailingQuote = unterminated_strrchr<wchar_t>( psType, psEnd, '"' );
		if( psTrailingQuote == NULL || psTrailingQuote <= psType )
		{
			node_error( "Unterminated string.\n" );
			goto PARSE_ERROR;
		}

		if( nOutputStyle == NODE_W )
			node_set_stringW_internal( pn, psType+1, psTrailingQuote );
		else
		{
			char * psA = WToA( pArena, psType+1, psTrailingQuote );
			node_set_stringA_internal( pn, psA );
			nfree( pArena, psA );
		}

		break;

	case '\'':
		/* remove trailing quote */
		psTrailingQuote = unterminated_strrchr<wchar_t>( psType, psEnd, '\'' );
		if( psTrailingQuote == NULL || psTrailingQuote <= psType )
		{
			node_error( "Unterminated string.\n" );
			goto PARSE_ERROR;
		}

		psUnescaped = node_unescapeW( pArena, psType+1, psTrailingQuote );

		if( nOutputStyle == NODE_W )
			node_set_stringW_internal( pn, psUnescaped );
		else
		{
			char * psA = WToA( pArena, psUnescaped );
			node_set_stringA_internal( pn, psA );
			nfree( pArena, psA );
		}

		nfree( pArena, psUnescaped );
		break;
	
	case 'D':
		nDataLength = wcstol( psType + 4, NULL, 10 );
		pb = (data_t *)node_malloc( pArena, nDataLength );
		/* loop over the rows of the binary data */
		for( nRows = 0; nRows < (nDataLength+15)/16; nRows++ )
		{
			const wchar_t * psDataEnd;

			/* read a new line */
			pnr->read_lineW( &psData, &psDataEnd );
			psCursor = psData+1;

			for( i = nRows*16; i < nDataLength && i < (nRows+1)*16; i++ )
			{
				/* skip whitespace */
				psCursor += wcsspn( psCursor, L" " );

				/* convert the hex data to a byte */
				b = wcstoul( psCursor, NULL, 16 );
				psCursor += 2;

				/* set the data */
				pb[i] = (data_t)b;
			}
			pnr->free_line( psData );
		}

		node_set_data( pn, nDataLength, pb );
		nfree( pArena, pb );
		break;

	case 'P':
		/* handle reading in pointer value: use NULL */
		node_set_ptr( pn, NULL );
		break;

	case '(':
		node_list_init( pn );
		while( (nResult = node_parse_internalW( pnr, &pnChild, nOutputStyle ) ) == NP_NODE )
		{
			node_list_add_internal( pn, pnChild );
		}

		if( nResult != NP_CPAREN )
		{
			node_error( "No close paren for list.\n" );
			goto PARSE_ERROR;
		}

		break;
	case '{':
		{
			node_t * pnList = node_list_alloc();

			while( (nResult = node_parse_internalW( pnr, &pnChild, nOutputStyle ) ) == NP_NODE )
			{
				if( nOutputStyle == NODE_A )
				{
					if( pnChild->psAName != NULL )
						node_push_internal( pnList, pnChild );
					else
					{
						node_free( pnChild );
						node_error( "Child of hash has no name -- skipping.\n" );
					}
				}
				else
				{
					if( pnChild->psWName != NULL )
						node_push_internal( pnList, pnChild );
					else
					{
						node_free( pnChild );
						node_error( "Child of hash has no name -- skipping.\n" );
					}
				}
			}

			if( nResult != NP_CBRACE )
			{
				node_error( "No close brace for hash.\n" );
				node_free( pnList );
				goto PARSE_ERROR;
			}

			node_hash_init( pn, __max( DEFAULT_HASHBUCKETS, pnList->nListElements>>3 )  );

			while( pnList->nListElements != 0 )
				node_hash_add_internal( pn, node_pop_internal( pnList ) );

			node_free( pnList );
	
		}
		break;
	}

	pnr->free_line( psLine );
	*ppn = pn;
	return NP_NODE;

PARSE_ERROR:
	if( pn != NULL )
		node_free_internal( pn, NOT_IN_COLLECTION );
	pnr->free_line( psLine );
	
	return NP_SERROR;
}


/*****************
 Utility Functions
 *****************/

/* deep copy a node */
node_t * node_copy_dbg( const char * psFile, int nLine, const node_t *pnSource )
{
	set_debug_allocator s( psFile, nLine );

	return node_copy( pnSource );
}

node_t * node_copy( const node_t *pnSource )
{
	/* if it's NULL, return NULL */
	if( pnSource == NULL )
	{
		return NULL;
	}

	return node_copy_internal( node_pArena, pnSource );
}

static node_t * node_copy_internal( node_arena * pArena, const node_t * pnSource )
{
	int i;

	node_t * pnCopy = NULL;
	node_t * pn = NULL;

	/* allocate new node */
	pnCopy = node_alloc_internal( pArena );

	if( pnSource->nType == NODE_HASH )
	{
		node_hash_init( pnCopy, pnSource->nHashBuckets );
		pnCopy->nHashFlags = pnSource->nHashFlags;
	}
	else if( pnSource->nType == NODE_LIST )
	{
		node_list_init( pnCopy );
	}

	/* copy the type */
	pnCopy->nType = pnSource->nType;
	pnCopy->nHash = pnSource->nHash;

	/* copy is not in a collection */
	pnCopy->bInCollection = NOT_IN_COLLECTION;

	/* copy the name */
	if( pnSource->psAName != NULL )
		pnCopy->psAName = node_safe_copyA( pArena, pnSource->psAName );
	if( pnSource->psWName != NULL )
		pnCopy->psWName = node_safe_copyW( pArena, pnSource->psWName );

	/* if there's a pnNext, ignore it */
	pnCopy->pnNext = NULL;

	/* copy the data */
	switch( pnCopy->nType )
	{
	case NODE_INT:
		pnCopy->nValue = pnSource->nValue;
		break;

	case NODE_INT64:
		pnCopy->n64Value = pnSource->n64Value;
		break;

	case NODE_REAL:
		pnCopy->dfValue = pnSource->dfValue;
		break;

	case NODE_STRINGA:
		node_set_stringA_internal( pnCopy, pnSource->psAValue );
		break;

	case NODE_STRINGW:
		node_set_stringW_internal( pnCopy, pnSource->psWValue );
		break;

	case NODE_PTR:
		pnCopy->pvValue = pnSource->pvValue;
		break;

	case NODE_DATA:
		node_set_data_internal( pnCopy, pnSource->nDataLength, pnSource->pbValue );
		break;

		/* LIST : deep copy the list */
	case NODE_LIST:

		for( pn = node_first( pnSource ); pn != NULL; pn = node_next( pn ) )
		{
			/* copy each element of list */
			node_list_add_internal( pnCopy, node_copy_internal( pArena, pn ) );
		}

		break;

		/* HASH: deep copy the hash */
	case NODE_HASH:

		/* copy the number of elements */
		pnCopy->nHashElements = pnSource->nHashElements;

		/* copy the elements */
		for( i = 0; i < pnCopy->nHashBuckets; i++ )
		{
			for( pn = pnSource->ppnHashHeads[i]; pn != NULL; pn = node_next( pn ) )
			{
				node_t * pnElement = node_copy_internal( pArena, pn );

				pnElement->bInCollection = IN_COLLECTION;

				/* add the element to that bucket */
				pnElement->pnNext = pnCopy->ppnHashHeads[i];
				pnCopy->ppnHashHeads[i] = pnElement;
			}

		}
	
		break;

	default:
		node_error( "Attempted to copy illegal node type (value %d).\n", pnSource->nType );
		node_assert( !"Attempted to copy illegal node type." );
	}

	/* return the copied node */		
	return pnCopy;

}

/* safely copy a string */
static char * node_safe_copyA( struct node_arena * pArena, const char * ps )
{
	char * psCopy;
	size_t length;

	/* allocate memory */
	length = (strlen(ps) + 1) * sizeof(char);

	psCopy = (char *)node_malloc( pArena, length );

	/* copy the string */
	strcpy( psCopy, ps );

	return psCopy;
}

/* safely copy W string */
static wchar_t * node_safe_copyW( struct node_arena * pArena, const wchar_t * ps )
{
	wchar_t * psCopy;

	size_t length;

	length = (wcslen(ps) + 1) * sizeof(wchar_t);

	/* allocate memory */
	psCopy = (wchar_t *)node_malloc( pArena, length );

	/* copy the string */
	wcscpy( psCopy, ps );

	return psCopy;
}

/* takes a byte and returns '.' if unprintable; else returns the byte */
static __inline int printable(int c)
{
	return isprint( c ) ? c : '.';
}

/********************************
 Private Function Implementations
 ********************************/

static void node_set_int( node_t * pn, int nValue )
{
	/* clean up */
	node_cleanup( pn );

	pn->nValue = nValue;

	pn->nType = NODE_INT;
}

static void node_set_int64( node_t * pn, int64_t nValue )
{
	/* clean up */
	node_cleanup( pn );

	pn->n64Value = nValue;

	pn->nType = NODE_INT64;
}

static void node_set_real( node_t * pn, double dfValue )
{
	/* clean up */
	node_cleanup( pn );

	pn->dfValue = dfValue;

	pn->nType = NODE_REAL;
}

static void node_set_stringA( node_t * pn, const char * psAValue )
{
	if(psAValue == NULL)
	{
		node_assert(psAValue != NULL);
		node_error("Attempted to set node value to null string.\n");
		return;
	}

	/* if setting to same as current, done */
	if( psAValue == pn->psAValue )
	{
		return;
	}

	/* clean up */
	node_cleanup( pn );

	if( node_nDebugUnicode )
	{
		node_check_ascii_string( psAValue, "NODE_STRINGA" );
	}

	node_set_stringA_internal( pn, psAValue );
}

static void node_set_stringA_internal( node_t * pn, const char * psAValue )
{
	node_set_stringA_internal( pn, psAValue, strlen( psAValue ) );
}

static void node_set_stringA_internal( node_t * pn, const char * psAValue, const char * psEnd )
{
	node_set_stringA_internal( pn, psAValue, psEnd-psAValue );
}

static void node_set_stringA_internal( node_t * pn, const char * psAValue, size_t cch )
{
#ifdef USE_BAGS
	if( !pn->bBagUsed && cch < BAG_SIZE )
	{
		pn->psAValue = (char *)GET_BAG( pn );
		memcpy( pn->psAValue, psAValue, cch );
		pn->psAValue[cch] = '\0';
		pn->bBagUsed = true;
	}
	else
#endif
	{
		/* node_safe_copy psValue */
		pn->psAValue = (char *)node_malloc( pn->pArena, (cch+1)*sizeof(char) );
		memcpy( pn->psAValue, psAValue, cch*sizeof(char) );	 
		pn->psAValue[cch] = '\0';
	}


	pn->nType = NODE_STRINGA;
}

static void node_set_stringW( node_t * pn, const wchar_t * psWValue )
{
	if(psWValue == NULL)
	{
		node_assert(psWValue != NULL);
		node_error("Attempted to set node value to null string.\n");
		return;
	}

	/* if setting to same as current, done */
	if( psWValue == pn->psWValue )
	{
		return;
	}

	/* clean up */
	node_cleanup( pn );

	if( node_nDebugUnicode )
	{
		node_check_unicode_string( psWValue, "NODE_STRINGW" );
	}
	
	node_set_stringW_internal( pn, psWValue );
}

static void node_set_stringW_internal( node_t * pn, const wchar_t * psWValue )
{
	node_set_stringW_internal( pn, psWValue, wcslen( psWValue ) );
}

static void node_set_stringW_internal( node_t * pn, const wchar_t * psWValue, const wchar_t * psEnd )
{
	node_set_stringW_internal( pn, psWValue, psEnd-psWValue );
}

static void node_set_stringW_internal( node_t * pn, const wchar_t * psWValue, size_t cch )
{
#ifdef USE_BAGS
	if( !pn->bBagUsed && (sizeof(wchar_t)*cch) < BAG_SIZE )
	{
		pn->psWValue = (wchar_t *)GET_BAG( pn );
		memcpy( pn->psWValue, psWValue, cch*sizeof(wchar_t) );
		pn->psWValue[cch] = '\0';
		pn->bBagUsed = true;
	}
	else
#endif
	{
		pn->psWValue = (wchar_t *)node_malloc( pn->pArena, (cch+1)*sizeof(wchar_t) );
		memcpy( pn->psWValue, psWValue, cch*sizeof(wchar_t) );	 
		pn->psWValue[cch] = '\0';
	}

	pn->nType = NODE_STRINGW;
}

node_t * node_set_data_dbg( const char * psFile, int nLine, node_t * pn, int nLength, const void * pbValue )
{
	set_debug_allocator s( psFile, nLine );
	return node_set_data( pn, nLength, pbValue );
}

node_t * node_set_data( node_t * pn, int nLength, const void * pvValue )
{
	const data_t * pbValue = reinterpret_cast<const data_t *>( pvValue );
	if( nLength <= 0 )
	{
		node_assert( nLength > 0 );
		return pn;
	}

	if( nLength > LOTS_OF_MEMORY ) 
	{
		if( nLength > GRATUITOUSLY_MUCH_MEMORY )
		{
			node_assert( nLength < GRATUITOUSLY_MUCH_MEMORY );
			return pn;
		}
		else
		{
			node_error("Suspicious amount of memory (%d) allocated for data element\n", nLength );
		}
	}

	if( pbValue == NULL )
	{
		node_assert(pbValue != NULL);
		node_error( "Attempted to set data with NULL pointer.\n");
		return pn;
	}
	
	/* if setting to same as current, return */
	if( pn->nType == NODE_DATA && pbValue == pn->pbValue )
	{
		/* shortening the data in place is OK */
		if( nLength <= pn->nDataLength )
			pn->nDataLength = nLength;
		else
			node_error( "Attempting to increase length of existing data from %d to %d.\n", pn->nDataLength, nLength );
		
		return pn;
	}

	/* clean up */
	node_cleanup( pn );

	node_set_data_internal( pn, nLength, pbValue );

	return pn;
}

static void node_set_data_internal( node_t * pn, int nLength, const data_t * pbValue )
{
	/* store the length */
	pn->nDataLength = nLength;

#ifdef USE_BAGS
	if( !pn->bBagUsed && nLength <= BAG_SIZE )
	{
		pn->pbValue = (data_t *)GET_BAG( pn );
		pn->bBagUsed = true;
	}
	else
#endif
	{
		/* allocate the buffer */
		pn->pbValue = (data_t *)node_malloc( pn->pArena, pn->nDataLength );
	}

	/* copy the data into the buffer */
	memcpy( pn->pbValue, pbValue, pn->nDataLength );

	pn->nType = NODE_DATA;
}

static void node_set_ptr( node_t * pn, void * pv )
{
	/* clean up */
	node_cleanup( pn );

	/* store the pointer */
	pn->pvValue = pv;

	pn->nType = NODE_PTR;
}

/* set the node to a value using variable arguments. nType determines how 
arguments are processed.*/
static void node_set_valist(node_t * pn, int nType, va_list valist)
{
	unsigned int nDataLength = 0;
	data_t * pbValue = NULL;

	if( pn == NULL )
	{
		node_assert( pn != NULL );
		pn = node_alloc_internal( node_pArena );
	}

	switch (nType)
	{
	case NODE_INT:
		node_set_int( pn, va_arg(valist, int) );
		break;

	case NODE_INT64:
		node_set_int64( pn, va_arg(valist, int64_t) );
		break;

	case NODE_REAL:
		node_set_real( pn, va_arg(valist, double) );
		break;

	case NODE_STRINGA:
		node_set_stringA( pn, va_arg(valist, char *) );
		break;

	case NODE_STRINGW:
		node_set_stringW( pn, va_arg(valist, wchar_t *) );
		break;

	case NODE_DATA:
		nDataLength = va_arg( valist, unsigned int );
		pbValue = va_arg( valist, data_t * );

		node_set_data( pn, nDataLength, pbValue );
		break;

	case NODE_PTR:
		node_set_ptr( pn, va_arg( valist, void * ) );
		break;

	case NODE_REF_DATA:
		{
			node_t * pnSource = va_arg( valist, node_t * );

			pn->nType = NODE_DATA;
			nDataLength = pn->nDataLength = pnSource->nDataLength;

#ifdef USE_BAGS
			if( IS_BAG( pnSource, pnSource->pbValue ) )
			{
				if( !pn->bBagUsed )
				{
					pn->bBagUsed = TRUE;
					pn->pbValue = GET_BAG( pn );
					memcpy( pn->pbValue, pnSource->pbValue, nDataLength );
				}
				else
					node_set_data( pn, nDataLength, pnSource->pbValue );
			}
			else
#endif
			{
				pn->pbValue = pnSource->pbValue;
				pnSource->nDataLength = 0;
				pnSource->pbValue = NULL;
			}

			node_free_internal( pnSource, FALSE );

			return;
		}
		break;

	case NODE_COPY_DATA:
		{
			node_t * pnSource = va_arg( valist, node_t * );

			node_set_data( pn, pnSource->nDataLength, pnSource->pbValue );

			pn->nType = NODE_DATA;
			return;
		}
		break;

	default:
	/* otherwise */
		node_assert(!"Invalid node type");
		node_error( "Tried to set node with invalid type (%d).\n", nType );
	}

	/* copy the type */
	pn->nType = nType;
	return;
}	

/* hash a string */
unsigned int node_hashA( const char * psKey )
{
	unsigned int nHash = 0x53378008;
	const char * pc = NULL;

	for( pc = psKey; *pc != 0; pc++ )
	{
		unsigned int c = *pc | 0x20;

		nHash = (nHash * 0x1F) + ( (c << 16) + c );
	}

	return (nHash & NODE_HASH_MASK);
}

unsigned int node_hashW( const wchar_t * psKey )
{
    unsigned int nHash = 0x55378008;
    const wchar_t * pc = NULL;

    for( pc = psKey; *pc != 0; pc++ )
    {
		/* set the 'lower case' bit */
		unsigned int c = *pc | 0x20;

		nHash = (nHash * 0x1F) + ( (c << 16) + c );
    }

    return (nHash & NODE_HASH_MASK);
}

static void node_cleanup( node_t * pn )
{
	int i = 0;

	/* on node_cleanup, make sure all members are freed and zeroed */
	switch( pn->nType )
	{
	case NODE_STRINGA:
	case NODE_STRINGW:
	case NODE_INT:
	case NODE_INT64:
	case NODE_REAL:
		/* because of implicit conversion, must clear all scalar values together */
		if( pn->psAValue != NULL && !IS_BAG( pn, pn->psAValue ) )
			nfree( pn->pArena, pn->psAValue );

		if( pn->psWValue != NULL && !IS_BAG( pn, pn->psWValue ) )
			nfree( pn->pArena, pn->psWValue );

		pn->psAValue = NULL;
		pn->psWValue = NULL;
		pn->nValue = 0;
		pn->dfValue = 0.0;
		break;

	case NODE_PTR:
		pn->pvValue = 0;
		break;

	case NODE_DATA:
		if( pn->pbValue != NULL && !IS_BAG( pn, pn->pbValue ) )
			nfree( pn->pArena, pn->pbValue );
		pn->pbValue = NULL;
		pn->nDataLength = 0;
		break;
	case NODE_LIST:
		node_free_internal( pn->pnListHead, IN_COLLECTION );
		pn->pnListHead = NULL;
		pn->pnListTail = NULL;
		pn->nListElements = 0;
		break;
	case NODE_HASH:
#ifdef _DEBUG
		if( node_nDebugHashPerf )
		{
			if( pn->psHashAllocated != NULL )
			{
				int nMaxElts = pn->nHashFlags>>8;
				/* check and report load factor */
				double dfLoad = (double)nMaxElts/pn->nHashBuckets;
				
				if( dfLoad > node_dfLoadLimit || ( pn->nHashBuckets > 128 && dfLoad < 0.001 ) )
				{
					char acBuffer[1024];
					sprintf( acBuffer, "%s Node hash debugging: hash has load factor %.2f (%d/%d)\n",
						pn->psHashAllocated, dfLoad, nMaxElts, pn->nHashBuckets );
//					fputs( acBuffer, stderr );
					OutputDebugStringA( acBuffer );
				}

				nfree( pn->pArena, pn->psHashAllocated );
			}
		}
#endif

		for( i = 0; i < pn->nHashBuckets; i ++ )
		{
			node_free_internal( pn->ppnHashHeads[i], IN_COLLECTION );
		}
		if( !IS_BAG( pn, pn->ppnHashHeads ) )
			nfree( pn->pArena, pn->ppnHashHeads );

		pn->ppnHashHeads = NULL;

		pn->nHashBuckets = 0;
		pn->nHashElements = 0;
		break;
	}
	pn->nType = NODE_UNKNOWN;
	pn->bBagUsed = 0;
}

/* node_hash_keys 
 * This function returns a newly-allocated list node containing 
 * a list of the key strings for hash pnHash. 
 */
node_t * node_hash_keys_dbgA( const char * psFile, int nLine, const node_t * pnHash ) 
{
	set_debug_allocator s( psFile, nLine );
	return node_hash_keysA( pnHash );
}


node_t * node_hash_keysA( const node_t * pnHash ) 
{
	node_t * pnList = NULL;
	node_t * pn = NULL;

	int i = 0;

	if( pnHash == NULL )
	{
		node_assert( pnHash != NULL );
		return NULL;
	}

	/* if pnHash is not a hash node */
	if( pnHash->nType != NODE_HASH )
	{
		node_assert( pnHash->nType == NODE_HASH );
		/* return the list */
		return node_list_alloc();
	}

	if( node_nDebugUnicode )
	{
		/* check that there are no W keys in this hash */
		if( pnHash->nHashFlags & HASH_CONTAINS_WKEYS )
		{
			node_error( "Attempting to get A keys from hash which contains W keys.\n" );
			node_assert( (pnHash->nHashFlags & HASH_CONTAINS_WKEYS) == 0 );
			return node_list_alloc();
		}
	}

	/* allocate a list node */
	pnList = node_alloc_internal( node_pArena );
	node_list_init( pnList );

	/* for each hash bucket */
	for( i = 0; i < pnHash->nHashBuckets; i++ )
	{
		/* for each node in the bucket */
		for( pn = pnHash->ppnHashHeads[i]; pn != NULL; pn = node_next( pn ) )
		{
			node_t * pnName = node_alloc_internal( pnList->pArena );

			node_set_stringA_internal( pnName, pn->psAName );

			node_list_add_internal( pnList, pnName );
		}
	}

	/* return the list */
	return pnList;
}

node_t * node_hash_keys_dbgW( const char * psFile, int nLine, const node_t * pnHash ) 
{
	set_debug_allocator s(psFile, nLine);
	return node_hash_keysW( pnHash );
}

node_t * node_hash_keysW( const node_t * pnHash ) 
{
	node_t * pnList = NULL;
	node_t * pn = NULL;

	int i = 0;

	if( pnHash == NULL )
	{
		node_assert( pnHash != NULL );
		return NULL;
	}

	/* if pnHash is not a hash node */
	if( pnHash->nType != NODE_HASH )
	{
		node_assert( pnHash->nType == NODE_HASH );
		/* return the list */
		return node_list_alloc();
	}

	if( node_nDebugUnicode )
	{
		/* check that there are no W keys in this hash */
		if( pnHash->nHashFlags & HASH_CONTAINS_AKEYS )
		{
			node_error( "Attempting to get W keys from hash which contains A keys.\n" );
			node_assert( (pnHash->nHashFlags & HASH_CONTAINS_AKEYS) == 0 );
			return node_list_alloc();
		}
	}

	/* allocate a list node */
	pnList = node_alloc_internal( node_pArena );
	node_list_init( pnList );

	/* for each hash bucket */
	for( i = 0; i < pnHash->nHashBuckets; i++ )
	{
		/* for each node in the bucket */
		for( pn = pnHash->ppnHashHeads[i]; pn != NULL; pn = node_next( pn ) )
		{
			node_t * pnName = node_alloc_internal( pnList->pArena );

			node_set_stringW_internal( pnName, pn->psWName );

			node_list_add_internal( pnList, pnName );
		}
	}

	/* return the list */
	return pnList;
}

static char * read_lineA( node_arena * pArena, FILE * pfIn, char ** ppsEnd )
{
	int nBufferSize = 40;
	char * psBuffer = NULL;
	int nBytesRead = 0;
	int nChar = 0;

	/* initialize output arg */
	*ppsEnd = NULL;

	/* check for end-of-file */
	nChar = fgetc( pfIn );
	if( feof( pfIn ) )
		return NULL;
	else
		ungetc( nChar, pfIn );

	/* allocate initial buffer */
	psBuffer = (char *)node_malloc( pArena, nBufferSize );

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
			psNewBuffer = (char*)node_malloc( pArena, nBufferSize );

			/* copy the old buffer onto the new one */
			memcpy( psNewBuffer, psBuffer, nBufferSize/2 );

			nfree( pArena, psBuffer );
			psBuffer = psNewBuffer;
		}

		/* store the character in the buffer */
		psBuffer[ nBytesRead-1 ] = (char)nChar;

		/* if the character was a newline */
		if( nChar == '\n' )
			break;
	}

	/* terminate the buffer */
	int nEnd = nBytesRead;

	if( nBytesRead >= 1 && psBuffer[ nBytesRead-1 ] == '\n' )
		nEnd = nBytesRead - 1;

	if( nBytesRead >= 2 &&psBuffer[ nBytesRead-2 ] == '\r' )
		nEnd = nBytesRead - 2;

	psBuffer[ nEnd ] = '\0';
	*ppsEnd = psBuffer + nEnd;

	/* return the read string */
	return psBuffer;
}

static wchar_t * read_lineW( node_arena * pArena, FILE * pfIn, wchar_t ** ppsEnd )
{
	int nBufferSize = 40;
	wchar_t * psBuffer = NULL;
	int nCharsRead = 0;
	wchar_t nChar = 0;

	/* check for end-of-file */
	nChar = fgetwc( pfIn );
	if( feof( pfIn ) )
		return NULL;
	else
		ungetwc( nChar, pfIn );

	/* allocate initial buffer */
	psBuffer = (wchar_t *)node_malloc( pArena, nBufferSize * sizeof(wchar_t) );

	for(;;)
	{
READ_CHAR:
		/* read a character */
		nChar = fgetwc( pfIn );

		/* if the character was EOF */
		if( nChar == WEOF )
			break;

		/* if byte-order marker, skip */
		/* TODO: handle other-endian unicode files */
		if( nChar == NODE_BOM )
			goto READ_CHAR;

		/* increment the read count */
		nCharsRead++;

		/* if we're past the length of the buffer */
		if( nCharsRead >= nBufferSize )
		{
			wchar_t * psNewBuffer = NULL;

			/* increase the buffer size */
			nBufferSize *= 2;

			/* reallocate the buffer */
			psNewBuffer = (wchar_t*)node_malloc( pArena, nBufferSize * sizeof(wchar_t) );

			/* copy the old buffer onto the new one */
			memcpy( psNewBuffer, psBuffer, (nBufferSize/2) * sizeof(wchar_t) );

			nfree( pArena, psBuffer );
			psBuffer = psNewBuffer;
		}

		/* store the character in the buffer */
		psBuffer[ nCharsRead-1 ] = (wchar_t)nChar;

		/* if the character was a newline */
		if( nChar == '\n' )
			break;
	}

	int nEnd = nCharsRead;

	if( nCharsRead >= 1 && psBuffer[ nCharsRead-1 ] == '\n' )
		nEnd = nCharsRead - 1;

	if( nCharsRead >= 2 &&psBuffer[ nCharsRead-2 ] == '\r' )
		nEnd = nCharsRead - 2;

	psBuffer[ nEnd ] = '\0';
	*ppsEnd = psBuffer + nEnd;

	/* return the read string */
	return psBuffer;
}

static char * node_escapeA( struct node_arena * pArena, const char * psUnescaped )
{
	char * psEscaped = NULL;
	char * psE = NULL;
	const char * psU = NULL;

	psEscaped = (char *)node_malloc( pArena, 3*strlen(psUnescaped) + 1 );

	psE = psEscaped;
	psU = psUnescaped;
	while( *psU != '\0' )
	{
		if( ( psU == psUnescaped && ( *psU == '}' || *psU == ')' || *psU == '$' ) ) ||
			*psU == '%' || *psU == ':' || *psU == '\r' || *psU == '\n' )
		{
			/* write out a percent */
			*psE++ = '%';

			/* write out the hex code */
			sprintf( psE, "%02X", *psU );
			psU++;
			psE += 2;
		}
		else
		{
			*psE++ = *psU++;
		}
	}
	*psE = '\0';

	return psEscaped;
}

static wchar_t * node_escapeW( struct node_arena * pArena, const wchar_t * psUnescaped )
{
	wchar_t * psEscaped = NULL;
	wchar_t * psE = NULL;
	const wchar_t * psU = NULL;

	const size_t nEscapedLen = (3*wcslen(psUnescaped) + 1);
	psEscaped = (wchar_t *)node_malloc( pArena, nEscapedLen*sizeof(wchar_t) );

	psE = psEscaped;
	psU = psUnescaped;
	while( *psU != '\0' )
	{
		if( ( psU == psUnescaped && ( *psU == '}' || *psU == ')' || *psU == '$' ) ) ||
			*psU == '%' || *psU == ':' || *psU == '\r' || *psU == '\n' )
		{
			/* write out a percent */
			*psE++ = '%';

			/* write out the hex code */
			swprintf( psE, nEscapedLen-(psE - psEscaped), L"%02X", *psU );
			psU++;
			psE += 2;
		}
		else
		{
			*psE++ = *psU++;
		}
	}
	*psE = '\0';

	return psEscaped;
}

static char * node_unescapeA( struct node_arena * pArena, const char * psEscaped, const char * psEnd )
{
	char * psUnescaped = (char *)node_malloc( pArena, psEnd-psEscaped+1 );
	const char * psE = psEscaped;
	char * psU = psUnescaped;

	int n = 0;

	while( psE < psEnd )
	{
		switch( *psE )
		{
		case '%':
			{
				/* step over the percent */
				psE++;

				/* convert the hex code */
				TempSZ<char> t( pArena, psE, psE+2 );
				n = strtoul( t, NULL, 16 );
				*psU++ = (char)n;
				psE += 2;
			}
			break;
		default:
			*psU++ = *psE++;
		}
	}
	*psU = '\0';

	return psUnescaped;
}

static wchar_t * node_unescapeW( struct node_arena * pArena, const wchar_t * psEscaped, const wchar_t * psEnd )
{
	wchar_t * psUnescaped = (wchar_t *)node_malloc( pArena, (psEnd-psEscaped+1)*sizeof(wchar_t) );
	const wchar_t * psE = psEscaped;
	wchar_t * psU = psUnescaped;

	int n = 0;

	while( psE < psEnd )
	{
		switch( *psE )
		{
		case '%':
			{
				/* step over the percent */
				psE++;

				/* convert the hex code */
				TempSZ<wchar_t> t( pArena, psE, psE+2 );
				n = wcstoul( t, NULL, 16 );
				*psU++ = (wchar_t)n;
				psE += 2;
			}
			break;
		default:
			*psU++ = *psE++;
		}
	}
	*psU = '\0';

	return psUnescaped;
}

static char * WToA( node_arena * pArena, const wchar_t * psW )
{
	return WToA( pArena, psW, wcslen(psW) );
}

static char * WToA( node_arena * pArena, const wchar_t * psW, const wchar_t * psWEnd )
{
	return WToA( pArena, psW, psWEnd - psW );
}

static char * WToA( node_arena * pArena, const wchar_t * psW, size_t cch )
{
	char * psA = NULL;

	int nCodePage = node_nCodePage;
	DWORD dwLength = WideCharToMultiByte( nCodePage, 0, psW, (int)cch, NULL, 0, NULL, NULL ) + 1;

	psA = (char *)node_malloc( pArena, dwLength );

	WideCharToMultiByte( nCodePage, 0, psW, (int)cch, psA, dwLength, NULL, NULL );
	psA[dwLength-1] = '\0';

	return psA;
}

static wchar_t * AToW( node_arena * pArena, const char * psA )
{
	return AToW( pArena, psA, strlen(psA) );
}

static wchar_t * AToW( node_arena * pArena, const char * psA, const char * psAEnd )
{
	return AToW( pArena, psA, psAEnd-psA );
}

static wchar_t * AToW( node_arena * pArena, const char * psA, size_t nLength )
{
	wchar_t * psW = NULL;

	int nCodePage = node_nCodePage;
	DWORD dwLength = MultiByteToWideChar( nCodePage, 0, psA, (int)nLength, NULL, 0 ) + 1;

	psW = (wchar_t *)node_malloc( pArena, dwLength * sizeof(wchar_t) );

	MultiByteToWideChar( nCodePage, 0, psA, (int)nLength, psW, dwLength );
	psW[dwLength-1] = '\0';

	return psW;
}

void node_set_codepage( int nCodePage )
{
	node_nCodePage = nCodePage;
}

int node_get_codepage()
{
	return node_nCodePage;
}

void node_set_debug( int nDebug )
{
	node_nDebug = nDebug;

	node_nDebugIntern = (node_nDebug & NODE_DEBUG_INTERN);
	node_nDebugUnicode = (node_nDebug & NODE_DEBUG_UNICODE);
	node_nDebugRef = (node_nDebug & NODE_DEBUG_REF);
	node_nDebugHashPerf = (node_nDebug & NODE_DEBUG_HASHPERF);
}

int node_get_debug()
{
	return node_nDebug;
}


void node_set_error_funcs( node_error_func_t pErrFunc, node_memory_func_t pMemFunc, 
								    node_assert_func_t pAssertFunc )
{
	node_pfError = pErrFunc;
	node_pfMemory = pMemFunc;
	node_pfAssert = reinterpret_cast<node_assert_func_internal_t>(pAssertFunc);
}

static void _node_assert( const char * psExpr, const char * psFile, unsigned int nLine )
{
	if( node_pfAssert != NULL )
	{
		if( node_source_file != NULL )
			node_pfAssert( psExpr, node_source_file, node_source_line );
		else
			node_pfAssert( psExpr, psFile, nLine );
	}
}

static void node_error( char * psError, ... )
{
	char acBuffer[1024] = {0};
	va_list ap;

	if( node_pfError != NULL )
	{
		va_start( ap, psError );
		_vsnprintf_s( acBuffer, sizeof(acBuffer)-1, psError, ap );
		va_end( ap );

		node_pfError( acBuffer );
	}
}

static int node_memory( size_t cb )
{
	if( node_pfMemory == NULL )
		return NODE_MEMORY_FAIL;
	else
		return node_pfMemory( cb );
}

#ifdef USE_DL_MALLOC
void * node_malloc( struct node_arena * pArena, size_t cb )
#else
void * node_malloc( struct node_arena * , size_t cb )
#endif
{
	int nRetry = 0;

RETRY:
	void * pv = NULL;

#if defined(USE_DL_MALLOC) || defined(DEBUG_DL_MALLOC)
	/* round up to multiple of 4 */
	cb = (cb + 3) & (~3);

	pv = mspace_malloc( pArena->pSpace, cb );
#else

#if defined(_DEBUG)
	if( node_source_file == NULL )
		pv = malloc( cb );
	else
		pv = _malloc_dbg( cb, _NORMAL_BLOCK, node_source_file, node_source_line );
#else
	pv = malloc( cb );
#endif

#endif

	if( pv == NULL ) 
	{
		if( node_memory( cb ) == NODE_MEMORY_RETRY && nRetry++ < 10 )
			goto RETRY;
		else
			exit(1);
	}

	return pv;
}

#if defined(USE_DL_MALLOC)
static void * node_valloc( struct node_arena * pArena, size_t nSize )
{
	int nRetry = 0;

RETRY:
	void * pv = VirtualAlloc( NULL, nSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE );

	if( pv == NULL ) 
	{
		if( node_memory( nSize ) == NODE_MEMORY_RETRY && nRetry++ < 10 )
			goto RETRY;
		else
			exit(1);
	}

	// stash a pointer on the arena so we can clean it later
	node_arena::va * pVA = (node_arena::va *)pv;
	pVA->next = pArena->pVA;
	pArena->pVA = pVA;

	return (char * )pv + sizeof(node_arena::va);

}
#endif

static int __inline hash_to_bucket( const node_t * pnHash, int nHash )
{
#ifdef HASH_USES_BITMASK
	return nHash & (pnHash->nHashBuckets-1);
#endif
#ifdef HASH_USES_MOD
	return nHash % pnHash->nHashBuckets;
#endif
}

/******************
 Freelist Functions
 ******************/
void freelist_cleanup()
{
}


/*******************
 Debugging Functions
 *******************/
static void node_check_ascii_string( const char * psValue, const char * psContext )
{
	int nType = node_string_type( (const unsigned char *)psValue );

	if( nType == NODE_STRING_UNICODE )
	{
		node_error( "Allegedly ascii value %s appears Unicode (%S) in context %s.\n", 
			psValue, (wchar_t *)psValue, psContext );
	}
}

static void node_check_unicode_string( const wchar_t * psValue, const char * psContext )
{
	int nType = node_string_type( (const unsigned char *)psValue );

	if( nType == NODE_STRING_ASCII )
	{
		node_error( "Allegedly Unicode value %S appears ascii (%s) in context %s.\n", 
			psValue, (char *)psValue, psContext );
	}
}

static int node_string_type( const unsigned char * psValue )
{
	size_t cb = strlen( (char *)psValue );
	if( cb == 1 )
	{
		/* check for Unicode-looking text */
		if( psValue[1] == 0 && psValue[3] == 0 && psValue[5] == 0 && 
			isprint( psValue[2] ) && isprint( psValue[4] ) && isprint( psValue[6] ) )
		{
			cb = 2*wcslen( (wchar_t *)psValue );
		}
	}

	if( cb < 6 )
		return NODE_STRING_CANTTELL;

	/* guard against running off end of page */
	cb = __min( cb, 64 );

	int iFlags = 0xFFDD;
	if( IsTextUnicode( psValue, (int)cb, &iFlags ) )
		return NODE_STRING_UNICODE;
	else
		return NODE_STRING_ASCII;

}

#ifdef USE_DL_MALLOC
node_arena_t node_create_arena( size_t size )
{
	/* create a new arena and don't select it */
	
	/* alloc from global arena so we can't be F'ed up by being destroyed along with some other arena... */
	node_arena * pNewArena = (node_arena *)node_malloc( &g_GlobalArena, sizeof(node_arena) );

	pNewArena->pSpace = create_mspace( size, 0 );
	pNewArena->pnFreeList = NULL;
	InitializeCriticalSection( &(pNewArena->csFreeList) );
	pNewArena->pVA = NULL;

	return (node_arena_t)pNewArena;
}

node_arena_t node_get_arena()
{
	return (node_arena_t)node_pArena;
}

node_arena_t node_set_arena( node_arena_t pNewArena )
{
	node_arena * pOld = node_pArena;

	node_pArena = (node_arena *)pNewArena;

	return (node_arena_t)pOld;
}

size_t node_delete_arena( node_arena_t pToDelete )
{
	node_arena * pArena = (node_arena *)pToDelete;
	if( pArena == &g_GlobalArena )
	{
		node_error( "Error! Attempting to delete global arena!\n" );
		return 0;
	}

	if( pArena == node_pArena )
	{
		/* deleting current arena - set current to global */
		pArena = &g_GlobalArena;
		/* other threads might have as current the arena we are about to delete.  If so, too bad! */
	}

	/* free the vas */
	node_arena::va * pVANext;
	for( node_arena::va * pVA = pArena->pVA; pVA != NULL; pVA = pVANext )
	{
		pVANext = pVA->next;
		VirtualFree( pVA, 0, MEM_RELEASE );
	}

	size_t result = destroy_mspace( pArena->pSpace );
	pArena->pnFreeList = NULL;
	DeleteCriticalSection( &(pArena->csFreeList) );
	nfree( &g_GlobalArena, pArena );

	return result;
}
#else
node_arena_t node_create_arena( size_t )
{
	return NULL;
}

node_arena_t node_get_arena()
{
	return NULL;
}

node_arena_t node_set_arena( node_arena_t  )
{
	return NULL;
}

size_t node_delete_arena( node_arena_t  )
{
	return 0;
}
#endif


#ifdef NODE_DLL
static void TLSThreadCleanup()
{
	node_tls * ptls = GetTLS();
	if( ptls != NULL )
		delete ptls;
}

BOOL WINAPI DllMain( HINSTANCE /*hInstance*/, DWORD fdwReason, LPVOID /*lpvReserved*/ )
{
	switch( fdwReason )
	{
	case DLL_PROCESS_ATTACH:
		if( m_dwTLSIndex < 0 )
			return FALSE;
		break;

	case DLL_THREAD_ATTACH:
		break;

	case DLL_THREAD_DETACH:
		TLSThreadCleanup();
		break;

	case DLL_PROCESS_DETACH:
		TLSThreadCleanup();
		break;
	}

	return TRUE;
}
#endif /* NODE_DLL */

class LibSetup
{
public:
	LibSetup() 
	{
//		HMODULE hKernel = LoadLibrary( "kernel32.dll" );

//		m_pfICSASC = reinterpret_cast<ICSASC_fn>( GetProcAddress( hKernel, "InitializeCriticalSectionAndSpinCount" ) );

//		FreeLibrary( hKernel );

#ifdef USE_DL_MALLOC
		InitializeCriticalSection( &(g_GlobalArena.csFreeList) );
#endif
//		_CrtSetBreakAlloc( 1380 );

		m_dwTLSIndex = TlsAlloc();
	}

	~LibSetup()
	{
#ifdef USE_DL_MALLOC
		DeleteCriticalSection( &(g_GlobalArena.csFreeList) );
		memset( &(g_GlobalArena.csFreeList), 0, sizeof(g_GlobalArena.csFreeList) );
#endif

		TlsFree( m_dwTLSIndex );
		m_dwTLSIndex = -1;
	}
};

static class LibSetup m_l;

void node_finalize()
{
}
