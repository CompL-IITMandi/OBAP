#include <iostream>
#include <stdlib.h>
#include <functional>
#include <fstream>

#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Support/raw_ostream.h"

#include "rir/Context.h"
#include "nlohmann/json.hpp"
#include "opt/ModuleManager.h"
#include "utils/hasse.h"

using json = nlohmann::json;

#define PRINT_PROGRESS 1


int main(int argc, char** argv) {
  std::cout << "argc: " << argc << std::endl;
  if (argc != 3) {
    std::cerr << "Error: No json file passed for anaylsis." << std::endl;
    std::cerr << "Usage: bcp test.json bitcodeFoldersPath" << std::endl;
    exit(EXIT_FAILURE);
  }

  auto jsonFileLocation = argv[1];
  auto bitcodesFolder = argv[2];


  std::ifstream stream(jsonFileLocation);
  if (!stream) {
    std::cerr << "Unable to open json at: " << jsonFileLocation << std::endl;
    exit(EXIT_FAILURE);
  }

  

  // Load the processed JSON data
  // We converted the .meta files to JSON to make it easier to work with and 
  // end dependence on GNUR for doing other things.
  json processedJson;
  stream >> processedJson;

  for (json::iterator it = processedJson.begin(); it != processedJson.end(); ++it) {
    auto currFolder = it.key();

    #if PRINT_PROGRESS == 1
    std::cout << "Reading Folder: " << currFolder << std::endl;
    #endif

    auto metadataFile = it.value();
    for (json::iterator it_metas = metadataFile.begin(); it_metas != metadataFile.end(); ++it_metas) {
      auto currHastJson = it_metas.value();

      #if PRINT_PROGRESS == 1
      std::cout << "  -- " << it_metas.key() << std::endl;
      std::cout << "     hast: " << currHastJson["hast"].get<std::string>() << std::endl;
      #endif

      auto offsetsAvailable = currHastJson["offsetMap"];
      #if PRINT_PROGRESS == 1
      std::cout << "     offsets:" << std::endl;
      #endif

      for (json::iterator currOffset = offsetsAvailable.begin(); currOffset != offsetsAvailable.end(); ++currOffset) {
        #if PRINT_PROGRESS == 1
        std::cout << "     -- " << currOffset.key() << std::endl;
        #endif

        std::vector<Context> contextsVec;

        auto contextsAvailable = currOffset.value();
        if (contextsAvailable.size() == 1) {
          continue;
        }
        for (json::iterator currContext = contextsAvailable.begin(); currContext != contextsAvailable.end(); ++currContext) {
          std::string conStr = currContext.value()["context"];
          unsigned long con = std::stoul(conStr);
          Context c(con);

          #if PRINT_PROGRESS == 1
          std::cout << "       -- (" << con << ") " << c << std::endl;
          #endif

          contextsVec.push_back(c);
          
          // Context c1(15);
          // std::cout << "       c: " << c << std::endl;
          // std::cout << "       c1: " << c1 << std::endl;
          // std::cout << "          c.smaller(c1)" << c.smaller(c1) << std::endl;
          // std::cout << "          c1.smaller(c)" << c1.smaller(c) << std::endl;
          // std::cout << "          c1.isImproving(c)" << c1.isImproving(c) << std::endl;
          
          
          std::stringstream bitcodePath;
          bitcodePath << bitcodesFolder << "/" << currFolder << "/" << currHastJson["hast"].get<std::string>() << "_" << currOffset.key() << "_" << conStr << ".bc";

          // load bitcode
          llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> mb = llvm::MemoryBuffer::getFile(bitcodePath.str().c_str());

          llvm::orc::ThreadSafeContext TSC = std::make_unique<llvm::LLVMContext>();

          // Load the memory buffer into a LLVM module
          llvm::Expected<std::unique_ptr<llvm::Module>> llModuleHolder = llvm::parseBitcodeFile(mb->get()->getMemBufferRef(), *TSC.getContext());

          // ModuleManager to run our passes on the bitcodes
          ModuleManager MM;

          MM.runPasses(*llModuleHolder.get().get());

        }

        // for (auto & ele : contextsVec) {
        //   for (auto & other : contextsVec) {
        //     if (ele == other) continue;
        //     std::cout << "c1: " << ele << std::endl;
        //     std::cout << "c2: " << other << std::endl;
        //     std::cout << "c1.smaller(c2): " << ele.smaller(other) << std::endl;
        //     std::cout << "c2.smaller(c1): " << other.smaller(ele) << std::endl;
        //   }
        // }
      }

    }
  }
  return 0;
}
