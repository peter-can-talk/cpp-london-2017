#ifndef USE_OVERRIDE_HPP
#define USE_OVERRIDE_HPP

// Clang includes
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/AttrIterator.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclarationName.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/RewriteBuffer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Basic/TokenKinds.h"

// LLVM includes
#include "llvm//Support/Path.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

// Standard includes
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace UseOverride {

namespace {
template <typename RangeType, typename FunctionType>
bool noneOf(const RangeType& Range, const FunctionType& Function) {
  using std::begin;
  using std::end;
  return std::none_of(begin(Range), end(Range), Function);
}
}


class Visitor : public clang::RecursiveASTVisitor<Visitor> {
 public:
  using MatchCallback = std::function<void(bool)>;

  Visitor(bool RewriteOption, clang::Rewriter& Rewriter, MatchCallback Callback)
  : Rewriter(Rewriter), Callback(Callback), RewriteOption(RewriteOption) {
  }

  bool VisitDecl(clang::Decl* Decl) {
    const auto* MethodDecl = llvm::dyn_cast<clang::CXXMethodDecl>(Decl);
    if (!MethodDecl) {
      return true;
    }

    if (shouldUseOverride(*MethodDecl)) {
      handleOverride(*MethodDecl);
      Callback(true);
    } else {
      Callback(false);
    }

    // Always continue traversing
    return true;
  }

  Visitor& setContext(const clang::ASTContext& Context) {
    this->Context = &Context;
    return *this;
  }

  const clang::ASTContext& getContext() const noexcept {
    assert(hasContext());
    return *Context;
  }

  bool hasContext() const noexcept {
    return Context != nullptr;
  }

 private:
  bool shouldUseOverride(const clang::CXXMethodDecl& MethodDecl) {
    if (MethodDecl.size_overridden_methods() == 0) return false;
    return noneOf(MethodDecl.getAttrs(), [](const auto* Attr) {
      return Attr->getSpelling() == "override";
    });
  }

  void handleOverride(const clang::CXXMethodDecl& MethodDecl) {
    clang::SourceLocation InsertionPoint = findEndOfParameterList(MethodDecl);

    auto& DiagnosticsEngine = Context->getDiagnostics();
    DiagnosticsEngine.setShowColors(true);

    const auto ID = DiagnosticsEngine.getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "method '%0' should be declared override");

    clang::DiagnosticBuilder Diagnostic =
        DiagnosticsEngine.Report(InsertionPoint, ID);
    Diagnostic.AddString(MethodDecl.getName());

    if (RewriteOption) {
      insertOverride(InsertionPoint);
    } else {
      const auto FixIt =
          clang::FixItHint::CreateInsertion(InsertionPoint, "override");
      Diagnostic.AddFixItHint(FixIt);
    }

    // Diagnostic emitted here.
  }

  void insertOverride(const clang::SourceLocation& InsertionPoint) {
    // Returns true if the Rewriter was *not* able to rewrite text.
    if (Rewriter.InsertText(InsertionPoint, " override ")) {
      assert(false && "Could not insert 'override'");
    }
  }

  clang::SourceLocation
  findEndOfParameterList(const clang::CXXMethodDecl& MethodDecl) {
    clang::SourceLocation Location;

    if (MethodDecl.param_empty()) {
      // NameInfo.getAsString().length()
      const clang::DeclarationNameInfo NameInfo = MethodDecl.getNameInfo();
      auto Offset =
          clang::Lexer::MeasureTokenLength(NameInfo.getLoc(),
                                           Context->getSourceManager(),
                                           Context->getLangOpts());
      Location = NameInfo.getLoc().getLocWithOffset(Offset);
    } else {
      const clang::ParmVarDecl* Last = *std::prev(MethodDecl.param_end());
      Location = Last->getLocEnd();
    }

    // Finds the first valid source location after the given token, which we
    // specify being of r_paren type. Setting the last parameter to true means
    // any whitespace will be skipped and we will effectively land at the next
    // token.
    Location = clang::Lexer::findLocationAfterToken(Location,
                                                    clang::tok::r_paren,
                                                    Context->getSourceManager(),
                                                    Context->getLangOpts(),
                                                    /*skipWhiteSpace=*/true);
    //   f() {          f() {
    // want ^   and not     ^
    return Location.getLocWithOffset(-1);
  }

  clang::Rewriter& Rewriter;
  const clang::ASTContext* Context;
  MatchCallback Callback;
  bool RewriteOption;
};

class Consumer : public clang::ASTConsumer {
 public:
  using MatchCallback = Visitor::MatchCallback;

  template <typename... Args>
  explicit Consumer(Args&&... args) : Visitor(std::forward<Args>(args)...) {
  }

  void HandleTranslationUnit(clang::ASTContext& Context) override {
    Visitor.setContext(Context).TraverseDecl(Context.getTranslationUnitDecl());
  }

 private:
  Visitor Visitor;
};

class Action : public clang::ASTFrontendAction {
 public:
  using ASTConsumerPointer = std::unique_ptr<clang::ASTConsumer>;

  explicit Action(bool RewriteOption)
  : RewriteOption(RewriteOption), GoodCount(0), BadCount(0) {
  }

  ASTConsumerPointer CreateASTConsumer(clang::CompilerInstance& Compiler,
                                       llvm::StringRef) override {
    Rewriter.setSourceMgr(Compiler.getSourceManager(), Compiler.getLangOpts());
    return std::make_unique<Consumer>(
        RewriteOption, Rewriter, [this](bool good) {
          if (good) {
            GoodCount += 1;
          } else {
            BadCount += 1;
          }
        });
  }

  bool BeginSourceFileAction(clang::CompilerInstance& Compiler,
                             llvm::StringRef AbsolutePath) override {
    CurrentFilename = truncatePath(AbsolutePath);
    llvm::errs() << "Processing " << CurrentFilename << "\n\n";
    return true;
  }

  void EndSourceFileAction() override {
    // clang-format off
    auto Message = (llvm::Twine("Found ")
                 + llvm::Twine(GoodCount)
                 + " function(s) that were missing 'override'. "
                 + llvm::Twine(BadCount)
                 + " function(s) were OK. \n");
    // clang-format on
    llvm::errs() << Message;

    if (RewriteOption) {
      llvm::errs() << "\n"
                   << std::string(Message.str().length() - 2, '~')
                   << "\n\nRewrote " << CurrentFilename << ": \n";
      const auto File = Rewriter.getSourceMgr().getMainFileID();
      Rewriter.getEditBuffer(File).write(llvm::outs());
    }
  }

 private:
  StringRef
  truncatePath(StringRef AbsolutePath, std::size_t WantedComponents = 3) const {
    auto Index = AbsolutePath.size() - 1;
    auto TotalNumberOfComponents = AbsolutePath.count('/');
    auto Amount = std::min(TotalNumberOfComponents, WantedComponents);
    for (std::size_t Count = 0; Count < Amount; ++Count) {
      Index = AbsolutePath.rfind('/', Index);
    }

    auto Substring = AbsolutePath.substr(Index + 1);

    if (TotalNumberOfComponents > WantedComponents) {
      return (llvm::Twine(".../") + Substring).str();
    } else {
      return Substring;
    }
  }

  bool RewriteOption;
  std::size_t GoodCount;
  std::size_t BadCount;

  clang::Rewriter Rewriter;
  std::string CurrentFilename;
};
}  // namespace UseOverride

namespace {
llvm::cl::OptionCategory UseOverrideCategory("use-override options");

llvm::cl::extrahelp UseOverrideHelp(R"(
This tool ensures that you use the 'override' keyword appropriately.
For example, given this snippet of code:

  struct Base {
    virtual void method(int);
  };

  struct Derived : public Base {
    void method(int);
  };

Running this tool over the code will produce a warning message stating that the
declaration 'method()' should be followed by the keyword 'override'.
)");

llvm::cl::opt<bool>
    RewriteOption("rewrite",
                  llvm::cl::init(false),
                  llvm::cl::desc("If set, emits rewritten source code"),
                  llvm::cl::cat(UseOverrideCategory));


llvm::cl::extrahelp
    CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
}  // namespace

struct ToolFactory : public clang::tooling::FrontendActionFactory {
  clang::FrontendAction* create() override {
    return new UseOverride::Action(RewriteOption);
  }
};

auto main(int argc, const char* argv[]) -> int {
  using namespace clang::tooling;

  CommonOptionsParser OptionsParser(argc, argv, UseOverrideCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  return Tool.run(new ToolFactory());
}

#endif  // USE_OVERRIDE_HPP
