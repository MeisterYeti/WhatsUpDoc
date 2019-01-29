#ifndef WHATSUPDOC_HELPER_H_
#define WHATSUPDOC_HELPER_H_

#include <EScript/Utils/StringId.h>
#include <clang-c/Index.h>
#include <string>
#include <ostream>

namespace WhatsUpDoc {
struct Location {
  std::string file;
  unsigned int line;
  unsigned int col;
};

std::ostream& operator<<(std::ostream& stream, const CXString& str);
std::ostream& operator<<(std::ostream& stream, const EScript::StringId& str);
std::ostream& operator<<(std::ostream& stream, const Location& loc);
std::string toString(const CXString& str);

bool isLiteral(CXCursorKind kind);

bool hasType(CXCursor cursor, const std::string& name);

CXCursor getCursorRef(CXCursor cursor);

Location getCursorLocation(CXCursor cursor);
Location getTokenLocation(CXTranslationUnit tu, CXToken token);

int extractIntLiteral(CXCursor cursor);
std::string extractStringLiteral(CXCursor cursor);
std::string parseComment(const std::string& comment, const Location& location);
void printAST(CXCursor cursor, int indent=0);
void printTokens(CXCursor cursor, int indent=0);
std::string toJSONFilename(const EScript::StringId& id);
} /* WhatsUpDoc */

#endif /* end of include guard: WHATSUPDOC_HELPER_H_ */
