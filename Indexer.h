#pragma once
#include "clang/Tooling/CompilationDatabase.h"

class Indexer {
private:
  std::string CompilationDbPath;
  clang::tooling::CompilationDatabase *CompilationDb;

  bool OpenCompilationDatabase();

public:
  Indexer(std::string Path);
  ~Indexer();

  bool Index();
};
