
CLANG_LIBS := \
	-lclangAST \
	-lclangASTMatchers \
	-lclangAnalysis \
	-lclangBasic \
	-lclangDriver \
	-lclangEdit \
	-lclangFrontend \
	-lclangFrontendTool \
	-lclangLex \
	-lclangParse \
	-lclangSema \
	-lclangEdit \
	-lclangRewrite \
	-lclangRewriteFrontend \
	-lclangStaticAnalyzerFrontend \
	-lclangStaticAnalyzerCheckers \
	-lclangStaticAnalyzerCore \
	-lclangSerialization \
	-lclangToolingCore \
	-lclangTooling \
	-lclangFormat \

WARNINGS := \
        -Wall \
        -Wextra \
        -Wunused \
        -Wdeprecated \
        -Wc++11-compat \
        -Wfuture-compat \
				-Wunreachable-code \

all: pointer-finder

.phony: clean

clean:
	rm pointer-finder || echo -n ""

pointer-finder: pointer-finder.cpp
	$(CXX) $(WARNINGS) -I/usr/local/opt/llvm/include `llvm-config --cxxflags` -std=c++1z -fno-rtti pointer-finder.cpp `llvm-config --libs --ldflags --system-libs` ${CLANG_LIBS} -o pointer-finder
