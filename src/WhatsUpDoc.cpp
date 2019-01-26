#include <iostream>
#include <clang-c/Documentation.h>
#include <vector>

#include "Helper.h"
using namespace WhatsUpDoc;

void printTokenInfo(CXTranslationUnit translationUnit,CXToken currentToken) {
  CXString tokenString = clang_getTokenSpelling(translationUnit, currentToken);
  CXTokenKind kind = clang_getTokenKind(currentToken);
  
  switch (kind) {
    case CXToken_Comment:
      printf("Token : %s \t| COMMENT\n", clang_getCString(tokenString));    
      break;
    case CXToken_Identifier:
      printf("Token : %s \t| IDENTIFIER\n", clang_getCString(tokenString));          
      break;
    case CXToken_Keyword:
      printf("Token : %s \t| KEYWORD\n", clang_getCString(tokenString));          
      break;
    case CXToken_Literal:
      printf("Token : %s \t| LITERAL\n", clang_getCString(tokenString));          
      break;
    case CXToken_Punctuation:
      printf("Token : %s \t| PUNCTUATION\n", clang_getCString(tokenString));          
      break;
    default:
      break;
  }  
}


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

void printCursorTokens(CXTranslationUnit translationUnit, CXCursor currentCursor) {
  CXToken *tokens;
  unsigned int nbTokens;
  CXSourceRange srcRange;
  
  srcRange = clang_getCursorExtent(currentCursor);
  
  clang_tokenize(translationUnit, srcRange, &tokens, &nbTokens);
  
  for (int i = 0; i < nbTokens; ++i) {
    CXToken currentToken = tokens[i];    
    printTokenInfo(translationUnit,currentToken);
  }
  
  clang_disposeTokens(translationUnit,tokens,nbTokens);
}

CXChildVisitResult initVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  CXCursorKind kind = clang_getCursorKind(cursor);
  CXString name = clang_getCursorSpelling(cursor);
    
  CXString com = clang_Cursor_getRawCommentText(cursor);
  if(clang_getCString(com))
    std::cout << "    comment " << com << std::endl;
    
  if(kind == CXCursor_CallExpr) {    
    std::cout << "  call " << name << " -> " << getCursorKindName(kind) << std::endl;
    CXCursor ref = clang_getCursorReferenced(cursor);
    std::cout << "  ref-usr " << clang_getCursorUSR(ref) << std::endl;
    
    int argc = clang_Cursor_getNumArguments(cursor);
    for(int i=0; i<argc; ++i) {
      CXCursor arg = clang_Cursor_getArgument(cursor, i);
      CXString argname = clang_getCursorSpelling(arg);
      CXCursorKind akind = clang_getCursorKind(arg);
      std::cout << "    arg " << argname << " -> " << akind << std::endl;
      
      CXSourceRange range = clang_getCursorExtent(arg);
      CXTranslationUnit tu = clang_Cursor_getTranslationUnit(arg);
      CXToken *tokens = 0;
      unsigned int nTokens = 0;
      clang_tokenize(tu, range, &tokens, &nTokens);
      for (unsigned int i = 0; i < nTokens; i++) {
          CXString spelling = clang_getTokenSpelling(tu, tokens[i]);
          std::cout << "      token " << spelling << std::endl;
      }
      clang_disposeTokens(tu, tokens, nTokens);
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
    
  CXString com = clang_Cursor_getRawCommentText(cursor);
  if(clang_getCString(com))
    std::cout << "comment " << com << std::endl;
  
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
    //if(name.compare(0, 6, "ES_FUN") == 0 || name.compare(0, 7, "ES_MFUN") == 0) {
      std::cout << "macro " << name << " -> " << getCursorKindName(kind) << std::endl;
      printCursorTokens(clang_Cursor_getTranslationUnit(cursor),cursor);
              
      CXSourceLocation location = clang_getCursorLocation(cursor);
      CXString filename;
      unsigned int line, column;      
      clang_getPresumedLocation(location, &filename, &line, &column);
      std::cout << "  source location " << filename << ", (" << line << "," << column << ")" << std::endl;
    //}
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
  CXIndex index = clang_createIndex(1, 1);
  
  if(index == 0) {
    std::cerr << "error creating index\n";
    return 1;
  }
 
  const char* args[] = { "-x", "c++", "-Wdocumentation", "-fparse-all-comments", "-Itest", "-Itest/EScript" }; 
  CXTranslationUnit translationUnit = clang_parseTranslationUnit(
   index,
   //"test/E_Util/ELibUtil.cpp",
   "test/test.h",
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