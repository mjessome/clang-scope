#pragma once
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"

#include <sqlite3.h>
#include <string>
#include <unordered_map>

#include "ClangScope.h"

class CrossReference {
public:
  CrossReference(std::string DbFileName)
      : TUCount(0), CurrentFile(0), DbFileName(DbFileName), Transaction(false) {
    if (!OpenDatabase())
      abort();
  }
  ~CrossReference() {
    if (DatabaseIsOpen())
      CloseDatabase();
  }

private:
  clang::SourceManager *SrcMgr; // Current SourceManager

public:
  void SetSourceManager(clang::SourceManager *SM) { SrcMgr = SM; }

private:
  unsigned TUCount;     // How many translation units have been processed.
  unsigned CurrentFile; // Index of the currently processing file.
  // Map filenames to their index in the files table.
  std::unordered_map<std::string, unsigned> FileIndex;

  // Adds the file to the database, and returns its index.
  unsigned AddFile(std::string FileName, std::string CmdLine = "");

  unsigned GetFileIndex(std::string Name) {
    auto II = FileIndex.find(Name);
    if (II == FileIndex.end())
      return AddFile(Name);
    return II->second;
  }

public:
  // Adds the given file to the FileIndex, handles transactions, and sets
  // CurrentFile.
  void StartNewFile(std::string FileName, std::string CmdLine);

  // Returns true if this file has been processed already.
  bool SeenFile(std::string FileName) {
    return (FileIndex.find(FileName) != FileIndex.end());
  }

private:
  // Map USRs to their index in the declarations table.
  std::unordered_map<std::string, unsigned> DeclarationIndex;

public:
  bool AddReference(clang::NamedDecl *d, ReferenceType RefType,
                    IdentifierType IdType);

  bool AddReference(clang::NamedDecl *d, clang::DeclRefExpr *e,
                    ReferenceType RefType, IdentifierType IdType);

private:
  std::string DbFileName;
  sqlite3 *Db;
  bool Transaction; // True if the database has an open transaction.

  bool Exec(std::string ExecStr);
  unsigned StepAndReset(sqlite3_stmt *ppstmt);
  bool CreateTables();

  // Prepared sqlite queries.
  sqlite3_stmt *InsDecl;
  sqlite3_stmt *InsRef;
  sqlite3_stmt *InsFile;
  sqlite3_stmt *GetByRefType;
  bool PrepareStatement(std::string Stmt, sqlite3_stmt **ppstmt);
  bool PrepareInsertStatements();
  bool PrepareQueryStatements();

  bool OpenDatabase(bool create = true);
  bool DatabaseIsOpen() { return Db; }
  void CloseDatabase();

  bool BeginTransaction() {
    assert(!Transaction && "Transaction already started.");
    Transaction = true;
    return Exec("BEGIN TRANSACTION;");
  }
  bool EndTransaction() {
    if (!Transaction)
      return true;
    Transaction = false;
    return Exec("END TRANSACTION;");
  }

  unsigned InsertDeclaration(std::string Name, std::string USR,
                             IdentifierType IdType);

  unsigned InsertReference(unsigned Decl, ReferenceType RefType, unsigned File,
                           unsigned Line, unsigned Col, std::string Text);

  unsigned InsertFile(std::string File, std::string CmdLine);

  void ListReferences(std::string QualifiedName, ReferenceType RefType,
                      IdentifierType IdType);
};
