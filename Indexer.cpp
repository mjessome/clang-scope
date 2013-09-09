#include "clang/Tooling/Tooling.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Index/USRGeneration.h"

#include <iostream>
#include <vector>
#include <time.h>

#include "ClangScope.h"
#include "Indexer.h"

using namespace clang::tooling;

extern llvm::cl::opt<unsigned> LogLevel;

struct IndexASTVisitor : clang::RecursiveASTVisitor<IndexASTVisitor> {
public:
  IndexASTVisitor() {}

  bool VisitVarDecl(clang::VarDecl *d) {
    if (d->isLocalVarDecl() || clang::isa<clang::ParmVarDecl>(d))
      return true; // Only want global decls
    LOG(3, "VarDecl: " << d->getNameAsString() << std::endl);
    // Add reference
    return true;
  }
  bool VisitFunctionDecl(clang::FunctionDecl *d) {
    LOG(3, "FunctionDecl: " << d->getNameAsString() << std::endl);
    // Add reference
    return true;
  }
  bool VisitNamespaceDecl(clang::NamespaceDecl *d) {
    LOG(3, "NamespaceDecl: " << d->getNameAsString() << std::endl);
    // Add reference
    return true;
  }
  bool VisitTagDecl(clang::TagDecl *d) {
    LOG(3, "TagDecl: " << d->getNameAsString() << std::endl);
    // Add reference
    return true;
  }
  bool VisitEnumConstantDecl(clang::EnumConstantDecl *d) {
    LOG(3, "EnumConstantDecl: " << d->getNameAsString() << std::endl);
    // Add reference
    return true;
  }
  bool VisitDeclRefExpr(clang::DeclRefExpr *e) {
    clang::NamedDecl *d = e->getDecl();
    if (clang::VarDecl *v = clang::dyn_cast<clang::VarDecl>(d)) {
      if (v->isLocalVarDecl())
        return true;
      else if (clang::isa<clang::ParmVarDecl>(d))
        return true;
    }
    LOG(3, "DeclRefExpr: " << d->getNameAsString() << std::endl);
    // Add reference
    return true;
  }
};

class IndexASTConsumer : public clang::ASTConsumer {
public:
  IndexASTConsumer(clang::CompilerInstance &ci) {}
  virtual void Initialize(clang::ASTContext &Ctx) {}
  virtual void HandleTranslationUnit(clang::ASTContext &Ctx) {
    IndexASTVisitor v;
    v.TraverseDecl(Ctx.getTranslationUnitDecl());
  }
};

class IndexASTAction : public clang::ASTFrontendAction {
public:
  virtual clang::ASTConsumer *CreateASTConsumer(clang::CompilerInstance &CI,
                                                llvm::StringRef InFile) {
    return new IndexASTConsumer(CI);
  }
};

Indexer::Indexer(std::string Path, CrossReference &CrossRef)
    : CompilationDbPath(Path), CrossRef(CrossRef) {}

Indexer::~Indexer() {
  delete CompilationDb;
}

// Opens the compilation database, and gets the file list.
bool Indexer::OpenCompilationDatabase() {
  std::string Err;
  CompilationDb =
      CompilationDatabase::loadFromDirectory(CompilationDbPath, Err);
  if (!CompilationDb) {
    std::cerr << Err;
    return false;
  }
  Files = CompilationDb->getAllFiles();
  return true;
}

bool Indexer::Index() {
  if (!OpenCompilationDatabase()) return false;

  ClangTool Tool(*CompilationDb, Files);
  bool result = Tool.run(newFrontendActionFactory<IndexASTAction>());
  return result;
}
