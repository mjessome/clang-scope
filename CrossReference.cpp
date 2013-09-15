#include "clang/Index/USRGeneration.h"
#include "clang/AST/ASTContext.h"
#include "llvm/ADT/SmallVector.h"

#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/Expr.h"

#include <iostream>
#include <sstream>
#include "clang/Frontend/FrontendActions.h"

#include "ClangScope.h"
#include "CrossReference.h"

extern llvm::cl::opt<unsigned> LogLevel;

unsigned CrossReference::AddFile(std::string FileName, std::string CmdLine) {
  return (FileIndex[FileName] = InsertFile(FileName, CmdLine));
}

void CrossReference::StartNewFile(std::string FileName, std::string CmdLine) {
  LOG(1, "New File: " << FileName << std::endl);
  if ((TUCount % 5) == 0) { // Transaction every 5th file
    if (CurrentFile) {
      EndTransaction();
      LOG(2, "End SQL Transaction.\n");
    }
    LOG(2, "Begin SQL Transaction.\n");
    BeginTransaction();
  }
  ++TUCount;
  CurrentFile = AddFile(FileName, CmdLine);
}

bool CrossReference::AddReference(clang::NamedDecl *D, ReferenceType RefType,
                                  IdentifierType IdType) {
  return AddReference(D, 0, RefType, IdType);
}

bool CrossReference::AddReference(clang::NamedDecl *D, clang::DeclRefExpr *E,
                                  ReferenceType RefType, IdentifierType IdType) {
  if (!D->getDeclName().isIdentifier() || !D->getLocation().isValid() ||
      D->getName().empty()) {
    // TODO: Verbose debug output.
    return false;
  }

  clang::SourceLocation Loc = D->getLocation();

  // Don't record standard library usage
  if (SrcMgr->isInSystemHeader(Loc) || SrcMgr->isInExternCSystemHeader(Loc))
    return false;

  std::string QName = D->getQualifiedNameAsString();

  // Generate USR
  std::string USR;
  {
    llvm::SmallVector<char, 32> Buf;
    if (clang::index::generateUSRForDecl(D, Buf))
      // TODO: Verbose debug output.
      return false;
    for (auto c : Buf)
      USR += c;
  }

  // If this is a usage, want to use that location rather than the decl.
  if (E) Loc = E->getLocation();

  // Can't use current file's name as this may be in a header.
  std::string FileName;
  unsigned Line, Col;
  {
    std::string LocStr(Loc.printToString(*SrcMgr));
    std::stringstream SS(LocStr);
    std::string FileName;
    std::getline(SS, FileName, ':');

    SS >> Line;
    SS.ignore(std::numeric_limits<std::streamsize>::max(), ':');
    SS >> Col;
  }

  // Check if we already have this declaration in the declarations table.
  unsigned Declaration = 0;
  if (!DeclarationIndex.count(USR))
    DeclarationIndex[USR] = InsertDeclaration(QName, USR, IdType);
  else
    Declaration = DeclarationIndex[USR];
  unsigned File = GetFileIndex(FileName);

  // TODO: Get spelling of this reference.
  std::string Text;

  return InsertReference(Declaration, RefType, File, Line, Col, Text);
}

static const std::string SQLCreateDB(
    "CREATE TABLE IF NOT EXISTS files( "
    " id INTEGER, "
    " name TEXT, "
    " cmd_line TEXT, " // Command used to build this file,
                       // from the compilationdb.
    " PRIMARY KEY (id) );"
    // Declarations holds a single top-level declaration for each qualified name
    "CREATE TABLE IF NOT EXISTS declarations( "
    " id INTEGER, "
    " name TEXT, "    // Qualified name.
    " usr TEXT,"      // This declaration's USR.
    " type INTEGER, " // IdentifierType.
    " PRIMARY KEY (id) );"
    // References holds all references that refer to a declaration.
    // In the case of a use, file:line:col corresponds to the usage location.
    "CREATE TABLE IF NOT EXISTS _references( "
    " id INTEGER, "
    " declaration INTEGER, " // References declarations(id).
    " reftype INTEGER, "     // Type of reference.
                             // 0 - Declaration
                             // 1 - Definition
                             // 2 - Override
                             // 3 - Use
    " file INTEGER, "        // References files(id).
    " line INTEGER, "
    " col INTEGER, "
    " txt TEXT, "
    " PRIMARY KEY (id), "
    " UNIQUE (declaration,file,line,col) " // No duplicated references.
    "   ON CONFLICT IGNORE );");

bool CrossReference::PrepareStatement(std::string Stmt, sqlite3_stmt **ppstmt) {
  const char *end = 0;
  int rc = sqlite3_prepare(Db, Stmt.c_str(), Stmt.size(), ppstmt, &end);
  if (rc != SQLITE_OK) {
    std::cerr << "ERROR ON PREPARE: " << rc << std::endl;
    std::cerr << Stmt << std::endl;
  }
  return (rc == SQLITE_OK);
}

bool CrossReference::PrepareInsertStatements() {
  int rc = 0;
  rc |= PrepareStatement("INSERT INTO declarations VALUES (NULL, ?, ?, ?);",
                         &InsDecl);
  rc |= PrepareStatement(
      "INSERT INTO _references VALUES (NULL, ?, ?, ?, ?, ?, ?);", &InsRef);
  rc |= PrepareStatement("INSERT INTO files VALUES (NULL, ?, ?);", &InsFile);
  return rc;
}

bool CrossReference::PrepareQueryStatements() {
  int rc = 0;
  // A query by ReferenceType / name
  rc |= PrepareStatement("SELECT name,line,col,txt FROM files "
                         "JOIN ("
                         "  SELECT file,line,col,txt FROM _references "
                         "    WHERE ( reftype=? AND declaration IN ("
                         "      SELECT id "
                         "      FROM declarations WHERE ( name=? "
                         "      AND type=? ) LIMIT 1 ) ) ) "
                         "ON file = id;",
                         &GetByRefType);
  return rc;
}

bool CrossReference::OpenDatabase(bool create) {
  int flags = SQLITE_OPEN_READWRITE;
  if (create)
    flags |= SQLITE_OPEN_CREATE;
  int rc = sqlite3_open_v2(DbFileName.c_str(), &Db, flags, NULL);
  if (rc != SQLITE_OK) {
    std::cerr << sqlite3_errmsg(Db);
    sqlite3_close(Db);
    return false;
  }

  CreateTables();

  // TODO: These should be on a config switch
  PrepareInsertStatements();
  PrepareQueryStatements();

  Exec("PRAGMA temp_store = 2;"        // Store temporary data in memory
       "PRAGMA journal_mode = MEMORY;" // Journal in memory
       "PRAGMA synchronous = OFF;" // Don't wait for disk to sync on file write
       );
  return true;
}

void CrossReference::CloseDatabase() {
  EndTransaction();
  sqlite3_close(Db);
  Db = NULL;
}

bool CrossReference::Exec(std::string ExecStr) {
  char *err = 0;
  int rc = sqlite3_exec(Db, ExecStr.c_str(), 0, 0, &err);
  if (rc != SQLITE_OK) {
    std::cerr << err;
    sqlite3_free(err);
  }
  return rc == SQLITE_OK;
}

unsigned CrossReference::StepAndReset(sqlite3_stmt *ppstmt) {
  int rc = sqlite3_step(ppstmt);
  if (rc != SQLITE_DONE) {
    std::cerr << "ERROR on STEP: " << rc << std::endl;
    return 0;
  }
  sqlite3_reset(ppstmt);
  return sqlite3_last_insert_rowid(Db);
}

bool CrossReference::CreateTables() { return Exec(SQLCreateDB); }

unsigned
CrossReference::InsertDeclaration(std::string Name, std::string USR,
                                  IdentifierType IdType) {
  int rc = 0;
  rc |= sqlite3_bind_text(InsDecl, 1, Name.c_str(), Name.length(), 0);
  rc |= sqlite3_bind_text(InsDecl, 2, USR.c_str(), USR.length(), 0);
  rc |= sqlite3_bind_int(InsDecl, 3, IdType);
  if (rc != SQLITE_OK) return 0;
  return (rc != SQLITE_OK) ? 0 : StepAndReset(InsDecl);
}

unsigned CrossReference::InsertReference(unsigned Decl, ReferenceType RefType,
                                         unsigned File, unsigned Line,
                                         unsigned Col, std::string Text) {
  int rc = 0;
  rc |= sqlite3_bind_int(InsRef, 1, Decl);
  rc |= sqlite3_bind_int(InsRef, 2, RefType);
  rc |= sqlite3_bind_int(InsRef, 3, File);
  rc |= sqlite3_bind_int(InsRef, 4, Line);
  rc |= sqlite3_bind_int(InsRef, 5, Col);
  rc |= sqlite3_bind_text(InsRef, 6, Text.c_str(), Text.length(), 0);
  return (rc != SQLITE_OK) ? 0 : StepAndReset(InsRef);
}

unsigned CrossReference::InsertFile(std::string File, std::string CmdLine) {
  int rc = 0;
  rc |= sqlite3_bind_text(InsFile, 1, File.c_str(), File.length(), 0);
  rc |= sqlite3_bind_text(InsFile, 2, CmdLine.c_str(), CmdLine.length(), 0);
  return (rc != SQLITE_OK) ? 0 : StepAndReset(InsFile);
}

void CrossReference::ListReferences(std::string QualifiedName,
                                    ReferenceType RefType,
                                    IdentifierType IdType) {
  int rc = 0;
  rc |= sqlite3_bind_int(GetByRefType, 1, RefType);
  rc |= sqlite3_bind_text(GetByRefType, 2, QualifiedName.c_str(),
                          QualifiedName.length(), 0);
  rc |= sqlite3_bind_int(GetByRefType, 3, IdType);
  for (;;) {
    rc = sqlite3_step(GetByRefType);
    if (rc != SQLITE_ROW) break;
    const unsigned char *File = sqlite3_column_text(GetByRefType, 0);
    unsigned Line = sqlite3_column_int(GetByRefType, 1);
    unsigned Col = sqlite3_column_int(GetByRefType, 2);
    const unsigned char *Text = sqlite3_column_text(GetByRefType, 3);
    std::cout << File << ":" << Line << ":" << Col << " " << Text << std::endl;
  }
  sqlite3_reset(GetByRefType);
}
