#ifndef WHATSUPDOC_COMMENTPARSER_H_
#define WHATSUPDOC_COMMENTPARSER_H_

#include "Helper.h"

#include <string>
#include <deque>
#include <memory>

namespace WhatsUpDoc {
  
namespace CommentTokens {
  
class Token {
public:
  static const uint32_t TYPE = 0;
  const uint32_t type;
  const uint32_t line;
  Token(uint32_t line, uint32_t type) : type(type), line(line) {}
  uint32_t getType() const { return type; }
  template<class T>
  T* to() { return static_cast<T*>(this); }
};

class TTextLine : public Token {
public:
  static const uint32_t TYPE = 1;
  TTextLine(uint32_t line, const std::string& text) : Token(line, TYPE), text(text) {}
  const std::string text;
};

class TBlockStart : public Token {
public:
  static const uint32_t TYPE = 2;
  TBlockStart(uint32_t line) : Token(line, TYPE) {}
};

class TBlockEnd : public Token {
public:
  static const uint32_t TYPE = 3;
  TBlockEnd(uint32_t line) : Token(line, TYPE) {}
};

class TDefGroup : public Token {
public:
  static const uint32_t TYPE = 4;
  const std::string id;
  const std::string name;
  TDefGroup(uint32_t line, const std::string& id, const std::string& name) : Token(line, TYPE), id(id), name(name) {}
};

class TInGroup : public Token {
public:
  static const uint32_t TYPE = 5;
  const std::string id;
  TInGroup(uint32_t line, const std::string& id) : Token(line, TYPE), id(id) {}
};

class TMemberGroup : public Token {
public:
  static const uint32_t TYPE = 6;
  const std::string id;
  TMemberGroup(uint32_t line, const std::string& id) : Token(line, TYPE), id(id) {}
};

class TCommentEnd : public Token {
public:
  static const uint32_t TYPE = 7;
  TCommentEnd(uint32_t line) : Token(line, TYPE) {}
};

class TDeprecated : public Token {
public:
  static const uint32_t TYPE = 8;
  const std::string description;
  TDeprecated(uint32_t line, const std::string& descr) : Token(line, TYPE), description(descr) {}
};

class TCodeBlockStart : public Token {
public:
  static const uint32_t TYPE = 9;
  TCodeBlockStart(uint32_t line, const std::string& lang) : Token(line, TYPE), lang(lang) {}
  const std::string lang;
};

class TCodeBlockEnd : public Token {
public:
  static const uint32_t TYPE = 10;
  TCodeBlockEnd(uint32_t line) : Token(line, TYPE) {}
};

class TCodeLine : public Token {
public:
  static const uint32_t TYPE = 11;
  TCodeLine(uint32_t line, const std::string& text) : Token(line, TYPE), text(text) {}
  const std::string text;
};

} /* CommentTokens */

typedef std::unique_ptr<CommentTokens::Token> CommentTokenPtr;
std::deque<CommentTokenPtr> parseComment(const std::string& comment, const Location& location);

} /* WhatsUpDoc */

#endif /* end of include guard: WHATSUPDOC_COMMENTPARSER_H_ */
