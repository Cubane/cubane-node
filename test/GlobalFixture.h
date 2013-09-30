#include <cxxtest/TestSuite.h>
#include <cxxtest/GlobalFixture.h>
#include <stdio.h>

class PrintingFixture : public CxxTest::GlobalFixture
{
public:
    bool setUpWorld() 
	{ 
		printf( "<world>" ); 
		return true; 
	}
    bool tearDownWorld() { printf( "</world>" ); return true; }
    bool setUp() { printf( "<test>" ); return true; }
    bool tearDown() { printf( "</test>" ); return true; }
};

static PrintingFixture printingFixture;