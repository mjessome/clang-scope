#include "clang/Tooling/Tooling.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include <iostream>
#include <vector>
#include <time.h>

#include "ClangScope.h"
#include "Indexer.h"

using namespace clang::tooling;

extern llvm::cl::opt<unsigned> LogLevel;
// FIXME: Don't want to have to grab this global Index pointer.
extern Indexer *Index;

IdentifierType GetIdentifierForDecl(clang::NamedDecl *d) {
  if (clang::isa<clang::VarDecl>(d))
    return Variable;
  if (clang::isa<clang::FunctionDecl>(d))
    return Function;
  if (clang::isa<clang::NamespaceDecl>(d))
    return Namespace;
  if (clang::isa<clang::TypeDecl>(d))
    return Type;
  if (clang::isa<clang::TypedefDecl>(d))
    return Typedef;
  if (clang::isa<clang::EnumConstantDecl>(d))
    return Enum;
  return IdentifierType_Max;
}

template <typename T> static ReferenceType DeclarationOrDefinition(T *d) {
  return (d->isThisDeclarationADefinition()) ? Definition : Declaration;
}

struct IndexASTVisitor : clang::RecursiveASTVisitor<IndexASTVisitor> {
public:
  IndexASTVisitor() {}

  bool VisitVarDecl(clang::VarDecl *d) {
    if (d->isLocalVarDecl() || clang::isa<clang::ParmVarDecl>(d))
      return true; // Only want global decls
    LOG(3, "VarDecl: " << d->getNameAsString() << std::endl);
    Index->CrossRef.AddReference(d, DeclarationOrDefinition(d), Variable);
    return true;
  }
  bool VisitFunctionDecl(clang::FunctionDecl *d) {
    LOG(3, "FunctionDecl: " << d->getNameAsString() << std::endl);
    // Add reference
    Index->CrossRef.AddReference(d, DeclarationOrDefinition(d), Function);
    return true;
  }
  bool VisitNamespaceDecl(clang::NamespaceDecl *d) {
    LOG(3, "NamespaceDecl: " << d->getNameAsString() << std::endl);
    // Add reference
    Index->CrossRef.AddReference(d, Declaration, Namespace);
    return true;
  }
  bool VisitTagDecl(clang::TagDecl *d) {
    LOG(3, "TagDecl: " << d->getNameAsString() << std::endl);
    // Add reference
    Index->CrossRef.AddReference(d, DeclarationOrDefinition(d), Typedef);
    return true;
  }
  bool VisitEnumConstantDecl(clang::EnumConstantDecl *d) {
    LOG(3, "EnumConstantDecl: " << d->getNameAsString() << std::endl);
    // Add reference
    Index->CrossRef.AddReference(d, Definition, Enum);
    return true;
  }
  bool VisitMemberExpr(clang::MemberExpr *e) {
    clang::NamedDecl *d = e->getFoundDecl().getDecl();
    if (clang::VarDecl *v = clang::dyn_cast<clang::VarDecl>(d)) {
      if (v->isLocalVarDecl())
        return true;
      else if (clang::isa<clang::ParmVarDecl>(d))
        return true;
    }
    IdentifierType Id = GetIdentifierForDecl(d);
    if (Id == IdentifierType_Max)
      return true;
    LOG(3, "MemberRefExpr: " << d->getNameAsString() << std::endl);
    Index->CrossRef.AddReference(d, e, Use, Id);
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
    IdentifierType Id = GetIdentifierForDecl(d);
    if (Id == IdentifierType_Max)
      return true;
    LOG(3, "DeclRefExpr: " << d->getNameAsString() << std::endl);
    Index->CrossRef.AddReference(d, e, Use, Id);
    return true;
  }
};

class IndexASTConsumer : public clang::ASTConsumer {
public:
  IndexASTConsumer(clang::CompilerInstance &ci) {}
  virtual void Initialize(clang::ASTContext &Ctx) {}
  virtual void HandleTranslationUnit(clang::ASTContext &Ctx) {
    IndexASTVisitor v;
    Index->CrossRef.SetSourceManager(&Ctx.getSourceManager());
    v.TraverseDecl(Ctx.getTranslationUnitDecl());
  }
};

class IndexASTAction : public clang::ASTFrontendAction {
public:
  virtual clang::ASTConsumer *CreateASTConsumer(clang::CompilerInstance &CI,
                                                llvm::StringRef InFile) {
    std::string FileName = InFile.str();
    if (Index->CrossRef.SeenFile(FileName))
      return 0;
    std::string CmdLine; // The constructed commandline from the compilationdb
    for (auto C : Index->CompilationDb->getCompileCommands(FileName))
      for (auto S : C.CommandLine)
        CmdLine += S + " ";
    Index->CrossRef.StartNewFile(FileName, CmdLine);
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

bool Indexer::Run() {
  if (!OpenCompilationDatabase()) return false;

  ClangTool Tool(*CompilationDb, Files);
  bool result = Tool.run(newFrontendActionFactory<IndexASTAction>());
  return result;
}
