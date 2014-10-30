
#include <exception>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#define NODE_TRANSPARENT
#include "../node.h"

TEST_CASE( "tests run", "[meta]" ) {
  REQUIRE( 1 == 1 );
}

void node_error( const char * psError ) {
  throw std::exception();
}

int node_memory( size_t /*cb*/ ) {
  throw std::bad_alloc();
  return NODE_MEMORY_FAIL;
}

void node_assert( const char * psExpr, const char * psFile, unsigned int nLine ) {
  char acBuffer[1024] = {0};

  sprintf( acBuffer, "Node assertion failed: %s at %s:%d", psExpr, psFile, nLine );
}


// get a filename
// g_psFilename = _strdup( "test_nodeXXXXXX" );
//

char * g_psFileName;

struct setup {
  setup () {
    node_set_error_funcs(node_error, node_memory, (node_assert_func_t) node_assert );
    g_psFileName = strdup("/tmp/test_nodeXXXXXX");
    mktemp(g_psFileName);
  }
  ~setup () {
    remove(g_psFileName);
    free(g_psFileName);
  }
};

setup __setup;

TEST_CASE("report version", "[meta][version]" ) {
  REQUIRE( node_version() != NULL );
  fprintf(stderr, "%s\n", node_version());
}

TEST_CASE("check size", "[meta][size]" ) {
  size_t nSize = sizeof(struct __node);

  REQUIRE( nSize == 64 );
}

TEST_CASE("regression - name is brace", "[regression][parse]" ) {
  node_t * pn = NULL;
  const char * psExpected = "%7D: \'foo\'\r\n";

  pn = node_alloc();
  node_set(pn, NODE_STRINGA, "foo");
  node_set_nameA( pn, "}" );

  FILE * pf = fopen(g_psFileName, "wb");
  node_dumpA( pn, pf, 0 );
  fclose(pf);

  char acBuffer[1024];
  pf = fopen(g_psFileName, "rb");
  fread(acBuffer, 1, sizeof(acBuffer), pf);
  fclose(pf);

  REQUIRE( strncmp( psExpected, acBuffer, 12 ) == 0 );

  node_free(pn);
}
