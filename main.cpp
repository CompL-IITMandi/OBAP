#include <iostream>
#include <stdlib.h>
#include <functional>
#include <fstream>
#include <algorithm>
#include <set>
#include <iterator>

#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Support/raw_ostream.h"

#include "runtime/Context.h"
#include "opt/ModuleManager.h"
#include "utils/hasse.h"

#include "utils/RshBuiltinsMap.h"
#include "utils/iter.h"
#include "utils/ContextAnalysisComparison.h"

#include "utils/serializerData.h"
#include "Rembedded.h"
#include "dirent.h"
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>

#include "Rinternals.h"

std::unordered_map<std::string, unsigned> RshBuiltinWeights::weightMap;

static void iterateOverMetadatasInDirectory(const char * folderPath) {
  static SEXP maskSym = Rf_install("mask");
  DIR *dir;
  struct dirent *ent;
  int i = 0;
  unsigned funCount = 0;
  unsigned masked = 0;
  unsigned removed = 0;
  unsigned totalContexts = 0;

  unsigned strictComparisons = 0;
  unsigned roughEQComparisons = 0;
  unsigned roughNEQComparisons = 0;

  std::ofstream maskDataStream(std::string(folderPath) + "/maskData");
  
  if ((dir = opendir(folderPath)) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      std::string fName = ent->d_name;
      if (fName.find(".meta") != std::string::npos) {
        // std::cout << "Processing " << fName << std::endl;
        i++;

        std::stringstream metadataPath;
        metadataPath << folderPath << "/" << fName;

        FILE *reader;
        reader = fopen(metadataPath.str().c_str(),"r");

        if (!reader) {
          for (int i = 0; i < 10; i++) {
            sleep(1);
            // std::cout << "waiting to open: " << metadataPath.str() << std::endl;
            reader = fopen(metadataPath.str().c_str(),"r");
            if (reader) break;
          }

          if (!reader) {
            std::cout << "unable to open " << metadataPath.str() << std::endl;
            Rf_error("unable to open file!");
            continue;
          }
        }

        // Initialize the deserializing stream
        R_inpstream_st inputStream;
        R_InitFileInPStream(&inputStream, reader, R_pstream_binary_format, NULL, R_NilValue);

        SEXP serDataContainer;
        PROTECT(serDataContainer = R_Unserialize(&inputStream));

        fclose(reader);

        // Get serialized metadata
        REnvHandler offsetMapHandler(rir::serializerData::getBitcodeMap(serDataContainer));

        offsetMapHandler.iterate([&] (SEXP offsetIndex, SEXP contextMap) {
          rir::Context mask;
          rir::Context oldMask;
          REnvHandler contextMapHandler(contextMap);
          if (SEXP existingMaskContainer = contextMapHandler.get(maskSym)){
            mask = oldMask = rir::Context(*((unsigned long *) DATAPTR(existingMaskContainer)));
          }

          maskDataStream << CHAR(PRINTNAME(rir::serializerData::getHast(serDataContainer))) << "_" << CHAR(PRINTNAME(offsetIndex)) << " ";


          // We are now processing a function, non zero index indicates an inner function
          std::set<unsigned long> toRemove;

          std::stringstream pathPrefix;
          pathPrefix << folderPath << "/" << CHAR(PRINTNAME(rir::serializerData::getHast(serDataContainer))) << "_" << CHAR(PRINTNAME(offsetIndex)) << "_";

          

          doAnalysisOverContexts(
            pathPrefix.str(),
            contextMap,
            [&] (
              std::vector<rir::Context> & contextsVec, 
              std::unordered_map<rir::Context, unsigned> & weightAnalysis, 
              std::unordered_map<rir::Context, std::vector<std::pair<unsigned, std::vector<std::string>>> > & simpleArgumentAnalysis,
              std::unordered_map<rir::Context, std::vector<std::set<std::string>>> & funCallBFData
            )  {

              if (contextsVec.size() > 1) {
                funCount++;
                totalContexts += contextsVec.size();
              }

              compareContexts(contextsVec, [&] (rir::Context & c1, rir::Context & c2, const ComparisonType & t) {
                // C1 -- Less specialized
                // C2 -- More specialized
                ContextAnalysisComparison cac(c1, c2, t);
                auto currMask = cac.getMask(weightAnalysis, simpleArgumentAnalysis, funCallBFData);
                


                if (currMask.toI() > 0) {
                  if (t == ComparisonType::STRICT) {
                    strictComparisons++;
                  } else if (t == ComparisonType::ROUGH_EQ) {
                    roughEQComparisons++;
                  } else if (t == ComparisonType::ROUGH_NEQ) {
                    roughNEQComparisons++;
                  }
                  if (cac.safeToRemoveContext(currMask)) {
                    toRemove.insert(c2.toI());
                  }
                }

                mask = mask + currMask;
              });
            }
          );

          removed += toRemove.size();

          
          unsigned numContexts = contextMapHandler.size();
          if (contextMapHandler.get(maskSym)){
            numContexts--;
          }

          // std::cout << "  [" << CHAR(PRINTNAME(offsetIndex)) << "] (" << numContexts << ")" << std::endl;
          
          maskDataStream << mask.toI() << " ";
          if (oldMask != mask) {
            masked++;
            // std::cout << "    [NEW MASK]: " << mask << std::endl;
            // std::cout << "    [OLD MASK]: " << oldMask << std::endl;
          } else {
            // if (oldMask.toI() != 0)
            // std::cout << "    [MASK]: " << oldMask << std::endl;
          }
          

          // std::cout << "    [DEPRECATED CONTEXTS]: [";

          for (auto & ele : toRemove) {
            // std::cout << ele;
            maskDataStream << ele << " ";
            
          }
          // std::cout << "]" << std::endl;

          maskDataStream << std::endl;
          

        });


        // R_outpstream_st outputStream;
        // FILE *fptr;
        // fptr = fopen(metadataPath.str().c_str(),"w");
        // if (!fptr) {
        //   for (int i = 0; i < 10; i++) {
        //     sleep(1);
        //     std::cout << "[W]waiting to open: " << metadataPath.str() << std::endl;
        //     fptr = fopen(metadataPath.str().c_str(),"w");
        //     if (fptr) break;
        //   }

        //   if (!fptr) {
        //     std::cout << "[W]unable to open " << metadataPath.str() << std::endl;
        //     Rf_error("[W]unable to open file!");
        //     continue;
        //   }
        // }
        
        // R_InitFileOutPStream(&outputStream,fptr,R_pstream_binary_format, 0, NULL, R_NilValue);
        // R_Serialize(serDataContainer, &outputStream);
        // fclose(fptr);

        UNPROTECT(1);
      }
    }
  } else {
    std::cout << "\"" << folderPath << "\" has no metas" << std::endl;
  }

  maskDataStream.close();

  std::cout << "=== SUMMARY ===" << std::endl;
  std::cout << "Strict Comparisons: " << strictComparisons << std::endl;
  std::cout << "Rough EQ Comparisons: " << roughEQComparisons << std::endl;
  std::cout << "Rough NEQ Comparisons: " << roughNEQComparisons << std::endl;
  // std::cout << "Diff zero miss Comparisons: " << diffComparisonsMissZero << std::endl;
  // std::cout << "Diff same miss Comparisons: " << diffComparisonsMissSame << std::endl;
  // std::cout << "Diff diff miss Comparisons: " << diffComparisonsMissDiff << std::endl;

  std::cout << "Total functions processed: " << funCount << std::endl;
  std::cout << "Functions masked: " << masked << std::endl;
  std::cout << "Bitcodes processed for functions (where contexts > 1): " << totalContexts << std::endl;
  std::cout << "Bitcodes removed: " << removed << std::endl;
  std::cout << "Bitcodes folder: " << folderPath << std::endl;

}

// https://stackoverflow.com/questions/46728680/how-to-temporarily-suppress-output-from-printf
int supress_stdout() {
  fflush(stdout);

  int ret = dup(1);
  int nullfd = open("/dev/null", O_WRONLY);
  // check nullfd for error omitted
  dup2(nullfd, 1);
  close(nullfd);

  return ret;
}

void resume_stdout(int fd) {
  fflush(stdout);
  dup2(fd, 1);
  close(fd);
}


int main(int argc, char** argv) {

  bool rHomeSet = getenv("R_HOME") ? true : false;

  if (!rHomeSet) {
    std::cerr << "R_HOME not set, set this to the GNUR built using the ./configure --enable-R-shlib flag" << std::endl;
    std::cerr << "Usage: R_HOME=PATH_TO_GNUR_BUILD bcp path_to_folder" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout.setstate(std::ios_base::failbit);
  int fd = supress_stdout();
  int status = Rf_initEmbeddedR(argc, argv);
  if (status < 0) {
    std::cerr << "R initialization failed." << std::endl;
    exit(EXIT_FAILURE);
  }
  resume_stdout(fd);
  std::cout.clear();

  if (argc != 2) {
    std::cerr << "Usage: bcp path_to_folder" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cerr << "R initialization successful!" << std::endl;

  auto bitcodesFolder = argv[1];
  GlobalData::bitcodesFolder = bitcodesFolder;

  RshBuiltinWeights::init();

  iterateOverMetadatasInDirectory(bitcodesFolder);

  Rf_endEmbeddedR(0);
  return 0;
}
