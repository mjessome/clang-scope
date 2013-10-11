#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Frontend/CompilerInstance.h"

#include <iostream>

#include "Indexer.h"
#include "CrossReference.h"

llvm::cl::opt<std::string> Path("p", llvm::cl::desc("Project path"),
                                llvm::cl::Optional);
llvm::cl::opt<int> LogLevel("l", llvm::cl::desc("Log level (0-3)"),
                            llvm::cl::Optional);
llvm::cl::opt<std::string> DbFile("--db", llvm::cl::desc("Database file"),
                                          llvm::cl::Optional);
llvm::cl::opt<ReferenceType> QueryType(llvm::cl::desc("Set the query type:"),
  llvm::cl::values(
    clEnumValN(Declaration, "d", "Declaration"),
    clEnumValN(Definition,  "D", "Definition"),
    clEnumValN(Override,    "o", "Override"),
    clEnumValN(Use,         "u", "Use"),
    clEnumValEnd));
llvm::cl::opt<IdentifierType> IdType(llvm::cl::desc("Set the identifier type:"),
  llvm::cl::values(
    clEnumValN(Enum,      "e", "Enum"),
    clEnumValN(Function,  "f", "Function"),
    clEnumValN(Namespace, "ns", "Namespace"),
    clEnumValN(Type,      "t", "Type"),
    clEnumValN(Typedef,   "td", "Typedef"),
    clEnumValN(Variable,  "v", "Variable"),
    clEnumValEnd));
llvm::cl::opt<std::string> Query("q",
                                 llvm::cl::desc("<filename:line:col>"),
                                 llvm::cl::Optional);

// FIXME: This needs to be made externally available so that IndexASTAction can
// access what it needs.
Indexer *Index;

int main(int argc, const char *argv[]) {
  llvm::cl::ParseCommandLineOptions(argc, argv);
  if (LogLevel < 0 || LogLevel > 3) {
    std::cerr << "Log level must be 0-3";
    return 1;
  }

  if (!DbFile.length() > 0)
    DbFile = "clscope.db";

  CrossReference CrossRef(DbFile);

  if (Query.length() > 0) {
    if (!IdType.good()) return 1;
    CrossRef.ListReferences(Query, QueryType, IdType);
  } else {
    // Index
    Index = new Indexer(Path, CrossRef);
    Index->Run();
  }
  return 0;
}
