#include <iostream>
#include <vector>
#include <clang-c/Index.h>
#include <clang-c/Documentation.h>
using namespace std;

ostream& operator<<(ostream& stream, const CXString& str) {
  const char* cstr = clang_getCString(str);
  if(cstr && cstr[0] != '\0') {
    stream << cstr;
  }
  clang_disposeString(str);
  return stream;
}

int main() {
  CXIndex index = clang_createIndex(1, 1);
  const char* args[] = { "-x", "c++", "-Wdocumentation" }; 
  CXTranslationUnit unit = clang_parseTranslationUnit(
    index,
    "test/test.h", args, 2,
    nullptr, 0,
    CXTranslationUnit_None);
  if (unit == nullptr) {
    cerr << "Unable to parse translation unit. Quitting." << endl;
    exit(-1);
  }  

  CXCursor cursor = clang_getTranslationUnitCursor(unit);
  clang_visitChildren(
    cursor,
    [](CXCursor c, CXCursor parent, CXClientData client_data) {
      cout << "Cursor '" << clang_getCursorSpelling(c) << "' of kind '" << clang_getCursorKindSpelling(clang_getCursorKind(c)) << endl;
      //auto comment = clang_Cursor_getParsedComment(c);
      //auto commentStr = clang_FullComment_getAsXML(comment);
      //auto commentStr = clang_Cursor_getRawCommentText(c);
      cout << clang_Cursor_getRawCommentText(c) << endl;
      return CXChildVisit_Recurse;
    },
    nullptr);
    
  clang_disposeTranslationUnit(unit);
  clang_disposeIndex(index);
}