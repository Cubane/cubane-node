
#include <exception>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

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

struct setup {
  setup () {
    node_set_error_funcs(node_error, node_memory, (node_assert_func_t) node_assert );
  }
};

setup __setup;

TEST_CASE("report version", "[meta][version]" ) {
  REQUIRE( node_version() != NULL );
  fprintf(stderr, "%s\n", node_version());
}
