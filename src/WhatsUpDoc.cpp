#include <iostream>
#include <clang-c/Documentation.h>

void printDiagnostics(CXTranslationUnit translationUnit) {
  int nbDiag = clang_getNumDiagnostics(translationUnit);
  printf("There is %i diagnostics\n",nbDiag);
  
  for(unsigned int currentDiag = 0; currentDiag < nbDiag; ++currentDiag) {
    CXDiagnostic diagnotic = clang_getDiagnostic(translationUnit, currentDiag); 
    CXString errorString = clang_formatDiagnostic(diagnotic,clang_defaultDiagnosticDisplayOptions());
    fprintf(stderr, "%s\n", clang_getCString(errorString));
    clang_disposeString(errorString);
  }
}

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

void printCursorTokens(CXTranslationUnit translationUnit,CXCursor currentCursor) {
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
  auto cstr = clang_getCString(com);
  if(cstr && cstr[0] != '\0')
    printf("    comment '%s'\n",cstr);
    
  if(kind == CXCursor_CallExpr) {    
    printf("  call '%s' -> %i\n",clang_getCString(name),kind);
    int argc = clang_Cursor_getNumArguments(cursor);
    for(int i=0; i<argc; ++i) {
      CXCursor arg = clang_Cursor_getArgument(cursor, i);
      CXString argname = clang_getCursorSpelling(arg);
      CXCursorKind akind = clang_getCursorKind(arg);
      printf("    arg '%s' -> %i\n",clang_getCString(argname), akind);
      
      CXSourceRange range = clang_getCursorExtent(arg);
      CXTranslationUnit tu = clang_Cursor_getTranslationUnit(arg);
      CXToken *tokens = 0;
      unsigned int nTokens = 0;
      clang_tokenize(tu, range, &tokens, &nTokens);
      for (unsigned int i = 0; i < nTokens; i++) {
          CXString spelling = clang_getTokenSpelling(tu, tokens[i]);
          printf("      token = %s\n", clang_getCString(spelling));
          clang_disposeString(spelling);
      }
      clang_disposeTokens(tu, tokens, nTokens);
    }
  } else if(kind == CXCursor_FunctionDecl) {
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
  auto cstr = clang_getCString(com);
  if(cstr && cstr[0] != '\0')
    printf("  comment '%s'\n",cstr);
    
  CXString cname = clang_getCursorSpelling(cursor);
  std::string name = clang_getCString(cname);
  if (kind == CXCursor_ParmDecl) {
    printf("\tparameter: '%s' of type '%i'\n",name.c_str(),type.kind);
    int *nbParams = (int *)client_data;
    (*nbParams)++;
  }
  printf("  name '%s' -> %i\n",name.c_str(),kind);

  clang_visitChildren(cursor, *initVisitor, nullptr);
  //printCursorTokens(clang_Cursor_getTranslationUnit(cursor),cursor);

  return CXChildVisit_Continue;
}

CXChildVisitResult cursorVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  
  CXCursorKind kind = clang_getCursorKind(cursor);
  CXString cname = clang_getCursorSpelling(cursor);
  std::string name = clang_getCString(cname);
  
  if(kind == CXCursor_MacroExpansion) {  
    if(name.compare(0, 6, "ES_FUN") == 0 || name.compare(0, 7, "ES_MFUN") == 0) {
      printf("macro '%s' -> %i\n",name.c_str(),kind);
    }
  } else if (kind == CXCursor_FunctionDecl) {
    if(name == "init") {
      
      CXString com = clang_Cursor_getRawCommentText(cursor);
      auto cstr = clang_getCString(com);
      if(cstr && cstr[0] != '\0')
        printf("comment '%s'\n",cstr);
      printf("method '%s'\n",name.c_str());
              
      CXSourceLocation location = clang_getCursorLocation(cursor);      
      CXString filename;
      unsigned int line, column;      
      clang_getPresumedLocation(location, &filename, &line, &column);      
      printf("source location : %s, (%i,%i)\n",clang_getCString(filename),line,column);
      

      // visit method childs
      int nbParams = 0;
      clang_visitChildren(cursor, *functionDeclVisitor,&nbParams);
    }
    return CXChildVisit_Continue;
  }
  //printf("cursor '%s' -> %i\n",name.c_str(),kind);
  return CXChildVisit_Recurse;
}

int main (int argc, const char * argv[]) {
  CXIndex index = clang_createIndex(1, 1);
  
  if(index == 0) {
    fprintf(stderr, "error creating index\n");
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
    fprintf(stderr, "error creating translationUnit\n");
    return 1;
  }
  
  printDiagnostics(translationUnit);

  
  CXCursor rootCursor = clang_getTranslationUnitCursor(translationUnit);
  
  //printCursorTokens(translationUnit,rootCursor);
  
  unsigned int res = clang_visitChildren(rootCursor, *cursorVisitor,0);
  
  clang_disposeTranslationUnit(translationUnit);
  clang_disposeIndex(index);
  return 0;
}