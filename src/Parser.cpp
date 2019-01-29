#include "Parser.h"
#include "Helper.h"

#include <clang-c/Index.h>

#include <EScript/Utils/StringId.h>
#include <EScript/Utils/StringUtils.h>
#include <EScript/Utils/IO/IO.h>

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <deque>

namespace WhatsUpDoc {
using namespace EScript;

struct FunctionAttr {
  Location location;
  int minParams = 0;
  int maxParams = 0;
  std::string name;
  std::string comment;
  std::string cpp;
};

struct ConstAttr {
  Location location;
  std::string name;
  std::string comment;
  std::string cpp;
};

struct RefAttr {
  Location location;
  std::string name;
  StringId ref;
};

/*struct InitCall {
  StringId call;
  StringId lib;
};*/

struct Comment {
  Comment(int l, const std::string& s) : line(l), comment(s) {}
  int line;
  std::string comment;
};

struct Compound {
  StringId id;
  StringId refId;
  StringId parentId;
  std::string name;
  Location location;
  enum {UNKNOWN, NAMESPACE, TYPE} kind = UNKNOWN;
  std::vector<FunctionAttr> functions;
  std::vector<ConstAttr> consts;
  std::vector<RefAttr> children;
  bool isNull() const { return id.empty(); }
  bool isRef() const { return !refId.empty(); }
};

struct InitFunction {
  StringId id;
  StringId paramId;
  Location location;
  //std::vector<InitCall> initCalls;
};

struct ParsingContext {
  CXIndex index;
  CXTranslationUnit tu;
  InitFunction* activeInit;
  std::unordered_map<StringId, InitFunction> inits;
  std::unordered_map<StringId, std::string> names;
  std::deque<Comment> comments;
  std::unordered_map<StringId, Compound> compounds;
};

// -------------------------------------------------

void mergeCompounds(Compound& c1, Compound& c2, ParsingContext* context) {
  if(c1.id == c2.id)
    return;
  //std::cout << "  merge " << c1.id << " & " << c2.id << std::endl;
  std::move(c2.functions.begin(), c2.functions.end(), std::back_inserter(c1.functions));
  c2.functions.clear();
  
  std::move(c2.consts.begin(), c2.consts.end(), std::back_inserter(c1.consts));
  c2.consts.clear();
  
  for(auto& v : c2.children)
    context->compounds[v.ref].parentId = c1.id;
  std::move(c2.children.begin(), c2.children.end(), std::back_inserter(c1.children));
  c2.children.clear();
  
  if(c1.name.empty())
    c1.name = c2.name;
  c2.name.clear();
    
  if(c1.parentId.empty())
    c1.parentId = c2.parentId;
  if(c1.location.file.empty())
    c1.location = c2.location;
  if(c1.kind == Compound::UNKNOWN)
    c1.kind = c2.kind;
  c2.refId = c1.id;
}

// -------------------------------------------------

void extractComments(CXCursor cursor, ParsingContext* context) {
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
        comment = parseComment(comment, location);
        context->comments.emplace_back(location.line, comment);
      }
    }
  }
  clang_disposeTokens(tu, tokens, nTokens);
}

// -------------------------------------------------

Compound& resolveCompound(CXCursor cursor, ParsingContext* context) {
  static Compound nullCompound;
  if(clang_Cursor_isNull(cursor) || (!hasType(cursor, "EScript::Namespace") && !hasType(cursor, "EScript::Type")))
    return nullCompound;
  StringId id = toString(clang_getCursorUSR(cursor));
  if(id == context->activeInit->paramId) {
    id = context->activeInit->id;
  } else {
    CXCursor ref = getCursorRef(cursor);
    if(!clang_Cursor_isNull(ref)) {
      id = toString(clang_getCursorUSR(ref));
    }
  }
  
  auto& cmp = context->compounds[id];
  if(cmp.isNull()) {
    cmp.id = id;
    cmp.location = getCursorLocation(cursor);
    //std::cout << "  resolve " << cmp.id << std::endl;
    if(hasType(cursor, "EScript::Namespace"))
      cmp.kind = Compound::NAMESPACE; 
    else if(hasType(cursor, "EScript::Type"))
      cmp.kind = Compound::TYPE;
  } else {
    while(cmp.isRef())
      cmp = context->compounds[cmp.refId];
    //std::cout << "  resolve ref " << cmp.id << std::endl;
  }
  return cmp;
}

// -------------------------------------------------

void handleDeclareFunction(CXCursor cursor, ParsingContext* context) {
  int argc = clang_Cursor_getNumArguments(cursor);
  auto location = getCursorLocation(cursor);
  FunctionAttr fun;
  fun.location = location;
  if(argc != 3 && argc != 5) {
    std::cerr << std::endl << "invalid function declaration at " << location << "." << std::endl;
    return;
  }
  CXCursor libRef = getCursorRef(clang_Cursor_getArgument(cursor, 0));
  auto& cmp = resolveCompound(libRef, context);
  if(cmp.isNull()) {
    std::cerr << std::endl << "invalid function declaration at " << location << "." << std::endl;
    return;
  }
  fun.name = extractStringLiteral(clang_Cursor_getArgument(cursor, 1));
  if(argc == 5) {
    fun.minParams = extractIntLiteral(clang_Cursor_getArgument(cursor, 2));
    fun.maxParams = extractIntLiteral(clang_Cursor_getArgument(cursor, 3));
  }
  while(!context->comments.empty() && context->comments.front().line <= location.line) {
    fun.comment += context->comments.front().comment + "\n";
    context->comments.pop_front();
  }
  if(!fun.comment.empty())
    fun.comment = fun.comment.substr(0, fun.comment.size()-1); // remove last \n
  
  // try to find corresponding c++ function
  CXCursor fnArg = getCursorRef(clang_Cursor_getArgument(cursor, argc == 5 ? 4 : 2));
  CXCursor fnRef = findCall(fnArg, fun.name);
  if(!clang_Cursor_isNull(fnRef))
    fun.cpp = getFullyQualifiedName(fnRef);
  
  cmp.functions.emplace_back(std::move(fun));
}

// -------------------------------------------------

void handleDeclareConstant(CXCursor cursor, ParsingContext* context) {
  int argc = clang_Cursor_getNumArguments(cursor);
  auto location = getCursorLocation(cursor);
  if(argc != 3) return;
  CXCursor libRef = getCursorRef(clang_Cursor_getArgument(cursor, 0));
  auto& cmp = resolveCompound(libRef, context);
  if(cmp.isNull()) {
    std::cerr << std::endl << "invalid constant declaration at " << location << "." << std::endl;
    return;
  }
  std::string name;
  CXCursor nameRef = getCursorRef(clang_Cursor_getArgument(cursor, 1));
  if(!clang_Cursor_isNull(nameRef)) {
    StringId refId = toString(clang_getCursorUSR(nameRef));
    auto nameIt = context->names.find(refId);
    if(nameIt != context->names.end())
      name = nameIt->second;
  } else {
    name = extractStringLiteral(clang_Cursor_getArgument(cursor, 1));
  }
  if(name.empty()) {
    std::cerr << std::endl << "could not resolve constant name at " << location << "." << std::endl;
    return;
  }
  
    
  CXCursor valueRef = getCursorRef(clang_Cursor_getArgument(cursor, 2));
  if(!clang_Cursor_isNull(valueRef) && (hasType(valueRef, "EScript::Namespace") || hasType(valueRef, "EScript::Type"))) {
    auto& cmpRef = resolveCompound(valueRef, context);
    cmpRef.name = name;
    if(cmpRef.id == cmp.id)
      return;
    cmpRef.parentId = cmp.id;
    RefAttr attr{location, name, cmpRef.id};
    cmp.children.emplace_back(std::move(attr));
  } else {
    ConstAttr attr{location, name, "", ""};
    
    // try to find corresponding c++ object
    CXCursor objRef = findCall(clang_Cursor_getArgument(cursor, 2), name);
    if(!clang_Cursor_isNull(objRef))
      attr.cpp = getFullyQualifiedName(objRef);
    
    cmp.consts.emplace_back(std::move(attr));
  }
}

// -------------------------------------------------

void handleInitCall(CXCursor cursor, ParsingContext* context) {
  CXCursor cursorRef = clang_getCursorReferenced(cursor);
  int argc = clang_Cursor_getNumArguments(cursor);
  if(argc != 1) return;
  CXCursor libRef = getCursorRef(clang_Cursor_getArgument(cursor, 0));
  auto& cmp = resolveCompound(libRef, context);
  auto& initCmp = resolveCompound(cursorRef, context);
  
  if(cmp.isNull() || initCmp.isNull()) {
    std::cerr << std::endl << "invalid init call at " << getCursorLocation(cursor) << "." << std::endl;
    return;
  }
  //nitCall call;
  //all.call = initCmp.id;
  //all.lib = cmp.id;
  mergeCompounds(cmp, initCmp, context);  
  //context->activeInit->initCalls.emplace_back(std::move(call));
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
    std::string name = toString(clang_getCursorSpelling(cursor));
      
    if(name == "init") {
      if(!isDef)
        return CXChildVisit_Continue;
        
      int argc = clang_Cursor_getNumArguments(cursor);
      if(argc != 1 || !hasType(clang_Cursor_getArgument(cursor, 0), "EScript::Namespace")) {
        return CXChildVisit_Continue;
      }
      StringId id = toString(clang_getCursorUSR(cursor));
      InitFunction& init = context->inits[id];
      init.id = id;
      if(argc == 1)
        init.paramId = toString(clang_getCursorUSR(clang_Cursor_getArgument(cursor, 0)));
      
      context->activeInit = &init;
      resolveCompound(clang_Cursor_getArgument(cursor, 0), context);
      
      //std::cout << "init " << id << std::endl;
      extractComments(cursor, context);
      clang_visitChildren(cursor, *visitInitFunction, context);
          
      //for(auto& v : init.initCalls)
      //  std::cout << "  init " << v.call << " (" << v.lib << ") @ " << std::endl;
              
      context->activeInit = nullptr;
      context->comments.clear();
    } else if(name == "getClassName") {
      auto clname = extractStringLiteral(cursor);
      StringId id = toString(clang_getCursorUSR(cursor));
      if(!clname.empty())
        context->names[id] = clname;
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
  
  context->tu = clang_parseTranslationUnit(context->index, filename.c_str(), args.data(), args.size(), nullptr, 0, CXTranslationUnit_None);
  //CXTranslationUnit translationUnit = clang_parseTranslationUnit(index, 0, argv, argc, 0, 0, CXTranslationUnit_None);
  
  /*Compound* root = nullptr;
  for(auto& c : context->compounds) {
    if(c.second.parentId.empty()) {
      if(!root)
        root = &c.second;
      else
        mergeCompounds(*root, c.second, context.get());
    }
  }*/
  
  if(!context->tu) {
    std::cerr << std::endl << "error creating translationUnit" << std::endl;
    return;
  }
  
  //std::cout << "parsing " << filename << std::endl;
  CXCursor rootCursor = clang_getTranslationUnitCursor(context->tu);  
  clang_visitChildren(rootCursor, *visitRoot, context.get());  
  clang_disposeTranslationUnit(context->tu);
  context->tu = nullptr;
}

void Parser::writeJSON(const std::string& path) const {
  using namespace EScript::StringUtils;
  
  size_t maxLength = 80;
  int progress = 0;
  
  for(auto& c : context->compounds) {
    if(c.second.isRef() || c.second.name.empty())
      continue;
    std::string fullname = c.second.name;
    auto pid = c.second.parentId;
    while(!pid.empty()) {
      auto& p = context->compounds[pid];
      if(!p.name.empty())
        fullname = p.name + "." + fullname;
      pid = p.parentId;
    }
    std::string kind = "unknown";
    switch(c.second.kind) {
      case Compound::NAMESPACE:
        kind = "namespace"; break;
      case Compound::TYPE:
        kind = "type"; break;
      default: break;
    }
    
    std::stringstream json;
    std::string filename = kind + "_" + replaceAll(fullname, ".", "_") + ".json";
      
    int percent = static_cast<float>(progress)/context->compounds.size()*100;
    maxLength = std::max(maxLength, filename.size());
    std::cout << "\r[" << percent << "%] Writing " << filename << std::string(maxLength-filename.size(), ' ') << std::flush;
    
    json << "{" << std::endl;
    json << "  \"id\" : \"" << c.second.id << "\"," << std::endl;
    json << "  \"name\" : \"" << c.second.name << "\"," << std::endl;
    json << "  \"fullname\" : \"" << fullname << "\"," << std::endl;
    json << "  \"kind\" : \"";
    switch(c.second.kind) {
      case Compound::NAMESPACE:
        json << "namespace"; break;
      case Compound::TYPE:
        json << "type"; break;
      default:
        json << "unknown";
    }
    json << "\"," << std::endl;
    json << "  \"location\" : \"" << c.second.location << "\"," << std::endl;
    if(!c.second.parentId.empty() && !context->compounds[c.second.parentId].name.empty())
      json << "  \"parent\" : \"" << c.second.parentId << "\"," << std::endl;
    else
      json << "  \"parent\" : \"\"," << std::endl;
    
    json << "  \"children\" : [" << std::endl;
    for(auto& v : c.second.children) {
      json << "    {" << std::endl;
      json << "      \"name\" : \"" << v.name << "\"," << std::endl;
      json << "      \"ref\" : \"" << v.ref << "\"," << std::endl;
      json << "      \"location\" : \"" << v.location << "\"," << std::endl;
      json << "    }," << std::endl;
    }
    json << "  ]," << std::endl;
    
    json << "  \"constants\" : [" << std::endl;
    for(auto& v : c.second.consts) {
      json << "    {" << std::endl;
      json << "      \"name\" : \"" << v.name << "\"," << std::endl;
      json << "      \"location\" : \"" << v.location << "\"," << std::endl;
      json << "      \"cpp\" : \"" << v.cpp << "\"," << std::endl;
      json << "    }," << std::endl;
    }
    json << "  ]," << std::endl;
    
    json << "  \"functions\" : [" << std::endl;
    for(auto& v : c.second.functions) {
      json << "    {" << std::endl;
      json << "      \"name\" : \"" << v.name << "\"," << std::endl;
      json << "      \"minParams\" : " << v.minParams << "," <<std::endl;
      json << "      \"maxParams\" : " << v.maxParams << "," << std::endl;
      json << "      \"location\" : \"" << v.location << "\"," << std::endl;
      json << "      \"comment\" : \"" << escape(v.comment) << "\"," << std::endl;
      json << "      \"cpp\" : \"" << v.cpp << "\"," << std::endl;
      json << "    }," << std::endl;
    }
    json << "  ]," << std::endl;
    json << "}" << std::endl;
    
    EScript::IO::saveFile(path + "/" + filename, json.str());
    ++progress;
  }
  std::cout << std::endl << "[100%] Finished writing json" << std::endl;
}

} /* WhatsUpDoc */