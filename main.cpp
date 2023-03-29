#include <fstream>
// Include fstream before Rinternals.h, otherwise a macro redef error might happen
#include "Rinternals.h"

#include "Rembedded.h"
#include "dirent.h"
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>


#include "utils/serializerData.h"
#include "utils/Debug.h"
// #include "utils/SerializedDataProcessor.h"
#include "utils/RshBuiltinsMap.h"
#include "utils/UMap.h"
#include "utils/deserializerData.h"

#include <iostream>
#include <stdlib.h>
#include <functional>
#include <sstream>
#include <algorithm>

#include "R/Protect.h"

#include "utils/SerializerAbstraction.h"
#include "utils/SlotSelection.h"

std::string outputPath;
std::string inputPath;

static int obapDEBUG = getenv("OBAP_DBG") ? std::stoi(getenv("OBAP_DBG")) : 0;
static bool completeSpeculativeContext = getenv("COMPLETE_CONTEXT") ? getenv("COMPLETE_CONTEXT")[0] == '1' : false;

static std::vector<SEXP> getDeserializerBinaries(rir::Protect & protecc, SEXP hast, std::unordered_map<SEXP, SpecializedBinaries> deserializerMap) {
  std::vector<SEXP> finalBinaries;
  
  for (auto & ele : deserializerMap) {
    SEXP offset = ele.first;
    auto specializedBinaries = ele.second;
    auto solutionMap = specializedBinaries.getSolution();

    for (auto & context : specializedBinaries.getContexts()) {
      std::vector<SBinary> binaries = specializedBinaries.getBinaries(context);
      auto soln = solutionMap[context];

      for (auto & binary : binaries) {
        auto currSpeculativeContext = binary.getSpeculativeContext();
        SEXP binStore;
        protecc(binStore = Rf_allocVector(VECSXP,rir::deserializerBinary::getContainerSize()));

        rir::deserializerBinary::addOffset(binStore, std::stoi(CHAR(PRINTNAME(offset))));
        rir::deserializerBinary::addContext(binStore, context.toI());
        rir::deserializerBinary::addDependencies(binStore, binary.getDependencies());
        rir::deserializerBinary::addEpoch(binStore, binary.getEpochAsSEXP());

        if (completeSpeculativeContext) {

          auto _container = currSpeculativeContext.getContainer();

          std::vector<SEXP> scVec;

          for (auto & ele : _container) {
            SEXP criteria = ele.first;
            std::vector<SCElement> currCriteriaFeedback = ele.second.getContainer();

            unsigned i = 0;
            for (auto & e : currCriteriaFeedback) {
              SEXP scContainer = e.getContainer();
              auto tag = rir::speculativeContextElement::getTag(scContainer);
              SEXP store;
              protecc(store = Rf_allocVector(VECSXP,rir::desSElement::getContainerSize()));
              rir::desSElement::addCriteria(store, criteria);
              rir::desSElement::addOffset(store, i++);
              rir::desSElement::addTag(store, tag);
              if (tag == 0 || tag == 1 || tag == 3 || tag == 4) {
                rir::desSElement::addVal(store, rir::speculativeContextElement::getValUint(scContainer));
              } else {
                rir::desSElement::addVal(store, rir::speculativeContextElement::getValSEXP(scContainer));
              }

              // std::cout << "==(desSElement): ";
              // desSElement::print(store, std::cout);
              // std::cout << std::endl;
              scVec.push_back(store);
            }
            
          }

          SEXP scStore;
          protecc(scStore = Rf_allocVector(VECSXP,scVec.size()));
          for (size_t i = 0; i < scVec.size(); i++) {
            SET_VECTOR_ELT(scStore, i, scVec[i]);
          }

          rir::deserializerBinary::addSpeculativeContext(binStore, scStore);
        } else if (soln.count(binary) > 0) {
          auto currSlotSelSolution = soln[binary].getSolution();
          std::vector<SEXP> scVec;

          for (auto & ele : currSlotSelSolution) {
            SEXP criteria = ele.first;
            auto solution = ele.second;
            auto fv = currSpeculativeContext.getVector(criteria);
            for (auto & s : solution) {
              SCElement e = fv.get(s);
              SEXP scContainer = e.getContainer();
              auto tag = rir::speculativeContextElement::getTag(scContainer);
              SEXP store;
              protecc(store = Rf_allocVector(VECSXP,rir::desSElement::getContainerSize()));
              rir::desSElement::addCriteria(store, criteria);
              rir::desSElement::addOffset(store, s);
              rir::desSElement::addTag(store, tag);
              if (tag == 0 || tag == 1 || tag == 3 || tag == 4) {
                rir::desSElement::addVal(store, rir::speculativeContextElement::getValUint(scContainer));
              } else {
                rir::desSElement::addVal(store, rir::speculativeContextElement::getValSEXP(scContainer));
              }

              // std::cout << "==(desSElement): ";
              // desSElement::print(store, std::cout);
              // std::cout << std::endl;
              scVec.push_back(store);
            }
          }
          SEXP scStore;
          protecc(scStore = Rf_allocVector(VECSXP,scVec.size()));
          for (size_t i = 0; i < scVec.size(); i++) {
            SET_VECTOR_ELT(scStore, i, scVec[i]);
          }

          rir::deserializerBinary::addSpeculativeContext(binStore, scStore);

        } else {
          auto fv = currSpeculativeContext.getVector(hast);
          if (fv.size() == 0) {
            rir::deserializerBinary::addSpeculativeContext(binStore, R_NilValue);
          } else {
            std::vector<SEXP> scVec;
            int s = fv.size() / 2;
            SCElement e = fv.get(s);
            SEXP scContainer = e.getContainer();
            auto tag = rir::speculativeContextElement::getTag(scContainer);
            SEXP store;
            protecc(store = Rf_allocVector(VECSXP,rir::desSElement::getContainerSize()));
            rir::desSElement::addCriteria(store, hast);
            rir::desSElement::addOffset(store, s);
            rir::desSElement::addTag(store, tag);
            if (tag == 0 || tag == 1 || tag == 3 || tag == 4) {
              rir::desSElement::addVal(store, rir::speculativeContextElement::getValUint(scContainer));
            } else {
              rir::desSElement::addVal(store, rir::speculativeContextElement::getValSEXP(scContainer));
            }
            // std::cout << "==(desSElement): ";
            // desSElement::print(store, std::cout);
            // std::cout << std::endl;
            scVec.push_back(store);
            SEXP scStore;
            protecc(scStore = Rf_allocVector(VECSXP,scVec.size()));
            for (size_t i = 0; i < scVec.size(); i++) {
              SET_VECTOR_ELT(scStore, i, scVec[i]);
            }

            rir::deserializerBinary::addSpeculativeContext(binStore, scStore);
          }
        }

        finalBinaries.push_back(binStore);
      }
    }
  }
  return finalBinaries;
}

static void saveDMetaAndCopyFiles(SEXP ddContainer) {
  std::stringstream outFilePath;

  outFilePath << outputPath << "/" << CHAR(PRINTNAME(rir::deserializerData::getHast(ddContainer))) << ".metad";

  FILE *fptr;
  fptr = fopen(outFilePath.str().c_str(),"w");
  if (!fptr) {
    for (int i = 0; i < 10; i++) {
      sleep(1);
      std::cout << "[W]waiting to open: " << outFilePath.str() << std::endl;
      fptr = fopen(outFilePath.str().c_str(),"w");
      if (fptr) break;
    }

    if (!fptr) {
      std::cout << "[W]unable to open " << outFilePath.str() << std::endl;
      Rf_error("[W]unable to open file!");
    }
  }
  
  R_SaveToFile(ddContainer, fptr, 0);
  fclose(fptr);

  SEXP hast = rir::deserializerData::getHast(ddContainer);
  SEXP binaries = rir::deserializerData::getBinaries(ddContainer);
  for (int i = 0; i < Rf_length(binaries); i++) {
    SEXP binary = VECTOR_ELT(binaries, i);
    // deserializerBinary::print(ele, std::cout, space + 2);
    std::stringstream binInPathPrefix, binOutPathPrefix;

    binInPathPrefix << inputPath << "/" << CHAR(PRINTNAME(hast)) 
                           << "_" << rir::deserializerBinary::getOffset(binary)
                           << "_" << CHAR(PRINTNAME(rir::deserializerBinary::getEpoch(binary)));

    binOutPathPrefix << outputPath  << "/" << CHAR(PRINTNAME(hast)) 
                           << "_" << rir::deserializerBinary::getOffset(binary)
                           << "_" << CHAR(PRINTNAME(rir::deserializerBinary::getEpoch(binary)));
    {
      // Copy BC
      std::ifstream  src(binInPathPrefix.str() + ".bc", std::ios::binary);
      std::ofstream  dst(binOutPathPrefix.str() + ".bc",   std::ios::binary);
      dst << src.rdbuf();
    }
    {
      // Copy pool
      std::ifstream  src(binInPathPrefix.str() + ".pool", std::ios::binary);
      std::ofstream  dst(binOutPathPrefix.str() + ".pool",   std::ios::binary); 
      dst << src.rdbuf();
    }
  }
}

static void addToRepository(SEXP serDataContainer, std::unordered_map<SEXP, SpecializedBinaries> deserializerMap) {
  rir::Protect protecc;
  SEXP hast = rir::serializerData::getHast(serDataContainer);
  std::vector<SEXP> bins = getDeserializerBinaries(protecc, hast, deserializerMap);
  SEXP store;
  protecc(store = Rf_allocVector(VECSXP,rir::deserializerData::getContainerSize()));
  rir::deserializerData::addHast(store, hast);
  rir::deserializerData::addBinaries(store, bins);
  rir::deserializerData::print(store, std::cout, 0);
  saveDMetaAndCopyFiles(store);
}

static void iterateOverMetadatasInDirectory() {
  DIR *dir;
  struct dirent *ent;  
  if ((dir = opendir(inputPath.c_str())) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      std::string fName = ent->d_name;
      if (fName.find(".meta") != std::string::npos) {
        std::stringstream metadataPath;
        metadataPath << inputPath << "/" << fName;

        FILE *reader;
        reader = fopen(metadataPath.str().c_str(),"r");

        if (!reader) {
          for (int i = 0; i < 10; i++) {
            sleep(1);
            reader = fopen(metadataPath.str().c_str(),"r");
            if (reader) break;
          }

          if (!reader) {
            std::cerr << "unable to open " << metadataPath.str() << std::endl;
            Rf_error("unable to open file!");
            continue;
          }
        }


        SEXP serDataContainer;
        rir::Protect protecc;
        protecc(serDataContainer= R_LoadFromFile(reader, 0));
        fclose(reader);

        // Get serialized metadata
        REnvHandler offsetMapHandler(rir::serializerData::getBitcodeMap(serDataContainer));
        SEXP hast = rir::serializerData::getHast(serDataContainer);

        rir::serializerData::print(serDataContainer, 0);

        std::unordered_map<SEXP, SpecializedBinaries> deserializerMap;

        offsetMapHandler.iterate([&] (SEXP offsetIndex, SEXP contextMap) {
          SpecializedBinaries sBins(contextMap, offsetIndex, hast, rir::serializerData::getName(serDataContainer));
          sBins.reduceRedundantBinaries();

          if (!completeSpeculativeContext) {
            for (auto & context : sBins.getContexts()) {
              if (sBins.getBinaries(context).size() == 1) continue;
              // printSpace(2);
              // std::cout << "█[SOLUTION]█══(context=" << context << ")" << std::endl;
              SlotSelectionGraph ssg(hast); // starting criteria is always the main 'Hast'
              ssg.addBinaries(sBins.getBinaries(context));
              ssg.init();
              // ssg.print(std::cout, 2);
              SlotSelectionSolution solnHolder;
              ssg.findSolution(&solnHolder, true);
              // solnHolder.print(std::cout, 4);
              sBins.addSolution(context, solnHolder.get());
            }
          }
          sBins.print(std::cout, 0);
          deserializerMap.emplace(offsetIndex, sBins);
        });

        addToRepository(serDataContainer, deserializerMap);
      }
    }
  } else {
    std::cerr << "\"" << inputPath << "\" has no metas, nothing to process" << std::endl;
    exit(EXIT_FAILURE);
  }
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
    std::cerr << "Usage: ./run.sh [path_to_folder] [output_folder]" << std::endl;
    exit(EXIT_FAILURE);
  }

  if (argc != 4) {
    std::cerr << "Usage: ./run.sh [path_to_folder] [output_folder]" << std::endl;
    exit(EXIT_FAILURE);
  }

  inputPath = argv[2];
  outputPath = argv[3];

  std::cout << "[OBAP Started]" << std::endl;
  std::cout << "Raw Bitcodes Folder       : " << inputPath << std::endl;
  std::cout << "Processed Bitcodes Folder : " << outputPath << std::endl;

  std::cout.setstate(std::ios_base::failbit);
  int fd = supress_stdout();
  int status = Rf_initEmbeddedR(argc, argv);
  if (status < 0) {
    std::cerr << "R initialization failed." << std::endl;
    exit(EXIT_FAILURE);
  }
  resume_stdout(fd);
  std::cout.clear();

  RshBuiltinWeights::init();

  iterateOverMetadatasInDirectory();

  Rf_endEmbeddedR(0);
  return 0;
}
