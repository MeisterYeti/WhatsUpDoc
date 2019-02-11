#include "Helper.h"

#include <EScript/Utils/StringUtils.h>

#include <regex>
#include <iostream>
#include <sstream>

namespace WhatsUpDoc {

std::ostream& operator<<(std::ostream& stream, const CXString& str) {
  stream << toString(str);
  return stream;
}

// -------------------------------------------------

std::ostream& operator<<(std::ostream& stream, const EScript::StringId& str) {
  stream << str.toString();
  return stream;
}

// -------------------------------------------------

std::ostream& operator<<(std::ostream& stream, const Location& loc) {
  stream << loc.file << ":" << loc.line << ":" << loc.col;
  return stream;
}


// -------------------------------------------------

std::ostream& operator<<(std::ostream& stream, const CXCursor& cursor) {
  CXCursorKind kind = clang_getCursorKind(cursor);
  auto name = toString(clang_getCursorSpelling(cursor));
  auto type = toString(clang_getTypeSpelling(clang_getCursorType(cursor)));
  std::cout << clang_getCursorKindSpelling(kind) << ": " << name << " -> " << type;
  return stream;
}

// -------------------------------------------------

std::string toString(const CXString& str) {
  auto cstr = clang_getCString(str);
  std::string s;
  if(cstr && cstr[0] != '\0') {
    s = cstr;
    clang_disposeString(str);
  }
  return s;
}

// -------------------------------------------------

bool isLiteral(CXCursorKind kind) {  
  switch(kind) {
    case CXCursor_IntegerLiteral:
    case CXCursor_FloatingLiteral:
    case CXCursor_ImaginaryLiteral:
    case CXCursor_StringLiteral:
    case CXCursor_CharacterLiteral:
      return true;
    default:
      return false;
  }
}

// -------------------------------------------------

bool hasType(CXCursor cursor, const std::string& name) {
  auto type = clang_getCursorType(cursor);
  auto spelling = toString(clang_getTypeSpelling(type));
  return spelling.find(name) != std::string::npos;
}

// -------------------------------------------------

CXChildVisitResult getCursorRefVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  CXCursorKind kind = clang_getCursorKind(cursor);
  auto name = toString(clang_getCursorSpelling(cursor));
  
  //*reinterpret_cast<CXCursor*>(client_data) = clang_getNullCursor();
  switch (kind) {
    case CXCursor_DeclRefExpr:
      *reinterpret_cast<CXCursor*>(client_data) = cursor;
      return CXChildVisit_Break;
    default: break;
  }
  return CXChildVisit_Recurse;
}

CXCursor getCursorRef(CXCursor cursor) {
  if(clang_getCursorKind(cursor) == CXCursor_DeclRefExpr)
    return clang_getCursorReferenced(cursor);
  CXCursor refCursor = clang_getNullCursor();
  clang_visitChildren(cursor, *getCursorRefVisitor, &refCursor);
  if(!clang_Cursor_isNull(refCursor)) {
    return clang_getCursorReferenced(refCursor);
  }
  return clang_getNullCursor();
}

// -------------------------------------------------

Location getCursorLocation(CXCursor cursor) {
  Location loc;
  CXString filename;
  CXSourceLocation location = clang_getCursorLocation(cursor);
  clang_getPresumedLocation(location, &filename, &loc.line, &loc.col);
  loc.file = toString(filename);
  return loc;
}

// -------------------------------------------------

Location getTokenLocation(CXTranslationUnit tu, CXToken token) {
  Location loc;
  CXString filename;
  CXSourceLocation location = clang_getTokenLocation(tu, token);
  clang_getPresumedLocation(location, &filename, &loc.line, &loc.col);
  loc.file = toString(filename);
  return loc;
}

// -------------------------------------------------

int extractIntLiteral(CXCursor cursor) {
  CXSourceRange range = clang_getCursorExtent(cursor);
  CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
  CXToken *tokens = 0;
  unsigned int nTokens = 0;
  clang_tokenize(tu, range, &tokens, &nTokens);
  int value = 0;
  for (unsigned int i = 0; i < nTokens; i++) {
    std::string spelling = toString(clang_getTokenSpelling(tu, tokens[i]));
    if(clang_getTokenKind(tokens[i]) == CXToken_Literal && spelling.find('\"') == std::string::npos) {
      std::stringstream ss(spelling);
      ss >> value;
      break;
    }
  }
  clang_disposeTokens(tu, tokens, nTokens);
  return value;
}

// -------------------------------------------------

CXChildVisitResult extractStringLiteralVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  CXCursorKind kind = clang_getCursorKind(cursor);
  if(kind == CXCursor_StringLiteral) {
    auto name = toString(clang_getCursorSpelling(cursor));
    reinterpret_cast<std::string*>(client_data)->swap(name);
    return CXChildVisit_Break;
  }
  return CXChildVisit_Recurse;
}

std::string extractStringLiteral(CXCursor cursor) {
  CXSourceRange range = clang_getCursorExtent(cursor);
  CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
  CXToken *tokens = 0;
  unsigned int nTokens = 0;
  clang_tokenize(tu, range, &tokens, &nTokens);
  std::string value;
  for (unsigned int i = 0; i < nTokens; i++) {
    std::string spelling = toString(clang_getTokenSpelling(tu, tokens[i]));
    if(clang_getTokenKind(tokens[i]) == CXToken_Literal && spelling.find('\"') != std::string::npos) {
      value = spelling;
      break;
    }
  }
  clang_disposeTokens(tu, tokens, nTokens);
  if(value.empty()) {
    clang_visitChildren(cursor, *extractStringLiteralVisitor, &value);
  }
  if(!value.empty() && value.find('\"') != std::string::npos)
    value = value.substr(1, value.size()-2);
  return value;
}

// -------------------------------------------------

CXChildVisitResult printASTVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  int indent = *reinterpret_cast<int*>(client_data);
  for(int i=0; i<indent; ++i)
    std::cout << "  ";
  std::cout << "cursor " << cursor << std::endl;
  indent++;
  clang_visitChildren(cursor, *printASTVisitor, &indent);
  return CXChildVisit_Continue;
}

void printAST(CXCursor cursor, int indent) {
  for(int i=0; i<indent; ++i)
    std::cout << "  ";
  std::cout << "cursor " << cursor << std::endl;
  indent++;
  clang_visitChildren(cursor, *printASTVisitor, &indent);
}

// -------------------------------------------------

std::string getTokenKindName(CXTokenKind kind) {    
  switch (kind) {
    case CXToken_Comment: return "Comment";
    case CXToken_Identifier: return "Identifier";
    case CXToken_Keyword: return "Keyword";
    case CXToken_Literal: return "Literal";
    case CXToken_Punctuation: return "Punctuation";
  }
  return std::to_string(kind);
}

void printTokens(CXCursor cursor, int indent) {
  CXSourceRange range = clang_getCursorExtent(cursor);
  CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
  CXToken *tokens = 0;
  unsigned int nTokens = 0;
  clang_tokenize(tu, range, &tokens, &nTokens);
  for (unsigned int i = 0; i < nTokens; i++) {
    std::string spelling = toString(clang_getTokenSpelling(tu, tokens[i]));
    for(int i=0; i<indent; ++i)
      std::cout << "  ";
    std::cout << "token " << spelling << " -> " << getTokenKindName(clang_getTokenKind(tokens[i])) << std::endl;
  }
  clang_disposeTokens(tu, tokens, nTokens);
}

// -------------------------------------------------

std::string toJSONFilename(const EScript::StringId& id) {
  using namespace EScript::StringUtils;
  std::string file = id.toString();
  file = replaceAll(file,"@","_");
  file = replaceAll(file,":","_");
  file = replaceAll(file,".","_");
  file = replaceAll(file,"*","");
  file = replaceAll(file,"#","");
  file = replaceAll(file,"$","");
  file = replaceAll(file,"/","_");
  file = replaceAll(file,"\\","_");
  file += ".json";
  return file;
}

// -------------------------------------------------

struct FindResult {
  CXCursor cursor;
  int kind;
  std::string name;
  std::string type;
};

CXChildVisitResult findCursorVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  auto name = toString(clang_getCursorSpelling(cursor));
  CXCursorKind kind = clang_getCursorKind(cursor);
  auto type = toString(clang_getTypeSpelling(clang_getCursorType(cursor)));
  auto* result = reinterpret_cast<FindResult*>(client_data);  
  if(
    (result->kind == 0 || result->kind == kind) &&
    (result->name.empty() || name == result->name) &&
    (result->type.empty() || type.find(result->type) != std::string::npos)
  ) {
    result->cursor = cursor;
    return CXChildVisit_Break;
  }
  return CXChildVisit_Recurse;
}

CXCursor findCursor(CXCursor cursor, const std::string& name, int kind, const std::string& type, bool includeSelf) {
  FindResult result{clang_getNullCursor(), kind, name, type};
  if(!includeSelf || findCursorVisitor(cursor, clang_getNullCursor(), &result) != CXChildVisit_Break)
    clang_visitChildren(cursor, *findCursorVisitor, &result);
  return result.cursor;
}

// -------------------------------------------------

CXCursor findRef(CXCursor cursor, const std::string& name) {
  CXCursor call = findCursor(cursor, name, CXCursor_MemberRefExpr);
  if(clang_Cursor_isNull(call))
    call = findCursor(cursor, name, CXCursor_DeclRefExpr);
  if(!clang_Cursor_isNull(call)) {
    return clang_getCursorReferenced(call);
  }
  return call;
}

// -------------------------------------------------

CXCursor findTypeRef(CXCursor cursor, const std::string& type) {
  CXCursor typeCursor = findCursor(cursor, "", CXCursor_MemberRefExpr, type);
  if(clang_Cursor_isNull(typeCursor))
    typeCursor = findCursor(cursor, "", CXCursor_DeclRefExpr, type);
  if(!clang_Cursor_isNull(typeCursor))
    return clang_getCursorReferenced(typeCursor);
  return typeCursor;
}

// -------------------------------------------------

CXChildVisitResult findExposedVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  CXCursorKind kind = clang_getCursorKind(cursor);
  switch(kind) {
    case CXCursor_UnexposedAttr:
    case CXCursor_UnexposedDecl:
    case CXCursor_UnexposedExpr:
    case CXCursor_UnexposedStmt:
    case CXCursor_UnaryOperator: // also skip * &
      return CXChildVisit_Recurse;
    default: break;
  }
  // also skip RtValue
  if(hasType(cursor, "EScript::RtValue"))
    return CXChildVisit_Recurse;
  auto* result = reinterpret_cast<FindResult*>(client_data);
  result->cursor = cursor;
  return CXChildVisit_Break;
}

CXCursor findExposed(CXCursor cursor) {
  FindResult result{clang_getNullCursor(), 0, "", ""};
  if(findExposedVisitor(cursor, clang_getNullCursor(), &result) != CXChildVisit_Break)
    clang_visitChildren(cursor, *findExposedVisitor, &result);
  return result.cursor;
}

// -------------------------------------------------

std::string getFullyQualifiedName(CXCursor cursor) {
  std::string name = "";
  CXCursorKind kind = clang_getCursorKind(cursor);
  if(!clang_Cursor_isNull(cursor) && kind != CXCursor_TranslationUnit) {
    name = getFullyQualifiedName(clang_getCursorSemanticParent(cursor));
    if(!name.empty())
      name += "::";
    name += toString(clang_getCursorSpelling(cursor));
  }
  return name;
}

// -------------------------------------------------

bool matchWildcard(const std::string& input, const std::string& pattern) {
  using namespace EScript::StringUtils;
  std::string pat = pattern;
  pat = replaceAll(pat,"\\","\\\\");
  pat = replaceAll(pat,".","\\.");
  pat = replaceAll(pat,"/","\\/");
  pat = replaceAll(pat,"?",".");
  pat = replaceAll(pat,"*",".*");
  pat = "^" + pat + "$";
  return std::regex_match(input, std::regex(pat));
}

// -------------------------------------------------

} /* WhatsUpDoc */