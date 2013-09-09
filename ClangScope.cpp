#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Frontend/CompilerInstance.h"

#include <iostream>

#include "Indexer.h"
#include "CrossReference.h"

llvm::cl::opt<std::string> Path("p", llvm::cl::desc("Project path"),
                                llvm::cl::Optional);
llvm::cl::opt<int> LogLevel("l", llvm::cl::desc("Log level (0-3)"),
                            llvm::cl::Optional);

int main(int argc, const char *argv[]) {
  llvm::cl::ParseCommandLineOptions(argc, argv);
  if (LogLevel < 0 || LogLevel > 3) {
    std::cerr << "Log level must be 0-3";
    return 1;
  }
  CrossReference CrossRef;
  Indexer Indexer(Path, CrossRef);
  Indexer.Index();
  return 0;
}
