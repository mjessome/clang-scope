#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Index/USRGeneration.h"
#include "clang/Frontend/CompilerInstance.h"

#include <iostream>
#include <vector>
#include <time.h>

#include "Indexer.h"

using namespace clang::tooling;

Indexer::Indexer(std::string Path) : CompilationDbPath(Path) {
}

Indexer::~Indexer() {
  delete CompilationDb;
}

bool Indexer::OpenCompilationDatabase() {
  std::string Err;
  CompilationDb = CompilationDatabase::loadFromDirectory(CompilationDbPath, Err);
  if (!CompilationDb) std::cerr << Err;
  return CompilationDb;
}

bool Indexer::Index() {
  return true;
}
