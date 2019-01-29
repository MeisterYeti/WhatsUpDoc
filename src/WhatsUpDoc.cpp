#include "Parser.h"
#include "Helper.h"
#include <EScript/Utils/IO/IO.h>
#include <EScript/Utils/StringUtils.h>
#include <iostream>
#include <cmath>
#include <regex>

using namespace WhatsUpDoc;
using namespace EScript;

int main(int argc, const char * argv[]) {
  if(argc != 2) {
    std::cout << "usage: WhatsUpDoc <DocFile>" << std::endl;
    return 0;
  }
  
  // parse config file
  if(IO::getEntryType(argv[1]) != IO::TYPE_FILE) {
    std::cerr << "config file '" <<  argv[1]<< "' not found." << std::endl;
    return 1;
  }
  std::string projectFolder = ".";
  std::string outputFolder = "json";
  std::vector<std::string> includes;
  std::vector<std::string> input;
  std::vector<std::string> defines;
  std::vector<std::string> flags;
  
  auto configLines = StringUtils::split(IO::loadFile(argv[1]).str(), "\n");
  int lineNr = 0;
  for(auto& line : configLines) {
    lineNr++;
    line = StringUtils::trim(line);
    if(line.empty() || line[0] == '#')
      continue;
    
    auto entry = StringUtils::split(line, "=");
    if(entry.size() != 2) {
      std::cerr << "invalid entry in config file at line " << lineNr << std::endl;
      return 1;
    }
    
    auto key = StringUtils::trim(entry[0]);
    auto value = StringUtils::trim(entry[1]);
    if(key == "PROJECT_FOLDER") {
      projectFolder = IO::condensePath(value);
    } else if(key == "OUTPUT_DIRECTORY") {
      outputFolder = value;
    } else if(key == "INPUT") {
      for(auto& v : StringUtils::split(value, " ")) {
        v = StringUtils::trim(v);
        if(!v.empty()) {
          input.emplace_back(v);
          includes.emplace_back(v);
        }
      }
    } else if(key == "INCLUDE") {
      for(auto& v : StringUtils::split(value, " ")) {
        v = StringUtils::trim(v);
        if(!v.empty())
          includes.emplace_back(v);
      }
    } else if(key == "PREDEFINED") {
      for(auto& v : StringUtils::split(value, " ")) {
        v = StringUtils::trim(v);
        if(!v.empty())
          defines.emplace_back(v);
      }
    } else if(key == "FLAGS") {
    for(auto& v : StringUtils::split(value, " ")) {
      v = StringUtils::trim(v);
      if(!v.empty())
        flags.emplace_back(v);
    }
  }
  }
  
  if(IO::getEntryType(projectFolder) != IO::TYPE_DIRECTORY) {
    std::cerr << "invalid project folder '" << projectFolder << "'." << std::endl;
    return 1;
  }
  
  outputFolder = IO::condensePath(projectFolder.empty() ? outputFolder : (projectFolder + "/" + outputFolder));
  if(IO::getEntryType(outputFolder) != IO::TYPE_DIRECTORY) {
    std::cerr << "invalid output folder '" << outputFolder << "'." << std::endl;
    return 1;
  }
    
  Parser parser;  
  parser.addInclude(projectFolder);
  for(auto& inc : includes) {
    inc = IO::condensePath(projectFolder.empty() ? inc : (projectFolder + "/" + inc));
    if(IO::getEntryType(inc) == IO::TYPE_DIRECTORY)
      parser.addInclude(inc);
    else
      std::cerr << "invalid include dir '" << inc << "'." << std::endl;
  }
  
  for(auto& def : defines)
    parser.addDefinition(def);
    
  for(auto& flag : flags)
    parser.addFlag(flag);
  
  std::vector<std::string> cppfiles;
  size_t maxLength = 1;
  
  if(input.empty()) input.emplace_back("");
  for(auto& path : input) {
    path = IO::condensePath(projectFolder.empty() ? path : (projectFolder + "/" + path));
    if(IO::getEntryType(path) != IO::TYPE_DIRECTORY) {
      std::cerr << "invalid input dir '" << path << "'." << std::endl;
      continue;
    }
    auto files = IO::getFilesInDir(IO::condensePath(path), 4|1);
    for(auto& f : files) {
      if(f.compare(f.size()-4, 4, ".cpp") == 0) {
        cppfiles.emplace_back(f);
        maxLength = std::max(maxLength, f.size());
      }
    }
  }
  
  int progress = 0;
  for(auto& f : cppfiles) {
    int percent = static_cast<float>(progress)/cppfiles.size()*100;
    std::cout << "\r[" << percent << "%] Parsing " << f << std::string(maxLength-f.size(), ' ') << std::flush;
    parser.parseFile(f);
    ++progress;
  }
  std::cout << std::endl << "[100%] Finished parsing" << std::endl;
  
  parser.writeJSON(outputFolder);
  return 0;
}