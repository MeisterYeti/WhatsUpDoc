#include <iostream>
#include <clang-c/Documentation.h>
#include <vector>

#include "Parser.h"
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
  std::cout << "c " << name << " -> " << getCursorKindName(kind) << std::endl;
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
          CXCursor ref = clang_getCursorReferenced(refCursor);
          CXType type = clang_getCursorType(ref);
          if(type.kind == CXType_Pointer)
            type = clang_getPointeeType(type);
          CXCursor decl = clang_getTypeDeclaration(type);
          std::cout << "      ref " << clang_getCursorSpelling(ref) << " -> " << getCursorKindName(clang_getCursorKind(ref)) << std::endl;
          std::cout << "      type " << clang_getTypeSpelling(type) << " -> " << clang_getTypeKindSpelling(type.kind) << std::endl;
          std::cout << "      decl " << clang_getCursorSpelling(decl) << " -> " << getCursorKindName(clang_getCursorKind(decl)) << std::endl;
          std::cout << "      usr " << clang_getCursorUSR(ref) << std::endl;
          if(clang_getCursorKind(ref) == CXCursor_VarDecl) {
            //printTokens(ref);
          }
        } else {
          printTokens(arg);
        }
      }
    }
  } else if(kind == CXCursor_FunctionDecl) {
    return CXChildVisit_Continue;
  } else if(kind == CXCursor_ReturnStmt) {
    std::cout << "  return " << name << " -> " << getCursorKindName(kind) << std::endl;
    printTokens(cursor);
    int depth = 2;
    clang_visitChildren(cursor, *testVisitor, &depth);
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
    std::cout << "  parameter: " << name << " -> " << clang_getTypeKindSpelling(type.kind) << std::endl;
    std::cout << "    usr " << clang_getCursorUSR(cursor) << std::endl;
  } else {
    //std::cout << "  cursor " << name << " -> " << getCursorKindName(kind) << std::endl;
  }
  auto comments = extractComments(clang_Cursor_getTranslationUnit(cursor), cursor);
  for(auto c : comments)
    std::cout << "  comment " << c << std::endl;
    
  clang_visitChildren(cursor, *initVisitor, nullptr);
  //printCursorTokens(clang_Cursor_getTranslationUnit(cursor),cursor);

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
  } else if (kind == CXCursor_FunctionDecl ||kind == CXCursor_CXXMethod) {
    bool isDef = clang_isCursorDefinition(cursor);
    if((name == "init" || name == "getClassName") && isDef) {
      std::cout << "method " << name << " -> " << getCursorKindName(kind) << std::endl;
      std::cout << "  usr " << clang_getCursorUSR(cursor) << std::endl;
              
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
  return CXChildVisit_Recurse;
}

int main(int argc, const char * argv[]) {
  Parser parser;
  
  parser.addInclude("test");
  parser.addInclude("test/EScript");
  parser.addInclude("test/E_Util");
  
  parser.parseFile("test/E_Util/Graphics/E_Bitmap.cpp");
  parser.parseFile("test/E_Util/ELibUtil.cpp");
  return 0;
}