#define CATCH_CONFIG_MAIN
#include "catch.hpp"

TEST_CASE( "tests run", "[meta]" ) {
  REQUIRE( 1 == 1 );
}
