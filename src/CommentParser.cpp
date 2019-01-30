#include "CommentParser.h"

#include <EScript/Utils/StringUtils.h>
#include <regex>

namespace WhatsUpDoc {
using namespace CommentTokens;
using namespace EScript::StringUtils;


// -------------------------------------------------

std::deque<CommentTokenPtr> parseComment(const std::string& comment, const Location& location) {
  std::deque<CommentTokenPtr> result;
  
  // TODO: parse doxygen commands
  std::stringstream regex;
  regex << R"((?:(?:/\*(?:!|\*+))|(?://(?:!|/+))|(?:\s*\*+/?)|(?:\s{)" << location.col << R"(}))\s?(.*))";    
  std::regex lineRegex = std::regex(regex.str());
  
  std::regex defGrpRegex = std::regex(R"((?:@|\\)defgroup\s+(\w+)\s+(.*))");
  std::regex grpRegex = std::regex(R"((?:@|\\)(addtogroup|ingroup)\s+(\w+))");
  std::regex mgrpRegex = std::regex(R"((?:@|\\)(name)\s+(.*))");
  std::regex blockRegex = std::regex(R"(@(\{|\}))");
  
  auto lines = split(comment, "\n");  
  int i=location.line;
  for(auto& line : lines) {
    line = std::regex_replace(line, lineRegex, "$1");
    std::smatch match;    
    if(std::regex_match(line, match, defGrpRegex)) {
      result.emplace_back(new TDefGroup(i,match[1],match[2]));
    } else if(std::regex_match(line, match, grpRegex)) {
      result.emplace_back(new TInGroup(i,match[2]));
    } else if(std::regex_match(line, match, mgrpRegex)) {
      result.emplace_back(new TMemberGroup(i,match[2]));
    } else if(std::regex_match(line, match, blockRegex)) {
      if(match[1] == "{")
        result.emplace_back(new TBlockStart(i));
      else if(match[1] == "}")
        result.emplace_back(new TBlockEnd(i));
    } else {
      result.emplace_back(new TTextLine(i, line));
    }
    ++i;
  }
  // remove first & last empty lines
  if(!result.empty() && result.front()->getType() == TTextLine::TYPE && trim(result.front()->to<TTextLine>()->text).empty())
    result.pop_front();
  if(!result.empty() && result.back()->getType() == TTextLine::TYPE && trim(result.back()->to<TTextLine>()->text).empty())
    result.pop_back();
  result.emplace_back(new TCommentEnd(i));
  return result;
}

} /* WhatsUpDoc */
