#ifndef WHATSUPDOC_HELPER_H_
#define WHATSUPDOC_HELPER_H_

#include <clang-c/Index.h>
#include <string>
#include <ostream>

namespace WhatsUpDoc {

std::ostream& operator<<(std::ostream& stream, const CXString& str);
std::string toString(const CXString& str);

std::string getCursorKindName(CXCursorKind kind);

} /* WhatsUpDoc */

#endif /* end of include guard: WHATSUPDOC_HELPER_H_ */
