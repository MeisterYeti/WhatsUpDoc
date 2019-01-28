#include "Helper.h"

#include <EScript/Utils/StringUtils.h>

#include <iostream>
#include <sstream>

namespace WhatsUpDoc {

std::ostream& operator<<(std::ostream& stream, const CXString& str) {
  auto cstr = clang_getCString(str);
  if(cstr && cstr[0] != '\0')
    stream << cstr;
  clang_disposeString(str);
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

std::string toString(const CXString& str) {
  auto cstr = clang_getCString(str);
  std::string s;
  if(cstr && cstr[0] != '\0')
    s = cstr;
  clang_disposeString(str);
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
  if(kind == CXCursor_DeclRefExpr) {
    //std::cout << "      c " << name << " -> " << getCursorKindName(kind) << std::endl;
    *reinterpret_cast<CXCursor*>(client_data) = cursor;
    return CXChildVisit_Break;
  }    
  return CXChildVisit_Recurse;
}

CXCursor getCursorRef(CXCursor cursor) {
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

std::string parseComment(const std::string& comment) {
  using namespace EScript::StringUtils;
  // TODO: parse doxygen commands
  std::string result;
  auto tmp = replaceAll(comment, "//!", "///");
  tmp = replaceAll(tmp, "/*!", "/**");
  tmp = replaceAll(tmp, "*/", "");
  auto lines = split(tmp, "\n");
  for(auto& line : lines) {
    auto trimmedLine = lTrim(line);
    if(trimmedLine.substr(0, 3) == "///" || trimmedLine.substr(0, 3) == "/**") {
      result += trimmedLine.substr((trimmedLine[3] == ' ' || trimmedLine[3] == '\t') ? 4 : 3) + "\n";
    } else if(trimmedLine[0] == '*') {
      result += trimmedLine.substr((trimmedLine[1] == ' ' || trimmedLine[1] == '\t') ? 2 : 1) + "\n";
    } else {
      // TODO: remove first spaces
      result += line + "\n";
    }
  }
  return result.substr(0, result.size()-1); // remove last \n
}

// -------------------------------------------------

CXChildVisitResult printASTVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
  CXCursorKind kind = clang_getCursorKind(cursor);
  auto name = toString(clang_getCursorSpelling(cursor));
  int indent = *reinterpret_cast<int*>(client_data);
  for(int i=0; i<indent; ++i)
    std::cout << "  ";
  std::cout << "cursor " << name << " -> " << clang_getCursorKindSpelling(kind) << std::endl;
  indent++;
  clang_visitChildren(cursor, *extractStringLiteralVisitor, &indent);
  return CXChildVisit_Continue;
}

void printAST(CXCursor cursor, int indent) {
  CXCursorKind kind = clang_getCursorKind(cursor);
  auto name = toString(clang_getCursorSpelling(cursor));
  for(int i=0; i<indent; ++i)
    std::cout << "  ";
  std::cout << "cursor " << name << " -> " << clang_getCursorKindSpelling(kind) << std::endl;
  indent++;
  clang_visitChildren(cursor, *extractStringLiteralVisitor, &indent);
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
} /* WhatsUpDoc */