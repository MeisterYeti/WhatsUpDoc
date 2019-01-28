#ifndef WHATSUPDOC_PARSER_H_
#define WHATSUPDOC_PARSER_H_

#include <string>
#include <vector>
#include <memory>

namespace WhatsUpDoc {
struct ParsingContext;

class Parser {
public:
  Parser();
  ~Parser();
  
  void addInclude(const std::string& path);
  void parseFile(const std::string& filename);
  void writeJSON(const std::string& path) const;
private:
  std::vector<std::string> include;
  std::unique_ptr<ParsingContext> context;
};

} /* WhatsUpDoc */

#endif /* end of include guard: WHATSUPDOC_PARSER_H_ */
