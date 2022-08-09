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

std::string outputPath;
std::string inputPath;

static void saveDMetaAndCopyFiles(SEXP ddContainer, const std::string & metaFilename) {
  std::stringstream outFilePath;

  outFilePath << outputPath << "/" << metaFilename;

  R_outpstream_st outputStream;
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
  
  R_InitFileOutPStream(&outputStream,fptr,R_pstream_binary_format, 0, NULL, R_NilValue);
  R_Serialize(ddContainer, &outputStream);
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
  path << outputPath << "/" << metaFilename;

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

  SEXP ddContainer;
  PROTECT(ddContainer = R_Unserialize(&inputStream));

  // rir::deserializerData::print(ddContainer, 2);

  UNPROTECT(1);
}

static void iterateOverMetadatasInDirectory(const char * folderPath) {
  DIR *dir;
  struct dirent *ent;
  
  if ((dir = opendir(folderPath)) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      std::string fName = ent->d_name;
      if (fName.find(".meta") != std::string::npos) {

        std::stringstream metadataPath;
        metadataPath << folderPath << "/" << fName;

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

        // Initialize the deserializing stream
        R_inpstream_st inputStream;
        R_InitFileInPStream(&inputStream, reader, R_pstream_binary_format, NULL, R_NilValue);

        SEXP serDataContainer;
        PROTECT(serDataContainer = R_Unserialize(&inputStream));

        unsigned int protecc = 0;
        // rir::serializerData::recursivelyProtect(serDataContainer);

        printSpace(0);
        std::cout << "Processing: " << fName << std::endl;
        printSpace(2);
        std::cout << "FunctionName: " << CHAR(PRINTNAME(rir::serializerData::getName(serDataContainer))) << std::endl;

        fclose(reader);

        // Get serialized metadata
        REnvHandler offsetMapHandler(rir::serializerData::getBitcodeMap(serDataContainer));

        int numOffsets = offsetMapHandler.size();
        SEXP ddContainer;
        PROTECT(ddContainer = Rf_allocVector(VECSXP, rir::deserializerData::getContainerSize(numOffsets)));
        rir::deserializerData::addHast(ddContainer, rir::serializerData::getHast(serDataContainer));
                
        int ddIdx = rir::deserializerData::offsetsStartingIndex();
        offsetMapHandler.iterate([&] (SEXP offsetIndex, SEXP contextMap) {          
          
          printSpace(4);
          std::cout << "Offset: " << CHAR(PRINTNAME(offsetIndex)) << std::endl;

          std::stringstream pathPrefix;
          pathPrefix << folderPath << "/" << CHAR(PRINTNAME(rir::serializerData::getHast(serDataContainer))) << "_" << CHAR(PRINTNAME(offsetIndex)) << "_";

          SerializedDataProcessor p(contextMap, pathPrefix.str());
          p.init();
          p.print(6);
          
          SEXP ouContainer;
          PROTECT(ouContainer = Rf_allocVector(VECSXP, rir::offsetUnit::getContainerSize(p.getNumContexts())));
          rir::offsetUnit::addOffsetIdx(ouContainer, std::stoi(CHAR(PRINTNAME(offsetIndex))));
          rir::offsetUnit::addMask(ouContainer, 0ul);

          p.populateOffsetUnit(ouContainer);

          rir::generalUtil::addSEXP(ddContainer, ouContainer, ddIdx);
          UNPROTECT(1);

          ddIdx++;
        });
        
        // rir::deserializerData::print(ddContainer, 2);

        saveDMetaAndCopyFiles(ddContainer, fName);

        testSavedDMeta(fName);

        UNPROTECT(protecc + 2);
      }
    }
  } else {
    std::cerr << "\"" << folderPath << "\" has no metas, nothing to process" << std::endl;
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
    std::cerr << "Usage: R_HOME=PATH_TO_GNUR_BUILD bcp path_to_folder" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout.setstate(std::ios_base::failbit);
  int fd = supress_stdout();
  int status = Rf_initEmbeddedR(argc - 2, argv);
  if (status < 0) {
    std::cerr << "R initialization failed." << std::endl;
    exit(EXIT_FAILURE);
  }
  resume_stdout(fd);
  std::cout.clear();

  if (argc != 3) {
    std::cerr << "Usage: bcp path_to_folder output_folder" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cerr << "R initialization successful!" << std::endl;

  auto bitcodesFolder = argv[1];

  inputPath = argv[1];
  outputPath = argv[2];

  RshBuiltinWeights::init();

  iterateOverMetadatasInDirectory(bitcodesFolder);

  Rf_endEmbeddedR(0);
  return 0;
}
