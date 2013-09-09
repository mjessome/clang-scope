#pragma once

#include <sqlite3.h>
#include <string>

#include "ClangScope.h"

class CrossReference {
private:
  std::string FileName;
  sqlite3 *Db;
  bool Transaction; // True if the database has an open transaction

  bool Exec(std::string ExecStr);
  unsigned StepAndReset(sqlite3_stmt *ppstmt);
  bool CreateTables();

  // Prepared sqlite queries
  sqlite3_stmt *InsDecl;
  sqlite3_stmt *InsRef;
  sqlite3_stmt *InsFile;
  sqlite3_stmt *GetByRefType;
  bool PrepareStatement(std::string Stmt, sqlite3_stmt **ppstmt);
  bool PrepareInsertStatements();
  bool PrepareQueryStatements();

public:
  CrossReference() : Transaction(true) {}
  ~CrossReference() {}

  bool OpenDb(std::string FileName, bool create = true);
  bool IsOpen() { return Db; }
  void CloseDb();

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
};
