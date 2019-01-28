#include "Parser.h"
#include <EScript/Utils/IO/IO.h>
#include <iostream>
using namespace WhatsUpDoc;
using namespace EScript;

int main(int argc, const char * argv[]) {
  Parser parser;
  
  parser.addInclude("test/API");
  parser.addInclude("test/API/EScript");
  parser.addInclude("test/API/E_Util");
  parser.addInclude("test/API/E_Rendering");
  
  auto files = IO::getFilesInDir("test/API/EScript", 4|1);
  std::vector<std::string> cppfiles;
  for(auto& f : files) {
    if(f.compare(f.size()-4, 4, ".cpp") == 0)
      cppfiles.emplace_back(f);
  }
  
  int progress = 0;
  for(auto& f : cppfiles) {
    int percent = static_cast<float>(progress)/cppfiles.size()*100;
    std::cout << "[" << percent << "%] Parsing " << f << std::endl;
    parser.parseFile(f);
    ++progress;
  }
  std::cout << "Finished!" << std::endl;
  
  parser.parseFile("test/API/E_Util/Graphics/E_Bitmap.cpp");
  parser.parseFile("test/API/E_Util/ELibUtil.cpp");
  parser.parseFile("test/API/E_Rendering/MeshUtils/E_MeshBuilder.cpp");
  parser.parseFile("test/API/E_Rendering/MeshUtils/E_PrimitiveShapes.cpp");
  parser.parseFile("test/API/E_Rendering/ELibRendering.cpp");
  
  parser.writeJSON("test/json");
  return 0;
}