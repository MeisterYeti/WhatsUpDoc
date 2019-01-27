#include <iostream>
#include <clang-c/Documentation.h>
#include <vector>

#include "Helper.h"
using namespace WhatsUpDoc;

std::vector<std::string> extractComments(CXTranslationUnit translationUnit, CXCursor currentCursor) {  
  std::vector<std::string> comments;
  
  CXSourceRange srcRange = clang_getCursorExtent(currentCursor);  
  CXToken *tokens;
  uint32_t nbTokens;
  clang_tokenize(translationUnit, srcRange, &tokens, &nbTokens);
  
  for (uint32_t i = 0; i < nbTokens; ++i) {
    CXTokenKind kind = clang_getTokenKind(tokens[i]);
    if(kind == CXToken_Comment) {
      CXString tokenString = clang_getTokenSpelling(translationUnit, tokens[i]);            
      CXSourceLocation location = clang_getTokenLocation(translationUnit, tokens[i]);
      CXString filename;
      unsigned int line, column;      
      clang_getPresumedLocation(location, &filename, &line, &column);

      comments.emplace_back(toString(tokenString) + " -> " + std::to_string(line));
    }
  }
  
  clang_disposeTokens(translationUnit,tokens,nbTokens);
  return comments;
}

void printTokens(CXCursor cursor) {
  CXSourceRange range = clang_getCursorExtent(cursor);
  CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
  CXToken *tokens = 0;
  unsigned int nTokens = 0;
  clang_tokenize(tu, range, &tokens, &nTokens);
  for (unsigned int i = 0; i < nTokens; i++) {
      CXString spelling = clang_getTokenSpelling(tu, tokens[i]);
      std::cout << "      token " << spelling << " -> " << getTokenKindName(clang_getTokenKind(tokens[i])) << std::endl;
  }
  clang_disposeTokens(tu, tokens, nTokens);
}

CXChildVisitResult findDeclRef(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  CXCursorKind kind = clang_getCursorKind(cursor);
  auto name = toString(clang_getCursorSpelling(cursor));
  if(kind == CXCursor_DeclRefExpr) {
    //std::cout << "      c " << name << " -> " << getCursorKindName(kind) << std::endl;
    *reinterpret_cast<CXCursor*>(client_data) = cursor;
    return CXChildVisit_Break;
  }    
  return CXChildVisit_Recurse;
}


CXChildVisitResult testVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  CXCursorKind kind = clang_getCursorKind(cursor);
  auto name = toString(clang_getCursorSpelling(cursor));
  int depth = *reinterpret_cast<int*>(client_data);
  for(int i=0; i<depth; ++i)
    std::cout << "  ";
  std::cout << "      c " << name << " -> " << getCursorKindName(kind) << std::endl;
  depth += 1;
  clang_visitChildren(cursor, *testVisitor, &depth);
    
  return CXChildVisit_Continue;
}

CXChildVisitResult initVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  CXCursorKind kind = clang_getCursorKind(cursor);
  auto name = toString(clang_getCursorSpelling(cursor));
  
  if(kind == CXCursor_CallExpr && (
      name == "declareFunction" ||
      name == "declareConstant" ||
      name == "init")) {
    std::cout << "  call " << name << " -> " << getCursorKindName(kind) << std::endl;
    CXCursor ref = clang_getCursorReferenced(cursor);
    std::cout << "    ref " << clang_getCursorUSR(ref) << std::endl;
    
    int argc = clang_Cursor_getNumArguments(cursor);
    for(int i=0; i<argc; ++i) {
      CXCursor arg = clang_Cursor_getArgument(cursor, i);
      CXString argname = clang_getCursorSpelling(arg);
      CXCursorKind akind = clang_getCursorKind(arg);
      std::cout << "    arg " << argname << " -> " << getCursorKindName(akind) << std::endl;
      
      if(isLiteral(akind)) {
        printTokens(arg);
      } else {
        CXCursor refCursor = clang_getNullCursor();
        clang_visitChildren(arg, *findDeclRef, &refCursor);
        if(!clang_Cursor_isNull(refCursor)) {
          std::cout << "      ref " << clang_getCursorSpelling(refCursor) << " -> " << getCursorKindName(clang_getCursorKind(refCursor)) << std::endl;
        } else {
          printTokens(arg);
        }
      }
      
      
    }
  } else if(kind == CXCursor_FunctionDecl) {
    std::cout << "  decl " << name << " -> " << getCursorKindName(kind) << std::endl;
    return CXChildVisit_Continue;
  }
  //printCursorTokens(clang_Cursor_getTranslationUnit(cursor),cursor);
  //printf("  fncursor '%s' -> %i\n",clang_getCString(name),kind);  
    
  return CXChildVisit_Recurse;
}

CXChildVisitResult functionDeclVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  CXCursorKind kind = clang_getCursorKind(cursor);
  CXType type = clang_getCursorType(cursor);
  
  CXString name = clang_getCursorSpelling(cursor);
  if (kind == CXCursor_ParmDecl) {
    std::cout << "  parameter: " << name << " -> " << type.kind << std::endl;
  } else {
    std::cout << "  name " << name << " -> " << getCursorKindName(kind) << std::endl;
  }

  clang_visitChildren(cursor, *initVisitor, nullptr);
  //printCursorTokens(clang_Cursor_getTranslationUnit(cursor),cursor);
  auto comments = extractComments(clang_Cursor_getTranslationUnit(cursor), cursor);
  for(auto c : comments)
    std::cout << "  comment " << c << std::endl;

  return CXChildVisit_Continue;
}

CXChildVisitResult cursorVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  
  CXCursorKind kind = clang_getCursorKind(cursor);
  std::string name = toString(clang_getCursorSpelling(cursor));
  
  if(kind == CXCursor_MacroExpansion) {  
    if(name.compare(0, 6, "ES_FUN") == 0 || name.compare(0, 7, "ES_MFUN") == 0 || name.compare(0, 7, "ES_CTOR") == 0) {
      //printCursorTokens(clang_Cursor_getTranslationUnit(cursor),cursor);
              
      CXSourceLocation location = clang_getCursorLocation(cursor);
      CXString filename;
      unsigned int line, column;
      clang_getPresumedLocation(location, &filename, &line, &column);
      std::cout << "macro " << name << " -> " << getCursorKindName(kind) << " @ " << line << std::endl;
    }
  } else if (kind == CXCursor_FunctionDecl) {
    bool isDef = clang_isCursorDefinition(cursor);
    if(name == "init" && isDef) {
      std::cout << "method " << name << " -> " << getCursorKindName(kind) << std::endl;
      std::cout << "  usr " << clang_getCursorUSR(cursor) << std::endl;
      
      CXString com = clang_Cursor_getRawCommentText(cursor);
      if(clang_getCString(com))
        std::cout << "  comment " << com << std::endl;
              
      CXSourceLocation location = clang_getCursorLocation(cursor);
      CXString filename;
      unsigned int line, column;      
      clang_getPresumedLocation(location, &filename, &line, &column);
      std::cout << "  source location " << filename << ", (" << line << "," << column << ")" << std::endl;
      
      // visit method childs
      clang_visitChildren(cursor, *functionDeclVisitor, 0);
    }
    return CXChildVisit_Continue;
  }
  //std::cout << "cursor " << name << " -> " << getCursorKindName(kind) << std::endl;
  return CXChildVisit_Recurse;
}

int main (int argc, const char * argv[]) {
  CXIndex index = clang_createIndex(0, 0);
  
  if(index == 0) {
    std::cerr << "error creating index\n";
    return 1;
  }
 
  const char* args[] = { "-x", "c++", "-Wdocumentation", "-fparse-all-comments", "-Itest", "-Itest/EScript" }; 
  CXTranslationUnit translationUnit = clang_parseTranslationUnit(
   index,
   "test/E_Util/ELibUtil.cpp",
   //"test/test.h",
   args, 6,
   nullptr, 0,
   CXTranslationUnit_DetailedPreprocessingRecord);
  //CXTranslationUnit translationUnit = clang_parseTranslationUnit(index, 0, argv, argc, 0, 0, CXTranslationUnit_None);
  
  if(translationUnit == 0) {
    std::cerr << "error creating translationUnit\n";
    return 1;
  }
  
  CXCursor rootCursor = clang_getTranslationUnitCursor(translationUnit);
  
  //printCursorTokens(translationUnit,rootCursor);
  
  clang_visitChildren(rootCursor, *cursorVisitor,0);
    
  clang_disposeTranslationUnit(translationUnit);
  clang_disposeIndex(index);
  return 0;
}