# Where to look for the tests
#TESTS            = test/nongui/*.h test/gui/*.h
TESTS            = test\*.h

# Where the CxxTest distribution is unpacked
CXXTESTDIR       = ..\..\cxxtest

# Check CXXTESTDIR
!if !exist($(CXXTESTDIR)\cxxtestgen.pl)
!error Please fix CXXTESTDIR
!endif

# cxxtestgen needs Perl or Python
!if defined(PERL)
CXXTESTGEN       = $(PERL) $(CXXTESTDIR)\cxxtestgen.pl
!elseif defined(PYTHON)
CXXTESTGEN       = $(PYTHON) $(CXXTESTDIR)\cxxtestgen.py
!else
!error You must define PERL or PYTHON
!endif

# The arguments to pass to cxxtestgen
#  - ParenPrinter is the way MSVC likes its compilation errors
#  - --have-eh/--abort-on-fail are nice when you have them
CXXTESTGEN_FLAGS = --gui=Win32Gui --runner=ParenPrinter --have-eh --abort-on-fail

ALL: runner.cpp git-version.h

# How to generate the test runner, "runner.cpp"
runner.cpp: $(TESTS)
	$(CXXTESTGEN) $(CXXTESTGEN_FLAGS) -o $@ $(TESTS)

# Command-line arguments to the runner
RUNNER_FLAGS = -title "CxxTest Runner"

# How to run the tests, which should be in DIR\runner.exe
run: "$(DIR)\runner.exe"
	"$(DIR)\runner.exe" $(RUNNER_FLAGS)

runU: "$(DIR)\runnerU.exe"
	"$(DIR)\runnerU.exe" $(RUNNER_FLAGS)


SolutionDir = .



