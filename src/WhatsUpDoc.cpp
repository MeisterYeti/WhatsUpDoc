#include <iostream>
#include <vector>
#include <clang-c/Index.h>
using namespace std;

ostream& operator<<(ostream& stream, const CXString& str)
{
  stream << clang_getCString(str);
  clang_disposeString(str);
  return stream;
}

int main()
{
  CXIndex index = clang_createIndex(1, 1);
  const char* args[] = {"-x","c++"}; 
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
      cout << "Cursor '" << clang_getCursorSpelling(c) << "' of kind '"
        << clang_getCursorKindSpelling(clang_getCursorKind(c)) << endl;
      return CXChildVisit_Recurse;
    },
    nullptr);
    
  clang_disposeTranslationUnit(unit);
  clang_disposeIndex(index);
}