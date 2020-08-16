#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <regex>
#include "CallSites.hpp"
#include "Util.hpp"

using namespace std;

/*
 !j9x 0x004FA4C0,0x000001D4	LargeObjectAllocateStats.cpp:31
 !j9x 0x004FA6D0,0x000001D4	LargeObjectAllocateStats.cpp:39
 !j9x 0x004FA8E0,0x000001E8	LargeObjectAllocateStats.cpp:45
 !j9x 0x004FAB00,0x000000A4	TLHAllocationInterface.cpp:53
*/
void readCallSitesFile(const char *filename, vector<CallSite>& callSites)
   {
   cout << "\nReading callSites file: " << string(filename) << endl;
   // Open the file
   ifstream myfile(filename);
   // check if successfull
   if (!myfile.is_open())
      {
      cerr << "Cannot open " << filename << endl;
      exit(-1);
      }
   string line;
   unsigned long long totalSize = 0;
   while (myfile.good())
      {
      getline(myfile, line);
      // skip empty lines
      size_t pos;
      if ((pos = line.find_first_not_of(" \t\n")) == string::npos)
         continue;
      // Skip lines that do not start with "!j9x"
      if (line.find("!j9x", pos) == string::npos)
         continue;

      std::cmatch result;       //!j9x 0xstart,0xsize	               filename:lineNo
#if 1
      std::regex  pattern1("\\s*\\!j9x 0x([0-9A-F]+),0x([0-9A-F]+)\\s+(\\S+):(\\d+)");
      std::regex  pattern2("\\s*\\!j9x 0x([0-9A-F]+),0x([0-9A-F]+)\\s+(\\S+)");
      bool match1, match2;
      if ((match1 = std::regex_search(line.c_str(), result, pattern1)) ||
          (match2 = std::regex_search(line.c_str(), result, pattern2)))
#else
      std::regex  pattern("\\s*\\!j9x 0x([0-9A-F]+),0x([0-9A-F]+)");
      bool match;
      if ((match = std::regex_search(line.c_str(), result, pattern)))
#endif	  
         {
         unsigned long long startAddr = hex2ull(result[1]);
         unsigned long long blockSize = hex2ull(result[2]);
         unsigned lineNo = match1 ? (unsigned)a2ull(result[4]) : 0;
         //cerr << "Match found: start=" << hex << startAddr << " blockSize=" << blockSize << " " << result[3] << ":" << lineNo << endl;
	 callSites.push_back(CallSite(startAddr, startAddr+blockSize, result[3], lineNo));
         totalSize += blockSize;
         }
      else // try another pattern
         {
         cerr << "No match for:" << line << endl;
         exit(-1);
         }
      }
   myfile.close();
   cout << "Total size of call sites: " << (totalSize >> 10) << " KB"<< endl;
   }

void readAllocTraceFile(const char *filename, vector<CallSite>& allocSites)
   {
   cout << "\nReading allocSites file: " << string(filename) << endl;
   // Open the file
   ifstream myfile(filename);
   // check if successfull
   if (!myfile.is_open())
      {
      cerr << "Cannot open " << filename << endl;
      exit(-1);
      }
   string line;
   unsigned long long totalSize = 0;
   while (myfile.good())
      {
      getline(myfile, line);
      // skip empty lines
      size_t pos;
      if ((pos = line.find_first_not_of(" \t\n")) == string::npos)
         continue;

      //          <func>  <start address> <size> <library name> <callsite name> <callsite address>
      // Example: malloc: 0x174a8d0 0x14 /home/ashu/data/builds/openj9/jdk8u232-b09/bin/../lib/amd64/jli/libjli.so (null) 7f2f14c8d9a9
      std::smatch result;
      std::regex pattern("(\\S*):\\s*0x([0-9a-f]+)\\s*0x([0-9a-f]+)\\s*(\\S*)\\s*\\S*\\s*([0-9a-f]+)");
      if (std::regex_search(line, result, pattern)) 
         {
	 string allocFunction = result.str(1);
         unsigned long long startAddr = hex2ull(result.str(2));
         unsigned long long blockSize = hex2ull(result.str(3));
         string libraryName = result.str(4);
         unsigned lineNo  = 0;
         //cout << "Match found: operation=" << result.str(1) << " start=" << hex << startAddr << " blockSize=" << blockSize << " " << result.str(4) << ":" << lineNo << endl;
	 allocSites.push_back(CallSite(startAddr, startAddr+blockSize, libraryName, lineNo, CallSite::getSource(libraryName), CallSite::getAllocType(allocFunction)));
	 totalSize += blockSize;
         }
      else // try another pattern
         {
         cerr << "No match for:" << line << endl;
         exit(-1);
         }
      }
   myfile.close();
   cout << "Total size of alloc tracing: " << (totalSize >> 10) << " KB"<< endl;
   }

void readMallocTraceFile(const char *filename, vector<CallSite>& allocSites)
   {
   cout << "\nReading callSites file: " << string(filename) << endl;
   // Open the file
   ifstream myfile(filename);
   // check if successfull
   if (!myfile.is_open())
      {
      cerr << "Cannot open " << filename << endl;
      exit(-1);
      }
   string line;
   unsigned long long totalSize = 0;
   while (myfile.good())
      {
      getline(myfile, line);
      // skip empty lines
      size_t pos;
      if ((pos = line.find_first_not_of(" \t\n")) == string::npos)
         continue;

      //          <start address> <size> at <library name> <callsite address>
      // Example: 0x174a8d0 0x14 at /home/ashu/data/builds/openj9/jdk8u232-b09/bin/../lib/amd64/jli/libjli.so 7f2f14c8d9a9
      std::smatch result;
      std::regex pattern("0x([0-9a-f]+)\\s*0x([0-9a-f]+)\\s*at\\s*(\\S*)\\s*0x([0-9a-f]+)");
      if (std::regex_search(line, result, pattern)) 
         {
         unsigned long long startAddr = hex2ull(result.str(1));
         unsigned long long blockSize = hex2ull(result.str(2));
         string libraryName = result.str(3);
         unsigned lineNo  = 0;
         //cout << "Match found: operation=" << result.str(1) << " start=" << hex << startAddr << " blockSize=" << blockSize << " " << result.str(4) << ":" << lineNo << endl;
	 allocSites.push_back(CallSite(startAddr, startAddr+blockSize, libraryName, lineNo, CallSite::getSource(libraryName), CallSite::ALLOC_TYPE_MALLOC));
	 totalSize += blockSize;
         }
      else // try another pattern
         {
         cerr << "No match for:" << line << endl;
         exit(-1);
         }
      }
   myfile.close();
   cout << "Total size of malloc tracing: " << (totalSize >> 10) << " KB"<< endl;
   }

void CallSite::print(std::ostream& os) const
   {
   const char *callSiteSourceString[CALLSITE_SOURCE_END] = {"CallSite_OpenJ9", "CallSite_Others"};

   os << hex << "CALLSITE Start=" << setfill('0') << setw(16) << getStart() <<
      " End=" << setfill('0') << setw(16) << getEnd() << dec <<
      " Size=" << setfill(' ') << setw(10) << sizeKB() << " " << _filename << ":" << _lineNo << " Source=" << callSiteSourceString[getSource()];
   }
