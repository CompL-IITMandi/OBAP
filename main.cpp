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
#include "utils/SerializedDataProcessor.h"
#include "utils/RshBuiltinsMap.h"
#include "utils/UMap.h"
#include "utils/deserializerData.h"

#include <iostream>
#include <stdlib.h>
#include <functional>
#include <sstream>

#include "R/Protect.h"

std::string outputPath;
std::string inputPath;

static void saveDMetaAndCopyFiles(SEXP ddContainer, const std::string & metaFilename) {
  std::stringstream outFilePath;

  outFilePath << outputPath << "/" << metaFilename << "d";

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

  // Copy all relavant binaries
  rir::deserializerData::iterateOverUnits(ddContainer, [&](SEXP ddContainer, SEXP offsetUnitContainer, SEXP contextUnitContainer, SEXP binaryUnitContainer) {
    std::stringstream binInPathPrefix, binOutPathPrefix;

    binInPathPrefix << inputPath << "/" << CHAR(PRINTNAME(rir::deserializerData::getHast(ddContainer))) 
                           << "_" << rir::offsetUnit::getOffsetIdxAsInt(offsetUnitContainer)
                           << "_" << CHAR(PRINTNAME(rir::binaryUnit::getEpoch(binaryUnitContainer)));

    binOutPathPrefix << outputPath  << "/" << CHAR(PRINTNAME(rir::deserializerData::getHast(ddContainer))) 
                           << "_" << rir::offsetUnit::getOffsetIdxAsInt(offsetUnitContainer)
                           << "_" << CHAR(PRINTNAME(rir::binaryUnit::getEpoch(binaryUnitContainer)));

    {
      // Copy BC
      std::ifstream  src(binInPathPrefix.str() + ".bc", std::ios::binary);
      std::ofstream  dst(binOutPathPrefix.str() + ".bc",   std::ios::binary);

      dst << src.rdbuf();
    }

    {
      // Copy BC
      std::ifstream  src(binInPathPrefix.str() + ".pool", std::ios::binary);
      std::ofstream  dst(binOutPathPrefix.str() + ".pool",   std::ios::binary);
      
      dst << src.rdbuf();
    }

  });
}

static void testSavedDMeta(const std::string & metaFilename) {
  std::stringstream path;
  path << outputPath << "/" << metaFilename << "d";

  FILE *reader;
  reader = fopen(path.str().c_str(),"r");

  if (!reader) {
    for (int i = 0; i < 10; i++) {
      sleep(1);
      reader = fopen(path.str().c_str(),"r");
      if (reader) break;
    }

    if (!reader) {
      std::cerr << "unable to open " << path.str() << std::endl;
      Rf_error("unable to open file!");
    }
  }

  // Initialize the deserializing stream
  R_inpstream_st inputStream;
  R_InitFileInPStream(&inputStream, reader, R_pstream_binary_format, NULL, R_NilValue);

  rir::Protect protecc;
  SEXP ddContainer;
  protecc(ddContainer= R_LoadFromFile(reader, 0));
  fclose(reader);

  // rir::deserializerData::print(ddContainer, 2);
}

static void iterateOverMetadatasInDirectory() {
  DIR *dir;
  struct dirent *ent;

  size_t functionsSeen = 0;
  size_t functionsWithMoreThanOneContext = 0, functionsMasked = 0;
  
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

        // printSpace(0);
        // std::cout << "Processing: " << fName << std::endl;
        // printSpace(2);
        // std::cout << "FunctionName: " << CHAR(PRINTNAME(rir::serializerData::getName(serDataContainer))) << std::endl;

        // rir::serializerData::print(serDataContainer, 0);

        // Get serialized metadata
        REnvHandler offsetMapHandler(rir::serializerData::getBitcodeMap(serDataContainer));

        int numOffsets = offsetMapHandler.size();
        SEXP ddContainer;
        protecc(ddContainer = Rf_allocVector(VECSXP, rir::deserializerData::getContainerSize(numOffsets)));
        rir::deserializerData::addHast(ddContainer, rir::serializerData::getHast(serDataContainer));
                
        int ddIdx = rir::deserializerData::offsetsStartingIndex();
        offsetMapHandler.iterate([&] (SEXP offsetIndex, SEXP contextMap) {
          functionsSeen++;
          
          // printSpace(4);
          // std::cout << "Offset: " << CHAR(PRINTNAME(offsetIndex)) << std::endl;

          std::stringstream pathPrefix;
          pathPrefix << inputPath << "/" << CHAR(PRINTNAME(rir::serializerData::getHast(serDataContainer))) << "_" << CHAR(PRINTNAME(offsetIndex)) << "_";

          SerializedDataProcessor p(contextMap, pathPrefix.str());
          p.init();
          // p.print(6);

          if (p.getOrigContextsCount() > 1) {
            functionsWithMoreThanOneContext++;
          }
          

          // 
          // Each deserializer data i.e. the parent container vector, contains 'n' unique offsets.
          //  These offsets are themselves a vector called as offset unit
          //    Inside each offset unit there are context units
          //      Inside each context unit there are binary units
          // 

          // 
          // Create a offset unit that contains the desired number of contexts
          // 
          SEXP ouContainer;
          rir::Protect protecc1;
          protecc1(ouContainer = Rf_allocVector(VECSXP, rir::offsetUnit::getContainerSize(p.getNumContexts())));
          
          // 
          // Add Offset idx
          // 
          rir::offsetUnit::addOffsetIdx(ouContainer, std::stoi(CHAR(PRINTNAME(offsetIndex))));
          
          // 
          // Add mask
          // 
          rir::offsetUnit::addMask(ouContainer, p.getMask().toI());

          if (p.getMask().toI() != 0) {
            functionsMasked++;
          }

          // 
          // Populate the context units in the relavant offsets inside the offset unit
          // 
          p.populateOffsetUnit(ouContainer);

          // 
          // Add the offset unit to the deserializer data.
          // 
          rir::generalUtil::addSEXP(ddContainer, ouContainer, ddIdx);

          ddIdx++;
        });
        
        // rir::deserializerData::print(ddContainer, 2);
        saveDMetaAndCopyFiles(ddContainer, fName);
        // testSavedDMeta(fName);

      }
    }

    SerializedDataProcessor::printStats(0);

    printSpace(0);
    std::cout << "Functions Unique          : " << functionsSeen << std::endl;
    
    printSpace(0);
    std::cout << "Functions (>1) Contexts   : " << functionsWithMoreThanOneContext << std::endl;
    printSpace(0);
    std::cout << "Functions Masked          : " << functionsMasked << std::endl;
  
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
