HEADERS = \
-isystem /Users/goldsborough/Documents/Projects/llvm/tools/clang/include/

LIB_DIRS := -L/Users/goldsborough/Documents/Projects/llvm/build/lib/

TARGET := code-complete

CXXFLAGS := -std=c++14

all: $(TARGET)

.phony: clean
.phony: run

clean:
	rm $(TARGET) || echo -n ""

code-complete: $(TARGET).cpp
	$(CXX) $(WARNINGS) $(HEADERS) $(LIB_DIRS) $(CXXFLAGS) $(TARGET).cpp -lclang -o $(TARGET)

run: code-complete
	DYLD_LIBRARY_PATH=/Users/goldsborough/Documents/Projects/llvm/build/lib/ ./$(TARGET) $(ARGS)
