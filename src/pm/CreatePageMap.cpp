#include <vector>
#include <iostream>
#include "smap.hpp"
#include "Util.hpp"

using namespace std;

typedef SmapEntry MapEntry;  // For Linux

int main(int argc, char* argv[])
   {
   // Verify number of arguments
   if (argc < 3)
      error("Need smaps file and pid");
   
   // Read the smaps file
   vector<MapEntry> sMaps;
   string smaps(argv[1]);
   readSmapsFile(smaps.c_str(), NULL, sMaps);
   string pagemap("/proc/");
   pagemap.append(argv[2]).append("/pagemap");
   createPageMapFile(pagemap.c_str(), sMaps); 
   readPageMapFile();
   }
