#include "Parser.h"
#include "Helper.h"

#include <clang-c/Index.h>

#include <EScript/Utils/StringId.h>

#include <iostream>
#include <unordered_map>
#include <set>

namespace WhatsUpDoc {
using namespace EScript;

struct FunctionAttr {
  uint32_t line;
  int minParams = 0;
  int maxParams = 0;
  std::string name;
  std::string comment;
  StringId lib;
};

struct ConstAttr {
  uint32_t line;
  std::string name;
  std::string comment;
  StringId lib;
};

struct RefAttr {
  uint32_t line;
  std::string name;
  StringId lib;
  StringId ref;
};

struct InitCall {
  StringId call;
  StringId lib;
};

struct InitFunction {
  StringId id;
  StringId paramId;
  std::vector<FunctionAttr> functions;
  std::vector<ConstAttr> consts;
  std::vector<RefAttr> refs;
  std::vector<InitCall> initCalls;
  std::unordered_map<StringId, CXCursor> usedLibDecl;
};

struct ParsingContext {
  CXIndex index;
  CXTranslationUnit tu;
  InitFunction* activeInit;
  std::unordered_map<StringId, InitFunction> inits;
};

// -------------------------------------------------

void assignComments(CXCursor cursor, ParsingContext* context) {
  auto it = context->activeInit->functions.begin();
  if(it == context->activeInit->functions.end())
    return;
  CXSourceRange range = clang_getCursorExtent(cursor);
  CXTranslationUnit tu = clang_Cursor_getTranslationUnit(cursor);
  CXToken *tokens = 0;
  unsigned int nTokens = 0;
  clang_tokenize(tu, range, &tokens, &nTokens);
  for (unsigned int i = 0; i < nTokens; i++) {
    CXTokenKind kind = clang_getTokenKind(tokens[i]);
    if(kind == CXToken_Comment) {
      auto location = getTokenLocation(tu, tokens[i]);
      std::string comment = toString(clang_getTokenSpelling(tu, tokens[i]));
      std::string pre = comment.substr(0, 3);
      if(pre == "///" || pre == "//!" || pre == "/**" || pre == "/*!") {
        // TODO: parse comments
        if(location.line > it->line)
          it++;
        if(it == context->activeInit->functions.end())
          break;
        it->comment += comment + "\n";
      }
    }    
  }
  clang_disposeTokens(tu, tokens, nTokens);
}

// -------------------------------------------------

void handleDeclareFunction(CXCursor cursor, ParsingContext* context) {
  int argc = clang_Cursor_getNumArguments(cursor);
  auto location = getCursorLocation(cursor);
  FunctionAttr fun;
  fun.line = location.line;
  if(argc != 3 && argc != 5) {
    std::cerr << "invalid function declaration at " << location << "." << std::endl;
    return;
  }
  CXCursor libRef = getCursorRef(clang_Cursor_getArgument(cursor, 0));
  if(clang_Cursor_isNull(libRef) || (!hasType(libRef, "EScript::Namespace") && !hasType(libRef, "EScript::Type"))) {
    std::cerr << "invalid function declaration at " << location << "." << std::endl;
    return;
  }
  fun.lib = toString(clang_getCursorUSR(libRef));
  context->activeInit->usedLibDecl[fun.lib] = libRef;
  fun.name = extractStringLiteral(clang_Cursor_getArgument(cursor, 1));
  if(argc == 5) {
    fun.minParams = extractIntLiteral(clang_Cursor_getArgument(cursor, 2));
    fun.maxParams = extractIntLiteral(clang_Cursor_getArgument(cursor, 3));
  }
  context->activeInit->functions.emplace_back(std::move(fun));
}

// -------------------------------------------------

void handleDeclareConstant(CXCursor cursor, ParsingContext* context) {
  int argc = clang_Cursor_getNumArguments(cursor);
  auto location = getCursorLocation(cursor);
  if(argc != 3) return;
  CXCursor libRef = getCursorRef(clang_Cursor_getArgument(cursor, 0));
  if(clang_Cursor_isNull(libRef) || (!hasType(libRef, "EScript::Namespace") && !hasType(libRef, "EScript::Type"))) {
    std::cerr << "invalid constant declaration at " << location << "." << std::endl;
    return;
  }
  std::string name;
  StringId lib = toString(clang_getCursorUSR(libRef));
  context->activeInit->usedLibDecl[lib] = libRef;
  CXCursor nameRef = getCursorRef(clang_Cursor_getArgument(cursor, 1));
  if(!clang_Cursor_isNull(nameRef)) {
    // TODO: resolve name
    name = toString(clang_getCursorSpelling(nameRef));
  } else {
    name = extractStringLiteral(clang_Cursor_getArgument(cursor, 1));
  }
  
  CXCursor valueRef = getCursorRef(clang_Cursor_getArgument(cursor, 2));
  if(!clang_Cursor_isNull(valueRef) && (hasType(valueRef, "EScript::Namespace") || hasType(valueRef, "EScript::Type"))) {
    RefAttr attr{location.line, name, lib, toString(clang_getCursorUSR(valueRef))};
    context->activeInit->refs.emplace_back(std::move(attr));
  } else {
    ConstAttr attr{location.line, name, "", lib};
    context->activeInit->consts.emplace_back(std::move(attr));
  }
  
}

// -------------------------------------------------

void handleInitCall(CXCursor cursor, ParsingContext* context) {
  CXCursor cursorRef = clang_getCursorReferenced(cursor);
  int argc = clang_Cursor_getNumArguments(cursor);
  if(argc < 1) return;
  CXCursor libRef = getCursorRef(clang_Cursor_getArgument(cursor, 0));
  if(clang_Cursor_isNull(libRef) || !hasType(libRef, "EScript::Namespace")) {
    std::cerr << "invalid init call at " << getCursorLocation(cursor) << "." << std::endl;
    return;
  }
  InitCall call;
  call.call = toString(clang_getCursorUSR(cursorRef));
  call.lib = toString(clang_getCursorUSR(libRef));
  context->activeInit->initCalls.emplace_back(std::move(call));
  context->activeInit->usedLibDecl[call.lib] = libRef;
}

// -------------------------------------------------

CXChildVisitResult visitInitFunction(CXCursor cursor, CXCursor parent, CXClientData data) {
  auto context = reinterpret_cast<ParsingContext*>(data);
  CXCursorKind kind = clang_getCursorKind(cursor);
  if(kind == CXCursor_FunctionDecl) {
    return CXChildVisit_Continue;
  } else if(kind == CXCursor_CallExpr) {
    auto name = toString(clang_getCursorSpelling(cursor));
    if(name == "declareFunction") {
      handleDeclareFunction(cursor, context);
    } else if(name == "declareConstant") {
      handleDeclareConstant(cursor, context);
    } else if(name == "init") {
      handleInitCall(cursor, context);
    }
    return CXChildVisit_Continue;
  }
  return CXChildVisit_Recurse;
}

// -------------------------------------------------

CXChildVisitResult visitRoot(CXCursor cursor, CXCursor parent, CXClientData data) {
  auto context = reinterpret_cast<ParsingContext*>(data);  
  CXCursorKind kind = clang_getCursorKind(cursor);
  auto location = clang_getCursorLocation(cursor);
  
  //if(!clang_Location_isFromMainFile(location))
  //  return CXChildVisit_Continue;
  if(clang_Location_isInSystemHeader(location))
    return CXChildVisit_Continue;
  /*if(kind == CXCursor_MacroExpansion) {  
    if(name.compare(0, 6, "ES_FUN") == 0 || name.compare(0, 7, "ES_MFUN") == 0 || name.compare(0, 7, "ES_CTOR") == 0) {
      std::cout << "macro " << name << " -> " << getCursorKindName(kind) << std::endl;
    }
  } */
  
  if (kind == CXCursor_FunctionDecl || kind == CXCursor_CXXMethod) {
    bool isDef = clang_isCursorDefinition(cursor);
    if(!isDef)
      return CXChildVisit_Continue;
    std::string name = toString(clang_getCursorSpelling(cursor));
      
    if(name == "init") {
      int argc = clang_Cursor_getNumArguments(cursor);
      if(argc != 1 || !hasType(clang_Cursor_getArgument(cursor, 0), "EScript::Namespace"))
        return CXChildVisit_Continue;
      StringId id = toString(clang_getCursorUSR(cursor));
      InitFunction& init = context->inits[id];
      init.id = id;
      init.paramId = toString(clang_getCursorUSR(clang_Cursor_getArgument(cursor, 0)));
      init.usedLibDecl[init.paramId] = clang_Cursor_getArgument(cursor, 0);
      
      context->activeInit = &init;
      
      std::cout << "init " << id << std::endl;
      clang_visitChildren(cursor, *visitInitFunction, context);
      assignComments(cursor, context);
      
      for(auto& v : init.refs)
        std::cout << "  type " << v.name << " (" << v.lib.getValue() << ", " << v.ref.getValue() << ") @ " << v.line << std::endl;
      
      for(auto& v : init.consts)
        std::cout << "  const " << v.name << " (" << v.lib.getValue() << ") @ " << v.line << std::endl;
    
      for(auto& v : init.functions) {
        if(!v.comment.empty())
          std::cout << "  " << v.comment;
        std::cout << "  fun " << v.name << " (" << v.lib.getValue() << ", " << v.minParams << ", " << v.maxParams << ") @ " << v.line << std::endl;
      }
    
    for(auto& v : init.initCalls)
      std::cout << "  init " << v.call.getValue() << " (" << v.lib.getValue() << ") @ " << std::endl;
      
      context->activeInit = nullptr;
    } else if(name == "getClassName") {
      //std::cout << "method " << name << " -> " << getCursorKindName(kind) << std::endl;
      //std::cout << "  usr " << clang_getCursorUSR(cursor) << std::endl;
      //clang_visitChildren(cursor, *functionDeclVisitor, 0);
    }
    return CXChildVisit_Continue;
  }
  return CXChildVisit_Recurse;
}

// ==============================================================================

Parser::Parser() : context(new ParsingContext) {
  context->index = clang_createIndex(0, 0);
  if(!context->index)
    throw std::runtime_error("error creating index");
}

Parser::~Parser() {
  clang_disposeIndex(context->index);
}

void Parser::addInclude(const std::string& path) {
  include.emplace_back("-I" + path);
}

void Parser::parseFile(const std::string& filename) {
  // = { "-x", "c++", "-Wdocumentation", "-fparse-all-comments", "-Itest", "-Itest/EScript", "-Itest/E_Util" };
  std::vector<const char*> args;
  args.reserve(include.size() + 4);
  args.emplace_back("-x");
  args.emplace_back("c++");
  args.emplace_back("-Wdocumentation");
  args.emplace_back("-fparse-all-comments");
  for(auto& s : include) 
    args.emplace_back(s.c_str());
  
  std::cout << "parse" << std::endl;
  context->tu = clang_parseTranslationUnit(context->index, filename.c_str(), args.data(), args.size(), nullptr, 0, CXTranslationUnit_None);
  //CXTranslationUnit translationUnit = clang_parseTranslationUnit(index, 0, argv, argc, 0, 0, CXTranslationUnit_None);
  
  if(!context->tu) {
    std::cerr << "error creating translationUnit" << std::endl;
    return;
  }
  std::cout << "translate" << std::endl;
  
  CXCursor rootCursor = clang_getTranslationUnitCursor(context->tu);  
  clang_visitChildren(rootCursor, *visitRoot, context.get());  
  clang_disposeTranslationUnit(context->tu);
  context->tu = nullptr;
}

} /* WhatsUpDoc */