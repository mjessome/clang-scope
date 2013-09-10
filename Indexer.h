#pragma once
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"

#include "CrossReference.h"

#include <vector>
#include <string>

class Indexer {
  friend class IndexASTAction;
  friend struct IndexASTVisitor;

private:
  std::string CompilationDbPath;
  clang::tooling::CompilationDatabase *CompilationDb;
  std::vector<std::string> Files;
  bool OpenCompilationDatabase();

  CrossReference &CrossRef;

public:
  Indexer(std::string Path, CrossReference &CrossRef);
  ~Indexer();

  bool Index();
};
