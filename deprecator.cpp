// #include <fstream>
// // Include fstream before Rinternals.h, otherwise a macro redef error might happen
// #include "Rinternals.h"

// #include "Rembedded.h"
// #include "dirent.h"
// #include <unistd.h>

// #include <fcntl.h>
// #include <sys/stat.h>


// #include "utils/serializerData.h"
// #include "utils/Debug.h"
// #include "utils/SerializedDataProcessor.h"
// #include "utils/RshBuiltinsMap.h"
// #include "utils/UMap.h"
// #include "utils/deserializerData.h"

#include <iostream>
// #include <stdlib.h>
// #include <functional>
// #include <sstream>

// #include "R/Protect.h"

// std::string dmetaPath;
// std::string bitcodePath;

// static void saveUpdatedDmeta(SEXP ddContainer) {

//   FILE *fptr;
//   fptr = fopen(dmetaPath.c_str(),"w");
//   if (!fptr) {
//     for (int i = 0; i < 10; i++) {
//       sleep(1);
//       std::cout << "[W]waiting to open: " << dmetaPath << std::endl;
//       fptr = fopen(dmetaPath.c_str(),"w");
//       if (fptr) break;
//     }

//     if (!fptr) {
//       std::cout << "[W]unable to open " << dmetaPath << std::endl;
//       Rf_error("[W]unable to open file!");
//     }
//   }
  
//   R_SaveToFile(ddContainer, fptr, 0);
//   fclose(fptr);

//   auto status = std::remove(bitcodePath.c_str());
//   if (status != 0) {
//     std::cout << "Failed to remove bitcode file: " << bitcodePath << ", status: " << status << std::endl;
//   }
//   // Remove deprecated bitcode
//   // remove

// }

// // https://stackoverflow.com/questions/46728680/how-to-temporarily-suppress-output-from-printf
// int supress_stdout() {
//   fflush(stdout);

//   int ret = dup(1);
//   int nullfd = open("/dev/null", O_WRONLY);
//   // check nullfd for error omitted
//   dup2(nullfd, 1);
//   close(nullfd);

//   return ret;
// }

// void resume_stdout(int fd) {
//   fflush(stdout);
//   dup2(fd, 1);
//   close(fd);
// }


int main(int argc, char** argv) {
  // bool rHomeSet = getenv("R_HOME") ? true : false;

  // if (!rHomeSet) {
  //   std::cerr << "R_HOME not set, set this to the GNUR built using the ./configure --enable-R-shlib flag" << std::endl;
  //   std::cerr << "Usage: ./deprecate.sh [.dmeta to process] [bitcode_to_deprecate]" << std::endl;
  //   exit(EXIT_FAILURE);
  // }

  // if (argc != 4) {
  //   std::cerr << "Usage: ./deprecate.sh [.dmeta to process] [bitcode_to_deprecate]" << std::endl;
  //   exit(EXIT_FAILURE);
  // }

  // dmetaPath = argv[2];
  // bitcodePath = argv[3];

  // std::cout << "[OBAP deprecator]" << std::endl;
  // std::cout << "DMeta Path        : " << dmetaPath << std::endl;
  // std::cout << "BC    Path        : "   << bitcodePath << std::endl;

  // std::cout.setstate(std::ios_base::failbit);
  // int fd = supress_stdout();
  // int status = Rf_initEmbeddedR(argc, argv);
  // if (status < 0) {
  //   std::cerr << "R initialization failed." << std::endl;
  //   exit(EXIT_FAILURE);
  // }
  // resume_stdout(fd);
  // std::cout.clear();

  // RshBuiltinWeights::init();


  // FILE *reader;
  // reader = fopen(dmetaPath.c_str(),"r");

  // if (!reader) {
  //   for (int i = 0; i < 10; i++) {
  //     sleep(1);
  //     reader = fopen(dmetaPath.c_str(),"r");
  //     if (reader) break;
  //   }

  //   if (!reader) {
  //     std::cerr << "unable to open " << dmetaPath << std::endl;
  //     Rf_error("unable to open file!");
  //   }
  // }

  // // Initialize the deserializing stream

  // rir::Protect protecc;
  // SEXP ddContainer;
  // protecc(ddContainer= R_LoadFromFile(reader, 0));
  // fclose(reader);

  // std::string base_filename = bitcodePath.substr(bitcodePath.find_last_of("/\\") + 1);
  // std::cout << "base bitcode name : " << base_filename << std::endl;

  // std::vector<std::string> bitcodeData;

  // std::string s = base_filename;
  // std::string delimiter = "_";

  // size_t pos = 0;
  // std::string token;

  // while ((pos = s.find(delimiter)) != std::string::npos) {
  //     token = s.substr(0, pos);
  //     bitcodeData.push_back(token);
  //     // std::cout << token << std::endl;
  //     s.erase(0, pos + delimiter.size());
  // }
  // // Get rid of extension .bc
  // s.erase(s.size()-3, 3);
  // bitcodeData.push_back(s);

  // assert(bitcodeData.size() == 3);

  // // std::cout << "=== Loaded deserializer meta ===" << std::endl;
  // // rir::deserializerData::print(ddContainer, 2);

  // rir::deserializerData::iterator(ddContainer, [&](SEXP offsetUnitContainer) {
  //   if (bitcodeData[1] != std::to_string(rir::offsetUnit::getOffsetIdxAsInt(offsetUnitContainer))) {
  //     return;
  //   }

  //   unsigned int numOffsets = Rf_length(offsetUnitContainer);
  //   for (int i = rir::offsetUnit::contextsStartingIndex(); i < numOffsets; i++) {
  //     SEXP contextUnitContainer = rir::generalUtil::getSEXP(offsetUnitContainer, i);
  //     int contains = -1;

  //     unsigned int numContexts = Rf_length(contextUnitContainer);
  //     for (int j = rir::contextUnit::binsStartingIndex(); j < numContexts; j++) {
  //       SEXP binaryUnitContainer = rir::generalUtil::getSEXP(contextUnitContainer, j);
  //       if (bitcodeData[2] == std::string(CHAR(PRINTNAME(rir::binaryUnit::getEpoch(binaryUnitContainer))))) {
  //         assert(contains == -1);
  //         contains = j;
  //       }
  //     }

  //     if (contains > -1) {
  //       SEXP newContextUnitContainer;
  //       protecc(newContextUnitContainer = Rf_allocVector(VECSXP, numContexts - 1));
  //       int it = 0;
  //       for (int j = 0; j < numContexts; j++) {
  //         SEXP binaryUnitContainer = rir::generalUtil::getSEXP(contextUnitContainer, j);
  //         if (j != contains) {
  //           rir::generalUtil::addSEXP(newContextUnitContainer, binaryUnitContainer, it++);
  //         }
  //       }

  //       int newSize = 0;
  //       unsigned int numContexts = Rf_length(newContextUnitContainer);
  //       for (int j = rir::contextUnit::binsStartingIndex(); j < numContexts; j++) {
  //         newSize++;
  //       }

  //       // If the number of binary units becomes one, then versioning will become 0
  //       if (newSize == 1) {
  //         rir::contextUnit::addVersioning(newContextUnitContainer, 0);
  //       }

  //       rir::generalUtil::addSEXP(offsetUnitContainer, newContextUnitContainer, i);
  //     }
  //   }
  // });

  // // std::cout << "=== Updated deserializer meta ===" << std::endl;
  // // rir::deserializerData::print(ddContainer, 2);

  // saveUpdatedDmeta(ddContainer);

  // Rf_endEmbeddedR(0);
  return 0;
}
