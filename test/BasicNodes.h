#include <cxxtest/TestSuite.h>
#include <cxxtest/GlobalFixture.h>
#include <cxxtest/ValueTraits.h>

#define _CRTDBG_MAP_ALLOC

#include <tchar.h>
#include <crtdbg.h>
#include <io.h>

#include "resource.h"

#define NODE_TRANSPARENT
#include "node.h"

#ifdef _WIN32
#   include <windows.h>
//#   define wait() Sleep( 1 )
#   define wait() 
#else // !_WIN32
    extern "C" unsigned sleep( unsigned seconds );
#   define CXXTEST_SAMPLE_GUI_WAIT() sleep( 1 )
#endif // _WIN32

#include <winbase.h>
#include <process.h>

void node_error( char * psError )
{
	throw std::exception( psError );
}

int node_memory( size_t /*cb*/ )
{
	throw std::exception( "Node library out of memory" );
	return NODE_MEMORY_FAIL;
}

void node_assert( const wchar_t * psExpr, const wchar_t * psFile, unsigned int nLine )
{
	char acBuffer[1024] = {0};

	sprintf( acBuffer, "Node assertion failed: %s at %s:%d", psExpr, psFile, nLine );

	throw std::exception( acBuffer );
}

node_t * LoadResourceNodeA( LPCTSTR ps )
{
	HRSRC hRes = FindResource( 0, ps, _T("NODE") );	
	HGLOBAL hGlob = LoadResource( 0, hRes );

	void * pvData = LockResource( hGlob );
	int nLength = SizeofResource( 0, hRes );

	node_t * pn = NULL;
	int nResult = node_parse_from_dataA( pvData, nLength, &pn );

	return pn;
}

node_t * LoadResourceNodeW( LPCTSTR ps )
{
	HRSRC hRes = FindResource( 0, ps, _T("NODE") );	
	HGLOBAL hGlob = LoadResource( 0, hRes );

	void * pvData = LockResource( hGlob );
	int nLength = SizeofResource( 0, hRes );

	node_t * pn = NULL;
	int nResult = node_parse_from_dataW( pvData, nLength, &pn );

	return pn;
}

int g_nAssertions;
int g_nErrors;

void node_assert_count( void * , void * , unsigned int )
{
	g_nAssertions++;
}

void node_error_count( char * )
{
	g_nErrors++;
}

char * g_psFileName = NULL;

class NodeFixture : public CxxTest::GlobalFixture
{
public:
    bool setUpWorld() 
	{ 
		_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
		node_set_error_funcs( node_error, node_memory, (node_assert_func_t)node_assert );
		node_set_debug( NODE_DEBUG_ALL );

		g_psFileName = _strdup( "test_nodeXXXXXX" );
		_mktemp( g_psFileName );

		return true; 
	}
    bool tearDownWorld() 
	{ 
		free( g_psFileName );
		node_finalize();
		return true; 
	}
//    bool setUp() { printf( "<test>" ); return true; }
//    bool tearDown() { printf( "</test>" ); return true; }
};

static NodeFixture nodeFixture;

// NodeVersion should remain the first fixture so we trace() the version at 
// the beginning of testing
class NodeVersion : public CxxTest::TestSuite
{
public:
	void test_version()
	{
		TS_ASSERT( node_version() != NULL );
		TS_TRACE( node_version() );
	}
};

class NodeSize : public CxxTest::TestSuite
{
public:
	void test_size()
	{
		int nSize = sizeof( struct __node );

#if _WIN64
#ifdef _DEBUG
		TS_ASSERT_EQUALS( nSize, 64 );
#else
		TS_ASSERT_EQUALS( nSize, 64 );
#endif
#else
#ifdef _DEBUG
		TS_ASSERT_EQUALS( nSize, 40 );
#else
		TS_ASSERT_EQUALS( nSize, 40 );
#endif
#endif
	}
};

class Case8921 : public CxxTest::TestSuite
{
public:
	void testAA()
	{
		node_t * pnMain = LoadResourceNodeA( (LPCTSTR)IDR_NODE1 );

		node_t * pn = node_hash_getA( pnMain, "One" );
		TS_ASSERT_EQUALS( node_get_type( pn ), NODE_INT );
		TS_ASSERT_EQUALS( node_get_int( pn ), 1 );

		node_free( pnMain );
	}
	void testAW()
	{
		node_t * pnMain = LoadResourceNodeW( (LPCTSTR)IDR_NODE1 );

		node_t * pn = node_hash_getW( pnMain, L"One" );
		TS_ASSERT_EQUALS( node_get_type( pn ), NODE_INT );
		TS_ASSERT_EQUALS( node_get_int( pn ), 1 );

		node_free( pnMain );
	}
	void testWA()
	{
		node_t * pnMain = LoadResourceNodeA( (LPCTSTR)IDR_NODE2 );

		node_t * pn = node_hash_getA( pnMain, "One" );
		TS_ASSERT_EQUALS( node_get_type( pn ), NODE_INT );
		TS_ASSERT_EQUALS( node_get_int( pn ), 1 );

		node_free( pnMain );
	}
	void testWW()
	{
		node_t * pnMain = LoadResourceNodeW( (LPCTSTR)IDR_NODE2 );

		node_t * pn = node_hash_getW( pnMain, L"One" );
		TS_ASSERT_EQUALS( node_get_type( pn ), NODE_INT );
		TS_ASSERT_EQUALS( node_get_int( pn ), 1 );

		node_free( pnMain );
	}
};

class Case7552 : public CxxTest::TestSuite
{
public:
	void testParseNameBraceA()
	{
		node_t * pn = NULL;
		const char * psExpected = "%7D: \'foo\'\r\n";

		pn = node_alloc();
		node_set( pn, NODE_STRINGA, "foo" );
		node_set_nameA( pn, "}" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 );
		fclose( pf );

		char acBuffer[1024];
		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );

		TS_ASSERT( strncmp( psExpected, acBuffer, strlen(psExpected) ) == 0 );

		node_free( pn );
		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT_EQUALS( NP_NODE, nResult );
		TS_ASSERT_EQUALS( std::string("}"), node_get_nameA( pn ) );
		TS_ASSERT_EQUALS( std::string("foo"), node_get_stringA( pn ) );

		node_free( pn );
	}
	void testParseNameParenA()
	{
		node_t * pn = NULL;
		const char * psExpected = "%29: \'foo\'\r\n";

		pn = node_alloc();
		node_set( pn, NODE_STRINGA, "foo" );
		node_set_nameA( pn, ")" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 );
		fclose( pf );

		char acBuffer[1024];
		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );

		TS_ASSERT( strncmp( psExpected, acBuffer, strlen(psExpected) ) == 0 );

		node_free( pn );
		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT_EQUALS( NP_NODE, nResult );
		TS_ASSERT_EQUALS( std::string(")"), node_get_nameA( pn ) );
		TS_ASSERT_EQUALS( std::string("foo"), node_get_stringA( pn ) );

		node_free( pn );
	}
	void testParseNameDollarA()
	{
		node_t * pn = NULL;
		const char * psExpected = "%24: \'foo\'\r\n";

		pn = node_alloc();
		node_set( pn, NODE_STRINGA, "foo" );
		node_set_nameA( pn, "$" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 );
		fclose( pf );

		char acBuffer[1024];
		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );

		TS_ASSERT( strncmp( psExpected, acBuffer, strlen(psExpected) ) == 0 );

		node_free( pn );
		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT_EQUALS( NP_NODE, nResult );
		TS_ASSERT_EQUALS( std::string("$"), node_get_nameA( pn ) );
		TS_ASSERT_EQUALS( std::string("foo"), node_get_stringA( pn ) );

		node_free( pn );
	}

	void testParseNameBraceW()
	{
		node_t * pn = NULL;
		const wchar_t * psExpected = L"\xFEFF%7D: \'foo\'\r\n";

		pn = node_alloc();
		node_set( pn, NODE_STRINGW, L"foo" );
		node_set_nameW( pn, L"}" );

		FILE * pf = fopen( g_psFileName, "wb" );
		wchar_t wBOM = 0xFEFF;
		fwrite( &wBOM, sizeof(wchar_t), 1, pf );
		node_dumpW( pn, pf, 0 );
		fclose( pf );

		wchar_t acBuffer[1024];
		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );

		TS_ASSERT( wcsncmp( psExpected, acBuffer, wcslen(psExpected) ) == 0 );

		node_free( pn );
		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT_EQUALS( NP_NODE, nResult );
		TS_ASSERT_EQUALS( std::wstring(L"}"), node_get_nameW( pn ) );
		TS_ASSERT_EQUALS( std::wstring(L"foo"), node_get_stringW( pn ) );

		node_free( pn );
	}
	void testParseNameParenW()
	{
		node_t * pn = NULL;
		const wchar_t * psExpected = L"\xFEFF%29: \'foo\'\r\n";

		pn = node_alloc();
		node_set( pn, NODE_STRINGW, L"foo" );
		node_set_nameW( pn, L")" );

		FILE * pf = fopen( g_psFileName, "wb" );
		wchar_t wBOM = 0xFEFF;
		fwrite( &wBOM, sizeof(wchar_t), 1, pf );
		node_dumpW( pn, pf, 0 );
		fclose( pf );

		wchar_t acBuffer[1024];
		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );

		TS_ASSERT( wcsncmp( psExpected, acBuffer, wcslen(psExpected) ) == 0 );

		node_free( pn );
		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT_EQUALS( NP_NODE, nResult );
		TS_ASSERT_EQUALS( std::wstring(L")"), node_get_nameW( pn ) );
		TS_ASSERT_EQUALS( std::wstring(L"foo"), node_get_stringW( pn ) );

		node_free( pn );
	}
	void testParseNameDollarW()
	{
		node_t * pn = NULL;
		const wchar_t * psExpected = L"\xFEFF%24: \'foo\'\r\n";

		pn = node_alloc();
		node_set( pn, NODE_STRINGW, L"foo" );
		node_set_nameW( pn, L"$" );

		FILE * pf = fopen( g_psFileName, "wb" );
		wchar_t wBOM = 0xFEFF;
		fwrite( &wBOM, sizeof(wchar_t), 1, pf );
		node_dumpW( pn, pf, 0 );
		fclose( pf );

		wchar_t acBuffer[1024];
		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );

		TS_ASSERT( wcsncmp( psExpected, acBuffer, wcslen(psExpected) ) == 0 );

		node_free( pn );
		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT_EQUALS( NP_NODE, nResult );
		TS_ASSERT_EQUALS( std::wstring(L"$"), node_get_nameW( pn ) );
		TS_ASSERT_EQUALS( std::wstring(L"foo"), node_get_stringW( pn ) );

		node_free( pn );
	}
};

class Case8400 : public CxxTest::TestSuite
{
public:
	void testParseW()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;

		pf = fopen( "i90demo.hst", "rb" );
		TS_ASSERT( pf != NULL );

		int nResult = node_parseW( pf, &pn );
		TS_ASSERT_EQUALS( NP_NODE, nResult );

		node_t * pnName = node_hash_getW( pn, L"Name" );
		wchar_t * psW = node_get_stringW( pnName );

		std::wstring s(L"I90Demo : Module 1,01,06 -- 1010601A CAD Drawing");

		TS_ASSERT_EQUALS( s, psW );

		node_free( pn );
		fclose( pf );
	}

	void testParseA()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;

		pf = fopen( "i90demo.hst", "rb" );
		TS_ASSERT( pf != NULL );

		int nResult = node_parseA( pf, &pn );
		TS_ASSERT_EQUALS( NP_NODE, nResult );

		node_t * pnName = node_hash_getA( pn, "Name" );
		char * psA = node_get_stringA( pnName );

		std::string s("I90Demo : Module 1,01,06 -- 1010601A CAD Drawing");

		TS_ASSERT_EQUALS( s, psA );

		node_free( pn );
		fclose( pf );
	}

	void testParseA_2()
	{
		const char * psSource = "Name: \"3-6231FT010TransmitterSelect\"\r\n";

		node_t * pn = NULL;
		int nResult = node_parse_from_stringA( psSource, &pn );
		TS_ASSERT_EQUALS( NP_NODE, nResult );

		char * psA = node_get_stringA( pn );

		std::string s("3-6231FT010TransmitterSelect");

		TS_ASSERT_EQUALS( s, psA );

		node_free( pn );
	}
};


class DebugAlloc : public CxxTest::TestSuite
{
public:
	void test()
	{
#ifdef _DEBUG
		const char * psFile = __FILE__;
		int nLine = __LINE__;
		node_t * pn = node_alloc_dbg( psFile, nLine );

		long num;
		char * psMemFile;
		int nMemLine;
		_CrtIsMemoryBlock( pn, _msize_dbg( pn, _NORMAL_BLOCK ), &num, &psMemFile, &nMemLine );

		TS_ASSERT_EQUALS( psMemFile, psFile );
		TS_ASSERT_EQUALS( nMemLine, nLine );

		node_free( pn );
#endif
	}
};

class NewDataModes : public CxxTest::TestSuite
{
public:
	void testSet1()
	{
		data_t abData[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };
		node_t * pn1 = node_alloc();
		node_t * pn2 = node_alloc();

		node_set_data( pn1, 8, abData );

		node_set( pn2, NODE_COPY_DATA, pn1 );

		TS_ASSERT_EQUALS( node_get_type( pn2 ), NODE_DATA );

		int nLength = 0;
		data_t * pb = node_get_data( pn2, &nLength );
		TS_ASSERT_EQUALS( nLength, 8 );
		TS_ASSERT_SAME_DATA( abData, pb, nLength );

		node_free( pn1 );
		node_free( pn2 );
	}
	void testSet2()
	{
		data_t abData[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };
		node_t * pn1 = node_alloc();
		node_t * pn2 = node_alloc();

		node_set_data( pn1, 8, abData );

		node_set( pn2, NODE_REF_DATA, pn1 );

		TS_ASSERT_EQUALS( node_get_type( pn2 ), NODE_DATA );

		int nLength = 0;
		data_t * pb = node_get_data( pn2, &nLength );
		TS_ASSERT_EQUALS( nLength, 8 );
		TS_ASSERT_SAME_DATA( abData, pb, nLength );

		node_free( pn2 );
	}
	void testSet3()
	{
		data_t abData[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };
		node_t * pn1 = node_alloc();
		node_t * pn2 = node_alloc();

		node_set_data( pn1, 40, abData );

		node_set( pn2, NODE_COPY_DATA, pn1 );

		TS_ASSERT_EQUALS( node_get_type( pn2 ), NODE_DATA );

		int nLength = 0;
		data_t * pb = node_get_data( pn2, &nLength );
		TS_ASSERT_EQUALS( nLength, 40 );
		TS_ASSERT_SAME_DATA( abData, pb, nLength );

		node_free( pn1 );
		node_free( pn2 );
	}
	void testSet4()
	{
		data_t abData[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };
		node_t * pn1 = node_alloc();
		node_t * pn2 = node_alloc();

		node_set_data( pn1, 40, abData );

		node_set( pn2, NODE_REF_DATA, pn1 );

		TS_ASSERT_EQUALS( node_get_type( pn2 ), NODE_DATA );

		int nLength = 0;
		data_t * pb = node_get_data( pn2, &nLength );
		TS_ASSERT_EQUALS( nLength, 40 );
		TS_ASSERT_SAME_DATA( abData, pb, nLength );

		node_free( pn2 );
	}
	void testAdd1()
	{
		data_t abData[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };
		node_t * pn1 = node_alloc();
		node_t * pnList = node_list_alloc();

		node_set_data( pn1, 8, abData );

		node_list_add( pnList, NODE_COPY_DATA, pn1 );

		node_t * pn2 = node_first( pnList );
		TS_ASSERT_EQUALS( node_get_type( pn2 ), NODE_DATA );

		int nLength = 0;
		data_t * pb = node_get_data( pn2, &nLength );
		TS_ASSERT_EQUALS( nLength, 8 );
		TS_ASSERT_SAME_DATA( abData, pb, nLength );

		node_free( pn1 );
		node_free( pnList );
	}
	void testAdd2()
	{
		data_t abData[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };
		node_t * pn1 = node_alloc();
		node_t * pnList = node_list_alloc();

		node_set_data( pn1, 8, abData );

		node_list_add( pnList, NODE_REF_DATA, pn1 );

		node_t * pn2 = node_first( pnList );
		TS_ASSERT_EQUALS( node_get_type( pn2 ), NODE_DATA );

		int nLength = 0;
		data_t * pb = node_get_data( pn2, &nLength );
		TS_ASSERT_EQUALS( nLength, 8 );
		TS_ASSERT_SAME_DATA( abData, pb, nLength );

		node_free( pnList );
	}
	void testAdd3()
	{
		data_t abData[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };
		node_t * pn1 = node_alloc();
		node_t * pnList = node_list_alloc();

		node_set_data( pn1, 40, abData );

		node_list_add( pnList, NODE_COPY_DATA, pn1 );

		node_t * pn2 = node_first( pnList );
		TS_ASSERT_EQUALS( node_get_type( pn2 ), NODE_DATA );

		int nLength = 0;
		data_t * pb = node_get_data( pn2, &nLength );
		TS_ASSERT_EQUALS( nLength, 40 );
		TS_ASSERT_SAME_DATA( abData, pb, nLength );

		node_free( pn1 );
		node_free( pnList );
	}
	void testAdd4()
	{
		data_t abData[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
							0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };
		node_t * pn1 = node_alloc();
		node_t * pnList = node_list_alloc();

		node_set_data( pn1, 40, abData );

		node_list_add( pnList, NODE_REF_DATA, pn1 );

		node_t * pn2 = node_first( pnList );
		TS_ASSERT_EQUALS( node_get_type( pn2 ), NODE_DATA );

		int nLength = 0;
		data_t * pb = node_get_data( pn2, &nLength );
		TS_ASSERT_EQUALS( nLength, 40 );
		TS_ASSERT_SAME_DATA( abData, pb, nLength );

		node_free( pnList );
	}
};

// Test A to W conversion and dumping
class case_6801 : public CxxTest::TestSuite
{
	int m_nOldCodePage;
public:
	void setUp()
	{
		m_nOldCodePage = node_get_codepage();
	}
	void tearDown()
	{
		node_set_codepage( m_nOldCodePage );
	}
	void test_1()
	{
		node_set_codepage( 936 );
		node_t * pn = node_hash_alloc();
		node_hash_addA( pn, "URL", NODE_STRINGA, "dbdoc://tag/D/DBDOC_BUILDS/ëŠ Unicode name longer than what it used to be/TAG/TDT1/TD_TAGLIST_sample_1.DBF/ZZTAG" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 );
		fclose( pf );

		node_free( pn );
		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( pn != NULL );

		node_t * pnURL = node_hash_getW( pn, L"URL" );
		TS_ASSERT_EQUALS( node_get_stringW( pnURL ), 
			std::wstring( L"dbdoc://tag/D/DBDOC_BUILDS/\x96FB Unicode name longer than what it used to be/TAG/TDT1/TD_TAGLIST_sample_1.DBF/ZZTAG" ) );

		node_free( pn );
	}
	void test_2()
	{
		node_set_codepage( 1252 );
		node_t * pn = node_hash_alloc();
		node_hash_addA( pn, "URL", NODE_STRINGA, "dbdoc://tag/D/DBDOC_BUILDS/ëŠ Unicode name longer than what it used to be/TAG/TDT1/TD_TAGLIST_sample_1.DBF/ZZTAG" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 );
		fclose( pf );

		node_free( pn );
		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( pn != NULL );

		node_t * pnURL = node_hash_getW( pn, L"URL" );
		TS_ASSERT_EQUALS( node_get_stringW( pnURL ), 
			std::wstring( L"dbdoc://tag/D/DBDOC_BUILDS/ëŠ Unicode name longer than what it used to be/TAG/TDT1/TD_TAGLIST_sample_1.DBF/ZZTAG" ) );

		node_free( pn );
	}
	void test_3()
	{
		node_set_codepage( 936 );
		node_t * pn = node_hash_alloc();
		node_hash_addA( pn, "URL", NODE_STRINGA, "dbdoc://tag/D/DBDOC_BUILDS/ëŠ Unicode name longer than what it used to be/TAG/TDT1/TD_TAGLIST_sample_1.DBF/ZZTAG" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpW( pn, pf, 0 );
		fclose( pf );

		node_free( pn );
		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( pn != NULL );

		node_t * pnURL = node_hash_getW( pn, L"URL" );
		TS_ASSERT_EQUALS( node_get_stringW( pnURL ), 
			std::wstring( L"dbdoc://tag/D/DBDOC_BUILDS/\x96FB Unicode name longer than what it used to be/TAG/TDT1/TD_TAGLIST_sample_1.DBF/ZZTAG" ) );

		node_free( pn );
	}

	void test_4()
	{
		/* this is the failure case: */

		/* set BIG5 codepage */
		node_set_codepage( 936 );
		node_t * pn = node_hash_alloc();

		/* add w-string containing Unicode character */
		node_hash_addA( pn, "URL", NODE_STRINGW, L"dbdoc://tag/D/DBDOC_BUILDS/\x96FB Unicode name longer than what it used to be/TAG/TDT1/TD_TAGLIST_sample_1.DBF/ZZTAG" );

		FILE * pf = fopen( g_psFileName, "wb" );
		/* dump as ANSI, converting to CP 936 */
		node_dumpA( pn, pf, 0 );
		fclose( pf );

		node_free( pn );

		/* hyperview running with Western code page */
		node_set_codepage( 1252 );
		pf = fopen( g_psFileName, "rb" );

		/* parse as "W", converting CP 936 data as if it were CP 1252 data */
		int nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( pn != NULL );

		node_t * pnURL = node_hash_getW( pn, L"URL" );

		/* result is a UTF-16 string containing Unicode encoding of the two parts of the CP936 MBCS character */
		TS_ASSERT_EQUALS( std::wstring( node_get_stringW( pnURL ) ), 
			std::wstring( L"dbdoc://tag/D/DBDOC_BUILDS/ëŠ Unicode name longer than what it used to be/TAG/TDT1/TD_TAGLIST_sample_1.DBF/ZZTAG" ) );

		node_free( pn );
	}
};

#define TEST_DATA_SIZE 17

class ParseFromString : public CxxTest::TestSuite
{
public:
	void test_parseA()
	{
		node_t * pn = NULL;
		
		TS_ASSERT( node_parse_from_stringA( ")", &pn ) == NP_CPAREN );
		TS_ASSERT( node_parse_from_stringA( "}", &pn ) == NP_CBRACE );
		TS_ASSERT( node_parse_from_stringA( "", &pn ) == NP_EOF );

		TS_ASSERT( node_parse_from_stringA( "Test: 'Value'", &pn ) == NP_NODE );
		TS_ASSERT( pn != NULL );
		TS_ASSERT( strcmp( node_get_nameA( pn ), "Test" ) == 0 );
		TS_ASSERT( strcmp( node_get_stringA( pn ), "Value" ) == 0 );
		
		node_free( pn );
	}

	void test_parseW()
	{
		node_t * pn = NULL;
		
		TS_ASSERT( node_parse_from_stringW( L")", &pn ) == NP_CPAREN );
		TS_ASSERT( node_parse_from_stringW( L"}", &pn ) == NP_CBRACE );
		TS_ASSERT( node_parse_from_stringW( L"", &pn ) == NP_EOF );

		TS_ASSERT( node_parse_from_stringW( L"Test: 'Value'", &pn ) == NP_NODE );
//		_CrtIsValidHeapPointer( node_get_nameW( pn ) );
		TS_ASSERT( pn != NULL );
		TS_ASSERT( wcscmp( node_get_nameW( pn ), L"Test" ) == 0 );
		TS_ASSERT( wcscmp( node_get_stringW( pn ), L"Value" ) == 0 );
		
		node_free( pn );
	}

	void test_parse()
	{
		node_t * pn = NULL;
		
		TS_ASSERT( node_parse_from_string( _T(")"), &pn ) == NP_CPAREN );
		TS_ASSERT( node_parse_from_string( _T("}"), &pn ) == NP_CBRACE );
		TS_ASSERT( node_parse_from_string( _T(""), &pn ) == NP_EOF );

		TS_ASSERT( node_parse_from_string( _T("Test: 'Value'"), &pn ) == NP_NODE );
		TS_ASSERT( pn != NULL );
		TS_ASSERT( _tcscmp( node_get_name( pn ), _T("Test") ) == 0 );
		TS_ASSERT( _tcscmp( node_get_string( pn ), _T("Value") ) == 0 );
		
		node_free( pn );
	}

	void test_parse_multiline()
	{
		node_t * pn = NULL;
		
		TS_ASSERT( node_parse_from_string( _T("Hash: {\nN1: 'Value'\nN2: 3\n}"), &pn ) == NP_NODE );
		TS_ASSERT( pn != NULL );
		TS_ASSERT( _tcscmp( node_get_name( pn ), _T("Hash") ) == 0 );
		TS_ASSERT( _tcscmp( node_get_string( node_hash_get( pn, _T("N1") ) ), _T("Value") ) == 0 );
		TS_ASSERT( node_get_int( node_hash_get( pn, _T("N2") ) ) == 3 );
		
		node_free( pn );
	}

	void test_parse_autorebalance()
	{
		node_t * pn = NULL;
		_TCHAR * ps = _T("Hash: {\n") \
			_T("0: 1\n1: 1\n2: 1\n3: 1\n4: 1\n5: 1\n6: 1\n7: 1\n8: 1\n9: 1\nA: 1\nB: 1\nC: 1\nD: 1\nE: 1\nF: 1\n" ) \
			_T("10: 1\n11: 1\n12: 1\n13: 1\n14: 1\n15: 1\n16: 1\n17: 1\n18: 1\n19: 1\n1A: 1\n1B: 1\n1C: 1\n1D: 1\n1E: 1\n1F: 1\n" ) \
			_T("20: 1\n21: 1\n22: 1\n23: 1\n24: 1\n25: 1\n26: 1\n27: 1\n28: 1\n29: 1\n2A: 1\n2B: 1\n2C: 1\n2D: 1\n2E: 1\n2F: 1\n" ) \
			_T("30: 1\n31: 1\n32: 1\n33: 1\n34: 1\n35: 1\n36: 1\n37: 1\n38: 1\n39: 1\n3A: 1\n3B: 1\n3C: 1\n3D: 1\n3E: 1\n3F: 1\n" ) \
			_T("40: 1\n41: 1\n42: 1\n43: 1\n44: 1\n45: 1\n46: 1\n47: 1\n48: 1\n49: 1\n4A: 1\n4B: 1\n4C: 1\n4D: 1\n4E: 1\n4F: 1\n" ) \
			_T("50: 1\n51: 1\n52: 1\n53: 1\n54: 1\n55: 1\n56: 1\n57: 1\n58: 1\n59: 1\n5A: 1\n5B: 1\n5C: 1\n5D: 1\n5E: 1\n5F: 1\n" ) \
			_T("60: 1\n61: 1\n62: 1\n63: 1\n64: 1\n65: 1\n66: 1\n67: 1\n68: 1\n69: 1\n6A: 1\n6B: 1\n6C: 1\n6D: 1\n6E: 1\n6F: 1\n" ) \
			_T("70: 1\n71: 1\n72: 1\n73: 1\n74: 1\n75: 1\n76: 1\n77: 1\n78: 1\n79: 1\n7A: 1\n7B: 1\n7C: 1\n7D: 1\n7E: 1\n7F: 1\n" ) \
			_T("80: 1\n81: 1\n82: 1\n83: 1\n84: 1\n85: 1\n86: 1\n87: 1\n88: 1\n89: 1\n8A: 1\n8B: 1\n8C: 1\n8D: 1\n8E: 1\n8F: 1\n" ) \
			_T("90: 1\n91: 1\n92: 1\n93: 1\n94: 1\n95: 1\n96: 1\n97: 1\n98: 1\n99: 1\n9A: 1\n9B: 1\n9C: 1\n9D: 1\n9E: 1\n9F: 1\n" ) \
			_T("A0: 1\nA1: 1\nA2: 1\nA3: 1\nA4: 1\nA5: 1\nA6: 1\nA7: 1\nA8: 1\nA9: 1\nAA: 1\nAB: 1\nAC: 1\nAD: 1\nAE: 1\nAF: 1\n" ) \
			_T("B0: 1\nB1: 1\nB2: 1\nB3: 1\nB4: 1\nB5: 1\nB6: 1\nB7: 1\nB8: 1\nB9: 1\nBA: 1\nBB: 1\nBC: 1\nBD: 1\nBE: 1\nBF: 1\n" ) \
			_T("C0: 1\nC1: 1\nC2: 1\nC3: 1\nC4: 1\nC5: 1\nC6: 1\nC7: 1\nC8: 1\nC9: 1\nCA: 1\nCB: 1\nCC: 1\nCD: 1\nCE: 1\nCF: 1\n" ) \
			_T("D0: 1\nD1: 1\nD2: 1\nD3: 1\nD4: 1\nD5: 1\nD6: 1\nD7: 1\nD8: 1\nD9: 1\nDA: 1\nDB: 1\nDC: 1\nDD: 1\nDE: 1\nDF: 1\n" ) \
			_T("E0: 1\nE1: 1\nE2: 1\nE3: 1\nE4: 1\nE5: 1\nE6: 1\nE7: 1\nE8: 1\nE9: 1\nEA: 1\nEB: 1\nEC: 1\nED: 1\nEE: 1\nEF: 1\n" ) \
			_T("F0: 1\nF1: 1\nF2: 1\nF3: 1\nF4: 1\nF5: 1\nF6: 1\nF7: 1\nF8: 1\nF9: 1\nFA: 1\nFB: 1\nFC: 1\nFD: 1\nFE: 1\nFF: 1\n" ) \
			_T("100: 1\n") \
			_T("}");
		
		TS_ASSERT( node_parse_from_string( ps, &pn ) == NP_NODE );
		TS_ASSERT( pn != NULL );
		TS_ASSERT( _tcscmp( node_get_name( pn ), _T("Hash") ) == 0 );
		TS_ASSERT( node_get_elements( pn ) == 257 );
//		TS_ASSERT( pn->nHashBuckets > 8 );
		
		node_free( pn );
	}

	void test_parse_multiline_len()
	{
		node_t * pn = NULL;
		_TCHAR * psNode = _T("Int: 12");
		int nResult = 0;

		nResult = node_parse_from_data( psNode, 6*sizeof(_TCHAR), &pn );
		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( node_get_int( pn ) == 1 );
		node_free( pn );

		nResult = node_parse_from_data( psNode, 7*sizeof(_TCHAR), &pn );
		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( node_get_int( pn ) == 12 );
		node_free( pn );
	}

	void test_parse_char_lenA()
	{
		char * psNode = "Foo: 'Bar'";
		size_t nBytes = strlen(psNode);

		node_t * pn = NULL;

		int nResult = node_parse_from_dataA( psNode, nBytes, &pn );
		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT_EQUALS( std::string("Foo"), node_get_nameA( pn ) );
		TS_ASSERT_EQUALS( std::string("Bar"), node_get_stringA( pn ) );
		TS_ASSERT_EQUALS( (wchar_t *)0, node_get_nameW( pn ) );
		node_free( pn );

		nResult = node_parse_from_dataW( psNode, nBytes, &pn );
		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( std::wstring(L"Foo") == node_get_nameW( pn ) );
		TS_ASSERT( std::wstring(L"Bar") == node_get_stringW( pn ) );
		TS_ASSERT( (char *)0 == node_get_nameA( pn ) );
		node_free( pn );
	}

	void test_parse_char_lenW()
	{
		wchar_t * psNode = L"Foo: 'Bar'";
		size_t nBytes = sizeof(wchar_t)*wcslen(psNode);

		node_t * pn = NULL;

		int nResult = node_parse_from_dataA( psNode, nBytes, &pn );
		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( std::string("Foo") == node_get_nameA( pn ) );
		TS_ASSERT( std::string("Bar") == node_get_stringA( pn ) );
		TS_ASSERT( NULL == node_get_nameW( pn ) );
		node_free( pn );

		nResult = node_parse_from_dataW( psNode, nBytes, &pn );
		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( std::wstring(L"Foo") == node_get_nameW( pn ) );
		TS_ASSERT( std::wstring(L"Bar") == node_get_stringW( pn ) );
		TS_ASSERT( NULL == node_get_nameA( pn ) );
		node_free( pn );
	}
};

class Arena : public CxxTest::TestSuite
{
public:

	void test_arena()
	{
		node_t * pn = NULL;

		node_arena_t pArena = node_create_arena( 0 );

		node_arena_t pOld = node_set_arena( pArena );

		pn = node_hash_alloc2( 128 );
		node_hash_add( pn, _T("foo"), NODE_INT, 3 );
		node_free( pn );

		pn = node_hash_alloc2( 128 );

		node_set_arena( pOld );
		node_delete_arena( pArena );
	}
};

class Int64 : public CxxTest::TestSuite
{
public:
	void test_create()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_INT64, 0x1234567887654321i64 );

		TS_ASSERT( 0x1234567887654321i64 == node_get_int64( pn ) );

		node_free( pn );
	}

	void test_convert_i6()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_INT, 0x12345678 );

		TS_ASSERT( 0x12345678i64 == node_get_int64( pn ) );

		node_free( pn );
	}

	void test_convert_d6()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_REAL, 214748364700. );

		TS_ASSERT( 214748364700i64 == node_get_int64( pn ) );

		node_free( pn );
	}

	void test_convert_a6()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_STRINGA, "214748364700" );

		TS_ASSERT( 214748364700i64 == node_get_int64( pn ) );

		node_free( pn );
	}

	void test_convert_w6()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_STRINGW, L"214748364700" );

		TS_ASSERT( 214748364700i64 == node_get_int64( pn ) );

		node_free( pn );
	}

	void test_convert_6i_notrunc()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_INT64, 0x12345678 );

		TS_ASSERT( 0x12345678 == node_get_int( pn ) );

		node_free( pn );
	}

	void test_convert_6i_trunc()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_INT64, 2*2147483648i64 );

		TS_ASSERT( 0 == node_get_int( pn ) );

		node_set( pn, NODE_INT64, 2*2147483648i64 + 1 );

		TS_ASSERT( 1 == node_get_int( pn ) );

		node_free( pn );
	}

	void test_convert_6d_notrunc()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_INT64, 0xFFFFFFFFFFFFF );

		TS_ASSERT( 4503599627370495. == node_get_real( pn ) );

		node_free( pn );
	}

	void test_convert_6d_trunc()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_INT64, 0xFFFFFFFFFFFFF0 );

		TS_ASSERT( 72057594037927920. == node_get_real( pn ) );

		node_set( pn, NODE_INT64, 0xFFFFFFFFFFFFF1 );

		double d = node_get_real( pn );
		TS_ASSERT( 72057594037927920. == d );

		node_free( pn );
	}

	void test_convert_6a()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_INT64, 214748364700i64 );

		TS_ASSERT_EQUALS( std::string("214748364700"), node_get_stringA( pn ) );
		TS_ASSERT_EQUALS( std::string("214748364700"), node_get_stringA( pn ) );

		node_free( pn );
	}

	void test_convert_6w()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_INT64, 214748364700i64 );

		TS_ASSERT_EQUALS( std::wstring(L"214748364700"), node_get_stringW( pn ) );
		TS_ASSERT_EQUALS( std::wstring(L"214748364700"), node_get_stringW( pn ) );

		node_free( pn );
	}

	void test_copy_6()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_INT64, 214748364700i64 );

		node_t * pnCopy = node_copy( pn );

		TS_ASSERT_EQUALS( 214748364700i64, node_get_int64( pnCopy ) );

		node_free( pnCopy );
		node_free( pn );
	}

	void test_parse_6a()
	{
		const char * ps = ": 214748364700L\r\n";

		node_t * pn = NULL;

		int nResult = node_parse_from_stringA( ps, &pn );

		TS_ASSERT_EQUALS( nResult, NP_NODE );
		TS_ASSERT_EQUALS( NODE_INT64, node_get_type( pn ) );
		TS_ASSERT_EQUALS( 214748364700i64, node_get_int64(pn) );

		node_free( pn );
	}

	void test_parse_6w()
	{
		const wchar_t * ps = L": 214748364700L\r\n";

		node_t * pn = NULL;

		int nResult = node_parse_from_stringW( ps, &pn );

		TS_ASSERT_EQUALS( nResult, NP_NODE );
		TS_ASSERT_EQUALS( NODE_INT64, node_get_type( pn ) );
		TS_ASSERT_EQUALS( 214748364700i64, node_get_int64(pn) );

		node_free( pn );
	}

	void test_dump_6a()
	{
		node_t * pn = node_alloc();
		node_t * pnRead = NULL;
		node_set( pn, NODE_INT64, 214748364700i64 );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 );
		fclose( pf );

		char acBuffer[1024];
		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );

		const char * psExpected = ": 214748364700L  (0x00000031FFFFFF9C)\r\n";
		TS_ASSERT( strncmp( psExpected, acBuffer, strlen(psExpected) ) == 0 );

		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parse( pf, &pnRead );

		TS_ASSERT_EQUALS( nResult, NP_NODE );
		TS_ASSERT_EQUALS( NODE_INT64, node_get_type( pnRead ) );
		TS_ASSERT_EQUALS( node_get_int64( pn ), node_get_int64( pnRead ) );

		node_free( pn );
		node_free( pnRead );
	}

	void test_dump_6w()
	{
		node_t * pn = node_alloc();
		node_t * pnRead = NULL;
		node_set( pn, NODE_INT64, 214748364700i64 );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpW( pn, pf, 0 );
		fclose( pf );

		wchar_t acBuffer[1024];
		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 2, sizeof(acBuffer)/2, pf );
		fclose( pf );

		const wchar_t * psExpected = L": 214748364700L  (0x00000031FFFFFF9C)\r\n";
		TS_ASSERT( wcsncmp( psExpected, acBuffer, wcslen(psExpected) ) == 0 );

		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parse( pf, &pnRead );

		TS_ASSERT_EQUALS( nResult, NP_NODE );
		TS_ASSERT_EQUALS( NODE_INT64, node_get_type( pnRead ) );
		TS_ASSERT_EQUALS( node_get_int64( pn ), node_get_int64( pnRead ) );

		node_free( pn );
		node_free( pnRead );
	}

	void test_get_list()
	{
		node_t * pnList = node_list_alloc();
		node_list_add( pnList, NODE_INT64, 1i64 );

		TS_ASSERT_EQUALS( node_get_int64( pnList ), 1i64 );

		node_free( pnList );
	}
};

class UnicodeAscii : public CxxTest::TestSuite
{
	int m_nOldDebug;
public:
	void setUp()
	{
		/* ensure that NODE_DEBUG_UNICODE is set */
		m_nOldDebug = node_get_debug();
		node_set_debug( m_nOldDebug&NODE_DEBUG_UNICODE );
	}
	void tearDown()
	{
		node_set_debug( m_nOldDebug );
	}
	void test_unicode()
	{
		node_t * pn = node_hash_alloc();

		node_set( pn, NODE_STRINGA, "LongEnough" );

		TS_ASSERT_THROWS_ANYTHING( node_set( pn, NODE_STRINGA, L"LongEnough" ) );

		node_free( pn );
	}

	void test_ascii()
	{
		node_t * pn = node_hash_alloc();

		node_set( pn, NODE_STRINGW, L"LongEnough" );

		TS_ASSERT_THROWS_ANYTHING( node_set( pn, NODE_STRINGW, "LongEnough" ) );

		node_free( pn );
	}
};

class BasicNode : public CxxTest::TestSuite
{
	node_t * pn;
	data_t ad[TEST_DATA_SIZE];
public:
	BasicNode()
	{
		for( unsigned char i = 0; i < TEST_DATA_SIZE; i++ )
			ad[i] = i;
	}

	void test_free_null()
	{
		node_free( NULL );
	}

    void test_Start()
    {
        wait();
    }

	void setUp() { pn = node_alloc(); }
	void tearDown() { node_free( pn ); }

	void test_Alloc()
	{
		TS_ASSERT( pn != NULL );
        wait();
	}

	void test_Set_Int()
	{
		node_set( pn, NODE_INT, 3 );
		TS_ASSERT_EQUALS( node_get_int( pn ), 3 );
        wait();
	}

	void test_Set_Float()
	{
		node_set( pn, NODE_REAL, 3.0 );
		TS_ASSERT_EQUALS( node_get_real( pn ), 3.0 );
        wait();
	}

	void test_Set_StringA()
	{
		node_set( pn, NODE_STRINGA, "Three" );
		TS_ASSERT( strcmp( node_get_stringA( pn ), "Three" ) == 0 );
        wait();
	}

	void test_Set_StringA_2()
	{
		node_set( pn, NODE_STRINGA, "Three" );
		TS_ASSERT( strcmp( node_get_stringA( pn ), "Three" ) == 0 );

		node_set( pn, NODE_STRINGA, node_get_stringA( pn ) );
		TS_ASSERT( strcmp( node_get_stringA( pn ), "Three" ) == 0 );
        wait();
	}

	void test_Set_StringW()
	{
		node_set( pn, NODE_STRINGW, L"Three" );
		TS_ASSERT( wcscmp( node_get_stringW( pn ), L"Three" ) == 0 );
        wait();
	}

	void test_Set_StringW_2()
	{
		node_set( pn, NODE_STRINGW, L"Three" );
		TS_ASSERT( wcscmp( node_get_stringW( pn ), L"Three" ) == 0 );

		node_set( pn, NODE_STRINGW, node_get_stringW( pn ) );
		TS_ASSERT( wcscmp( node_get_stringW( pn ), L"Three" ) == 0 );
        wait();
	}

	void checkData( int nLengthIn, char * sMessage )
	{
		int nLengthOut;
		const data_t * pd = NULL;

		TSM_ASSERT( sMessage, nLengthIn <= TEST_DATA_SIZE );

		node_set_data( pn, nLengthIn, ad );
		pd = node_get_data( pn, &nLengthOut );

		TSM_ASSERT_EQUALS( sMessage, nLengthOut, nLengthIn );
		TSM_ASSERT_SAME_DATA( sMessage, ad, pd, nLengthOut );
        wait();
	}

	void test_Data1()
	{
		checkData( 1, "1 Byte" );
	}

	void test_Data8()
	{
		checkData( 8, "8 Bytes" );
	}

	void test_Data15()
	{
		checkData( 15, "15 Bytes" );
	}
	
	void test_Data16()
	{
		checkData( 16, "16 Bytes" );
	}

	void test_Data17()
	{
		checkData( 17, "17 Bytes" );
	}

	void test_DataSizeChange()
	{
		int nLengthOut;
		const data_t * pd = NULL;
		int nLengthIn = 17;

		node_set_data( pn, nLengthIn, ad );
		pd = node_get_data( pn, &nLengthOut );

		TS_ASSERT_EQUALS( nLengthOut, nLengthIn );
		TS_ASSERT_SAME_DATA( ad, pd, nLengthOut );
        wait();

		nLengthIn = 15;
		node_set_data( pn, nLengthIn, pd );
		pd = node_get_data( pn, &nLengthOut );
		TS_ASSERT_EQUALS( nLengthOut, nLengthIn );
		TS_ASSERT_SAME_DATA( ad, pd, nLengthOut );
	}

	void test_DataSizeChangeGrow()
	{
		int nLengthOut;
		const data_t * pd = NULL;
		int nLengthIn = 17;

		node_set_data( pn, nLengthIn, ad );
		pd = node_get_data( pn, &nLengthOut );

		TS_ASSERT_EQUALS( nLengthOut, nLengthIn );
		TS_ASSERT_SAME_DATA( ad, pd, nLengthOut );
        wait();

		nLengthIn = 15;
		node_set_data( pn, nLengthIn, pd );
		pd = node_get_data( pn, &nLengthOut );
		TS_ASSERT_EQUALS( nLengthOut, nLengthIn );
		TS_ASSERT_SAME_DATA( ad, pd, nLengthOut );

		nLengthIn = 17;
		TS_ASSERT_THROWS_ANYTHING( node_set_data( pn, nLengthIn, pd ) );
	}
};

class ConvertNode : public CxxTest::TestSuite
{
	node_t * pn;
public:
	void setUp() { pn = node_alloc(); }
	void tearDown() { node_free( pn ); }

	void test_Int_to_Real()
	{
		node_set( pn, NODE_INT, 3 );
		TS_ASSERT_EQUALS( node_get_real( pn ), 3.0 );
		wait();
	}

	void test_Int_To_StringA()
	{
		node_set( pn, NODE_INT, 3 );
		TS_ASSERT_SAME_DATA( node_get_stringA( pn ), "3", 2 );

		node_set( pn, NODE_INT, 4 );
		TS_ASSERT_EQUALS( node_get_stringA( pn ), std::string( "4" ) );

		/* second time to get coverage for case when string value is already set */
		TS_ASSERT_EQUALS( node_get_stringA( pn ), std::string( "4" ) );
	}

	void test_Int_To_StringW()
	{
		node_set( pn, NODE_INT, 3 );
		TS_ASSERT_SAME_DATA( node_get_stringW( pn ), L"3", 4 );

		node_set( pn, NODE_INT, 4 );
		TS_ASSERT_EQUALS( node_get_stringW( pn ), std::wstring( L"4" ) );

		/* second time to get coverage for case when string value is already set */
		TS_ASSERT_EQUALS( node_get_stringW( pn ), std::wstring( L"4" ) );
	}

	void test_Real_to_Int_a()
	{
		node_set( pn, NODE_REAL, 3.1 );
		TS_ASSERT_EQUALS( node_get_int( pn ), 3 );
		wait();
	}

	void test_Real_to_Int_b()
	{
		node_set( pn, NODE_REAL, 3.9 );
		TS_ASSERT_EQUALS( node_get_int( pn ), 3 );
		wait();
	}

	void test_Real_to_Int_c()
	{
		node_set( pn, NODE_REAL, -3.9 );
		TS_ASSERT_EQUALS( node_get_int( pn ), -3 );
		wait();
	}

	void test_Real_to_StringA()
	{
		node_set( pn, NODE_REAL, 3.1 );
		TS_ASSERT_SAME_DATA( node_get_stringA( pn ), "3.1", 3 );
		TS_ASSERT_SAME_DATA( node_get_stringA( pn ), "3.10", 4 );
		wait();
	}

	void test_Real_to_StringW()
	{
		node_set( pn, NODE_REAL, 3.1 );
		TS_ASSERT_SAME_DATA( node_get_stringW( pn ), L"3.1", 6 );
		TS_ASSERT_SAME_DATA( node_get_stringW( pn ), L"3.10", 8 );
		wait();
	}

	void test_StringA_To_Int_valid()
	{
		node_set( pn, NODE_STRINGA, "5foo" );
		TS_ASSERT_EQUALS( node_get_int( pn ), 5 );
		wait();
	}

	void test_StringA_To_Int_invalid()
	{
		node_set( pn, NODE_STRINGA, "foo5" );
		TS_ASSERT_EQUALS( node_get_int( pn ), 0 );
		wait();
	}

	void test_StringA_To_Real_valid()
	{
		node_set( pn, NODE_STRINGA, "5.1foo" );
		TS_ASSERT_EQUALS( node_get_real( pn ), 5.1 );
		wait();
	}

	void test_StringA_To_Real_invalid()
	{
		node_set( pn, NODE_STRINGA, "foo5" );
		TS_ASSERT_EQUALS( node_get_real( pn ), 0. );
		wait();
	}

	void test_StringW_To_Int_valid()
	{
		node_set( pn, NODE_STRINGW, L"5foo" );
		TS_ASSERT_EQUALS( node_get_int( pn ), 5 );
		wait();
	}

	void test_StringW_To_Int_invalid()
	{
		node_set( pn, NODE_STRINGW, L"foo5" );
		TS_ASSERT_EQUALS( node_get_int( pn ), 0 );
		wait();
	}

	void test_StringW_To_Real_valid()
	{
		node_set( pn, NODE_STRINGW, L"5.1foo" );
		TS_ASSERT_EQUALS( node_get_real( pn ), 5.1 );
		wait();
	}

	void test_StringW_To_Real_invalid()
	{
		node_set( pn, NODE_STRINGW, L"foo5" );
		TS_ASSERT_EQUALS( node_get_real( pn ), 0. );
		wait();
	}

	void test_Int_to_Data()
	{
		int nLength;
		node_set( pn, NODE_INT, 3 );
		try
		{
			node_get_data( pn, &nLength );
		}
		catch( ... )
		{
			goto end;
		}
		TS_FAIL( "Should have thrown exception." );
end:
		wait();
	}

	void test_Data_to_Int()
	{
		int nValue = 0x12345678;
		node_set_data( pn, 4, &nValue );
		TS_ASSERT_THROWS_ANYTHING( node_get_int( pn ) );
		wait();
	}

	void test_StringA_to_StringW()
	{
		node_set( pn, NODE_STRINGA, "Value" );

		TS_ASSERT_EQUALS( node_get_stringW( pn ), std::wstring( L"Value" ) );
	}

	void test_StringW_to_StringA()
	{
		node_set( pn, NODE_STRINGW, L"Value" );

		TS_ASSERT_EQUALS( node_get_stringA( pn ), std::string( "Value" ) );
	}
};

class RealNode : public CxxTest::TestSuite
{
public:

	void test_dumpA()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_REAL, 4.310 );
		FILE * pf = fopen( g_psFileName, "wb" );

		node_dumpA( pn, pf, 0 );
		fclose( pf );

		char acBuffer[1024];
		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );

		char * psResult = ": 4.310";
		TS_ASSERT( strncmp( acBuffer, psResult, strlen( psResult ) ) == 0 );
		node_free( pn );
	}

	void test_parseA()
	{
		node_t * pn = NULL;

		FILE * pf = fopen( g_psFileName, "wb" );
		fputs( ": 4.310\r\n", pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( node_get_type( pn ) == NODE_REAL );
		TS_ASSERT_DELTA( node_get_real( pn ), 4.310, 0.001 );
		node_free( pn );
	}

	void test_dumpW()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_REAL, 4.310 );
		FILE * pf = fopen( g_psFileName, "wb" );

		node_dumpW( pn, pf, 0 );
		fclose( pf );

		wchar_t acBuffer[1024];
		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );

		wchar_t * psResult = L": 4.310";
		TS_ASSERT( wcsncmp( acBuffer, psResult, wcslen( psResult ) ) == 0 );

		node_free( pn );
	}

	void test_parseW()
	{
		node_t * pn = NULL;

		FILE * pf = fopen( g_psFileName, "wb" );
		fputws( L": 4.310\r\n", pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( node_get_type( pn ) == NODE_REAL );
		TS_ASSERT_DELTA( node_get_real( pn ), 4.310, 0.001 );
		node_free( pn );
	}

	void test_copy()
	{
		node_t * pn = node_alloc();

		node_set( pn, NODE_REAL, 4.310 );

		node_t * pnCopy = node_copy( pn );

		TS_ASSERT( node_get_type( pnCopy ) == NODE_REAL );
		TS_ASSERT_DELTA( node_get_real( pnCopy ), node_get_real( pn ), .001 );

		node_free( pnCopy );
		node_free( pn );
	}
};

class PtrNode : public CxxTest::TestSuite
{
public:
	void test_create()
	{
		node_t * pn = node_alloc();

		/* store a pointer */
		node_set( pn, NODE_PTR, pn );

		TS_ASSERT( node_get_type( pn ) == NODE_PTR );
		TS_ASSERT( node_get_ptr( pn ) == (void *)pn );

		node_free( pn );
	}
	/* TODO: confirm throw on attempt to convert */

	void test_copy()
	{
		node_t * pn = node_alloc();

		/* store a pointer */
		node_set( pn, NODE_PTR, pn );

		TS_ASSERT( node_get_type( pn ) == NODE_PTR );
		TS_ASSERT( node_get_ptr( pn ) == (void *)pn );

		node_t * pnCopy = node_copy( pn );

		TS_ASSERT( node_get_type( pnCopy ) == NODE_PTR );
		TS_ASSERT( node_get_ptr( pnCopy ) == (void *)pn );

		node_free( pn );
		node_free( pnCopy );
	}

	void test_dumpA()
	{
		node_t * pn = node_alloc();

		/* store a pointer */
		node_set( pn, NODE_PTR, pn );

		TS_ASSERT( node_get_type( pn ) == NODE_PTR );
		TS_ASSERT( node_get_ptr( pn ) == (void *)pn );

//		node_dump( pn, stderr, 0 );

		FILE * pf = fopen( g_psFileName, "wb" );
		TS_ASSERT( pf != NULL );

		node_dumpA( pn, pf, 0 );

		fclose( pf );
		pf = fopen( g_psFileName, "rb" );

		node_t * pnNew = NULL;

		node_parseA( pf, &pnNew );

		fclose( pf );

		TS_ASSERT( node_get_type( pnNew ) == NODE_PTR );
		TS_ASSERT( node_get_ptr( pnNew ) == NULL );

		node_free( pn );
		node_free( pnNew );

		remove( g_psFileName );
	}

	void test_dumpW()
	{
		node_t * pn = node_alloc();

		/* store a pointer */
		node_set( pn, NODE_PTR, pn );

		TS_ASSERT( node_get_type( pn ) == NODE_PTR );
		TS_ASSERT( node_get_ptr( pn ) == (void *)pn );

//		node_dump( pn, stderr, 0 );

		FILE * pf = fopen( g_psFileName, "wb" );
		TS_ASSERT( pf != NULL );

		node_dumpW( pn, pf, 0 );

		fclose( pf );
		pf = fopen( g_psFileName, "rb" );

		node_t * pnNew = NULL;

		node_parseW( pf, &pnNew );

		fclose( pf );

		TS_ASSERT( node_get_type( pnNew ) == NODE_PTR );
		TS_ASSERT( node_get_ptr( pnNew ) == NULL );

		node_free( pn );
		node_free( pnNew );

		remove( g_psFileName );
	}

};


class ListNode : public CxxTest::TestSuite
{
	node_t * pnList;
public:
	void setUp() { pnList = node_list_alloc(); }
	void tearDown() { node_free( pnList ); }

	void test_Exists()
	{
		TS_ASSERT( pnList != NULL );
		TS_ASSERT( node_get_type( pnList ) == NODE_LIST );
		wait();
	}

	void test_Add()
	{
		node_list_add( pnList, NODE_INT, 1 );
		TS_ASSERT( node_first( pnList ) != NULL );
		TS_ASSERT( node_get_int( node_first( pnList ) ) == 1 );
		wait();
	}

	void test_Add2()
	{
		node_list_add( pnList, NODE_INT, 1 );
		node_list_add( pnList, NODE_INT, 2 );
		TS_ASSERT( node_next( node_first( pnList ) ) != NULL );
		TS_ASSERT( node_get_int( node_next( node_first( pnList ) ) ) == 2 );
		TS_ASSERT( node_get_elements( pnList ) == 2 );
		wait();
	}

	void test_Convert()
	{
		node_list_add( pnList, NODE_INT, 1 );
		node_list_add( pnList, NODE_INT, 2 );
		TS_ASSERT( node_get_int( pnList ) == 1 );
		TS_ASSERT( node_get_elements( pnList ) == 2 );
		wait();
	}

	void test_Delete1()
	{
		node_list_add( pnList, NODE_INT, 1 );
		node_t * pn = node_first( pnList );
		node_list_delete( pnList, pn );
		node_free( pn );

		TS_ASSERT( node_first( pnList ) == NULL );
		TS_ASSERT( node_get_elements( pnList ) == 0 );
		wait();
	}

	void test_Delete2()
	{
		node_list_add( pnList, NODE_INT, 1 );
		node_list_add( pnList, NODE_INT, 2 );
		node_t * pn = node_first( pnList );
		node_list_delete( pnList, pn );
		node_free( pn );

		TS_ASSERT( node_first( pnList ) != NULL );
		TS_ASSERT( node_get_int( pnList ) == 2 );
		TS_ASSERT( node_get_elements( pnList ) == 1 );
		wait();
	}

	void test_Delete3()
	{
		node_list_add( pnList, NODE_INT, 1 );
		node_list_add( pnList, NODE_INT, 2 );
		node_t * pn = node_next( node_first( pnList ) );
		node_list_delete( pnList, pn );
		node_free( pn );

		TS_ASSERT( node_first( pnList ) != NULL );
		TS_ASSERT( node_get_int( pnList ) == 1 );
		TS_ASSERT( node_get_elements( pnList ) == 1 );
		wait();
	}

	void test_Repeated()
	{
		for( int i = 0; i < 3; i++ )
		{
			for( int j = 0; j < 3; j++ )
				node_list_add( pnList, NODE_INT, j );

			while( node_first( pnList ) != NULL )
			{
				node_t * pn = node_first( pnList );
				node_list_delete( pnList, pn );
				node_free( pn );
			}
		}
	}

	void test_Repeated2()
	{
		for( int i = 0; i < 3; i++ )
		{
			for( int j = 0; j < 3; j++ )
				node_list_add( pnList, NODE_INT, j );

			while( node_next( node_first( pnList ) ) != NULL )
			{
				node_t * pn = node_next( node_first( pnList ) );
				node_list_delete( pnList, pn );
				node_free( pn );
			}
		}
	}

	void test_Push()
	{
		int i;
		node_t * pn = NULL;

		for( i = 3; i > 0; i-- )
			node_push( pnList, NODE_INT, i );

		for( i = 1, pn = node_first( pnList ); pn != NULL; pn = node_next( pn), ++i )
			TS_ASSERT_EQUALS( i, node_get_int( pn ) );
	}

	void test_Pop()
	{
		int i;
		node_t * pn = NULL;

		for( i = 3; i > 0; i-- )
			node_push( pnList, NODE_INT, i );

		for( i = 1, pn = node_pop( pnList ); pn != NULL; pn = node_pop( pnList ), ++i )
		{
			TS_ASSERT_EQUALS( i, node_get_int( pn ) );
			node_free( pn );
		}
	}

	void test_Pop_Underflow()
	{
		node_push( pnList, NODE_INT, 1 );
		node_push( pnList, NODE_INT, 2 );

		node_free( node_pop( pnList ) );
		node_free( node_pop( pnList ) );

		/* underflow - should not assert, throw, or crash */
		node_free( node_pop( pnList ) );
	}

	void test_Copy()
	{
		int i;
		node_t * pn = NULL;

		for( i = 3; i > 0; i-- )
			node_push( pnList, NODE_INT, i );

		node_t * pnCopy = node_copy( pnList );
		node_free( pnList );
		pnList = pnCopy;

		for( i = 1, pn = node_first( pnList ); pn != NULL; pn = node_next( pn), ++i )
			TS_ASSERT_EQUALS( i, node_get_int( pn ) );
	}
};

class StringEscape : public CxxTest::TestSuite
{
public:
	void test_EscapeNullA()
	{
		node_t * pn = node_alloc();
		char acBuffer[1024] = {0};
		char * psExpected = "Name: 'Value'\r\n";

		node_set_nameA( pn, "Name" );
		node_set( pn, NODE_STRINGA, "Value" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::string(acBuffer), std::string(psExpected) );
		node_free( pn );
	}

	void test_EscapeNullW()
	{
		node_t * pn = node_alloc();
		wchar_t acBuffer[1024] = {0};
		wchar_t * psExpected = L"Name: 'Value'\r\n";

		node_set_nameW( pn, L"Name" );
		node_set( pn, NODE_STRINGW, L"Value" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpW( pn, pf, 0 ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::wstring(acBuffer), std::wstring(psExpected) );
		node_free( pn );
	}

	// DO_NOESCAPE
	void test_NoEscapeNullA()
	{
		node_t * pn = node_alloc();
		char acBuffer[1024] = {0};
		char * psExpected = "Name: \"Value\"\r\n";

		node_set_nameA( pn, "Name" );
		node_set( pn, NODE_STRINGA, "Value" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, DO_NOESCAPE ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::string(acBuffer), std::string(psExpected) );
		node_free( pn );
	}

	void test_NoEscapeNullW()
	{
		node_t * pn = node_alloc();
		wchar_t acBuffer[1024] = {0};
		wchar_t * psExpected = L"Name: \"Value\"\r\n";

		node_set_nameW( pn, L"Name" );
		node_set( pn, NODE_STRINGW, L"Value" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpW( pn, pf, DO_NOESCAPE ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::wstring(acBuffer), std::wstring(psExpected) );
		node_free( pn );
	}

	// now try it with escape chars
	void test_EscapeA()
	{
		node_t * pn = node_alloc();
		char acBuffer[1024] = {0};
		char * psExpected = "Name%0D%0A%25%3A: 'Value%0D%0A%25%3A'\r\n";

		node_set_nameA( pn, "Name\r\n%:" );
		node_set( pn, NODE_STRINGA, "Value\r\n%:" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::string(acBuffer), std::string(psExpected) );
		node_free( pn );
	}

	void test_EscapeW()
	{
		node_t * pn = node_alloc();
		wchar_t acBuffer[1024] = {0};
		wchar_t * psExpected = L"Name%0D%0A%25%3A: 'Value%0D%0A%25%3A'\r\n";

		node_set_nameW( pn, L"Name\r\n%:" );
		node_set( pn, NODE_STRINGW, L"Value\r\n%:" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpW( pn, pf, 0 ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::wstring(acBuffer), std::wstring(psExpected) );
		node_free( pn );
	}

	// DO_NOESCAPE
	void test_NoEscapeA()
	{
		node_t * pn = node_alloc();
		char acBuffer[1024] = {0};
		char * psExpected = "Name%0D%0A%25%3A: \"Value\r\n%:\"\r\n";

		node_set_nameA( pn, "Name\r\n%:" );
		node_set( pn, NODE_STRINGA, "Value\r\n%:" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, DO_NOESCAPE ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::string(acBuffer), std::string(psExpected) );
		node_free( pn );
	}

	void test_NoEscapeW()
	{
		node_t * pn = node_alloc();
		wchar_t acBuffer[1024] = {0};
		wchar_t * psExpected = L"Name%0D%0A%25%3A: \"Value\r\n%:\"\r\n";

		node_set_nameW( pn, L"Name\r\n%:" );
		node_set( pn, NODE_STRINGW, L"Value\r\n%:" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpW( pn, pf, DO_NOESCAPE ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::wstring(acBuffer), std::wstring(psExpected) );
		node_free( pn );
	}

	// now -- all the same tests again, but dumping the A data to W and vice-versa!
	void test_EscapeNullA_toW()
	{
		node_t * pn = node_alloc();
		wchar_t acBuffer[1024] = {0};
		wchar_t * psExpected = L"Name: 'Value'\r\n";

		node_set_nameA( pn, "Name" );
		node_set( pn, NODE_STRINGA, "Value" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpW( pn, pf, 0 ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::wstring(acBuffer), std::wstring(psExpected) );
		node_free( pn );
	}

	void test_EscapeNullW_toA()
	{
		node_t * pn = node_alloc();
		char acBuffer[1024] = {0};
		char * psExpected = "Name: 'Value'\r\n";

		node_set_nameW( pn, L"Name" );
		node_set( pn, NODE_STRINGW, L"Value" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::string(acBuffer), std::string(psExpected) );
		node_free( pn );
	}

	// DO_NOESCAPE
	void test_NoEscapeNullA_toW()
	{
		node_t * pn = node_alloc();
		wchar_t acBuffer[1024] = {0};
		wchar_t * psExpected = L"Name: \"Value\"\r\n";

		node_set_nameA( pn, "Name" );
		node_set( pn, NODE_STRINGA, "Value" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpW( pn, pf, DO_NOESCAPE ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::wstring(acBuffer), std::wstring(psExpected) );
		node_free( pn );
	}

	void test_NoEscapeNullW_toA()
	{
		node_t * pn = node_alloc();
		char acBuffer[1024] = {0};
		char * psExpected = "Name: \"Value\"\r\n";

		node_set_nameW( pn, L"Name" );
		node_set( pn, NODE_STRINGW, L"Value" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, DO_NOESCAPE ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::string(acBuffer), std::string(psExpected) );
		node_free( pn );
	}

	// now try it with escape chars
	void test_EscapeA_toW()
	{
		node_t * pn = node_alloc();
		wchar_t acBuffer[1024] = {0};
		wchar_t * psExpected = L"Name%0D%0A%25%3A: 'Value%0D%0A%25%3A'\r\n";

		node_set_nameA( pn, "Name\r\n%:" );
		node_set( pn, NODE_STRINGA, "Value\r\n%:" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpW( pn, pf, 0 ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::wstring(acBuffer), std::wstring(psExpected) );
		node_free( pn );
	}

	void test_EscapeW_toA()
	{
		node_t * pn = node_alloc();
		char acBuffer[1024] = {0};
		char * psExpected = "Name%0D%0A%25%3A: 'Value%0D%0A%25%3A'\r\n";

		node_set_nameW( pn, L"Name\r\n%:" );
		node_set( pn, NODE_STRINGW, L"Value\r\n%:" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::string(acBuffer), std::string(psExpected) );
		node_free( pn );
	}

	// DO_NOESCAPE
	void test_NoEscapeA_toW()
	{
		node_t * pn = node_alloc();
		wchar_t acBuffer[1024] = {0};
		wchar_t * psExpected = L"Name%0D%0A%25%3A: \"Value\r\n%:\"\r\n";

		node_set_nameA( pn, "Name\r\n%:" );
		node_set( pn, NODE_STRINGA, "Value\r\n%:" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpW( pn, pf, DO_NOESCAPE ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::wstring(acBuffer), std::wstring(psExpected) );
		node_free( pn );
	}

	void test_NoEscapeW_toA()
	{
		node_t * pn = node_alloc();
		char acBuffer[1024] = {0};
		char * psExpected = "Name%0D%0A%25%3A: \"Value\r\n%:\"\r\n";

		node_set_nameW( pn, L"Name\r\n%:" );
		node_set( pn, NODE_STRINGW, L"Value\r\n%:" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, DO_NOESCAPE ); /* default - with escaping */
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		fread( acBuffer, 1, sizeof(acBuffer), pf );
		fclose( pf );
	
		TS_ASSERT_EQUALS( std::string(acBuffer), std::string(psExpected) );
		node_free( pn );
	}

	void test_parseEscapedA()
	{
		char psNode[] = "Name%0D%0A%3A%25: 'Foo%20Bar'";
		node_t * pn = NULL;

		int nResult = node_parse_from_dataA( psNode, sizeof(psNode), &pn );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT_EQUALS( std::string( node_get_nameA( pn ) ), "Name\r\n:%" );
		TS_ASSERT_EQUALS( std::string( node_get_stringA( pn ) ), "Foo Bar" );

		node_free( pn );
	}

	void test_parseUnEscapedA()
	{
		char psNode[] = "Name%0D%0A%3A%25: \"Foo%20Bar\"";
		node_t * pn = NULL;

		int nResult = node_parse_from_dataA( psNode, sizeof(psNode), &pn );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT_EQUALS( std::string( node_get_nameA( pn ) ), "Name\r\n:%" );
		TS_ASSERT_EQUALS( std::string( node_get_stringA( pn ) ), "Foo%20Bar" );

		node_free( pn );
	}

	void test_parseEscapedW()
	{
		wchar_t psNode[] = L"Name%0D%0A%3A%25: 'Foo%20Bar'";
		node_t * pn = NULL;

		int nResult = node_parse_from_dataW( psNode, sizeof(psNode), &pn );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT_EQUALS( std::wstring( node_get_nameW( pn ) ), L"Name\r\n:%" );
		TS_ASSERT_EQUALS( std::wstring( node_get_stringW( pn ) ), L"Foo Bar" );

		node_free( pn );
	}

	void test_parseUnEscapedW()
	{
		wchar_t psNode[] = L"Name%0D%0A%3A%25: \"Foo%20Bar\"";
		node_t * pn = NULL;

		int nResult = node_parse_from_dataW( psNode, sizeof(psNode), &pn );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT_EQUALS( std::wstring( node_get_nameW( pn ) ), L"Name\r\n:%" );
		TS_ASSERT_EQUALS( std::wstring( node_get_stringW( pn ) ), L"Foo%20Bar" );

		node_free( pn );
	}

	void test_parseEscapedA_fromW()
	{
		wchar_t psNode[] = L"Name%0D%0A%3A%25: 'Foo%20Bar'";
		node_t * pn = NULL;

		int nResult = node_parse_from_dataA( psNode, sizeof(psNode), &pn );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT_EQUALS( std::string( node_get_nameA( pn ) ), "Name\r\n:%" );
		TS_ASSERT_EQUALS( std::string( node_get_stringA( pn ) ), "Foo Bar" );

		node_free( pn );
	}

	void test_parseUnEscapedA_fromW()
	{
		wchar_t psNode[] = L"Name%0D%0A%3A%25: \"Foo%20Bar\"";
		node_t * pn = NULL;

		int nResult = node_parse_from_dataA( psNode, sizeof(psNode), &pn );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT_EQUALS( std::string( node_get_nameA( pn ) ), "Name\r\n:%" );
		TS_ASSERT_EQUALS( std::string( node_get_stringA( pn ) ), "Foo%20Bar" );

		node_free( pn );
	}

	void test_parseEscapedW_fromA()
	{
		char psNode[] = "Name%0D%0A%3A%25: 'Foo%20Bar'";
		node_t * pn = NULL;

		int nResult = node_parse_from_dataW( psNode, sizeof(psNode), &pn );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT_EQUALS( std::wstring( node_get_nameW( pn ) ), L"Name\r\n:%" );
		TS_ASSERT_EQUALS( std::wstring( node_get_stringW( pn ) ), L"Foo Bar" );

		node_free( pn );
	}

	void test_parseUnEscapedW_fromA()
	{
		char psNode[] = "Name%0D%0A%3A%25: \"Foo%20Bar\"";
		node_t * pn = NULL;

		int nResult = node_parse_from_dataW( psNode, sizeof(psNode), &pn );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT_EQUALS( std::wstring( node_get_nameW( pn ) ), L"Name\r\n:%" );
		TS_ASSERT_EQUALS( std::wstring( node_get_stringW( pn ) ), L"Foo%20Bar" );

		node_free( pn );
	}
};

#define ERROR_SETUP		int __t, __nError = g_nErrors
#define ERROR_AFTER		(__t = g_nErrors - __nError, __nError = g_nErrors, __t )

class ParseErrors : public CxxTest::TestSuite
{
public:
	void setUp()
	{
		node_set_error_funcs( node_error_count, node_memory, (node_assert_func_t)node_assert );
	}
	void tearDown()
	{
		node_set_error_funcs( node_error, node_memory, (node_assert_func_t)node_assert );
	}

	void test_noColonA()
	{
		node_t * pn = NULL;

		TS_ASSERT( node_parse_from_stringA( "Foo Bar", &pn ) == NP_SERROR );
	}

	void test_UntermNQA()
	{
		node_t * pn = NULL;

		TS_ASSERT( node_parse_from_stringA( ": \"Foo Bar", &pn ) == NP_SERROR );
	}

	void test_UntermQA()
	{
		node_t * pn = NULL;

		TS_ASSERT( node_parse_from_stringA( ": 'Foo Bar", &pn ) == NP_SERROR );
	}

	void test_UntermListA()
	{
		node_t * pn = NULL;

		TS_ASSERT( node_parse_from_stringA( ": (\r\n:'Foo Bar'\r\n", &pn ) == NP_SERROR );
	}

	void test_UntermHashA()
	{
		node_t * pn = NULL;

		TS_ASSERT( node_parse_from_stringA( ": {\r\nKey:'Foo Bar'\r\n", &pn ) == NP_SERROR );
	}

	void test_NamelessHashChildAA()
	{
		node_t * pn = NULL;
		char acData[] = ": {\r\n:'Foo Bar'\r\n}\r\n";
		ERROR_SETUP;

		TS_ASSERT( node_parse_from_dataA( acData, sizeof(acData), &pn ) == NP_NODE );
		TS_ASSERT( ERROR_AFTER == 1 );

		node_free( pn );
	}

	void test_NamelessHashChildAW()
	{
		node_t * pn = NULL;
		wchar_t acData[] = L": {\r\n:'Foo Bar'\r\n}\r\n";
		ERROR_SETUP;

		TS_ASSERT( node_parse_from_dataA( acData, sizeof(acData), &pn ) == NP_NODE );
		TS_ASSERT( ERROR_AFTER == 1 );

		node_free( pn );
	}

	void test_noColonW()
	{
		node_t * pn = NULL;

		TS_ASSERT( node_parse_from_stringW( L"Foo Bar", &pn ) == NP_SERROR );
	}

	void test_UntermNQW()
	{
		node_t * pn = NULL;

		TS_ASSERT( node_parse_from_stringW( L": \"Foo Bar", &pn ) == NP_SERROR );
	}

	void test_UntermQW()
	{
		node_t * pn = NULL;

		TS_ASSERT( node_parse_from_stringW( L": 'Foo Bar", &pn ) == NP_SERROR );
	}

	void test_UntermListW()
	{
		node_t * pn = NULL;

		TS_ASSERT( node_parse_from_stringW( L": (\r\n:'Foo Bar'\r\n", &pn ) == NP_SERROR );
	}

	void test_UntermHashW()
	{
		node_t * pn = NULL;

		TS_ASSERT( node_parse_from_stringW( L": {\r\nKey:'Foo Bar'\r\n", &pn ) == NP_SERROR );
	}

	void test_NamelessHashChildWA()
	{
		node_t * pn = NULL;
		char acData[] = ": {\r\n:'Foo Bar'\r\n}\r\n";
		ERROR_SETUP;

		TS_ASSERT( node_parse_from_dataW( acData, sizeof(acData), &pn ) == NP_NODE );
		TS_ASSERT( ERROR_AFTER == 1 );

		node_free( pn );
	}

	void test_NamelessHashChildWW()
	{
		node_t * pn = NULL;
		wchar_t acData[] = L": {\r\n:'Foo Bar'\r\n}\r\n";
		ERROR_SETUP;

		TS_ASSERT( node_parse_from_dataW( acData, sizeof(acData), &pn ) == NP_NODE );
		TS_ASSERT( ERROR_AFTER == 1 );

		node_free( pn );
	}
};

class InvalidArgs : public CxxTest::TestSuite
{
public:
	void testParseA()
	{
		TS_ASSERT( node_parseA( NULL, NULL ) == NP_INVALID );
		TS_ASSERT( node_parseA( (FILE*)1, NULL ) == NP_INVALID );
		TS_ASSERT( node_parse_from_stringA( NULL, NULL ) == NP_INVALID );
		TS_ASSERT( node_parse_from_stringA( (char*)1, NULL ) == NP_INVALID );
		TS_ASSERT( node_parse_from_dataA( NULL, 0, NULL ) == NP_INVALID );
		TS_ASSERT( node_parse_from_dataA( (void*)1, 0, NULL ) == NP_INVALID );
	}

	void testParseW()
	{
		TS_ASSERT( node_parseW( NULL, NULL ) == NP_INVALID );
		TS_ASSERT( node_parseW( (FILE*)1, NULL ) == NP_INVALID );
		TS_ASSERT( node_parse_from_stringW( NULL, NULL ) == NP_INVALID );
		TS_ASSERT( node_parse_from_stringW( (wchar_t*)1, NULL ) == NP_INVALID );
		TS_ASSERT( node_parse_from_dataW( NULL, 0, NULL ) == NP_INVALID );
		TS_ASSERT( node_parse_from_dataW( (void*)1, 0, NULL ) == NP_INVALID );
	}
};

#define ASSERT_SETUP		int __t, __nAssert = g_nAssertions
#define ASSERT_AFTER		(__t = g_nAssertions - __nAssert, __nAssert = g_nAssertions, __t )

class Assertions : public CxxTest::TestSuite
{
public:

	void setUp()
	{
		node_set_error_funcs( NULL, NULL, node_assert_count );
	}
	void tearDown()
	{
		node_set_error_funcs( node_error, node_memory, (node_assert_func_t)node_assert );
	}

	void testListIteratorAssert()
	{
		node_t * pn = NULL;
		ASSERT_SETUP;

		TS_ASSERT( node_first( pn ) == NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		pn = node_alloc();
		TS_ASSERT( node_first( pn ) == NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		TS_ASSERT( node_next( NULL ) == NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_free( pn );
	}

	void testFreeAssert()
	{
		node_t * pnHash = node_hash_alloc();
		node_t * pn = node_hash_add( pnHash, _T("Foo"), NODE_INT, 3 );
		ASSERT_SETUP;

		/* try freeing an internal node */
		node_free( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );
		
		/* assert that the data structure was unmodified */
		TS_ASSERT( node_get_elements( pnHash ) == 1 );
		pn = node_hash_get( pnHash, _T("Foo") );
		TS_ASSERT( pn != NULL );
		TS_ASSERT( node_get_int( pn ) == 3 );
		
		node_free( pnHash );
	}

	void testSet()
	{
		node_t * pn = node_alloc();
		ASSERT_SETUP;

		node_set( NULL, NODE_INT, 1 );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_set( pn, NODE_STRINGA, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );
		
		node_set( pn, NODE_STRINGW, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_free( pn );
	}

	void testGet()
	{
		node_t * pn = NULL;
		ASSERT_SETUP;

		node_get_int( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_get_real( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );
		
		node_get_stringA( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_get_stringW( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_get_int64( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );

		data_t ab[4] = {0,1,2,3};

		pn = node_alloc();
		node_set_data( pn, sizeof(ab), ab );
		
		int nLength = 1;
		node_get_data( NULL, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );
		node_get_data( NULL, &nLength );
		TS_ASSERT( ASSERT_AFTER == 1 );
		TS_ASSERT_EQUALS( nLength, 0 );

		node_set( pn, NODE_INT, 1 );
		node_get_data( pn, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		nLength = 1;
		node_get_data( pn, &nLength );
		TS_ASSERT( ASSERT_AFTER == 1 );
		TS_ASSERT_EQUALS( nLength, 0 );

		node_free( pn );

		node_get_ptr( NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );
		
		pn = node_list_alloc();

		node_get_int( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_get_real( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );
		
		node_get_stringA( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_get_stringW( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_get_int64( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_get_data( pn, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_get_ptr( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_free( pn );
	}

	void testGetElements()
	{
		ASSERT_SETUP;

		node_get_elements( NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_t * pn = node_alloc();
		node_get_elements( pn );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_free( pn );
	}

	void testSetName()
	{
		ASSERT_SETUP;

		node_t * pn = node_alloc();

		node_set_nameA( NULL, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_set_nameA( pn, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_set_nameA( NULL, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_set_nameA( pn, "Foo" );
		node_set_nameA( pn, node_get_nameA( pn ) );

		node_set_nameW( NULL, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_set_nameW( pn, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_set_nameW( NULL, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );

		node_set_nameW( pn, L"Foo" );
		node_set_nameW( pn, node_get_nameW( pn ) );

		node_free( pn );
	}

	void testAddCommon()
	{
		ASSERT_SETUP;

		node_t * pnHash = node_hash_alloc();

		node_hash_addA( pnHash, "Foo", NODE_INT, 1 );

		node_hash_add( pnHash, NULL, NODE_INT, 2 );
		TS_ASSERT( ASSERT_AFTER == 1 );
		TS_ASSERT( node_get_elements( pnHash ) == 1 );
		TS_ASSERT( node_get_int( node_hash_getA( pnHash, "Foo" ) ) == 1 );

		node_hash_addA( pnHash, "Foo", NODE_NODE, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );
		TS_ASSERT( node_get_elements( pnHash ) == 1 );
		TS_ASSERT( node_get_int( node_hash_getA( pnHash, "Foo" ) ) == 1 );

		node_hash_addA( pnHash, "Foo", NODE_REF, NULL );
		TS_ASSERT( ASSERT_AFTER == 1 );
		TS_ASSERT( node_get_elements( pnHash ) == 1 );
		TS_ASSERT( node_get_int( node_hash_getA( pnHash, "Foo" ) ) == 1 );

		node_hash_addA( pnHash, "Bar", NODE_INT, 3 );
		node_hash_addA( pnHash, "Baz", NODE_REF, node_hash_getA( pnHash, "Bar" ) );
		TS_ASSERT( ASSERT_AFTER == 1 );
		TS_ASSERT( node_get_elements( pnHash ) == 3 );
		TS_ASSERT( node_get_int( node_hash_getA( pnHash, "Baz" ) ) == 3 );

		node_free( pnHash );
	}

	void testListDelete()
	{
		ASSERT_SETUP;

		node_t * pnList = node_list_alloc();

		node_list_delete( NULL, pnList );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_list_delete( pnList, NULL );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_t * pnMember = node_push( pnList, NODE_INT, 1 );
		node_list_delete( pnMember, pnList ); /* member is not a list */
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		
		node_list_delete( pnList, pnMember ); /* success */
		TS_ASSERT_EQUALS( ASSERT_AFTER, 0 );
		node_list_delete( pnList, pnMember ); /* it's not in a collection anymore! */
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_t * pnList2 = node_list_alloc();
		node_push( pnList2, NODE_REF, pnMember );
		node_list_delete( pnList, pnMember ); /* in a different collection */
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_push( pnList, NODE_INT, 2 );
		node_list_delete( pnList, pnMember ); /* in a different collection */
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_free( pnList );
		node_free( pnList2 );
	}

	void testPush()
	{
		ASSERT_SETUP;

		node_push( NULL, NODE_INT, 1 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
	}

	void testPop()
	{
		ASSERT_SETUP;

		node_pop( NULL );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_t * pn = node_alloc();
		node_pop( pn );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_free( pn );
	}

	void testHashAddA()
	{
		ASSERT_SETUP;

		node_t * pn = node_alloc();

		node_hash_addA( NULL, "Key", NODE_INT, 1 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_hash_addA( pn, NULL, NODE_INT, 1 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_free( pn );

		/* A/W issue */
		pn = node_hash_alloc();
		node_hash_addW( pn, L"Foo", NODE_INT, 1 );
		node_hash_addA( pn, "Bar", NODE_INT, 2 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT_EQUALS( node_get_elements( pn ), 1 );

		node_free( pn );
	}

	void testHashAddW()
	{
		ASSERT_SETUP;

		node_t * pn = node_alloc();

		node_hash_addW( NULL, L"Key", NODE_INT, 1 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_hash_addW( pn, NULL, NODE_INT, 1 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_free( pn );

		/* A/W issue */
		pn = node_hash_alloc();
		node_hash_addA( pn, "Bar", NODE_INT, 2 );
		node_hash_addW( pn, L"Foo", NODE_INT, 1 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT_EQUALS( node_get_elements( pn ), 1 );
		node_free( pn );
	}

	void testHashGetA()
	{
		ASSERT_SETUP;

		node_t * pn = NULL;
		node_t * pnHash = node_alloc();

		pn = node_hash_getA( NULL, "Key" );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT( pn == NULL );

		pn = node_hash_getA( pnHash, NULL );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT( pn == NULL );

		/* not a hash */
		pn = node_hash_getA( pnHash, "Key");
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT( pn == NULL );

		node_hash_addW( pnHash, L"Foo", NODE_INT, 1 );
		pn = node_hash_getA( pnHash, "Key");
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT( pn == NULL );
		TS_ASSERT_EQUALS( node_get_elements( pnHash ), 1 );
		node_free( pnHash );
	}

	void testHashGetW()
	{
		ASSERT_SETUP;

		node_t * pn = NULL;
		node_t * pnHash = node_alloc();

		pn = node_hash_getW( NULL, L"Key" );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT( pn == NULL );

		pn = node_hash_getW( pnHash, NULL );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT( pn == NULL );

		/* not a hash */
		pn = node_hash_getW( pnHash, L"Key");
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT( pn == NULL );

		node_hash_addA( pnHash, "Foo", NODE_INT, 1 );
		pn = node_hash_getW( pnHash, L"Key");
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT( pn == NULL );
		TS_ASSERT_EQUALS( node_get_elements( pnHash ), 1 );
		node_free( pnHash );
	}

	void testHashDelete()
	{
		ASSERT_SETUP;
		node_t * pnHash = node_hash_alloc();
		node_t * pnNotElement = node_alloc();

		node_hash_delete( NULL, pnNotElement );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_hash_delete( pnHash, NULL );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		/* not a hash */
		node_hash_delete( pnNotElement, pnHash );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		/* empty hash */
		node_hash_delete( pnHash, pnNotElement );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		/* wrong bucket */
		node_hash_addA( pnHash, "One", NODE_INT, 1 );
		node_t * pnOtherHash = node_hash_alloc();
		node_t * pnTwo = node_hash_addA( pnOtherHash, "Two", NODE_INT, 2 );
		node_hash_delete( pnHash, pnTwo );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		/* empty hash */
		node_hash_delete( pnHash, pnNotElement );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 2 );

		node_free( pnHash );
		node_free( pnOtherHash );
		node_free( pnNotElement );
	}

	void testListAdd()
	{
		node_t * pnList = node_list_alloc();
		ASSERT_SETUP;

		node_list_add( NULL, NODE_INT, 1 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_list_add( pnList, NODE_NODE, NULL );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_free( pnList );
	}
	
	void testHashKeysA()
	{
		node_t * pnHash = NULL;
		node_t * pnKeys = NULL;
		ASSERT_SETUP;

		node_hash_keysA( NULL );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		pnHash = node_list_alloc();
		pnKeys = node_hash_keysA( pnHash );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT( pnKeys != NULL );
		node_free( pnKeys );

		node_hash_addW( pnHash, L"Test", NODE_INT, 1 );
		pnKeys = node_hash_keysA( pnHash );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT( pnKeys != NULL );
		node_free( pnKeys );

		node_free( pnHash );
	}

	void testHashKeysW()
	{
		node_t * pnHash = NULL;
		node_t * pnKeys = NULL;
		ASSERT_SETUP;

		node_hash_keysW( NULL );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		pnHash = node_list_alloc();
		pnKeys = node_hash_keysW( pnHash );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT( pnKeys != NULL );
		node_free( pnKeys );

		node_hash_addA( pnHash, "Test", NODE_INT, 1 );
		pnKeys = node_hash_keysW( pnHash );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT( pnKeys != NULL );
		node_free( pnKeys );

		node_free( pnHash );
	}

	void testGetType()
	{
		ASSERT_SETUP;

		TS_ASSERT_EQUALS( node_get_type( NULL ), NODE_UNKNOWN );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		TS_ASSERT_EQUALS( node_get_nameA( NULL ), (char *)0 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		TS_ASSERT_EQUALS( node_get_nameW( NULL ), (wchar_t*)0 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
	}

	void testDumpA()
	{
		node_t * pn = node_alloc();
		FILE * pf = fopen( g_psFileName, "wb" );
		ASSERT_SETUP;

		node_dumpA( (node_t *)1, 0, 0 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_dumpA( 0, pf, 0 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_dumpA( pn, pf, 0 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_free( pn );
		fclose( pf );
	}

	void testDumpW()
	{
		node_t * pn = node_alloc();
		FILE * pf = fopen( g_psFileName, "wb" );
		ASSERT_SETUP;

		node_dumpW( (node_t *)1, 0, 0 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_dumpW( 0, pf, 0 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_dumpW( pn, pf, 0 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_free( pn );
		fclose( pf );
	}

	void test_Copy()
	{
		node_t * pn = node_alloc();
		ASSERT_SETUP;

		node_t * pn2 = node_copy( pn );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		node_free( pn );
		node_free( pn2 );
	}

	void test_GetData()
	{
		node_t * pn = node_alloc();
		int nLength = 1;
		ASSERT_SETUP;

		node_get_data( pn, NULL );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_get_data( pn, &nLength );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		TS_ASSERT_EQUALS( nLength, 0 );

		node_set_data( pn, 15, NULL );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		/* suspicious amount of memory -- not an assertion, just error */
		void * pv = malloc( 1<< 17 );
		node_set_data( pn, 1<<17, pv );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 0 );
		free( pv );

		/* negative length */
		node_set_data( pn, -1, pn );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_free( pn );
	}

	void test_GetInvalid()
	{
		node_t * pn = node_alloc();
		ASSERT_SETUP;

		node_get_int( pn );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_get_real( pn );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		
		node_get_stringA( pn );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_get_stringW( pn );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_get_int64( pn );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_free( pn );
	}

	void test_AddInvalid()
	{
		node_t * pn = node_hash_alloc();
		ASSERT_SETUP;

		node_hash_addW( pn, L"Test", NODE_NODE, NULL );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_free( pn );
		pn = node_list_alloc();
		
		node_push( pn, NODE_NODE, NULL );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );
		node_free( pn );
	}

	void test_InvalidSet()
	{
		node_t * pn = node_alloc();
		ASSERT_SETUP;

		node_set( pn, NODE_HASH, 0 );
		TS_ASSERT_EQUALS( ASSERT_AFTER, 1 );

		node_free( pn );
	}
	/*TODO: test rest of assertions */
};

class LoadLimit : public CxxTest::TestSuite
{
public:
	void test_Set()
	{
#ifdef _DEBUG
		double dLimit = node_get_loadlimit();

		node_set_loadlimit( 1.0 );

		node_set_loadlimit( dLimit );
#endif
	}

	/* TODO: test various loads */
};

class CodePage : public CxxTest::TestSuite
{
public:
	void test_Set()
	{
		int nCodePage = node_get_codepage();

		node_set_codepage( 936 );

		node_set_codepage( nCodePage );
	}

	/* TODO: test various code pages, character conversion etc. */
};

/* NOTE: the following tests will fail if USE_BAGS is turned off;
   in which case, disable te tests by commenting out the region with
   // comments */

//#define NODE_SIZE 72

#if defined(_WIN64)
#define NODE_SIZE 128
#define BAG_SIZE 64
#elif defined(_WIN32)
#define NODE_SIZE 80
#define BAG_SIZE 32
#endif
class NoBag : public CxxTest::TestSuite
{
public:
	void test_stringA()
	{
		node_t * pn1 = node_alloc();
		node_t * pn2 = node_alloc();
		node_t * pn3 = node_alloc();

		BYTE bLoBefore = *((BYTE *)pn2 - 1);
		BYTE bHiBefore = *((BYTE *)pn2 + NODE_SIZE);

		node_set( pn2, NODE_STRINGA, "123456789012345678901234567890" );
		/* if this test fails, see NOTE above */
		TS_ASSERT_EQUALS( pn2->bBagUsed, 1 );
		node_set( pn2, NODE_STRINGA, "1234567890123456789012345678901" ); /* 31 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, 1 );
		node_set( pn2, NODE_STRINGA, "12345678901234567890123456789012" ); /* 32 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, BAG_SIZE == 32 ? 0 : 1 );
		node_set( pn2, NODE_STRINGA, "123456789012345678901234567890123" ); /* 33 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, BAG_SIZE == 32 ? 0 : 1 );
		node_set( pn2, NODE_STRINGA, "1234567890123456789012345678901234" ); /* 34 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, BAG_SIZE == 32 ? 0 : 1 );

		node_set( pn2, NODE_STRINGA, "123456789012345678901234567890123456789012345678901234567890123" ); /* 63 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, BAG_SIZE == 32 ? 0 : 1 );
		node_set( pn2, NODE_STRINGA, "1234567890123456789012345678901234567890123456789012345678901234" ); /* 64 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, 0 );
		node_set( pn2, NODE_STRINGA, "12345678901234567890123456789012345678901234567890123456789012345" ); /* 65 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, 0 );

		BYTE bLoAfter = *((BYTE *)pn2 - 1);
		BYTE bHiAfter = *((BYTE *)pn2 + NODE_SIZE);

		TS_ASSERT_EQUALS( bLoBefore, bLoAfter );
		TS_ASSERT_EQUALS( bHiBefore, bHiAfter );

		node_free( pn1 );
		node_free( pn3 );
		node_free( pn2 );
	}

	void test_stringW()
	{
		node_t * pn1 = node_alloc();
		node_t * pn2 = node_alloc();
		node_t * pn3 = node_alloc();

		BYTE bLoBefore = *((BYTE *)pn2 - 1);
		BYTE bHiBefore = *((BYTE *)pn2 + NODE_SIZE);

		node_set( pn2, NODE_STRINGW, L"12345678901234" ); /* 14 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, 1 );
		node_set( pn2, NODE_STRINGW, L"123456789012345" ); /* 15 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, 1 );
		node_set( pn2, NODE_STRINGW, L"1234567890123456" ); /* 16 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, BAG_SIZE == 32 ? 0 : 1 );
		node_set( pn2, NODE_STRINGW, L"12345678901234567" ); /* 17 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, BAG_SIZE == 32 ? 0 : 1 );
		node_set( pn2, NODE_STRINGW, L"123456789012345678" ); /* 18 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, BAG_SIZE == 32 ? 0 : 1 );

		node_set( pn2, NODE_STRINGW, L"1234567890123456789012345678901" ); /* 31 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, BAG_SIZE == 32 ? 0 : 1 );
		node_set( pn2, NODE_STRINGW, L"12345678901234567890123456789012" ); /* 32 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, 0 );
		node_set( pn2, NODE_STRINGW, L"123456789012345678901234567890123" ); /* 33 */
		TS_ASSERT_EQUALS( pn2->bBagUsed, 0 );

		BYTE bLoAfter = *((BYTE *)pn2 - 1);
		BYTE bHiAfter = *((BYTE *)pn2 + NODE_SIZE);

		TS_ASSERT_EQUALS( bLoBefore, bLoAfter );
		TS_ASSERT_EQUALS( bHiBefore, bHiAfter );

		node_free( pn1 );
		node_free( pn3 );
		node_free( pn2 );
	}

	void test_data()
	{
		node_t * pn1 = node_alloc();
		node_t * pn2 = node_alloc();
		node_t * pn3 = node_alloc();
		char ac[] =   { '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', \
						'\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', \
						'\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', \
						'\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', \
						'\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', \
						'\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', \
						'\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', \
						'\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', \
						'\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB', '\xBB' };
		data_t * pv = (data_t *)ac;

		BYTE bLoBefore = *((BYTE *)pn2 - 1);
		BYTE bHiBefore = *((BYTE *)pn2 + NODE_SIZE);

		node_set_data( pn2, 30, pv );
		TS_ASSERT_EQUALS( pn2->bBagUsed, 1 );
		node_set_data( pn2, 31, pv );
		TS_ASSERT_EQUALS( pn2->bBagUsed, 1 );
		node_set_data( pn2, 32, pv );
		TS_ASSERT_EQUALS( pn2->bBagUsed, 1 );
		node_set_data( pn2, 33, pv );
		TS_ASSERT_EQUALS( pn2->bBagUsed, BAG_SIZE == 32 ? 0 : 1 );
		node_set_data( pn2, 34, pv );
		TS_ASSERT_EQUALS( pn2->bBagUsed, BAG_SIZE == 32 ? 0 : 1 );
		node_set_data( pn2, 63, pv );
		TS_ASSERT_EQUALS( pn2->bBagUsed, BAG_SIZE == 32 ? 0 : 1 );
		node_set_data( pn2, 64, pv );
		TS_ASSERT_EQUALS( pn2->bBagUsed, BAG_SIZE == 32 ? 0 : 1 );
		node_set_data( pn2, 65, pv );
		TS_ASSERT_EQUALS( pn2->bBagUsed, 0 );

		BYTE bLoAfter = *((BYTE *)pn2 - 1);
		BYTE bHiAfter = *((BYTE *)pn2 + NODE_SIZE);

		TS_ASSERT_EQUALS( bLoBefore, bLoAfter );
		TS_ASSERT_EQUALS( bHiBefore, bHiAfter );
	
		node_free( pn1 );
		node_free( pn3 );
		node_free( pn2 );
	}
};

class Conversion : public CxxTest::TestSuite
{
public:
	void test_listhead()
	{
		node_t * pnList = node_list_alloc();
		node_push( pnList, NODE_INT, 1 );

		TS_ASSERT_EQUALS( node_get_int( pnList ), 1 );
		TS_ASSERT_EQUALS( node_get_real( pnList ), 1.0 );
		TS_ASSERT_EQUALS( node_get_stringA( pnList ), std::string( "1" ) );
		TS_ASSERT_EQUALS( node_get_stringW( pnList ), std::wstring( L"1" ) );

		node_free( node_pop( pnList ) );

		char ab[] = { 9, 10, 11, 13 };
		node_push( pnList, NODE_REF_DATA, node_set_data( node_alloc(), sizeof(ab), ab ) );
		int nLength = 0;
		data_t * pb = node_get_data( pnList, &nLength );
		TS_ASSERT_EQUALS( nLength, sizeof(ab) );
		TS_ASSERT( memcmp( ab, pb, nLength ) == 0 );

		node_free( node_pop( pnList ) );

		node_push( pnList, NODE_PTR, pnList );

		TS_ASSERT_EQUALS( node_get_ptr( pnList ), pnList );
		node_free( pnList );
	}
};

/* explicitly test Hash functions for A string keys */
class HashANode : public CxxTest::TestSuite
{
	node_t * pnHash;
public:
	void setUp() { pnHash = node_hash_alloc(); }
	void tearDown() { node_free( pnHash ); }

	void test_Exists()
	{
		TS_ASSERT( pnHash != NULL );
		TS_ASSERT( node_get_type( pnHash ) == NODE_HASH );
		wait();
	}

	void test_alloc2()
	{
		node_free( node_hash_alloc2( 101 ) );
		node_free( node_hash_alloc2( 1009 ) );
		node_free( node_hash_alloc2( 5003 ) );
		node_free( node_hash_alloc2( 151 ) );
		node_free( node_hash_alloc2( 3001 ) );
		node_free( node_hash_alloc2( 127 ) );
	}

	void test_Copy()
	{
		node_hash_addA( pnHash, "A", NODE_INT, 1 );
		node_hash_addA( pnHash, "B", NODE_INT, 1 );
		node_hash_addA( pnHash, "C", NODE_INT, 1 );
		node_hash_addA( pnHash, "D", NODE_INT, 1 );
		node_hash_addA( pnHash, "E", NODE_INT, 1 );
		node_hash_addA( pnHash, "F", NODE_INT, 1 );
		node_hash_addA( pnHash, "G", NODE_INT, 1 );
		node_hash_addA( pnHash, "H", NODE_INT, 1 );
		node_hash_addA( pnHash, "I", NODE_INT, 1 );
		node_hash_addA( pnHash, "J", NODE_INT, 1 );

		node_t * pnCopy = node_copy( pnHash );
		TS_ASSERT_EQUALS( node_get_elements( pnHash ), node_get_elements( pnCopy ) );

		TS_ASSERT( node_hash_getA( pnCopy, "A" ) != NULL );
		TS_ASSERT( node_hash_getA( pnCopy, "E" ) != NULL );
		TS_ASSERT( node_hash_getA( pnCopy, "J" ) != NULL );

		node_free( pnCopy );

		TS_ASSERT( node_copy( NULL ) == NULL );
	}

	void test_Add()
	{
		node_hash_addA( pnHash, "One", NODE_INT, 1 );
		TS_ASSERT( node_get_int( node_hash_getA( pnHash, "One" ) ) == 1 );
		TS_ASSERT( node_get_elements( pnHash ) == 1 );
		wait();
	}

	void test_Add2()
	{
		node_hash_addA( pnHash, "One", NODE_INT, 1 );
		node_hash_addA( pnHash, "Two", NODE_INT, 2 );
		TS_ASSERT( node_get_int( node_hash_getA( pnHash, "One" ) ) == 1 );
		TS_ASSERT( node_get_int( node_hash_getA( pnHash, "Two" ) ) == 2 );
		TS_ASSERT( node_get_elements( pnHash ) == 2 );
		wait();
	}

	void test_Delete1()
	{
		node_hash_addA( pnHash, "One", NODE_INT, 1 );
		node_hash_addA( pnHash, "Two", NODE_INT, 2 );

		node_t * pn = node_hash_getA( pnHash, "One" );
		node_hash_delete( pnHash, pn );
		node_free( pn );

		TS_ASSERT( node_get_elements( pnHash ) == 1 );
		TS_ASSERT( node_hash_getA( pnHash, "One" ) == NULL );
		wait();
	}

	void test_Delete2()
	{
		node_hash_addA( pnHash, "One", NODE_INT, 1 );
		node_hash_addA( pnHash, "Two", NODE_INT, 2 );

		node_t * pn = node_hash_getA( pnHash, "One" );
		node_hash_delete( pnHash, pn );
		node_free( pn );

		pn = node_hash_getA( pnHash, "Two" );
		node_hash_delete( pnHash, pn );
		node_free( pn );

		TS_ASSERT( node_get_elements( pnHash ) == 0 );
		TS_ASSERT( node_hash_getA( pnHash, "One" ) == NULL );
		TS_ASSERT( node_hash_getA( pnHash, "Two" ) == NULL );
		wait();
	}

	void test_Keys()
	{
		node_hash_addA( pnHash, "One", NODE_INT, 1 );
		node_hash_addA( pnHash, "Two", NODE_INT, 2 );

		node_t * pnKeys = node_hash_keysA( pnHash );
		TS_ASSERT_EQUALS( node_get_type( pnKeys ), NODE_LIST );
		TS_ASSERT_EQUALS( node_get_elements( pnKeys ), 2 );

		for( node_t * pn = node_first( pnKeys ); pn != NULL; pn = node_next( pn ) )
		{
			TS_ASSERT( node_hash_getA( pnHash, node_get_stringA( pn ) ) != NULL );
		}

		node_free( pnKeys );
		wait();
	}

	void test_Alloc2()
	{
		node_free( pnHash );

		pnHash = node_hash_alloc2(2);

		node_hash_addA( pnHash, "A", NODE_INT, 1 );
		node_hash_addA( pnHash, "B", NODE_INT, 1 );
		node_hash_addA( pnHash, "C", NODE_INT, 1 );
		node_hash_addA( pnHash, "D", NODE_INT, 1 );
		node_hash_addA( pnHash, "E", NODE_INT, 1 );
		node_hash_addA( pnHash, "F", NODE_INT, 1 );
		node_hash_addA( pnHash, "G", NODE_INT, 1 );
		node_hash_addA( pnHash, "H", NODE_INT, 1 );
		node_hash_addA( pnHash, "I", NODE_INT, 1 );
		node_hash_addA( pnHash, "J", NODE_INT, 1 );

		/* delete one from the beginning */
		node_t * pn = node_hash_getA( pnHash, "A" );
		node_hash_delete( pnHash, pn );
		node_free( pn );

		/* delete one from the end */
		pn = node_hash_getA( pnHash, "J" );
		node_hash_delete( pnHash, pn );
		node_free( pn );

		/* delete some from the middle */
		pn = node_hash_getA( pnHash, "E" );
		node_hash_delete( pnHash, pn );
		node_free( pn );
		pn = node_hash_getA( pnHash, "F" );
		node_hash_delete( pnHash, pn );
		node_free( pn );

		/* add them back */
		node_hash_addA( pnHash, "J", NODE_INT, 1 );
		node_hash_addA( pnHash, "F", NODE_INT, 1 );
		node_hash_addA( pnHash, "E", NODE_INT, 1 );
		node_hash_addA( pnHash, "A", NODE_INT, 1 );
		
		node_free( node_copy( pnHash ) );
	}
};

/* explicitly test Hash functions for W string keys */
class HashWNode : public CxxTest::TestSuite
{
	node_t * pnHash;
public:
	void setUp() { pnHash = node_hash_alloc(); }
	void tearDown() { node_free( pnHash ); }

	void test_Exists()
	{
		TS_ASSERT( pnHash != NULL );
		TS_ASSERT( node_get_type( pnHash ) == NODE_HASH );
		wait();
	}

	void test_Add()
	{
		node_hash_addW( pnHash, L"One", NODE_INT, 1 );
		TS_ASSERT( node_get_int( node_hash_getW( pnHash, L"One" ) ) == 1 );
		TS_ASSERT( node_get_elements( pnHash ) == 1 );
		wait();
	}

	void test_Add2()
	{
		node_hash_addW( pnHash, L"One", NODE_INT, 1 );
		node_hash_addW( pnHash, L"Two", NODE_INT, 2 );
		TS_ASSERT( node_get_int( node_hash_getW( pnHash, L"One" ) ) == 1 );
		TS_ASSERT( node_get_int( node_hash_getW( pnHash, L"Two" ) ) == 2 );
		TS_ASSERT( node_get_elements( pnHash ) == 2 );
		wait();
	}

	void test_Delete1()
	{
		node_hash_addW( pnHash, L"One", NODE_INT, 1 );
		node_hash_addW( pnHash, L"Two", NODE_INT, 2 );

		node_t * pn = node_hash_getW( pnHash, L"One" );
		node_hash_delete( pnHash, pn );
		node_free( pn );

		TS_ASSERT( node_get_elements( pnHash ) == 1 );
		TS_ASSERT( node_hash_getW( pnHash, L"One" ) == NULL );
		wait();
	}

	void test_Delete2()
	{
		node_hash_addW( pnHash, L"One", NODE_INT, 1 );
		node_hash_addW( pnHash, L"Two", NODE_INT, 2 );

		node_t * pn = node_hash_getW( pnHash, L"One" );
		node_hash_delete( pnHash, pn );
		node_free( pn );

		pn = node_hash_getW( pnHash, L"Two" );
		node_hash_delete( pnHash, pn );
		node_free( pn );

		TS_ASSERT( node_get_elements( pnHash ) == 0 );
		TS_ASSERT( node_hash_getW( pnHash, L"One" ) == NULL );
		TS_ASSERT( node_hash_getW( pnHash, L"Two" ) == NULL );
		wait();
	}

	void test_Keys()
	{
		node_hash_addW( pnHash, L"One", NODE_INT, 1 );
		node_hash_addW( pnHash, L"Two", NODE_INT, 2 );

		node_t * pnKeys = node_hash_keysW( pnHash );
		TS_ASSERT_EQUALS( node_get_type( pnKeys ), NODE_LIST );
		TS_ASSERT_EQUALS( node_get_elements( pnKeys ), 2 );

		for( node_t * pn = node_first( pnKeys ); pn != NULL; pn = node_next( pn ) )
		{
			TS_ASSERT( node_hash_getW( pnHash, node_get_stringW( pn ) ) != NULL );
		}

		node_free( pnKeys );
		wait();
	}

	void test_Copy()
	{
		node_hash_addW( pnHash, L"A", NODE_INT, 1 );
		node_hash_addW( pnHash, L"B", NODE_INT, 1 );
		node_hash_addW( pnHash, L"C", NODE_INT, 1 );
		node_hash_addW( pnHash, L"D", NODE_INT, 1 );
		node_hash_addW( pnHash, L"E", NODE_INT, 1 );
		node_hash_addW( pnHash, L"F", NODE_INT, 1 );
		node_hash_addW( pnHash, L"G", NODE_INT, 1 );
		node_hash_addW( pnHash, L"H", NODE_INT, 1 );
		node_hash_addW( pnHash, L"I", NODE_INT, 1 );
		node_hash_addW( pnHash, L"J", NODE_INT, 1 );

		node_t * pnCopy = node_copy( pnHash );
		TS_ASSERT_EQUALS( node_get_elements( pnHash ), node_get_elements( pnCopy ) );

		TS_ASSERT( node_hash_getW( pnCopy, L"A" ) != NULL );
		TS_ASSERT( node_hash_getW( pnCopy, L"E" ) != NULL );
		TS_ASSERT( node_hash_getW( pnCopy, L"J" ) != NULL );

		node_free( pnCopy );
	}

	void test_Replacement()
	{
		node_hash_addW( pnHash, L"Test", NODE_INT, 1 );
		node_hash_addW( pnHash, L"Test", NODE_REAL, 2.0 );
		TS_ASSERT_EQUALS( node_get_real( node_hash_getW( pnHash, L"Test" ) ), 2.0 );
	}
};

/* explicitly test Hash functions for mixed string keys (note: this behaviour is illegal!) */
class HashMixedNode : public CxxTest::TestSuite
{
	node_t * pnHash;
public:
	void setUp()
	{
	}

	void tearDown()
	{
		node_free( pnHash );
	}

	void test_AthenW()
	{
		pnHash = node_hash_alloc();
		node_hash_addA( pnHash, "One", NODE_INT, 1 );
		TS_ASSERT_THROWS_ANYTHING( node_hash_addW( pnHash, L"Two", NODE_INT, 2 ) );
	}

	void test_WthenA()
	{
		pnHash = node_hash_alloc();
		node_hash_addW( pnHash, L"Two", NODE_INT, 2 );
		TS_ASSERT_THROWS_ANYTHING( node_hash_addA( pnHash, "One", NODE_INT, 1 ) );
	}

	void test_AlookupW()
	{
		pnHash = node_hash_alloc();
		node_hash_addA( pnHash, "One", NODE_INT, 1 );
		TS_ASSERT_THROWS_ANYTHING( node_hash_getW( pnHash, L"Two" ) );
	}

	void test_WlookupA()
	{
		pnHash = node_hash_alloc();
		node_hash_addW( pnHash, L"One", NODE_INT, 1 );
		TS_ASSERT_THROWS_ANYTHING( node_hash_getA( pnHash, "Two" ) );
	}

	void test_AkeysW()
	{
		pnHash = node_hash_alloc();
		node_hash_addA( pnHash, "One", NODE_INT, 1 );
		TS_ASSERT_THROWS_ANYTHING( node_hash_keysW( pnHash ) );
	}

	void test_WkeysA()
	{
		pnHash = node_hash_alloc();
		node_hash_addW( pnHash, L"One", NODE_INT, 1 );
		TS_ASSERT_THROWS_ANYTHING( node_hash_keysA( pnHash ) );
	}

	//	void test_Straight()
//	{
//		TS_ASSERT_EQUALS( node_get_int( node_hash_getA( pnHash, "One" ) ), 1 );
//		TS_ASSERT_EQUALS( node_get_int( node_hash_getW( pnHash, L"Two" ) ), 2 );
//		wait();
//	}
//
//	void test_Cross1()
//	{
//		/* Problem: tries to dereference the W name of a node which only has A name set.
//		 * Solution: don't add both A and W members to same hash!! */
//		TS_ASSERT_THROWS_ANYTHING( node_hash_getW( pnHash, L"One" ) );
//		wait();
//	}
//
//	void test_Cross2()
//	{
//		/* Problem: tries to dereference the W name of a node which only has A name set.
//		 * Solution: don't add both A and W members to same hash!! */
//		TS_ASSERT_THROWS_ANYTHING( node_hash_getA( pnHash, "Two" ) );
//		wait();
//	}
};

/* implicitly test Hash functions for A string keys (note TCHAR) */
class HashDefaultANode : public CxxTest::TestSuite
{
	node_t * pnHash;
public:
	void setUp() { pnHash = node_hash_alloc(); }
	void tearDown() { node_free( pnHash ); }

	void test_Exists()
	{
		TS_ASSERT( pnHash != NULL );
		TS_ASSERT( node_get_type( pnHash ) == NODE_HASH );
		wait();
	}

	void test_Add()
	{
		node_hash_add( pnHash, _T("One"), NODE_INT, 1 );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("One") ) ) == 1 );
		TS_ASSERT( node_get_elements( pnHash ) == 1 );
		wait();
	}

	void test_Add2()
	{
		node_hash_add( pnHash, _T("One"), NODE_INT, 1 );
		node_hash_add( pnHash,_T("Two"), NODE_INT, 2 );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("One") ) ) == 1 );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("Two") ) ) == 2 );
		TS_ASSERT( node_get_elements( pnHash ) == 2 );
		wait();
	}

	void test_Delete1()
	{
		node_hash_add( pnHash, _T("One"), NODE_INT, 1 );
		node_hash_add( pnHash, _T("Two"), NODE_INT, 2 );

		node_t * pn = node_hash_get( pnHash, _T("One") );
		node_hash_delete( pnHash, pn );
		node_free( pn );

		TS_ASSERT( node_get_elements( pnHash ) == 1 );
		TS_ASSERT( node_hash_get( pnHash, _T("One") ) == NULL );
		wait();
	}

	void test_Delete2()
	{
		node_hash_add( pnHash, _T("One"), NODE_INT, 1 );
		node_hash_add( pnHash, _T("Two"), NODE_INT, 2 );

		node_t * pn = node_hash_get( pnHash, _T("One") );
		node_hash_delete( pnHash, pn );
		node_free( pn );

		pn = node_hash_get( pnHash, _T("Two") );
		node_hash_delete( pnHash, pn );
		node_free( pn );

		TS_ASSERT( node_get_elements( pnHash ) == 0 );
		TS_ASSERT( node_hash_get( pnHash, _T("One") ) == NULL );
		TS_ASSERT( node_hash_get( pnHash, _T("Two") ) == NULL );
		wait();
	}

	void test_Keys()
	{
		node_hash_add( pnHash, _T("One"), NODE_INT, 1 );
		node_hash_add( pnHash, _T("Two"), NODE_INT, 2 );

		node_t * pnKeys = node_hash_keys( pnHash );
		TS_ASSERT_EQUALS( node_get_type( pnKeys ), NODE_LIST );
		TS_ASSERT_EQUALS( node_get_elements( pnKeys ), 2 );

		for( node_t * pn = node_first( pnKeys ); pn != NULL; pn = node_next( pn ) )
		{
			TS_ASSERT( node_hash_get( pnHash, node_get_string( pn ) ) != NULL );
		}

		node_free( pnKeys );
		wait();
	}

	void test_Copy()
	{
		node_hash_add( pnHash, _T("A"), NODE_INT, 1 );
		node_hash_add( pnHash, _T("B"), NODE_INT, 1 );
		node_hash_add( pnHash, _T("C"), NODE_INT, 1 );
		node_hash_add( pnHash, _T("D"), NODE_INT, 1 );
		node_hash_add( pnHash, _T("E"), NODE_INT, 1 );
		node_hash_add( pnHash, _T("F"), NODE_INT, 1 );
		node_hash_add( pnHash, _T("G"), NODE_INT, 1 );
		node_hash_add( pnHash, _T("H"), NODE_INT, 1 );
		node_hash_add( pnHash, _T("I"), NODE_INT, 1 );
		node_hash_add( pnHash, _T("J"), NODE_INT, 1 );

		node_t * pnCopy = node_copy( pnHash );
		TS_ASSERT_EQUALS( node_get_elements( pnHash ), node_get_elements( pnCopy ) );

		TS_ASSERT( node_hash_get( pnCopy, _T("A") ) != NULL );
		TS_ASSERT( node_hash_get( pnCopy, _T("E") ) != NULL );
		TS_ASSERT( node_hash_get( pnCopy, _T("J") ) != NULL );

		node_free( pnCopy );
	}

};

class FileNode : public CxxTest::TestSuite
{
	char * psFileContentsA;
	char * psFileContentsMod;
	char * psFileContentsBitmask;
	char * psFileContentsB;
	data_t ad[TEST_DATA_SIZE];

public:
	FileNode()
	{
		psFileContentsA = \
		psFileContentsMod = \
		"List: (\r\n" \
		"  Hash: {\r\n" \
		"    String: 'Value'\r\n" \
		"    Int: 1  (0x00000001)\r\n" \
		"    Data1: DATA 1\r\n" \
	    "$ 00                                                 .               \r\n" \
		"  }\r\n" \
		")\r\n";

		psFileContentsBitmask = \
		"List: (\r\n" \
		"  Hash: {\r\n" \
		"    Int: 1  (0x00000001)\r\n" \
		"    String: 'Value'\r\n" \
		"    Data1: DATA 1\r\n" \
	    "$ 00                                                 .               \r\n" \
		"  }\r\n" \
		")\r\n";

		psFileContentsB = \
		"List: (\r\n" \
		"  Hash: {\r\n" \
		"    String: 'Value'\r\n" \
		"    Int: 1 (0x00000001)\r\n" \
		"    Data1: DATA 1\r\n" \
	    "$ 00                                                 .               \r\n" \
		"  }\r\n" \
		")\r\n";

		for( unsigned char i = 0; i < TEST_DATA_SIZE; i++ )
			ad[i] = i;
	}

	void setUp() {}
	void tearDown() 
	{ 
		remove( g_psFileName );
	}

	void test_WriteA()
	{
		node_t * pnHash = node_hash_alloc();
		node_t * pnList = node_list_alloc();

		node_set_nameA( pnHash, "Hash" );
		node_set_nameA( pnList, "List" );

		node_hash_addA( pnHash, "Int", NODE_INT, 1 );
		node_hash_addA( pnHash, "String", NODE_STRINGA, "Value" );
		node_hash_addA( pnHash, "Data1", NODE_REF_DATA, node_set_data( node_alloc(), 1, ad ) );

		node_list_add( pnList, NODE_NODE, pnHash );

		node_free( pnHash );

		FILE * pf = fopen( g_psFileName, "wb" );
		TS_ASSERT( pf != NULL );

		node_dumpA( pnList, pf, 0 );

		fclose( pf );

		node_free( pnList );

		pf = fopen( g_psFileName, "rb" );

		char acBuffer[2048] = {0};
		fread( acBuffer, 1, sizeof( acBuffer ), pf );

		fclose( pf );

		/* check if match */
		bool bMatchMod = strcmp( acBuffer, psFileContentsMod ) == 0;
		bool bMatchBitmask = strcmp( acBuffer, psFileContentsBitmask ) == 0;

		TS_ASSERT( bMatchMod || bMatchBitmask );
		wait();
	}

	void test_WriteW()
	{
		node_t * pnHash = node_hash_alloc();
		node_t * pnList = node_list_alloc();

		wchar_t * psFileContentsModW = \
		L"List: (\r\n" \
		L"  Hash: {\r\n" \
		L"    String: 'Value'\r\n" \
		L"    Int: 1  (0x00000001)\r\n" \
		L"    Data1: DATA 1\r\n" \
	    L"$ 00                                                 .               \r\n" \
		L"  }\r\n" \
		L")\r\n";

		wchar_t * psFileContentsBitmaskW = \
		L"List: (\r\n" \
		L"  Hash: {\r\n" \
		L"    Int: 1  (0x00000001)\r\n" \
		L"    String: 'Value'\r\n" \
		L"    Data1: DATA 1\r\n" \
	    L"$ 00                                                 .               \r\n" \
		L"  }\r\n" \
		L")\r\n";


		node_set_nameW( pnHash, L"Hash" );
		node_set_nameW( pnList, L"List" );

		node_hash_addW( pnHash, L"Int", NODE_INT, 1 );
		node_hash_addW( pnHash, L"String", NODE_STRINGW, L"Value" );
		node_hash_addW( pnHash, L"Data1", NODE_REF_DATA, node_set_data( node_alloc(), 1, ad ) );

		node_list_add( pnList, NODE_NODE, pnHash );

		node_free( pnHash );

		FILE * pf = fopen( g_psFileName, "wb" );
		TS_ASSERT( pf != NULL );

		node_dumpW( pnList, pf, 0 );

		fclose( pf );

		node_free( pnList );

		pf = fopen( g_psFileName, "rb" );

		wchar_t acBuffer[2048] = {0};
		fread( acBuffer, 1, sizeof( acBuffer ), pf );

		fclose( pf );

		/* check if match */
		bool bMatchMod = wcscmp( acBuffer, psFileContentsModW ) == 0;
		bool bMatchBitmask = wcscmp( acBuffer, psFileContentsBitmaskW ) == 0;

		TS_ASSERT( bMatchMod || bMatchBitmask );
		wait();
	}

	void test_WriteW2()
	{
		node_t * pnHash = node_hash_alloc();
		node_t * pnList = node_list_alloc();

		wchar_t * psFileContentsModW = \
		L"List: (\r\n" \
		L"  Hash: {\r\n" \
		L"    String: 'Value'\r\n" \
		L"    Int: 1  (0x00000001)\r\n" \
		L"    Data1: DATA 1\r\n" \
	    L"$ 00                                                 .               \r\n" \
		L"  }\r\n" \
		L")\r\n";

		wchar_t * psFileContentsBitmaskW = \
		L"List: (\r\n" \
		L"  Hash: {\r\n" \
		L"    Int: 1  (0x00000001)\r\n" \
		L"    String: 'Value'\r\n" \
		L"    Data1: DATA 1\r\n" \
	    L"$ 00                                                 .               \r\n" \
		L"  }\r\n" \
		L")\r\n";


		node_set_nameW( pnHash, L"Hash" );
		node_set_nameW( pnList, L"List" );

		node_hash_addW( pnHash, L"Int", NODE_INT, 1 );
		node_hash_addW( pnHash, L"String", NODE_STRINGA, "Value" );
		node_hash_addW( pnHash, L"Data1", NODE_REF_DATA, node_set_data( node_alloc(), 1, ad ) );

		node_list_add( pnList, NODE_NODE, pnHash );

		node_free( pnHash );

		FILE * pf = fopen( g_psFileName, "wb" );
		TS_ASSERT( pf != NULL );

		node_dumpW( pnList, pf, 0 );

		fclose( pf );

		node_free( pnList );

		pf = fopen( g_psFileName, "rb" );

		wchar_t acBuffer[2048] = {0};
		fread( acBuffer, 1, sizeof( acBuffer ), pf );

		fclose( pf );

		/* check if match */
		bool bMatchMod = wcscmp( acBuffer, psFileContentsModW ) == 0;
		bool bMatchBitmask = wcscmp( acBuffer, psFileContentsBitmaskW ) == 0;

		TS_ASSERT( bMatchMod || bMatchBitmask );
		wait();
	}

	void test_Parse_A_to_A()
	{
		node_t * pnList = NULL;

		FILE * pf = NULL;

		pf = fopen( g_psFileName, "wb" );

		fwrite( psFileContentsB, strlen(psFileContentsB)+1, 1, pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );

		node_parseA( pf, &pnList );

		fclose( pf );

		check_A_node( pnList, "Parse_A_to_A" );
	}

	void check_A_node( node_t * pnList, char * sMessage )
	{
		TSM_ASSERT_EQUALS( sMessage, node_get_type( pnList ), NODE_LIST );
		TSM_ASSERT_EQUALS(  sMessage, node_get_elements( pnList ), 1 );
		TSM_ASSERT_EQUALS( sMessage, std::string( node_get_nameA( pnList ) ), std::string( "List" ) );

		node_t * pnHash = node_first( pnList );

		TSM_ASSERT_SAME_DATA( sMessage, node_get_nameA( pnHash ), "Hash", 5 );

		TSM_ASSERT_EQUALS( sMessage, node_get_elements( pnHash ), 3 );
		TSM_ASSERT_EQUALS( sMessage, node_get_int( node_hash_getA( pnHash, "Int" ) ), 1 );
		TSM_ASSERT_EQUALS( sMessage, std::string( node_get_stringA( node_hash_getA( pnHash, "String" ) ) ), std::string( "Value" ) );

		int nLength;
		const data_t * pd;

		pd = node_get_data( node_hash_getA( pnHash, "Data1" ), &nLength );

		TSM_ASSERT_SAME_DATA( sMessage, pd, ad, nLength );

		node_free( pnList );
		wait();
	}

	void test_Parse_A_to_W()
	{
		node_t * pnList = NULL;

		FILE * pf = NULL;

		pf = fopen( g_psFileName, "wb" );

		fwrite( psFileContentsA, strlen(psFileContentsA)+1, 1, pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );

		node_parseW( pf, &pnList );

		fclose( pf );

		check_W_node( pnList, "Parse_A_to_W" );
	}

	void check_W_node( node_t * pnList, char * sMessage )
	{
		TSM_ASSERT_EQUALS(sMessage,  node_get_type( pnList ), NODE_LIST );
		TSM_ASSERT_EQUALS( sMessage, node_get_elements( pnList ), 1 );
		TSM_ASSERT_SAME_DATA( sMessage, node_get_nameW( pnList ), L"List", 10 );

		node_t * pnHash = node_first( pnList );

		TSM_ASSERT_SAME_DATA( sMessage, node_get_nameW( pnHash ), L"Hash", 10 );

		TSM_ASSERT_EQUALS( sMessage, node_get_elements( pnHash ), 3 );
		TSM_ASSERT_EQUALS( sMessage, node_get_int( node_hash_getW( pnHash, L"Int" ) ), 1 );
		TSM_ASSERT_SAME_DATA( sMessage, node_get_stringW( node_hash_getW( pnHash, L"String" ) ), L"Value", 12 );

		int nLength;
		const data_t * pd;

		pd = node_get_data( node_hash_getW( pnHash, L"Data1" ), &nLength );

		TSM_ASSERT_SAME_DATA( sMessage, pd, ad, nLength );

		node_free( pnList );
		wait();
	}

	void test_parse_W_to_W()
	{
		node_t * pnList = NULL;
		wchar_t pwFileContents[2048] = {0};

		FILE * pf = NULL;

		pf = fopen( g_psFileName, "wb" );

		swprintf( pwFileContents, L"%S", psFileContentsB );
		fwrite( pwFileContents, wcslen(pwFileContents), 2, pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );

		node_parseW( pf, &pnList );

		fclose( pf );

		check_W_node( pnList, "Parse_W_to_W" );
	}

	void test_parse_W_to_A()
	{
		node_t * pnList = NULL;
		wchar_t pwFileContents[2048] = {0};

		FILE * pf = NULL;

		pf = fopen( g_psFileName, "wb" );

		swprintf( pwFileContents, L"%S", psFileContentsB );
		fwrite( pwFileContents, wcslen(pwFileContents), 2, pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );

		node_parseA( pf, &pnList );

		fclose( pf );

		check_A_node( pnList, "Parse_W_to_A" );

	}

	void test_escape_colon()
	{
		node_t * pn = node_alloc();

		node_set_nameA( pn, "A:B" );
		node_set( pn, NODE_STRINGA, "C:D" );

		FILE * pf = NULL;

		pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 );
		fclose( pf );

		node_free( pn );

		pf = fopen( g_psFileName, "rb" );
		node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT_SAME_DATA( node_get_nameA( pn ), "A:B", 4 );
		TS_ASSERT_SAME_DATA( node_get_stringA( pn ), "C:D", 4 );

		node_free( pn );
	}

	void test_escape_newline()
	{
		node_t * pn = node_alloc();

		node_set_nameA( pn, "A\r\nB" );
		node_set( pn, NODE_STRINGA, "C\r\nD" );

		FILE * pf = NULL;

		pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 );
		fclose( pf );

		node_free( pn );

		pf = fopen( g_psFileName, "rb" );
		node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT_SAME_DATA( node_get_nameA( pn ), "A\r\nB", 5 );
		TS_ASSERT_SAME_DATA( node_get_stringA( pn ), "C\r\nD", 5 );

		node_free( pn );
	}

	void test_writing_data()
	{
		char * psData =																\
		"Data1: DATA 1\r\n"															\
	    "$ 00                                                 .               \r\n"	\
		"Data8: DATA 8\r\n"															\
		"$ 00 01 02 03 04 05 06 07                            ........        \r\n"	\
		"Data15: DATA 15\r\n"															\
		"$ 00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e      ............... \r\n"	\
		"Data16: DATA 16\r\n"															\
		"$ 00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e 0f   ................\r\n"	\
		"Data17: DATA 17\r\n"															\
		"$ 00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e 0f   ................\r\n"	\
		"$ 10                                                 .               \r\n";
	
		node_t * pn = node_alloc();

		FILE * pf = NULL;

		pf = fopen( g_psFileName, "wb" );

		node_set_nameA( pn, "Data1" );
		node_set_data( pn, 1, ad );
		node_dumpA( pn, pf, 0 );

		node_set_nameA( pn, "Data8" );
		node_set_data( pn, 8, ad );
		node_dumpA( pn, pf, 0 );

		node_set_nameA( pn, "Data15" );
		node_set_data( pn, 15, ad );
		node_dumpA( pn, pf, 0 );

		node_set_nameA( pn, "Data16" );
		node_set_data( pn, 16, ad );
		node_dumpA( pn, pf, 0 );

		node_set_nameA( pn, "Data17" );
		node_set_data( pn, 17, ad );
		node_dumpA( pn, pf, 0 );

		fclose( pf );

		node_free( pn );

		pf = fopen( g_psFileName, "rb" );

		char acBuffer[2048] = {0};
		fread( acBuffer, 1, sizeof( acBuffer ), pf );

		fclose( pf );

		TS_ASSERT_EQUALS( std::string( acBuffer ), std::string( psData ) );
		wait();
	}

	void test_writing_dataW()
	{
		wchar_t * psData = 																\
		L"Data1: DATA 1\r\n"															\
	    L"$ 00                                                 .               \r\n"	\
		L"Data8: DATA 8\r\n"															\
		L"$ 00 01 02 03 04 05 06 07                            ........        \r\n"	\
		L"Data15: DATA 15\r\n"															\
		L"$ 00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e      ............... \r\n"	\
		L"Data16: DATA 16\r\n"															\
		L"$ 00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e 0f   ................\r\n"	\
		L"Data17: DATA 17\r\n"															\
		L"$ 00 01 02 03 04 05 06 07  08 09 0a 0b 0c 0d 0e 0f   ................\r\n"	\
		L"$ 10                                                 .               \r\n";
	
		node_t * pn = node_alloc();

		FILE * pf = NULL;

		pf = fopen( g_psFileName, "wb" );

		node_set_nameW( pn, L"Data1" );
		node_set_data( pn, 1, ad );
		node_dumpW( pn, pf, 0 );

		node_set_nameW( pn, L"Data8" );
		node_set_data( pn, 8, ad );
		node_dumpW( pn, pf, 0 );

		node_set_nameW( pn, L"Data15" );
		node_set_data( pn, 15, ad );
		node_dumpW( pn, pf, 0 );

		node_set_nameW( pn, L"Data16" );
		node_set_data( pn, 16, ad );
		node_dumpW( pn, pf, 0 );

		node_set_nameW( pn, L"Data17" );
		node_set_data( pn, 17, ad );
		node_dumpW( pn, pf, 0 );

		fclose( pf );

		node_free( pn );

		pf = fopen( g_psFileName, "rb" );

		wchar_t acBuffer[1024] = {0};
		fread( acBuffer, 1, sizeof( acBuffer ), pf );
		fclose( pf );

		TS_ASSERT_EQUALS( std::wstring( acBuffer ), std::wstring( psData ) );
		wait();
	}

	void test_writing_stringsA_A()
	{
		node_t * pnList = node_list_alloc();

		node_list_add( pnList, NODE_STRINGA, "String A" );
		node_list_add( pnList, NODE_STRINGW, L"String W" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pnList, pf, 0 );
		fclose( pf );

		node_free( pnList );

		pf = fopen( g_psFileName, "rb" );

		char acBuffer[1024] = {0};
		fread( acBuffer, 1, sizeof( acBuffer ), pf );
		fclose( pf );

		TS_ASSERT_EQUALS( std::string( acBuffer ), std::string( ": (\r\n  : 'String A'\r\n  : 'String W'\r\n)\r\n" ) );
		wait();
	}

	void test_writing_stringsA_W()
	{
		node_t * pnList = node_list_alloc();

		node_list_add( pnList, NODE_STRINGA, "String A" );
		node_list_add( pnList, NODE_STRINGW, L"String W" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpW( pnList, pf, 0 );
		fclose( pf );

		node_free( pnList );

		pf = fopen( g_psFileName, "rb" );

		wchar_t acBuffer[1024] = {0};
		fread( acBuffer, 1, sizeof( acBuffer ), pf );
		fclose( pf );

		TS_ASSERT_EQUALS( std::wstring( acBuffer ), std::wstring( L": (\r\n  : 'String A'\r\n  : 'String W'\r\n)\r\n" ) );
		wait();
	}

	void test_empty_ascii()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;
		pf = fopen( g_psFileName, "wb" );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_EOF );
	}

	void test_empty_unicode()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;

		pf = fopen( g_psFileName, "wb" );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_EOF );
	}

	void test_bom_unicode()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;

		wchar_t wBOM = 0xFEFF;
		pf = fopen( g_psFileName, "wb" );
		fwrite( &wBOM, sizeof(wchar_t), 1, pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_EOF );

		pf = fopen( g_psFileName, "rb" );
		nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_EOF );
	}

	void test_cparen()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;

		pf = fopen( g_psFileName, "wb" );
		fputs( ")\r\n", pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_CPAREN );

		pf = fopen( g_psFileName, "rb" );
		nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_CPAREN );
	}

	void test_cparen_unicode()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;

		wchar_t wBOM = 0xFEFF;
		pf = fopen( g_psFileName, "wb" );
		fwrite( &wBOM, sizeof(wchar_t), 1, pf );
		fputws( L")\r\n", pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_CPAREN );

		pf = fopen( g_psFileName, "rb" );
		nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_CPAREN );
	}

	void test_cbrace()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;

		pf = fopen( g_psFileName, "wb" );
		fputs( "}\r\n", pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_CBRACE );

		pf = fopen( g_psFileName, "rb" );
		nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_CBRACE );
	}

	void test_cbrace_unicode()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;

		wchar_t wBOM = 0xFEFF;
		pf = fopen( g_psFileName, "wb" );
		fwrite( &wBOM, sizeof(wchar_t), 1, pf );
		fputws( L"}\r\n", pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_CBRACE );

		pf = fopen( g_psFileName, "rb" );
		nResult = node_parseW( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_CBRACE );
	}


	void test_deep_nest()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;

		pn = node_list_alloc();

		for( int i = 0; i < 50; i++ )
		{
			node_t * pnList = node_list_alloc();
			node_list_add( pnList, NODE_REF, pn );
			pn = pnList;
		}

		pf = fopen( g_psFileName, "wb" );
		node_dump( pn, pf, 0 );
		fclose( pf );
		node_free( pn );

		pf = fopen( g_psFileName, "rb" );
		int nResult = node_parse( pf, &pn );
		fclose( pf );
	
		TS_ASSERT( nResult == NP_NODE );
		node_free( pn );
	}

	void test_shortline_1()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;
		int nResult = 0;

		pf = fopen( g_psFileName, "wb" );
		fputs( "i: 1\n", pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		nResult = node_parse( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( _tcscmp( node_get_name( pn ), _T("i") ) == 0 );
		TS_ASSERT( node_get_int( pn ) == 1 );
		node_free( pn );
	}

	void test_shortline_2()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;
		int nResult = 0;

		pf = fopen( g_psFileName, "wb" );
		fputs( "\ni: 1\n", pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		nResult = node_parse( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( _tcscmp( node_get_name( pn ), _T("i") ) == 0 );
		TS_ASSERT( node_get_int( pn ) == 1 );
		node_free( pn );
	}

	void test_shortline_3()
	{
		node_t * pn = NULL;
		FILE * pf = NULL;
		int nResult = 0;

		pf = fopen( g_psFileName, "wb" );
		fputs( "\r\ni: 1\n", pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		nResult = node_parse( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( _tcscmp( node_get_name( pn ), _T("i") ) == 0 );
		TS_ASSERT( node_get_int( pn ) == 1 );
		node_free( pn );
	}
};

class RealLifeProblems : public CxxTest::TestSuite
{
	char * psFileContents;

public:
	RealLifeProblems()
	{
		psFileContents = \
": {\r\n" \
"  Configuration Description: 'pcu191                                                                                                                                                                                                                                                        '\r\n" \
"  Firmware Revision: 'A'\r\n" \
"  Site: ''\r\n" \
"  Type: 'IIT02 '\r\n" \
"  Module ID: '  '\r\n" \
"  Drawing Title: '                              '\r\n" \
"  Drawing Number: '                                                                                                                                                                                                                                                              '\r\n" \
"  Loop: 1  (0x00000001)\r\n" \
"  PCU: 191  (0x000000BF)\r\n" \
"  Module: 2  (0x00000002)\r\n" \
"  Customer: ''\r\n" \
"}\r\n";
	}

	void setUp()
	{
		node_set_error_funcs( node_error_count, node_memory, (node_assert_func_t)node_assert );
	}
	void tearDown()
	{
		node_set_error_funcs( node_error, node_memory, (node_assert_func_t)node_assert );
		remove( g_psFileName );
	}

	void test_something()
	{
		FILE * pf = fopen( g_psFileName, "wb" );
		fwrite( psFileContents, strlen( psFileContents ), 1, pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		node_t * pn = NULL;
		node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT( node_hash_getA( pn, "Module ID" ) != NULL );
//		node_dump( pn, stderr, 0 );

		node_free( pn );
	}

	void test_4743()
	{
		char * ps = \
"Turbo Dump  Version 4.1 Copyright (c) 1988, 1994 Borland International\r\n" \
"                     Display of File 22100.MHD\r\n" \
"\r\n" \
"000000: 00 00 DE 07 E7 18 01 00  00 00 00 00 00 00 00 00 ................\r\n";


		FILE * pf = fopen( g_psFileName, "wb" );
		ERROR_SETUP;

		fwrite( ps, strlen( ps ), 1, pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		node_t * pn = NULL;
		int nResult = node_parseA( pf, &pn );
		fclose( pf );

		TS_ASSERT( nResult == NP_SERROR );
		TS_ASSERT( ERROR_AFTER == 1 );

		node_free( pn );
	}

	void test_4743b()
	{
		char * ps = \
"Turbo Dump  Version 4.1 Copyright (c) 1988, 1994 Borland International\r\n" \
"                     Display of File 22100.MHD\r\n" \
"\r\n" \
"000000: 00 00 DE 07 E7 18 01 00  00 00 00 00 00 00 00 00 ................\r\n";


		FILE * pf = fopen( g_psFileName, "wb" );
		ERROR_SETUP;

		fwrite( ps, strlen( ps ), 1, pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		node_t * pn = NULL;
		int nResult = 0;
		
		for( int i = 0; i < 2; i++ )
		{
			nResult = node_parse( pf, &pn );
			TS_ASSERT( nResult == NP_SERROR );
		}
		TS_ASSERT( ERROR_AFTER == 2 );
	
		nResult = node_parse( pf, &pn );
		TS_ASSERT( nResult == NP_NODE );
		TS_ASSERT( _tcscmp( node_get_name( pn ), _T("000000") ) == 0 );
		TS_ASSERT( node_get_int( pn ) == 0 );

		node_free( pn );

		nResult = node_parseA( pf, &pn );
		TS_ASSERT( nResult == NP_EOF );

		fclose( pf );


		node_free( pn );
	}

	void test_4747()
	{
		char * ps = \
": (\n" \
"  2614: 0  (0x00000000)\n" \
"  329: 0  (0x00000000)\n" \
"  312: 0  (0x00000000)\n" \
"  2104: 0  (0x00000000)\n" \
"  bottling: 0  (0x00000000)\n" \
"  306: 0  (0x00000000)\n" \
"  4113: 0  (0x00000000)\n" \
"  1510: 0  (0x00000000)\n" \
"  m61: 0  (0x00000000)\n" \
"  3075: 0  (0x00000000)\n" \
"  4361: 0  (0x00000000)\n" \
"  xa55-1: 0  (0x00000000)\n" \
"  xa5320: 0  (0x00000000)\n" \
"  ki355: 0  (0x00000000)\n" \
"  xa5600: 0  (0x00000000)\n" \
"  xa5607: 0  (0x00000000)\n" \
"  2108: 0  (0x00000000)\n" \
"  2114: 0  (0x00000000)\n" \
"  2101: 0  (0x00000000)\n" \
"  2101a: 0  (0x00000000)\n" \
"  5315: 0  (0x00000000)\n" \
"  5808: 0  (0x00000000)\n" \
"  1510A: 0  (0x00000000)\n" \
"  1510: 0  (0x00000000)\n" \
"  3982: 0  (0x00000000)\n" \
"  2421: 0  (0x00000000)\n" \
"  2411: 0  (0x00000000)\n" \
"  2403: 0  (0x00000000)\n" \
"  2402: 0  (0x00000000)\n" \
"  2401: 0  (0x00000000)\n" \
"  8492: 0  (0x00000000)\n" \
"  3401: 0  (0x00000000)\n" \
"  2610: 0  (0x00000000)\n" \
"  3283: 0  (0x00000000)\n" \
"  3203: 0  (0x00000000)\n" \
"  3207: 0  (0x00000000)\n" \
"  4340: 0  (0x00000000)\n" \
"  4111: 0  (0x00000000)\n" \
")\n" \
": (\n" \
"  113: 0  (0x00000000)\n" \
"  1710: 0  (0x00000000)\n" \
"  1772: 0  (0x00000000)\n" \
"  2614: 0  (0x00000000)\n" \
"  3075: 0  (0x00000000)\n" \
"  2240: 0  (0x00000000)\n" \
"  2239: 0  (0x00000000)\n" \
"  2238: 0  (0x00000000)\n" \
"  2237: 0  (0x00000000)\n" \
"  2236: 0  (0x00000000)\n" \
"  2235: 0  (0x00000000)\n" \
"  2234: 0  (0x00000000)\n" \
"  2233: 0  (0x00000000)\n" \
"  2232: 0  (0x00000000)\n" \
"  2231: 0  (0x00000000)\n" \
"  2230: 0  (0x00000000)\n" \
"  2229: 0  (0x00000000)\n" \
"  2228: 0  (0x00000000)\n" \
"  2227: 0  (0x00000000)\n" \
"  2226: 0  (0x00000000)\n" \
"  2225: 0  (0x00000000)\n" \
"  2224: 0  (0x00000000)\n" \
"  2223: 0  (0x00000000)\n" \
"  2222: 0  (0x00000000)\n" \
"  2221: 0  (0x00000000)\n" \
"  2220: 0  (0x00000000)\n" \
"  2219: 0  (0x00000000)\n" \
"  2218: 0  (0x00000000)\n" \
"  2217: 0  (0x00000000)\n" \
"  2216: 0  (0x00000000)\n" \
"  2215: 0  (0x00000000)\n" \
"  2214: 0  (0x00000000)\n" \
"  2213: 0  (0x00000000)\n" \
"  2212: 0  (0x00000000)\n" \
"  2211: 0  (0x00000000)\n" \
"  2210: 0  (0x00000000)\n" \
"  2209: 0  (0x00000000)\n" \
"  2208: 0  (0x00000000)\n" \
"  2207: 0  (0x00000000)\n" \
"  2206: 0  (0x00000000)\n" \
"  2205: 0  (0x00000000)\n" \
"  2204: 0  (0x00000000)\n" \
"  2203: 0  (0x00000000)\n" \
"  2202: 0  (0x00000000)\n" \
"  2201: 0  (0x00000000)\n" \
"  2200: 0  (0x00000000)\n" \
"  354: 0  (0x00000000)\n" \
"  353: 0  (0x00000000)\n" \
"  354: 0  (0x00000000)\n" \
")\n" \
": (\n" \
")\n";

		FILE * pf = fopen( g_psFileName, "wb" );
		fwrite( ps, strlen( ps ), 1, pf );
		fclose( pf );

		pf = fopen( g_psFileName, "rb" );
		node_t * pn = NULL;
		int nResult = 0;
		
		for( int i = 0; i < 2; i++ )
		{
			nResult = node_parse( pf, &pn );
			TS_ASSERT( nResult == NP_NODE );
			node_free( pn );
		}
		fclose( pf );
	
//		nResult = node_parse( pf, &pn );
//		TS_ASSERT( nResult == NP_NODE );
//		TS_ASSERT( _tcscmp( node_get_name( pn ), _T("000000") ) == 0 );
//		TS_ASSERT( node_get_int( pn ) == 0 );
//
//		node_free( pn );
//
//		nResult = node_parseA( pf, &pn );
//		TS_ASSERT( nResult == NP_EOF );
//
//		fclose( pf );
//
//
//		node_free( pn );
	}

	void test_dumpAtoW()
	{
		node_t * pn = node_hash_alloc();
		node_hash_addA( pn, "Test", NODE_STRINGA, "1" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpW( pn, pf, 0 );
		fclose( pf );

		/* now preconvert the value */
		node_get_stringW( node_hash_getA( pn, "Test" ) );
		pf = fopen( g_psFileName, "wb" );
		node_dumpW( pn, pf, 0 );
		fclose( pf );

		node_free( pn );
	}

	void test_dumpWtoA()
	{
		node_t * pn = node_hash_alloc();
		node_hash_addW( pn, L"Test", NODE_STRINGW, L"1" );

		FILE * pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 );
		fclose( pf );

		/* now preconvert the value */
		node_get_stringA( node_hash_getW( pn, L"Test" ) );
		pf = fopen( g_psFileName, "wb" );
		node_dumpA( pn, pf, 0 );
		fclose( pf );

		node_free( pn );
	}
};

class StringData : public CxxTest::TestSuite
{
public:
	void test_stringA()
	{
		node_t * pn = node_alloc();

		char * apsStrings[] = { "0123456789012345678901234567890",
								"01234567890123456789012345678901",
								"012345678901234567890123456789012",
								"0123456789012345678901234567890123",
								NULL };

		char ** pps = NULL;

		for( pps = apsStrings; *pps != NULL; ++pps )
		{
			node_set( pn, NODE_STRINGA, *pps );
			TS_ASSERT_EQUALS( std::string( *pps ), node_get_stringA( pn ) );
		}

		node_free( pn );
	}
	void test_stringW()
	{
		node_t * pn = node_alloc();

		wchar_t * apsStrings[] = { L"012345678901234",
								L"0123456789012345",
								L"01234567890123456",
								L"012345678901234567",
								NULL };

		wchar_t ** pps = NULL;

		for( pps = apsStrings; *pps != NULL; ++pps )
		{
			node_set( pn, NODE_STRINGW, *pps );
			TS_ASSERT_EQUALS( std::wstring( *pps ), node_get_stringW( pn ) );
		}

		node_free( pn );
	}
};


class NodeRef : public CxxTest::TestSuite
{
public:
	void test_addByRef()
	{
		node_t * pnHash = node_hash_alloc();

		node_t * pn = node_alloc();

		node_set( pn, NODE_INT, 3 );

		node_hash_add( pnHash, _T("Int"), NODE_REF, pn );
		pn = NULL;

		TS_ASSERT( node_hash_get( pnHash, _T("Int") ) != NULL );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("Int") ) ) == 3 );

//		TS_ASSERT( _CrtIsMemoryBlock( pn, sizeof(node_t), NULL, NULL, NULL ) );

		node_free( pnHash );

//		TS_ASSERT( !_CrtIsMemoryBlock( pn, sizeof(node_t), NULL, NULL, NULL ) );
	}
};

class HashCaseInsensitivity : public CxxTest::TestSuite
{
public:
	void test_addInsensitive()
	{
		node_t * pnHash = node_hash_alloc();

		node_hash_add( pnHash, _T("Int"), NODE_INT, 3 );

		TS_ASSERT( node_hash_get( pnHash, _T("Int") ) != NULL );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("Int") ) ) == 3 );

		TS_ASSERT( node_hash_get( pnHash, _T("INT") ) != NULL );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("INT") ) ) == 3 );

		TS_ASSERT( node_hash_get( pnHash, _T("int") ) != NULL );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("int") ) ) == 3 );

		node_free( pnHash );
	}

	void test_replaceInsensitive()
	{
		node_t * pnHash = node_hash_alloc();

		node_hash_add( pnHash, _T("Int"), NODE_INT, 3 );

		TS_ASSERT( node_hash_get( pnHash, _T("Int") ) != NULL );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("Int") ) ) == 3 );
		TS_ASSERT( node_get_elements( pnHash ) == 1 );

		node_hash_add( pnHash, _T("INT"), NODE_INT, 3 );
		TS_ASSERT( node_hash_get( pnHash, _T("INT") ) != NULL );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("INT") ) ) == 3 );
		TS_ASSERT( node_hash_get( pnHash, _T("Int") ) != NULL );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("Int") ) ) == 3 );
		TS_ASSERT( _tcscmp( node_get_name( node_hash_get( pnHash, _T("int") ) ), _T("INT") ) == 0 );

		TS_ASSERT( node_get_elements( pnHash ) == 1 );

		node_hash_add( pnHash, _T("int"), NODE_INT, 3 );
		TS_ASSERT( node_hash_get( pnHash, _T("int") ) != NULL );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("int") ) ) == 3 );
		TS_ASSERT( node_hash_get( pnHash, _T("INT") ) != NULL );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("INT") ) ) == 3 );
		TS_ASSERT( node_hash_get( pnHash, _T("Int") ) != NULL );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("Int") ) ) == 3 );
		TS_ASSERT( node_get_elements( pnHash ) == 1 );

		node_free( pnHash );
	}

//	void test_addSensitive()
//	{
//		node_t * pnHash = node_hash_alloc_sensitive(8);
//
//		node_hash_add( pnHash, _T("Int"), NODE_INT, 3 );
//		node_hash_add( pnHash, _T("INT"), NODE_INT, 4 );
//		node_hash_add( pnHash, _T("int"), NODE_INT, 5 );
//
//		TS_ASSERT( node_hash_get( pnHash, _T("Int") ) != NULL );
//		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("Int") ) ) == 3 );
//
//		TS_ASSERT( node_hash_get( pnHash, _T("INT") ) != NULL );
//		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("INT") ) ) == 4 );
//
//		TS_ASSERT( node_hash_get( pnHash, _T("int") ) != NULL );
//		TS_ASSERT( node_get_int( node_hash_get( pnHash, _T("int") ) ) == 5 );
//
//		node_free( pnHash );
//	}
//
//	void test_copySensitive()
//	{
//		node_t * pnHash = node_hash_alloc_sensitive(8);
//
//		node_hash_add( pnHash, _T("Int"), NODE_INT, 3 );
//		node_hash_add( pnHash, _T("INT"), NODE_INT, 4 );
//		node_hash_add( pnHash, _T("int"), NODE_INT, 5 );
//
//		node_t * pnCopy = node_copy( pnHash );
//
//		TS_ASSERT( node_hash_get( pnCopy, _T("Int") ) != NULL );
//		TS_ASSERT( node_get_int( node_hash_get( pnCopy, _T("Int") ) ) == 3 );
//
//		TS_ASSERT( node_hash_get( pnCopy, _T("INT") ) != NULL );
//		TS_ASSERT( node_get_int( node_hash_get( pnCopy, _T("INT") ) ) == 4 );
//
//		TS_ASSERT( node_hash_get( pnCopy, _T("int") ) != NULL );
//		TS_ASSERT( node_get_int( node_hash_get( pnCopy, _T("int") ) ) == 5 );
//
//		node_free( pnHash );
//		node_free( pnCopy );
//	}
};


class HashLongKey : public CxxTest::TestSuite
{
public:
	void test_shortkey()
	{
		node_t * pnHash = node_hash_alloc();
		_TCHAR * psKey = _T("AbcdefghijklmnopqrstuvwxyzAbcdefghijklmnopqrstuvwxyzAbcdefghijklmnopqrstuvwxyz");

		node_hash_add( pnHash, psKey, NODE_INT, 3 );

		TS_ASSERT( node_hash_get( pnHash, psKey ) != NULL );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, psKey ) ) == 3 );

		node_free( pnHash );
	}

	void test_longkey()
	{
		node_t * pnHash = node_hash_alloc();
		_TCHAR * psKey = _T("AbcdefghijklmnopqrstuvwxyzAbcdefghijklmnopqrstuvwxyzAbcdefghijklmnopqrstuvwxyzAbcdefghijklmnopqrstuvwxyzAbcdefghijklmnopqrstuvwxyzAbcdefghijklmnopqrstuvwxyzAbcdefghijklmnopqrstuvwxyzAbcdefghijklmnopqrstuvwxyzAbcdefghijklmnopqrstuvwxyz");

		node_hash_add( pnHash, psKey, NODE_INT, 3 );

		TS_ASSERT( node_hash_get( pnHash, psKey ) != NULL );
		TS_ASSERT( node_get_int( node_hash_get( pnHash, psKey ) ) == 3 );

		node_free( pnHash );
	}
};

struct EventAndCount
{
	HANDLE hEvent;
	volatile long nCount;
};

//// SLOW TESTS BELOW THIS POINT

class InternHog : public CxxTest::TestSuite
{
public:
	void test_internhog()
	{
		node_t * pnHash = node_hash_alloc2( 8192 );
		_TCHAR acBuffer[10];

		for( int i = 0; i < 5000; i++ )
		{
			_stprintf( acBuffer, _T("%05d"), i );
			node_hash_add( pnHash, acBuffer, NODE_INT, i );
		}

		node_free( pnHash );
	}

	void test_internthrash()
	{
		node_t * pnHash = node_hash_alloc();
		_TCHAR acBuffer[10];

		for( int i = 0; i < 10000; i++ )
		{
			_stprintf( acBuffer, _T("%06d"), 50000+i );
			node_set_name( pnHash, acBuffer );
		}

		node_free( pnHash );
	}
};


/* threads */
class NodeThread : public CxxTest::TestSuite
{
static node_t * m_pnThreadStack;
static CRITICAL_SECTION m_csStack;
static int m_nFilesInStack;

#define PRODUCER_RUNNING 0
#define CONSUMER_RUNNING 1
#define EVENT_COUNT		 2

public:
	void test_threadedaccess()
	{
		HANDLE aHandles[EVENT_COUNT] = {0};

		InitializeCriticalSection( &m_csStack );

		int i = 0;
		for( i = 0; i < EVENT_COUNT; i++ )
			aHandles[i] = CreateEvent( NULL, TRUE, FALSE, NULL );

		m_pnThreadStack = node_list_alloc();

		_beginthread( thread_producer, 0, aHandles );
		_beginthread( thread_consumer, 0, aHandles );

		WaitForMultipleObjects( EVENT_COUNT, aHandles, TRUE, INFINITE );

		DeleteCriticalSection( &m_csStack );
		for( i = 0; i < EVENT_COUNT; i++ )
			CloseHandle( aHandles[i] );

		node_free( m_pnThreadStack );
	}

	static void thread_producer( void * v )
	{
		HANDLE * aHandles = (HANDLE*)v;
		node_set_error_funcs( node_error, node_memory, (node_assert_func_t)node_assert );

		for( int j = 0; j < 10; j++ )
		{
			for( int i = 0; i < 10; i++ )
			{
				_TCHAR acBuffer[ 10 ] = {0};
				_stprintf( acBuffer, _T("%d"), i );
				node_t * pn = node_alloc();
				node_set( pn, NODE_INT, i );
				node_set_name( pn, acBuffer );

				EnterCriticalSection( &m_csStack );
				node_list_add( m_pnThreadStack, NODE_REF, pn );
				m_nFilesInStack++;
				TS_ASSERT( m_nFilesInStack == node_get_elements( m_pnThreadStack ) );
				LeaveCriticalSection( &m_csStack );
			}
			Sleep( 10 );
		}
		SetEvent( aHandles[ PRODUCER_RUNNING ] );
	}

	static void thread_consumer( void * v)
	{
		HANDLE * aHandles = (HANDLE*)v;
		node_set_error_funcs( node_error, node_memory, (node_assert_func_t)node_assert );

		while( WaitForSingleObject( aHandles[PRODUCER_RUNNING], 10 ) == WAIT_TIMEOUT )
		{
			if( m_nFilesInStack > 0 )
			{
				EnterCriticalSection( &m_csStack );

				node_t * pn = node_pop( m_pnThreadStack );
				m_nFilesInStack--;
	
				TS_ASSERT( m_nFilesInStack == node_get_elements( m_pnThreadStack ) );

				LeaveCriticalSection( &m_csStack );
				node_free( pn );
			}
		}
		SetEvent( aHandles[ CONSUMER_RUNNING ] );
	}

	void test_threadedAllocFree()
	{
#define ALLOC_THREADS	20

		struct EventAndCount e = {0};
		e.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
		e.nCount = 0;

		for( int i = 0; i < ALLOC_THREADS; i++ )
		{
			InterlockedIncrement( &(e.nCount) );
			_beginthread( threadmain_allocfree, 0, &e );
		}
		
		WaitForSingleObject( e.hEvent, INFINITE );

		CloseHandle( e.hEvent );

		TS_ASSERT( 1 );
	}

	static void threadmain_allocfree( void * pv )
	{
		node_set_error_funcs( node_error, node_memory, (node_assert_func_t)node_assert );
		for( int i = 0; i < 100; i++ )
		{
			node_t * pn = node_alloc();
			node_free(pn);
			pn = node_alloc();
			node_free(pn);
			Sleep(1);
		}
		struct EventAndCount * pe = (struct EventAndCount *)pv;
		if( InterlockedDecrement( &(pe->nCount) ) == 0 )
			SetEvent( pe->hEvent );
	}

	void test_threadedInternThrash()
	{
#define THRASH_THREADS	2
		struct EventAndCount e = {0};
		e.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
		e.nCount = 0;

		for( int i = 0; i < THRASH_THREADS; i++ )
		{
			InterlockedIncrement( &(e.nCount) );
			_beginthread( threadmain_internthrash, 0, &e );
		}
		
		WaitForSingleObject( e.hEvent, INFINITE );

		CloseHandle( e.hEvent );

		TS_ASSERT( 1 );
	}

	static void threadmain_internthrash( void * pv )
	{
		node_set_error_funcs( node_error, node_memory, (node_assert_func_t)node_assert );
		for( int i = 0; i < 40; i++ )
		{
			node_t * pnQ = node_hash_alloc2(64);
			_TCHAR acBuffer[10] = {0};

			for( int j = 0; j < 250; j++ )
			{
				/* rand on 4096 */
//				_stprintf( acBuffer, _T("%d"), rand() & 0xFFF );
				_itot( rand() & 0xFFF, acBuffer, 36 );
				node_hash_add( pnQ, acBuffer, NODE_INT, j ); 
			}
			node_free( pnQ );
			Sleep(3);
		}
		struct EventAndCount * pe = (struct EventAndCount *)pv;
		if( InterlockedDecrement( &(pe->nCount) ) == 0 )
			SetEvent( pe->hEvent );
	}

	void test_threadedAllocFreeHold()
	{
#define HOLD_THREADS 5
		struct EventAndCount e = {0};
		e.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
		e.nCount = 0;

		for( int i = 0; i < HOLD_THREADS; i++ )
		{
			InterlockedIncrement( &(e.nCount) );
			_beginthread( threadmain_allocfree, 0, &e);
			InterlockedIncrement( &(e.nCount) );
			_beginthread( threadmain_hold, 0, &e );
		}
		

		WaitForSingleObject( e.hEvent, INFINITE );

		CloseHandle( e.hEvent );

		TS_ASSERT( 1 );
	}

	static void threadmain_hold( void * pv )
	{
		node_set_error_funcs( node_error, node_memory, (node_assert_func_t)node_assert );

		node_t * pnList = node_list_alloc();
		for( int j = 0; j < 30; j++ )
		{
			for( int i = 0; i < 100; i++ )
			{
				node_list_add( pnList, NODE_INT, 1 );
			}
			Sleep(3);
		}
		node_free( pnList );

		struct EventAndCount * pe = (struct EventAndCount *)pv;
		if( InterlockedDecrement( &(pe->nCount) ) == 0 )
			SetEvent( pe->hEvent );
	}

};


node_t * NodeThread::m_pnThreadStack = NULL;
CRITICAL_SECTION NodeThread::m_csStack = {0};
int NodeThread::m_nFilesInStack = 0;


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


class NodeCollision : public CxxTest::TestSuite
{
public:
	void testCollideA()
	{
//		Collision: '100110' and '810006' hash to 27314311
		int nKey1 = node_hashA( "100110" );
		int nKey2 = node_hashA( "810006" );

		TS_ASSERT_EQUALS( nKey1, nKey2 );
		TS_ASSERT_EQUALS( nKey1, 27314311 );
	}
	void testCollideW()
	{
		int nKey1 = node_hashW( L"100110" );
		int nKey2 = node_hashW( L"810006" );

		TS_ASSERT_EQUALS( nKey1, nKey2 );
		TS_ASSERT_EQUALS( nKey1, 27314311 );
	}

	void testAddBothA()
	{
		node_t * pnHash = node_hash_alloc();

		const char * psKey1 = "100110";
		const char * psKey2 = "810006";

		node_hash_addA( pnHash, psKey1, NODE_INT, 1 );
		node_hash_addA( pnHash, psKey2, NODE_INT, 2 );

		node_t * pn1 = node_hash_getA( pnHash, psKey1 );
		node_t * pn2 = node_hash_getA( pnHash, psKey2 );

		TS_ASSERT( pn1 != pn2 );
		TS_ASSERT_EQUALS( node_get_int( pn1 ), 1 );
		TS_ASSERT_EQUALS( node_get_int( pn2 ), 2 );

		node_free( pnHash );
	}

	void testAddBothW()
	{
		node_t * pnHash = node_hash_alloc();

		const wchar_t * psKey1 = L"100110";
		const wchar_t * psKey2 = L"810006";

		node_hash_addW( pnHash, psKey1, NODE_INT, 1 );
		node_hash_addW( pnHash, psKey2, NODE_INT, 2 );

		node_t * pn1 = node_hash_getW( pnHash, psKey1 );
		node_t * pn2 = node_hash_getW( pnHash, psKey2 );

		TS_ASSERT( pn1 != pn2 );
		TS_ASSERT_EQUALS( node_get_int( pn1 ), 1 );
		TS_ASSERT_EQUALS( node_get_int( pn2 ), 2 );

		node_free( pnHash );
	}

	

};
