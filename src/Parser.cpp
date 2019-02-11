#include "Parser.h"
#include "Helper.h"
#include "CommentParser.h"

#include <clang-c/Index.h>

#include <EScript/Utils/StringId.h>
#include <EScript/Utils/StringUtils.h>
#include <EScript/Utils/IO/IO.h>

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <deque>

//#define DEBUG 2

#if DEBUG >= 1
#define DEBUG1(s) std::cout << s << std::endl;
#else
#define DEBUG1(s)
#endif

#if DEBUG >= 2
#define DEBUG2(s) std::cout << s << std::endl;
#else
#define DEBUG2(s)
#endif

namespace WhatsUpDoc {
using namespace EScript;

struct Member {
  std::string name;
  enum {UNKNOWN, FUNCTION, CONST} kind = UNKNOWN;
  StringId compound;
  Location location;
  std::string description;
  std::string cppRef;
  std::string group;
  int minParams = 0;
  int maxParams = 0;
  bool deprecated = false;
};

struct Reference {
  std::string name;
  StringId compound;
  Location location;
  StringId ref;
};

struct InitCall {
  StringId id;
  StringId lib;
  StringId group;
};

struct Compound {
  StringId id;
  StringId refId;
  StringId parentId;
  StringId group;
  StringId base;
  std::string name;
  std::string fullname;
  std::string decription;
  Location location;
  enum {UNKNOWN, NAMESPACE, TYPE, GROUP} kind = UNKNOWN;
  std::vector<Member> member;
  std::vector<Reference> children;
  bool isNull() const { return id.empty(); }
  bool isRef() const { return !refId.empty(); }
};

struct InitFunction {
  StringId id;
  StringId paramId;
  StringId group;
  Location location;
};

struct ParsingContext {
  CXIndex index;
  CXTranslationUnit tu;
  InitFunction activeInit;
  StringId activeGroup;
  bool groupBlock = false;
  StringId activeMemberGroup;
  bool memberGroupBlock = false;
  bool deprecated = false; 
  //std::unordered_map<StringId, InitFunction> inits;
  std::unordered_map<StringId, std::string> names;
  std::deque<CommentTokenPtr> comments;
  std::unordered_map<StringId, Compound> compounds;
  std::unordered_map<StringId, InitCall> initCalls;
};

// -------------------------------------------------

void mergeCompounds(Compound& c1, Compound& c2, ParsingContext* context) {
  DEBUG1("  merge " << c1.id << " & " << c2.id)
  if(c1.id == c2.id) return;
  c2.refId = c1.id;
  
  if(c1.name.empty())
    c1.name = c2.name;
  c2.name.clear();
    
  if(c1.kind == Compound::UNKNOWN)
    c1.kind = c2.kind;
  c2.kind = Compound::UNKNOWN;
  
  if(c1.group.empty())
    c1.group = c2.group;
  
  if(c1.decription.empty())
    c1.decription = c2.decription;
  c2.decription.clear();
    
  if(c1.parentId.empty())
    c1.parentId = c2.parentId;    
    
  if(c1.base.empty())
    c1.base = c2.base;
    
  if(c1.location.file.empty())
    c1.location = c2.location;
      
  for(auto& v : c2.member)
    v.compound = c1.id;
  std::move(c2.member.begin(), c2.member.end(), std::back_inserter(c1.member));
  c2.member.clear();
  
  for(auto& v : c2.children) {
    v.compound = c1.id;
    auto& c = context->compounds[v.ref];
    c.parentId = c1.id;
  }
  std::move(c2.children.begin(), c2.children.end(), std::back_inserter(c1.children));
  c2.children.clear();
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
        auto comments = parseComment(comment, location);
        // merge consecutive comment blocks
        if(!context->comments.empty() && !comments.empty() && 
              context->comments.back()->getType() == CommentTokens::TCommentEnd::TYPE && 
              context->comments.back()->line == comments.front()->line)
          context->comments.pop_back();
        std::move(comments.begin(), comments.end(), back_inserter(context->comments));
      }
    }
  }
  clang_disposeTokens(tu, tokens, nTokens);
}

// -------------------------------------------------

Compound& getCompound(const StringId& id, ParsingContext* context) {
  static Compound nullCompound;
  if(id.empty())
    return nullCompound;
  auto* cmp = &context->compounds[id];
  while(cmp->isRef()) {
    cmp = &context->compounds[cmp->refId];
  }
  return *cmp;
}

// -------------------------------------------------

Compound& resolveCompound(CXCursor cursor, ParsingContext* context) {
  static Compound nullCompound;
  if(clang_Cursor_isNull(cursor))
    return nullCompound;
  cursor = findExposed(cursor);
  auto kind = clang_getCursorKind(cursor);
  
  DEBUG2("  resolve " << cursor)
  
  if(!hasType(cursor, "EScript::Type") && !hasType(cursor, "EScript::Namespace"))
    return nullCompound;
  
  DEBUG2("    cursor " << cursor)
  
  
  if(kind == CXCursor_CallExpr) {
    CXCursor ref = findTypeRef(cursor, "EScript::Type");
    if(!clang_Cursor_isNull(ref)) {
      cursor = ref;
      kind = clang_getCursorKind(cursor);
      DEBUG2("    ref " << cursor)
    }
  }
  
  if(kind == CXCursor_DeclRefExpr) {
    CXCursor ref = clang_getCursorReferenced(cursor);
    if(!clang_Cursor_isNull(ref)) {
      cursor = ref;
      kind = clang_getCursorKind(cursor);
      DEBUG2("    ref " << cursor)
    }
  }
  
  if(kind == CXCursor_VarDecl) {
    // search for a getTypeObject call or new Type
    auto newType = findCursor(cursor, "", CXCursor_CXXNewExpr, "EScript::Type");
    if(clang_Cursor_isNull(newType)) {
      CXCursor ref = getCursorRef(cursor);
      if(!clang_Cursor_isNull(ref)) {
        cursor = ref;
        kind = clang_getCursorKind(cursor);
        DEBUG2("    ref " << cursor)
      }
    }
  }
  StringId id = toString(clang_getCursorUSR(cursor));
  Location location = getCursorLocation(cursor);
  if(id.empty()) {
    CXCursor ref = getCursorRef(cursor);
    if(!clang_Cursor_isNull(ref)) {
      id = toString(clang_getCursorUSR(ref));
      location = getCursorLocation(ref);
    }
  }
  if(id == context->activeInit.paramId)
    id = context->activeInit.id;
  if(id.empty())
    return nullCompound;
  DEBUG2("    id " << id)
  
  auto& cmp = getCompound(id, context);
  if(cmp.isNull()) {
    cmp.id = id;    
    DEBUG2("    new " << cmp.id)
    cmp.location = location;
    if(hasType(cursor, "EScript::Namespace"))
      cmp.kind = Compound::NAMESPACE; 
    else if(hasType(cursor, "EScript::Type")) {
      cmp.kind = Compound::TYPE;
    }
  }
  DEBUG2("    resolved " << cmp.id)
  if(cmp.base.empty() && cmp.kind == Compound::TYPE) {
    // try to find base type
    auto newType = findCursor(cursor, "", CXCursor_CXXNewExpr, "EScript::Type", false);
    if(!clang_Cursor_isNull(newType)) {
      DEBUG2("  resolve base")
      auto baseCmp = resolveCompound(newType, context);
      if(!baseCmp.isNull())
        cmp.base = baseCmp.id;
    }
  }
  return cmp;
}

// -------------------------------------------------

std::string resolveComments(const Location& location, ParsingContext* context) {
  using namespace CommentTokens;
  std::string result;
  Location cloc{location.file,0,0};
  bool descriptionMode = false;
  
  if(!context->groupBlock)
    context->activeGroup = StringId();
  if(!context->memberGroupBlock)
    context->activeMemberGroup = StringId();
  context->deprecated = false;
  StringId defGrp;
  
  while(!context->comments.empty() && context->comments.front()->line <= location.line) {
    auto& token = context->comments.front();
    cloc.line = token->line;
    switch(token->getType()) {
      case TDefGroup::TYPE: {
        auto t = token->to<TDefGroup>();
        auto& grp = context->compounds[t->id];
        grp.id = t->id;
        grp.name = t->name;
        grp.kind = Compound::GROUP;
        grp.location = cloc;
        //context->activeGroup = grp.id;
        defGrp = grp.id;
        descriptionMode = true;
        break;
      }
      case TInGroup::TYPE: {
        auto t = token->to<TInGroup>();
        auto& grp = context->compounds[t->id];
        defGrp = StringId();
        grp.id = t->id;
        context->activeGroup = grp.id;
        break;
      }
      case TMemberGroup::TYPE: {
        auto t = token->to<TMemberGroup>();
        context->activeMemberGroup = t->id;
        break;
      }
      case TBlockStart::TYPE: {
        descriptionMode = false;
        if(!context->activeMemberGroup.empty()) {
          context->memberGroupBlock = true;
        } else if(!context->activeGroup.empty()) {
          context->groupBlock = true;
        } else if(!defGrp.empty()) {
          context->activeGroup = defGrp;
          context->groupBlock = true;
        } else {
          std::cerr << std::endl << "Invalid group start at " << cloc << "." << std::endl;
        }
        break;
      }
      case TBlockEnd::TYPE: {
        descriptionMode = false;
        if(!context->activeMemberGroup.empty()) {
          context->memberGroupBlock = false;
          context->activeMemberGroup = StringId();
        } else if(!context->activeGroup.empty()) {
          context->groupBlock = false;
          context->activeGroup = StringId();
        } else {
          std::cerr << std::endl << "Invalid group end at " << cloc << "." << std::endl;
        }
        break;
      }
      case TCommentEnd::TYPE: {
        defGrp = StringId();
        descriptionMode = false;
        break;
      }
      case TTextLine::TYPE: {
        auto t = token->to<TTextLine>();
        if(descriptionMode && !defGrp.empty()) {
          auto& grp = context->compounds[defGrp];
          if(!grp.decription.empty()) grp.decription += "<br/>";
          grp.decription += t->text;
        } else {
          if(!result.empty()) result += "<br/>";
          result += t->text;
        }
        break;
      }
      case TDeprecated::TYPE: {
        auto t = token->to<TDeprecated>();
        context->deprecated = true;
        if(!result.empty()) result += "<br/>";
        if(t->description.empty())
          result += "**Deprecated**";
        else
          result += "**Deprecated:** " + t->description;
        break;
      }
    }
    context->comments.pop_front();
  }
  return result;
}

// -------------------------------------------------

void handleDeclareFunction(CXCursor cursor, ParsingContext* context) {
  int argc = clang_Cursor_getNumArguments(cursor);
  auto location = getCursorLocation(cursor);
  Member fun;
  fun.location = location;
  fun.description = resolveComments(location, context);
  
  if(argc != 3 && argc != 5) {
    std::cerr << std::endl << "invalid function declaration at " << location << "." << std::endl;
    return;
  }
  
  fun.name = extractStringLiteral(clang_Cursor_getArgument(cursor, 1));
  DEBUG1("declare function " << fun.name << " @ " << location)
  
  CXCursor libArg = clang_Cursor_getArgument(cursor, 0);
  DEBUG2("  resolve lib")
  auto& cmp = resolveCompound(libArg, context);
  if(cmp.isNull()) {
    std::cerr << std::endl << "invalid function declaration at " << location << "." << std::endl;
    return;
  }
  fun.compound = cmp.id;
  fun.kind = Member::FUNCTION;
  if(argc == 5) {
    fun.minParams = extractIntLiteral(clang_Cursor_getArgument(cursor, 2));
    fun.maxParams = extractIntLiteral(clang_Cursor_getArgument(cursor, 3));
  }
  if(!context->activeMemberGroup.empty())
    fun.group = context->activeMemberGroup.toString();
  fun.deprecated = context->deprecated;
  
  // try to find corresponding c++ function
  CXCursor fnArg = getCursorRef(clang_Cursor_getArgument(cursor, argc == 5 ? 4 : 2));
  CXCursor fnRef = findRef(fnArg, fun.name);
  if(!clang_Cursor_isNull(fnRef))
    fun.cppRef = getFullyQualifiedName(fnRef);
  StringId grpId;
  
  StringId libId = toString(clang_getCursorUSR(libArg));
  if(!context->activeGroup.empty()) {
    grpId = context->activeGroup;
  } else if(libId == context->activeInit.paramId) {
    grpId = context->activeInit.group;
  }
  
  if(!grpId.empty()) {
    auto& grp = context->compounds[grpId];
    grp.member.emplace_back(fun);
  }
  cmp.member.emplace_back(std::move(fun));
}

// -------------------------------------------------

void handleDeclareConstant(CXCursor cursor, ParsingContext* context) {
  int argc = clang_Cursor_getNumArguments(cursor);
  auto location = getCursorLocation(cursor);
  if(argc != 3) return;
  std::string comment = resolveComments(location, context);
  
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
  DEBUG1("declare constant " << name << " @ " << location)
  
  CXCursor libArg = clang_Cursor_getArgument(cursor, 0);
  DEBUG2("  resolve lib ")
  auto& cmp = resolveCompound(libArg, context);
  if(cmp.isNull()) {
    std::cerr << std::endl << "invalid constant declaration at " << location << "." << std::endl;
    return;
  }
  StringId grpId;
  StringId libId = toString(clang_getCursorUSR(getCursorRef(libArg)));
  if(!context->activeGroup.empty()) {
    grpId = context->activeGroup;
  } else if(libId == context->activeInit.paramId) {
    grpId = context->activeInit.group;
  }
  
  DEBUG2("  resolve value ")
  auto& cmpRef = resolveCompound(clang_Cursor_getArgument(cursor, 2), context);
  if(!cmpRef.isNull()) {
    cmpRef.name = name;
    if(cmpRef.id == cmp.id)
      return;
    cmpRef.parentId = cmp.id;
    cmpRef.group = grpId;
    Reference attr{name, cmp.id, location, cmpRef.id};
    if(!cmpRef.group.empty()) {
      auto& grp = context->compounds[cmpRef.group];
      grp.children.emplace_back(attr);
    }
    cmp.children.emplace_back(std::move(attr));
  } else {
    Member attr;
    attr.name = name;
    attr.kind = Member::CONST;
    attr.compound = cmp.id;
    attr.location = location;
    attr.description = comment;
    attr.deprecated = context->deprecated;
    if(!context->activeMemberGroup.empty())
      attr.group = context->activeMemberGroup.toString();
    
    // try to find corresponding c++ object
    CXCursor objRef = findRef(clang_Cursor_getArgument(cursor, 2), name);
    if(!clang_Cursor_isNull(objRef))
      attr.cppRef = getFullyQualifiedName(objRef);
    
    if(!grpId.empty()) {
      auto& grp = context->compounds[grpId];
      grp.member.emplace_back(attr);
    } 
    cmp.member.emplace_back(std::move(attr));
  }
}

// -------------------------------------------------

void handleInitCall(CXCursor cursor, ParsingContext* context) {
  int argc = clang_Cursor_getNumArguments(cursor);
  if(argc != 1) return;
  auto location = getCursorLocation(cursor);
  DEBUG1("init call @ " << location)
  DEBUG2("  resolve lib ")
  auto& cmp = resolveCompound(clang_Cursor_getArgument(cursor, 0), context);
  DEBUG2("  resolve call ")
  CXCursor callRef = clang_getCursorReferenced(cursor);
  StringId callId = toString(clang_getCursorUSR(callRef));
  auto& initCmp = getCompound(callId, context);
  if(cmp.isNull() || callId.empty()) {
    std::cerr << std::endl << "invalid init call at " << location << "." << std::endl;
    return;
  }
  InitCall call;
  call.id = callId;
  call.lib = cmp.id;
  resolveComments(location, context);
  if(!context->activeGroup.empty()) {
    call.group = context->activeGroup;
    auto& grp = context->compounds[context->activeGroup];
    for(auto& ref : initCmp.children) {
      auto& c = context->compounds[ref.ref];
      if(c.group.empty()) {
        c.group = context->activeGroup;
        grp.children.push_back(ref);
      }
    }
  }
  mergeCompounds(cmp, initCmp, context);
  context->initCalls[call.id] = std::move(call);
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
      context->activeInit.id = toString(clang_getCursorUSR(cursor));
      context->activeInit.paramId = toString(clang_getCursorUSR(clang_Cursor_getArgument(cursor, 0)));
      context->activeInit.location = getCursorLocation(cursor);
      DEBUG2("init ")
      resolveCompound(clang_Cursor_getArgument(cursor, 0), context);
      auto& call = context->initCalls[context->activeInit.id];
      if(!call.group.empty()) {
        context->activeInit.group = call.group;
        //context->activeGroup = call.group;
        //context->groupBlock = true;
      } else {
        context->activeInit.group = StringId();
      }
      
      DEBUG1("init " << context->activeInit.id);
      extractComments(cursor, context);
      clang_visitChildren(cursor, *visitInitFunction, context);
          
      //for(auto& v : context->initCalls)
      //  std::cout << "  init " << v.first << " (" << v.second.lib << ") @ " << std::endl;
              
      context->comments.clear();
      context->activeGroup = StringId();
      context->groupBlock = false;
      context->deprecated = false;
      context->activeMemberGroup = StringId();
      context->memberGroupBlock = false;
    } else if(name == "getClassName") {
      auto clname = extractStringLiteral(cursor);
      StringId id = toString(clang_getCursorUSR(cursor));
      if(!clname.empty()) {
        context->names[id] = clname;
      }
    }
    return CXChildVisit_Continue;
  }
  return CXChildVisit_Recurse;
}

// ==============================================================================

Parser::Parser() : context(new ParsingContext) {
  context->index = clang_createIndex(1, 1);
  if(!context->index)
    throw std::runtime_error("error creating index");
}

Parser::~Parser() {
  clang_disposeIndex(context->index);
}

void Parser::addDefinition(const std::string& def) {
  include.emplace_back("-D" + def);
}

void Parser::addFlag(const std::string& flag) {
  include.emplace_back(flag);
}

void Parser::addInclude(const std::string& path) {
  include.emplace_back("-I" + path);
}

void Parser::parseFile(const std::string& filename) {
  // = { "-x", "c++", "-Wdocumentation", "-fparse-all-comments", "-Itest", "-Itest/EScript", "-Itest/E_Util" };
  std::vector<const char*> args;
  args.reserve(include.size() + 7);
  args.emplace_back("-x");
  args.emplace_back("c++");
  args.emplace_back("-std=c++11");
  //args.emplace_back("-Wdocumentation");
  args.emplace_back("-fparse-all-comments");
  args.emplace_back("-Wno-inconsistent-missing-override");
  #ifdef _WIN32
    args.emplace_back("--target=x86_64-w64-mingw32");
  #endif
  for(auto& s : include) 
    args.emplace_back(s.c_str());
  
  context->tu = clang_parseTranslationUnit(context->index, filename.c_str(), args.data(), args.size(), nullptr, 0, CXTranslationUnit_None);
  //CXTranslationUnit translationUnit = clang_parseTranslationUnit(index, 0, argv, argc, 0, 0, CXTranslationUnit_None);
  
  /*Compound* root = nullptr;
  for(auto& c : context->compounds) {
    if(cmp.parentId.empty()) {
      if(!root)
        root = &cmp;
      else
        mergeCompounds(*root, cmp, context.get());
    }
  }*/
  
  if(!context->tu) {
    std::cerr << std::endl << "error creating translationUnit" << std::endl;
    return;
  }
  
  DEBUG1(std::endl << "parsing " << filename);
  CXCursor rootCursor = clang_getTranslationUnitCursor(context->tu);  
  clang_visitChildren(rootCursor, *visitRoot, context.get());  
  clang_disposeTranslationUnit(context->tu);
  context->tu = nullptr;
}

void Parser::writeJSON(const std::string& path) const {
  using namespace EScript::StringUtils;
  
  size_t maxLength = 80;
  int progress = 0;
  
  // compute full names
  for(auto& c : context->compounds) {
    auto& cmp = c.second;
    if(cmp.isRef() || cmp.name.empty())
      continue;
    
    if(cmp.kind == Compound::GROUP) {
      if(!cmp.children.empty()) {
        cmp.parentId = getCompound(cmp.children.front().compound, context.get()).id;
      } else if(!cmp.member.empty()) {
        cmp.parentId = getCompound(cmp.member.front().compound, context.get()).id;
      }
    }
    
    cmp.fullname = cmp.name;
    auto pid = cmp.parentId;
    while(!pid.empty()) {
      auto& p = getCompound(pid, context.get());
      if(!p.name.empty())
        cmp.fullname = p.name + "." + cmp.fullname;
      pid = p.parentId;
    }
  }
  
  for(auto& c : context->compounds) {
    auto& cmp = c.second;
    if(cmp.isRef() || cmp.name.empty()) {
      ++progress;
      continue;
    }
    std::string kind = "unknown";
    switch(cmp.kind) {
      case Compound::NAMESPACE:
        kind = "namespace"; break;
      case Compound::TYPE:
        kind = "type"; break;
      case Compound::GROUP:
        kind = "group"; break;
      default: break;
    }
    
    std::stringstream json;
    std::string filename = kind + "_" + replaceAll(cmp.fullname, ".", "_") + ".json";
      
    int percent = static_cast<float>(progress)/context->compounds.size()*100;
    maxLength = std::max(maxLength, filename.size());
    std::cout << "\r[" << percent << "%] Writing " << filename << std::string(maxLength-filename.size(), ' ') << std::flush;
    
    auto parent = getCompound(cmp.parentId, context.get());
    auto group = getCompound(cmp.group, context.get());
    auto base = getCompound(cmp.base, context.get());
    
    json << "{" << std::endl;
    json << "  \"id\" : \"" << cmp.id << "\"," << std::endl;
    json << "  \"name\" : \"" << cmp.name << "\"," << std::endl;
    json << "  \"fullname\" : \"" << cmp.fullname << "\"," << std::endl;
    json << "  \"kind\" : \"" << kind << "\"," << std::endl;
    json << "  \"location\" : \"" << cmp.location << "\"," << std::endl;
    json << "  \"parent\" : \"" << (parent.name.empty() ? "" : cmp.parentId.toString()) << "\"," << std::endl;
    json << "  \"group\" : \"" << (group.name.empty() ? "" : cmp.group.toString()) << "\"," << std::endl;
    json << "  \"base\" : \"" << (base.name.empty() ? "" : cmp.base.toString()) << "\"," << std::endl;
    json << "  \"description\" : \"" << escape(cmp.decription) << "\"," << std::endl;    
    json << "  \"children\" : [" << std::endl;
    for(auto& v : cmp.children) {
      std::string fullname = getCompound(v.compound, context.get()).fullname + "." + v.name;
      json << "    {" << std::endl;
      json << "      \"name\" : \"" << v.name << "\"," << std::endl;
      json << "      \"fullname\" : \"" << fullname << "\"," << std::endl;
      json << "      \"ref\" : \"" << v.ref << "\"," << std::endl;
      json << "      \"location\" : \"" << v.location << "\"," << std::endl;
      json << "    }," << std::endl;
    }
    json << "  ]," << std::endl;
    
    json << "  \"member\" : [" << std::endl;
    for(auto& v : cmp.member) {      
      std::string kind = "unknown";
      switch(v.kind) {
        case Member::CONST:
          kind = "const"; break;
        case Member::FUNCTION:
          kind = "function"; break;
        default: break;
      }
      std::string fullname = getCompound(v.compound, context.get()).fullname + "." + v.name;
      json << "    {" << std::endl;
      json << "      \"name\" : \"" << v.name << "\"," << std::endl;
      json << "      \"fullname\" : \"" << fullname << "\"," << std::endl;
      json << "      \"kind\" : \"" << kind << "\"," << std::endl;
      json << "      \"minParams\" : " << v.minParams << "," <<std::endl;
      json << "      \"maxParams\" : " << v.maxParams << "," << std::endl;
      json << "      \"location\" : \"" << v.location << "\"," << std::endl;
      json << "      \"description\" : \"" << escape(v.description) << "\"," << std::endl;
      json << "      \"cpp\" : \"" << v.cppRef << "\"," << std::endl;
      json << "      \"group\" : \"" << v.group << "\"," << std::endl;
      json << "      \"deprecated\" : " << (v.deprecated ? "true" : "false") << "," << std::endl;
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