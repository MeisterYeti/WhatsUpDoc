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
  std::string name;
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
  //std::unordered_map<StringId, InitFunction> inits;
  std::unordered_map<StringId, std::string> names;
  std::deque<CommentTokenPtr> comments;
  std::unordered_map<StringId, Compound> compounds;
  std::unordered_map<StringId, InitCall> initCalls;
};

// -------------------------------------------------

void mergeCompounds(Compound& c1, Compound& c2, ParsingContext* context) {
  //std::cout << "  merge " << c1.id << " & " << c2.id << std::endl;
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

Compound& resolveCompound(CXCursor cursor, ParsingContext* context) {
  static Compound nullCompound;
  if(clang_Cursor_isNull(cursor) || (!hasType(cursor, "EScript::Namespace") && !hasType(cursor, "EScript::Type")))
    return nullCompound;
  StringId id = toString(clang_getCursorUSR(cursor));
  if(id == context->activeInit.paramId) {
    id = context->activeInit.id;
  } else {
    CXCursor ref = getCursorRef(cursor);
    if(!clang_Cursor_isNull(ref)) {
      id = toString(clang_getCursorUSR(ref));
    }
  }
  auto* cmp = &context->compounds[id];
  if(cmp->isNull()) {
    cmp->id = id;
    cmp->location = getCursorLocation(cursor);
    //std::cout << "  resolve " << cmp->id << std::endl;
    if(hasType(cursor, "EScript::Namespace"))
      cmp->kind = Compound::NAMESPACE; 
    else if(hasType(cursor, "EScript::Type"))
      cmp->kind = Compound::TYPE;
  } else {
    while(cmp->isRef())
      cmp = &context->compounds[cmp->refId];
    //std::cout << "  resolve ref " << cmp->id << std::endl;
  }
  return *cmp;
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
        if(grp.id.empty()) {
          std::cerr << std::endl << "Unknown group '" << t->id << "' at " << cloc << "." << std::endl;
        } else {
          context->activeGroup = grp.id;
        }
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
          if(!grp.decription.empty()) grp.decription += "\n";
          grp.decription += t->text;
        } else {
          if(!result.empty()) result += "\n";
          result += t->text;
        }
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
  CXCursor libRef = getCursorRef(clang_Cursor_getArgument(cursor, 0));
  auto& cmp = resolveCompound(libRef, context);
  if(cmp.isNull()) {
    std::cerr << std::endl << "invalid function declaration at " << location << "." << std::endl;
    return;
  }
  fun.name = extractStringLiteral(clang_Cursor_getArgument(cursor, 1));
  fun.compound = cmp.id;
  if(argc == 5) {
    fun.minParams = extractIntLiteral(clang_Cursor_getArgument(cursor, 2));
    fun.maxParams = extractIntLiteral(clang_Cursor_getArgument(cursor, 3));
  }
  if(!context->activeMemberGroup.empty())
    fun.group = context->activeMemberGroup.toString();
  
  // try to find corresponding c++ function
  CXCursor fnArg = getCursorRef(clang_Cursor_getArgument(cursor, argc == 5 ? 4 : 2));
  CXCursor fnRef = findCall(fnArg, fun.name);
  if(!clang_Cursor_isNull(fnRef))
    fun.cppRef = getFullyQualifiedName(fnRef);
  
  cmp.member.emplace_back(std::move(fun));
}

// -------------------------------------------------

void handleDeclareConstant(CXCursor cursor, ParsingContext* context) {
  int argc = clang_Cursor_getNumArguments(cursor);
  auto location = getCursorLocation(cursor);
  if(argc != 3) return;
  CXCursor libRef = getCursorRef(clang_Cursor_getArgument(cursor, 0));
  std::string comment = resolveComments(location, context);
  auto& cmp = resolveCompound(libRef, context);
  if(cmp.isNull()) {
    std::cerr << std::endl << "invalid constant declaration at " << location << "." << std::endl;
    return;
  }
  std::string name;
  StringId libId = toString(clang_getCursorUSR(libRef));
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
    Reference attr{name, cmp.id, location, cmpRef.id};
    if(!context->activeGroup.empty()) {
      cmpRef.group = context->activeGroup;
    } else if(libId == context->activeInit.paramId) {
      cmpRef.group = context->activeInit.group;
    }
    
    if(!cmpRef.group.empty()) {
      auto& grp = context->compounds[cmpRef.group];
      grp.children.push_back(attr);
    }
    cmp.children.emplace_back(std::move(attr));
  } else {
    Member attr;
    attr.name = name;
    attr.kind = Member::CONST;
    attr.compound = cmp.id;
    attr.location = location;
    attr.description = comment;
    if(!context->activeMemberGroup.empty())
      attr.group = context->activeMemberGroup.toString();
    
    // try to find corresponding c++ object
    CXCursor objRef = findCall(clang_Cursor_getArgument(cursor, 2), name);
    if(!clang_Cursor_isNull(objRef))
      attr.cppRef = getFullyQualifiedName(objRef);
    
    cmp.member.emplace_back(std::move(attr));
  }
}

// -------------------------------------------------

void handleInitCall(CXCursor cursor, ParsingContext* context) {
  CXCursor cursorRef = clang_getCursorReferenced(cursor);
  int argc = clang_Cursor_getNumArguments(cursor);
  if(argc != 1) return;
  auto location = getCursorLocation(cursor);
  CXCursor libRef = getCursorRef(clang_Cursor_getArgument(cursor, 0));
  auto& cmp = resolveCompound(libRef, context);
  auto& initCmp = resolveCompound(cursorRef, context);
  if(cmp.isNull() || initCmp.isNull()) {
    std::cerr << std::endl << "invalid init call at " << location << "." << std::endl;
    return;
  }
  InitCall call;
  call.id = initCmp.id;
  call.lib = cmp.id;
  resolveComments(location, context);
  if(!context->activeGroup.empty()) {
    call.group = context->activeGroup;
    auto& grp = context->compounds[context->activeGroup];
    for(auto& ref : initCmp.children) {
      auto& c = context->compounds[ref.ref];
      c.group = context->activeGroup;
      grp.children.push_back(ref);
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
      resolveCompound(clang_Cursor_getArgument(cursor, 0), context);
      auto& call = context->initCalls[context->activeInit.id];
      if(!call.group.empty()) {
        context->activeInit.group = call.group;
        //context->activeGroup = call.group;
        //context->groupBlock = true;
      } else {
        context->activeInit.group = StringId();
      }
      
      //std::cout << "init " << id << std::endl;
      extractComments(cursor, context);
      clang_visitChildren(cursor, *visitInitFunction, context);
          
      //for(auto& v : init.initCalls)
      //  std::cout << "  init " << v.call << " (" << v.lib << ") @ " << std::endl;
              
      context->comments.clear();
      context->activeGroup = StringId();
      context->groupBlock = false;
      context->activeMemberGroup = StringId();
      context->memberGroupBlock = false;
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
    if(c.second.isRef() || c.second.name.empty()) {
      ++progress;
      continue;
    }
    
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
      case Compound::GROUP:
        kind = "group"; break;
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
    json << "  \"kind\" : \"" << kind << "\"," << std::endl;
    json << "  \"location\" : \"" << c.second.location << "\"," << std::endl;
    if(!c.second.parentId.empty() && !context->compounds[c.second.parentId].name.empty())
      json << "  \"parent\" : \"" << c.second.parentId << "\"," << std::endl;
    else
      json << "  \"parent\" : \"\"," << std::endl;
        
    if(!c.second.group.empty() && !context->compounds[c.second.group].name.empty())
      json << "  \"group\" : \"" << c.second.group << "\"," << std::endl;
    else
      json << "  \"group\" : \"\"," << std::endl;      
    json << "  \"description\" : \"" << escape(c.second.decription) << "\"," << std::endl;
    
    json << "  \"children\" : [" << std::endl;
    for(auto& v : c.second.children) {
      json << "    {" << std::endl;
      json << "      \"name\" : \"" << v.name << "\"," << std::endl;
      json << "      \"ref\" : \"" << v.ref << "\"," << std::endl;
      json << "      \"location\" : \"" << v.location << "\"," << std::endl;
      json << "    }," << std::endl;
    }
    json << "  ]," << std::endl;
    
    json << "  \"member\" : [" << std::endl;
    for(auto& v : c.second.member) {
      json << "    {" << std::endl;
      json << "      \"name\" : \"" << v.name << "\"," << std::endl;
      json << "      \"minParams\" : " << v.minParams << "," <<std::endl;
      json << "      \"maxParams\" : " << v.maxParams << "," << std::endl;
      json << "      \"location\" : \"" << v.location << "\"," << std::endl;
      json << "      \"description\" : \"" << escape(v.description) << "\"," << std::endl;
      json << "      \"cpp\" : \"" << v.cppRef << "\"," << std::endl;
      json << "      \"group\" : \"" << v.group << "\"," << std::endl;
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