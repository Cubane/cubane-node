#include <cxxtest/TestSuite.h>

#ifdef _WIN32
#   include <windows.h>
#   define CXXTEST_SAMPLE_GUI_WAIT() Sleep( 200 )
#else // !_WIN32
    extern "C" unsigned sleep( unsigned seconds );
#   define CXXTEST_SAMPLE_GUI_WAIT() sleep( 1 )
#endif // _WIN32

class GreenYellowRed : public CxxTest::TestSuite
{
public:

};
