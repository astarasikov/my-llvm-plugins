/***   CIrewriter.cpp   ******************************************************
 * This code is licensed under the New BSD license.
 * See LICENSE.txt for details.
 *
 * This rewriter is made by Alexander Tarasikov <alexander.tarasikov@gmail.com>
 * based on a tutorial by Robert Ankeney <rrankene@gmail.com>
 *
 * This rewriter looks for uninitialized local variables
 * and adds the corresponding initializers:
 * zero (=0) for primitive types
 * and curly braces (={}) for record types
 *
 * Usage:
 * CIrewriter <options> <file>.c
 * where <options> allow for parameters to be passed to the preprocessor
 * such as -DFOO to define FOO.
 *
 * Generated as output <file>_out.c
 *****************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <vector>
#include <set>
#include <system_error>
#include <iostream>

#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"

#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/Lexer.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"

/*
 * TODO: stop traversing current TranslationUnit if a rewritten file exists
 * TODO: fix isInSystemHeader check
 * TODO: get rid of explicit HeaderSearchOptions population
 * TODO: convert to libtooling plugin
 */

enum {
    W_DEBUG = 0,
};

#if 0
#define DBG(stmt) stmt
#else
#define DBG(stmt) do {} while (0)
#endif

using namespace clang;

class MyRecursiveASTVisitor
    : public RecursiveASTVisitor<MyRecursiveASTVisitor> {

public:
  MyRecursiveASTVisitor(Rewriter &R, ASTContext &astContext)
      : Rewrite(R), mContext(astContext) {}
  bool VisitDecl(Decl *d);
  void MyVisitVarDecl(VarDecl *d);

  Rewriter &Rewrite;

  ASTContext &mContext;
  std::set<FileID> mRewrittenFileIDs;

  void markFileForCurrentDecl(Decl *d);
};

void MyRecursiveASTVisitor::markFileForCurrentDecl(Decl *d)
{
    clang::SourceLocation sourceLocation = d->getLocation();
    clang::SourceManager &sourceManager = Rewrite.getSourceMgr();
    clang::FileID fileID = sourceManager.getFileID(sourceLocation);

    if (sourceManager.isInSystemHeader(sourceLocation)) {
        return;
    }
    if (sourceManager.isInExternCSystemHeader(sourceLocation)) {
        return;
    }
    mRewrittenFileIDs.insert(fileID);
}

void MyRecursiveASTVisitor::MyVisitVarDecl(VarDecl *d) {
  enum clang::StorageClass storageClass = d->getStorageClass();
  if (storageClass == clang::StorageClass::SC_Static) {
    DBG(std::cerr << "static decl, skipping\n");
    return;
  }
  if (isa<ParmVarDecl>(d)) {
      DBG(std::cerr << "ParmVarDecl, skipping\n");
      return;
  }

  /*
  if (!d->isLocalVarDecl()) {
    if (W_DEBUG) {
        std::cerr << "not a local vardecl, skipping\n";
    }
    return;
  }
  */

  if (d->hasExternalStorage()) {
      DBG(std::cerr << "Extrnal storage, skipping\n");
      return;
  }
  if (d->hasInit()) {
    DBG(std::cerr << "has an initializer, skipping\n");
    return;
  }

  DBG(std::cerr << "\n\n");
  DBG(d->dumpColor());

  auto typeSourceInfo = d->getTypeSourceInfo();
  clang::QualType tsiQualType = typeSourceInfo->getType();
  if (tsiQualType->isRecordType() || tsiQualType->isArrayType()) {
    DBG(std::cerr << "Record\n");
    Rewrite.InsertTextAfterToken(d->getLocEnd(), " = {}");
  } else {
    DBG(std::cerr << "Non Record\n");
    Rewrite.InsertTextAfterToken(d->getLocEnd(), " = 0");
  }
  markFileForCurrentDecl(d);

  return;
}

bool MyRecursiveASTVisitor::VisitDecl(Decl *d) {
  // d->dump();
  if (isa<VarDecl>(d)) {
    MyVisitVarDecl(reinterpret_cast<VarDecl *>(d));
  }
  return true;
}

class MyASTConsumer : public ASTConsumer {
public:
  MyASTConsumer(Rewriter &Rewrite, ASTContext &astContext)
      : rv(Rewrite, astContext) {}
  virtual bool HandleTopLevelDecl(DeclGroupRef d);

  MyRecursiveASTVisitor rv;
};

bool MyASTConsumer::HandleTopLevelDecl(DeclGroupRef d) {
  typedef DeclGroupRef::iterator iter;

  for (iter b = d.begin(), e = d.end(); b != e; ++b) {
    rv.TraverseDecl(*b);
  }

  return true; // keep going
}

// XXX: can we make clang do it for us or convert ourselves into a plugin?
static void
AddPlatformSpecificHeaders(HeaderSearchOptions &headerSearchOptions) {
  // <Warning!!> -- Platform Specific Code lives here
  // This depends on A) that you're running linux and
  // B) that you have the same GCC LIBs installed that
  // I do.
  // Search through Clang itself for something like this,
  // go on, you won't find it. The reason why is Clang
  // has its own versions of std* which are installed under
  // /usr/local/lib/clang/<version>/include/
  // See somewhere around Driver.cpp:77 to see Clang adding
  // its version of the headers to its include path.
  // To see what include paths need to be here, try
  // clang -v -c test.c
  // or clang++ for C++ paths as used below:
  headerSearchOptions.AddPath("/usr/include/c++/4.9", clang::frontend::Angled,
                              false, false);
  headerSearchOptions.AddPath("/usr/include/i386-linux-gnu/c++/4.9/",
                              clang::frontend::Angled, false, false);
  headerSearchOptions.AddPath("/usr/include/c++/4.9/backward",
                              clang::frontend::Angled, false, false);
  headerSearchOptions.AddPath("/usr/include", clang::frontend::Angled, false,
                              false);
  headerSearchOptions.AddPath("/usr/lib/llvm-3.5/lib/clang/3.5.0/include",
                              clang::frontend::Angled, false, false);
  headerSearchOptions.AddPath("/usr/include/i386-linux-gnu",
                              clang::frontend::Angled, false, false);
  headerSearchOptions.AddPath("/usr/include", clang::frontend::Angled, false,
                              false);
  // </Warning!!> -- End of Platform Specific Code
}

int dumpRewrittenCodeForFile(FileID fileId, std::string &fileName, Rewriter &Rewrite)
{
  std::cerr << "Getting RewriteBuf for " << fileName << "\n";
  const RewriteBuffer *RewriteBuf = Rewrite.getRewriteBufferFor(fileId);

  if (!RewriteBuf) {
    std::cerr << "Failed to get RewriteBuffer, switching to EditBuffer\n";
    RewriteBuf = &Rewrite.getEditBuffer(fileId);
  }

  if (!RewriteBuf) {
    std::cerr << "Failed to get RewriteBuf, no changes\n";
    return -1;
  }

  // Convert <file>.c to <file_out>.c
  std::string outName(fileName);
  size_t ext = outName.rfind(".");
  if (ext == std::string::npos)
    ext = outName.length();
  outName.insert(ext, "_out");

  llvm::errs() << "Output to: " << outName << "\n";

#if 1 // llvm > 3.5
  std::error_code errCode;
  llvm::raw_fd_ostream outFile(outName.c_str(), errCode, llvm::sys::fs::F_None);

#else
  std::string SErrorInfo;
  llvm::raw_fd_ostream outFile(outName.c_str(), SErrorInfo,
                               llvm::sys::fs::F_None);

  std::cerr << SErrorInfo;

  if (outFile.has_error()) {
    llvm::errs() << "Cannot open " << outName << " for writing\n";
    return -1;
  }
#endif

  outFile << std::string(RewriteBuf->begin(), RewriteBuf->end());
  outFile.close();
  return 0;
}

//XXX: this is a hack
bool shouldIgnorePath(std::string &path)
{
    if (0 == path.find("/usr")) {
        return true;
    }

    if (0 == path.find("/opt")) {
        return true;
    }

    return false;
}

int main(int argc, char **argv) {
  struct stat sb;

  if (argc < 2) {
    llvm::errs() << "Usage: CIrewriter <options> <filename>\n";
    return 1;
  }

  // Get filename
  std::string fileName(argv[argc - 1]);

  // Make sure it exists
  if (stat(fileName.c_str(), &sb) == -1) {
    perror(fileName.c_str());
    exit(EXIT_FAILURE);
  }

  CompilerInstance compiler;
  DiagnosticOptions diagnosticOptions;
  compiler.createDiagnostics();
  // compiler.createDiagnostics(argc, argv);

  // Create an invocation that passes any flags to preprocessor
  CompilerInvocation *Invocation = new CompilerInvocation;
  CompilerInvocation::CreateFromArgs(*Invocation, argv + 1, argv + argc,
                                     compiler.getDiagnostics());
  compiler.setInvocation(Invocation);

  // Set default target triple
  std::shared_ptr<clang::TargetOptions> pto =
      std::make_shared<clang::TargetOptions>();
  pto->Triple = llvm::sys::getDefaultTargetTriple();
  TargetInfo *pti =
      TargetInfo::CreateTargetInfo(compiler.getDiagnostics(), pto);
  compiler.setTarget(pti);

  compiler.createFileManager();
  compiler.createSourceManager(compiler.getFileManager());

  HeaderSearchOptions &headerSearchOptions = compiler.getHeaderSearchOpts();
  AddPlatformSpecificHeaders(headerSearchOptions);

  // Allow C++ code to get rewritten
  LangOptions langOpts;
  langOpts.GNUMode = 1;
  langOpts.CXXExceptions = 0;
  langOpts.RTTI = 0;
  langOpts.Bool = 0;
  langOpts.CPlusPlus = 0;
  Invocation->setLangDefaults(langOpts, clang::IK_C,
                              clang::LangStandard::lang_c99);

  compiler.createPreprocessor(clang::TU_Complete);
  compiler.getPreprocessorOpts().UsePredefines = false;

  compiler.createASTContext();

  // Initialize rewriter
  Rewriter Rewrite;
  clang::SourceManager &sourceManager = compiler.getSourceManager();
  Rewrite.setSourceMgr(sourceManager, compiler.getLangOpts());

  const FileEntry *pFile = compiler.getFileManager().getFile(fileName);
  sourceManager.setMainFileID(
      sourceManager.createFileID(pFile, clang::SourceLocation(),
                                               clang::SrcMgr::C_User));
  compiler.getDiagnosticClient().BeginSourceFile(compiler.getLangOpts(),
                                                 &compiler.getPreprocessor());

  ASTContext &astContext = compiler.getASTContext();
  MyASTConsumer astConsumer(Rewrite, astContext);

  // Parse the AST
  ParseAST(compiler.getPreprocessor(), &astConsumer, astContext);
  compiler.getDiagnosticClient().EndSourceFile();

  // Now output rewritten source code

  //FileID fileId = sourceManager.getMainFileID();
  //dumpRewrittenCodeForFile(fileId, fileName, Rewrite);

  for (FileID currFileID : astConsumer.rv.mRewrittenFileIDs)
  {
      const clang::FileEntry *fileEntry = sourceManager.getFileEntryForID(currFileID);
      if (!fileEntry) {
          std::cerr << "OOPS, failed to find the file entry for fileID\n";
          continue;
      }

      const char *fname = fileEntry->getName();
      std::string sFname(fname);
      if (shouldIgnorePath(sFname)) {
          continue;
      }
      dumpRewrittenCodeForFile(currFileID, sFname, Rewrite);
  }

  return 0;
}

void __cxa_guard_acquire(){}
