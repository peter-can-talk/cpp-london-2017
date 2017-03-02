// Clang includes
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

// LLVM includes
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

// Standard includes
#include <memory>
#include <string>
#include <vector>

class Handler : public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  using MatchResult = clang::ast_matchers::MatchFinder::MatchResult;
  explicit Handler(const clang::ASTContext& Context)
  : Context(Context), SourceManager(Context.getSourceManager()) {
  }

  void run(const MatchResult& Result) {
    for (auto Binding : {"field", "var"}) {
      const auto* Decl = Result.Nodes.getNodeAs<clang::DeclaratorDecl>(Binding);

      if (!Decl || SourceManager.isInSystemHeader(Decl->getLocation())) return;

      const auto Name = Decl->getName();

      if (!Name.empty() && !Name.startswith("p_")) {
        auto& DE = Context.getDiagnostics();
        const auto ID = DE.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                           "pointer variable '%0' should "
                                           "have a 'p_' prefix");

        const auto FixIt =
            clang::FixItHint::CreateInsertion(Decl->getLocation(), "p_");

        auto DB = DE.Report(Decl->getLocation(), ID);
        DB.AddString(Name);
        DB.AddFixItHint(FixIt);
      }
    }
  }

  const clang::ASTContext& Context;
  const clang::SourceManager& SourceManager;
};

class Consumer : public clang::ASTConsumer {
 public:
  void HandleTranslationUnit(clang::ASTContext& Context) override {
    using namespace clang::ast_matchers;
    MatchFinder Finder;
    Handler Handler(Context);
    // clang-format off
    auto Matcher = decl(anyOf(
      fieldDecl(hasType(pointerType())).bind("field"),
      varDecl(hasType(pointerType())).bind("var"))
    );
    // clang-format on
    Finder.addMatcher(Matcher, &Handler);
    Finder.matchAST(Context);
  }
};

class Action : public clang::ASTFrontendAction {
 public:
  using ASTConsumerPointer = std::unique_ptr<clang::ASTConsumer>;

  ASTConsumerPointer CreateASTConsumer(clang::CompilerInstance& Compiler,
                                       llvm::StringRef Filename) override {
    return std::make_unique<Consumer>();
  }

  bool BeginSourceFileAction(clang::CompilerInstance& Compiler,
                             llvm::StringRef Filename) override {
    llvm::outs() << "Processing file " << Filename << " ...\n";
    return true;
  }

  void EndSourceFileAction() override {
    llvm::outs() << "Done processing file ...\n";
  }
};

namespace {
llvm::cl::extrahelp MoreHelp("\nMakes sure pointers have a 'p_' prefix\n");
llvm::cl::OptionCategory ToolCategory("PointerFinder");
llvm::cl::extrahelp
    CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
}

auto main(int argc, const char* argv[]) -> int {
  using namespace clang::tooling;

  CommonOptionsParser OptionsParser(argc, argv, ToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  auto action = newFrontendActionFactory<Action>();
  return Tool.run(action.get());
}
