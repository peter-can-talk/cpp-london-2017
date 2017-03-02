#include <clang-c/Index.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

void printChunks(CXCompletionString completion) {
  std::string resultType;

  unsigned numberOfChunks = clang_getNumCompletionChunks(completion);
  for (unsigned chunk = 0; chunk < numberOfChunks; ++chunk) {
    CXString cxString = clang_getCompletionChunkText(completion, chunk);
    auto text = clang_getCString(cxString);

    switch (clang_getCompletionChunkKind(completion, chunk)) {
      case CXCompletionChunk_ResultType:
        resultType = text;
        break;
      case CXCompletionChunk_Placeholder:
        std::cout << "<" << text << ">";
        break;
      case CXCompletionChunk_Informative:
        break;
      default:
        std::cout << text;
    }

    clang_disposeString(cxString);
  }

  if (!resultType.empty()) {
    std::cout << " -> " << resultType;
  }
}

void codeCompleteAt(CXTranslationUnit tu,
                    const char* filename,
                    unsigned line,
                    unsigned column) {
  auto options =
      CXCodeComplete_IncludeCodePatterns | CXCodeComplete_IncludeBriefComments;
  CXCodeCompleteResults* listOfResults =
      clang_codeCompleteAt(tu, filename, line, column, nullptr, 0, options);

  for (unsigned index = 0; index < listOfResults->NumResults; ++index) {
    const CXCompletionResult result = listOfResults->Results[index];
    const CXCompletionString completion = result.CompletionString;

    if (result.CursorKind == CXCursor_NotImplemented) continue;

    printChunks(completion);

    CXString cxString = clang_getCompletionBriefComment(completion);
    if (cxString.data) {
      std::cout << " // " << clang_getCString(cxString);
    }
    clang_disposeString(cxString);

    std::cout << '\n';
  }

  clang_disposeCodeCompleteResults(listOfResults);
}

auto main(int argc, const char* argv[]) -> int {
  if (argc != 4) {
    std::cerr << "usage: file line column\n";
    return EXIT_FAILURE;
  }

  CXIndex index = clang_createIndex(1, 1);

  CXTranslationUnit tu =
      clang_parseTranslationUnit(index, argv[1], nullptr, 0, nullptr, 0, 0);

  if (tu == nullptr) {
    std::cerr << "Error\n";
  } else {
    codeCompleteAt(tu, argv[1], std::atoi(argv[2]), std::atoi(argv[3]));
    clang_disposeTranslationUnit(tu);
  }

  clang_disposeIndex(index);
}
