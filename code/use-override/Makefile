
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

all: use-override

.phony: clean
.phony: iwyu

clean:
	rm use-override || echo -n ""

iwyu:
	$(MAKE) clean all CXX=include-what-you-use

use-override: use-override.cpp
	$(CXX) $(WARNINGS) -I/usr/local/opt/llvm/include $(shell llvm-config --cxxflags) -std=c++1z -fno-rtti use-override.cpp `llvm-config --libs --ldflags --system-libs` ${CLANG_LIBS} -o use-override
